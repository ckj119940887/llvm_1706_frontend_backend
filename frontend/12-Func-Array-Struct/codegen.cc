#include "codegen.h"
using namespace llvm;
#include "llvm/IR/Verifier.h"

llvm::Value *CodeGen::VisitProgram(Program *p)
{
    for (const auto &decl : p->externalDecls)
    {
        decl->Accept(this);
    }
    return nullptr;
}

llvm::Value *CodeGen::VisitBlockStmt(BlockStmt *p)
{
    llvm::Value *lastVal = nullptr;
    for (const auto &stmt : p->nodeVec)
    {
        lastVal = stmt->Accept(this);
    }
    return lastVal;
}

llvm::Value *CodeGen::VisitDeclStmt(DeclStmt *p)
{
    llvm::Value *lastVal = nullptr;
    for (const auto &node : p->nodeVec)
    {
        lastVal = node->Accept(this);
    }
    return lastVal;
}

llvm::Value *CodeGen::VisitIfStmt(IfStmt *p)
{
    llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context, "cond", curFunc);
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context, "then");
    llvm::BasicBlock *elseBB = nullptr;
    if (p->elseNode)
        elseBB = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock *lastBB = llvm::BasicBlock::Create(context, "last");

    irBuilder.CreateBr(condBB);
    irBuilder.SetInsertPoint(condBB);
    llvm::Value *val = p->condNode->Accept(this);
    CastValue(val, irBuilder.getInt32Ty());
    // 整型比较指令
    llvm::Value *condVal = irBuilder.CreateICmpNE(val, irBuilder.getInt32(0));
    if (p->elseNode)
    {
        irBuilder.CreateCondBr(condVal, thenBB, elseBB);

        thenBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(thenBB);
        p->thenNode->Accept(this);
        irBuilder.CreateBr(lastBB);

        elseBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(elseBB);
        p->elseNode->Accept(this);
        irBuilder.CreateBr(lastBB);
    }
    else
    {
        irBuilder.CreateCondBr(condVal, thenBB, lastBB);

        thenBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(thenBB);
        p->thenNode->Accept(this);
        irBuilder.CreateBr(lastBB);
    }

    lastBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(lastBB);

    return nullptr;
}

llvm::Value *CodeGen::VisitForStmt(ForStmt *p)
{
    llvm::BasicBlock *initBB = llvm::BasicBlock::Create(context, "for.init", curFunc);
    llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context, "for.cond");
    llvm::BasicBlock *incBB = llvm::BasicBlock::Create(context, "for.inc");
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context, "for.body");
    llvm::BasicBlock *lastBB = llvm::BasicBlock::Create(context, "for.last");

    breakBBs.insert({p, lastBB});
    continueBBs.insert({p, incBB});

    irBuilder.CreateBr(initBB);
    irBuilder.SetInsertPoint(initBB);
    if (p->initNode)
    {
        p->initNode->Accept(this);
    }

    irBuilder.CreateBr(condBB);
    condBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(condBB);
    if (p->condNode)
    {
        llvm::Value *val = p->condNode->Accept(this);
        CastValue(val, irBuilder.getInt32Ty());
        llvm::Value *condVal = irBuilder.CreateICmpNE(val, irBuilder.getInt32(0));
        irBuilder.CreateCondBr(condVal, bodyBB, lastBB);
    }
    else
    {
        irBuilder.CreateBr(bodyBB);
    }

    bodyBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(bodyBB);
    if (p->bodyNode)
    {
        p->bodyNode->Accept(this);
    }

    irBuilder.CreateBr(incBB);
    incBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(incBB);
    if (p->incNode)
    {
        p->incNode->Accept(this);
    }

    irBuilder.CreateBr(condBB);

    breakBBs.erase(p);
    continueBBs.erase(p);

    lastBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(lastBB);

    return nullptr;
}

llvm::Value *CodeGen::VisitBreakStmt(BreakStmt *p)
{
    llvm::BasicBlock *bb = breakBBs[p->target.get()];
    irBuilder.CreateBr(bb);

    llvm::BasicBlock *out = llvm::BasicBlock::Create(context, "for.break.death", curFunc);
    irBuilder.SetInsertPoint(out);

    return nullptr;
}

llvm::Value *CodeGen::VisitContinueStmt(ContinueStmt *p)
{
    llvm::BasicBlock *bb = continueBBs[p->target.get()];
    irBuilder.CreateBr(bb);

    llvm::BasicBlock *out = llvm::BasicBlock::Create(context, "for.continue.death", curFunc);
    irBuilder.SetInsertPoint(out);

    return nullptr;
}

llvm::Value *CodeGen::VisitReturnStmt(ReturnStmt *p)
{
    if (p->expr)
    {
        llvm::Value *val = p->expr->Accept(this);
        return irBuilder.CreateRet(val);
    }
    else
    {
        return irBuilder.CreateRetVoid();
    }
}

llvm::Value *CodeGen::VisitBinaryExpr(BinaryExpr *binaryExpr)
{

    /*
        nsw	No Signed Wrap	假定不发生有符号溢出
        nuw	No Unsigned Wrap	假定不发生无符号溢出
    */
    llvm::Value *left = nullptr;
    llvm::Value *right = nullptr;
    if (binaryExpr->op != BinaryOp::logical_and && binaryExpr->op != BinaryOp::logical_or)
    {
        left = binaryExpr->left->Accept(this);
        right = binaryExpr->right->Accept(this);
    }

    switch (binaryExpr->op)
    {
    case BinaryOp::add:
    {
        llvm::Type *ty = binaryExpr->left->ty->Accept(this);
        if (ty->isPointerTy())
        {
            // GEP = GetElementPtr，LLVM 里唯一的地址计算指令。它不访问内存，只做「基地址 + 偏移」的算术，返回一个新指针。
            // CreateInBoundsGEP(Type *SrcElemTy, Value *Ptr, ArrayRef<Value*> IdxList)
            llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, left, {right});
            return newVal;
        }
        else
        {
            return irBuilder.CreateNSWAdd(left, right);
        }
    }
    case BinaryOp::sub:
    {
        llvm::Type *ty = binaryExpr->left->ty->Accept(this);
        if (ty->isPointerTy())
        {
            llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, left, {irBuilder.CreateNeg(right)});
            return newVal;
        }
        else
        {
            return irBuilder.CreateNSWSub(left, right);
        }
    }
    case BinaryOp::mul:
    {
        return irBuilder.CreateNSWMul(left, right);
    }
    case BinaryOp::div:
    {
        return irBuilder.CreateSDiv(left, right);
    }
    case BinaryOp::mod:
    {
        return irBuilder.CreateSRem(left, right);
    }
    case BinaryOp::bitwise_and:
    {
        return irBuilder.CreateAnd(left, right);
    }
    case BinaryOp::bitwise_or:
    {
        return irBuilder.CreateOr(left, right);
    }
    case BinaryOp::bitwise_xor:
    {
        return irBuilder.CreateXor(left, right);
    }
    case BinaryOp::logical_and:
    {
        llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(context, "nextBB", curFunc);
        llvm::BasicBlock *falseBB = llvm::BasicBlock::Create(context, "falseBB");
        llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "mergeBB");

        llvm::Value *left = binaryExpr->left->Accept(this);
        CastValue(left, irBuilder.getInt32Ty());
        llvm::Value *val = irBuilder.CreateICmpNE(left, irBuilder.getInt32(0));

        irBuilder.CreateCondBr(val, nextBB, falseBB);
        irBuilder.SetInsertPoint(nextBB);

        llvm::Value *right = binaryExpr->right->Accept(this);
        right = irBuilder.CreateICmpNE(right, irBuilder.getInt32(0));
        // 32bit的0或者1
        right = irBuilder.CreateZExt(right, irBuilder.getInt32Ty());
        irBuilder.CreateBr(mergeBB);

        // right这个值，所在的基本块，并不一定是之前的nextBB,应为右子树中可能还有其他基本块
        nextBB = irBuilder.GetInsertBlock();

        falseBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(falseBB);
        irBuilder.CreateBr(mergeBB);

        mergeBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(mergeBB);
        llvm::PHINode *phi = irBuilder.CreatePHI(irBuilder.getInt32Ty(), 2);
        phi->addIncoming(right, nextBB);
        phi->addIncoming(irBuilder.getInt32(0), falseBB);

        return phi;
    }
    case BinaryOp::logical_or:
    {
        llvm::BasicBlock *nextBB = llvm::BasicBlock::Create(context, "nextBB", curFunc);
        llvm::BasicBlock *trueBB = llvm::BasicBlock::Create(context, "trueBB");
        llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "mergeBB");

        llvm::Value *left = binaryExpr->left->Accept(this);
        CastValue(left, irBuilder.getInt32Ty());
        llvm::Value *val = irBuilder.CreateICmpNE(left, irBuilder.getInt32(0));

        irBuilder.CreateCondBr(val, trueBB, nextBB);
        irBuilder.SetInsertPoint(nextBB);

        llvm::Value *right = binaryExpr->right->Accept(this);
        right = irBuilder.CreateICmpNE(right, irBuilder.getInt32(0));
        // 32bit的0或者1
        right = irBuilder.CreateZExt(right, irBuilder.getInt32Ty());
        irBuilder.CreateBr(mergeBB);

        // right这个值，所在的基本块，并不一定是之前的nextBB,应为右子树中可能还有其他基本块
        nextBB = irBuilder.GetInsertBlock();

        trueBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(trueBB);
        irBuilder.CreateBr(mergeBB);

        mergeBB->insertInto(curFunc);
        irBuilder.SetInsertPoint(mergeBB);
        llvm::PHINode *phi = irBuilder.CreatePHI(irBuilder.getInt32Ty(), 2);
        phi->addIncoming(right, nextBB);
        phi->addIncoming(irBuilder.getInt32(1), trueBB);

        return phi;
    }
    case BinaryOp::left_shift:
    {
        return irBuilder.CreateShl(left, right);
    }
    case BinaryOp::right_shift:
    {
        return irBuilder.CreateAShr(left, right);
    }
    case BinaryOp::equal:
    {
        llvm::Value *val = irBuilder.CreateICmpEQ(left, right);
        return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
    }
    case BinaryOp::not_equal:
    {
        llvm::Value *val = irBuilder.CreateICmpNE(left, right);
        return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
    }
    case BinaryOp::less:
    {
        llvm::Value *val = irBuilder.CreateICmpSLT(left, right);
        return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
    }
    case BinaryOp::less_equal:
    {
        llvm::Value *val = irBuilder.CreateICmpSLE(left, right);
        return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
    }
    case BinaryOp::greater:
    {
        llvm::Value *val = irBuilder.CreateICmpSGT(left, right);
        return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
    }
    case BinaryOp::greater_equal:
    {
        llvm::Value *val = irBuilder.CreateICmpSGE(left, right);
        return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
    }
    case BinaryOp::assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        irBuilder.CreateStore(right, load->getPointerOperand());
        return right;
    }
    case BinaryOp::add_assign:
    {
        // a = a + 3;
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Type *ty = binaryExpr->left->ty->Accept(this);
        if (ty->isPointerTy())
        {
            llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, left, {right});
            irBuilder.CreateStore(newVal, load->getPointerOperand());
            return newVal;
        }
        else
        {
            llvm::Value *tmp = irBuilder.CreateAdd(left, right);
            irBuilder.CreateStore(tmp, load->getPointerOperand());
            return tmp;
        }
    }
    case BinaryOp::sub_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Type *ty = binaryExpr->left->ty->Accept(this);
        if (ty->isPointerTy())
        {
            llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, left, {irBuilder.CreateNeg(right)});
            irBuilder.CreateStore(newVal, load->getPointerOperand());
            return newVal;
        }
        else
        {
            llvm::Value *tmp = irBuilder.CreateSub(left, right);
            irBuilder.CreateStore(tmp, load->getPointerOperand());
            return tmp;
        }
    }
    case BinaryOp::mul_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateMul(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::div_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateSDiv(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::mod_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateSRem(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::bitwise_or_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateOr(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::bitwise_xor_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateXor(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::bitwise_and_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateAnd(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::left_shift_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateShl(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    case BinaryOp::right_shift_assign:
    {
        llvm::LoadInst *load = llvm::dyn_cast<llvm::LoadInst>(left);
        assert(load);
        llvm::Value *tmp = irBuilder.CreateAShr(left, right);
        irBuilder.CreateStore(tmp, load->getPointerOperand());
        return tmp;
    }
    default:
        break;
    }
    return nullptr;
}

llvm::Value *CodeGen::VisitNumberExpr(NumberExpr *numberExpr)
{
    return irBuilder.getInt32(numberExpr->tok.value);
}

llvm::Value *CodeGen::VisitThreeExpr(ThreeExpr *threeExpr)
{
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context, "then", curFunc);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(context, "else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(context, "merge");

    llvm::Value *val = threeExpr->cond->Accept(this);
    CastValue(val, irBuilder.getInt32Ty());
    llvm::Value *cond = irBuilder.CreateICmpNE(val, irBuilder.getInt32(0));
    irBuilder.CreateCondBr(cond, thenBB, elseBB);

    irBuilder.SetInsertPoint(thenBB);
    llvm::Value *thenVal = threeExpr->then->Accept(this);
    thenBB = irBuilder.GetInsertBlock();
    irBuilder.CreateBr(mergeBB);

    elseBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(elseBB);
    llvm::Value *elseVal = threeExpr->els->Accept(this);
    elseBB = irBuilder.GetInsertBlock();
    irBuilder.CreateBr(mergeBB);

    mergeBB->insertInto(curFunc);
    irBuilder.SetInsertPoint(mergeBB);

    llvm::PHINode *phi = irBuilder.CreatePHI(threeExpr->then->ty->Accept(this), 2);
    phi->addIncoming(thenVal, thenBB);
    phi->addIncoming(elseVal, elseBB);

    return phi;
}

llvm::Value *CodeGen::VisitUnaryExpr(UnaryExpr *unaryExpr)
{
    llvm::Value *val = unaryExpr->node->Accept(this);
    llvm::Type *ty = unaryExpr->node->ty->Accept(this);

    switch (unaryExpr->op)
    {
    case UnaryOp::positive:
        return val;
    case UnaryOp::negative:
    {
        return irBuilder.CreateNeg(val);
    }
    case UnaryOp::logical_not:
    {
        llvm::Value *tmp = irBuilder.CreateICmpNE(val, irBuilder.getInt32(0));
        return irBuilder.CreateZExt(irBuilder.CreateNot(tmp), irBuilder.getInt32Ty());
    }
    case UnaryOp::bitwise_not:
    {
        return irBuilder.CreateNot(val);
    }
    case UnaryOp::addr:
        return llvm::dyn_cast<LoadInst>(val)->getPointerOperand();
    case UnaryOp::deref:
    {
        llvm::Type *ty = unaryExpr->ty->Accept(this);
        return irBuilder.CreateLoad(ty, val);
    }
    case UnaryOp::inc:
    {
        // ++a
        if (ty->isPointerTy())
        {
            llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, val, {irBuilder.getInt32(1)});
            irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
            return newVal;
        }
        else if (ty->isIntegerTy())
        {
            llvm::Value *newVal = irBuilder.CreateAdd(val, irBuilder.getInt32(1));
            irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
            return newVal;
        }
        else
        {
            assert(0);
            return nullptr;
        }
    }
    case UnaryOp::dec:
    {
        // --a
        if (ty->isPointerTy())
        {
            llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, val, {irBuilder.getInt32(-1)});
            irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
            return newVal;
        }
        else if (ty->isIntegerTy())
        {
            llvm::Value *newVal = irBuilder.CreateSub(val, irBuilder.getInt32(1));
            irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
            return newVal;
        }
        else
        {
            assert(0);
            return nullptr;
        }
    }
    default:
        break;
    }

    return nullptr;
}

llvm::Value *CodeGen::VisitSizeOfExpr(SizeOfExpr *sizeOfExpr)
{
    std::shared_ptr<CType> ty = nullptr;
    if (sizeOfExpr->type)
    {
        ty = sizeOfExpr->type;
    }
    else
    {
        ty = sizeOfExpr->node->ty;
    }

    return irBuilder.getInt32(ty->GetSize());
}

llvm::Value *CodeGen::VisitPostIncExpr(PostIncExpr *postIncExpr)
{
    // p++
    llvm::Value *val = postIncExpr->left->Accept(this);
    llvm::Type *ty = postIncExpr->left->ty->Accept(this);

    if (ty->isPointerTy())
    {
        llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, val, {irBuilder.getInt32(1)});
        irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
        return val;
    }
    else if (ty->isIntegerTy())
    {
        llvm::Value *newVal = irBuilder.CreateAdd(val, irBuilder.getInt32(1));
        irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
        return val;
    }
    else
    {
        assert(0);
        return nullptr;
    }
}

llvm::Value *CodeGen::VisitPostDecExpr(PostDecExpr *postDecExpr)
{
    llvm::Value *val = postDecExpr->left->Accept(this);
    llvm::Type *ty = postDecExpr->left->ty->Accept(this);

    if (ty->isPointerTy())
    {
        llvm::Value *newVal = irBuilder.CreateInBoundsGEP(ty, val, {irBuilder.getInt32(-1)});
        irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
        return val;
    }
    else if (ty->isIntegerTy())
    {
        llvm::Value *newVal = irBuilder.CreateSub(val, irBuilder.getInt32(1));
        irBuilder.CreateStore(newVal, llvm::dyn_cast<LoadInst>(val)->getPointerOperand());
        return val;
    }
    else
    {
        assert(0);
        return nullptr;
    }
}

llvm::Value *CodeGen::VisitPostSubscript(PostSubscript *postSubscript)
{
    llvm::Type *ty = postSubscript->ty->Accept(this);
    llvm::Value *left = postSubscript->left->Accept(this);
    llvm::Value *offset = postSubscript->node->Accept(this);

    llvm::Value *addr;
    if (left->getType()->isPointerTy())
    {
        addr = irBuilder.CreateInBoundsGEP(ty, left, {offset});
    }
    else if (left->getType()->isArrayTy())
    {
        addr = irBuilder.CreateInBoundsGEP(ty, llvm::dyn_cast<LoadInst>(left)->getPointerOperand(), {offset});
    }
    else
    {
        assert(0);
    }

    return irBuilder.CreateLoad(ty, addr);
}

// a.b -> base[a] + offset[b]
llvm::Value *CodeGen::VisitPostMemberDotExpr(PostMemberDotExpr *postMemberDot)
{
    llvm::Value *leftValue = postMemberDot->left->Accept(this);
    llvm::Type *leftType = postMemberDot->left->ty->Accept(this);

    CRecordType *cLeftType = llvm::dyn_cast<CRecordType>(postMemberDot->left->ty.get());
    TagKind tagKind = cLeftType->GetTagKind();

    llvm::Type *memType = postMemberDot->member.ty->Accept(this);

    llvm::Value *zero = irBuilder.getInt32(0);
    if (tagKind == TagKind::kStruct)
    {
        llvm::Value *next = irBuilder.getInt32(postMemberDot->member.elemIdx);
        llvm::Value *memAddr = irBuilder.CreateInBoundsGEP(leftType, llvm::dyn_cast<LoadInst>(leftValue)->getPointerOperand(), {zero, next});
        return irBuilder.CreateLoad(memType, memAddr);
    }
    else
    {
        llvm::Value *memAddr = irBuilder.CreateInBoundsGEP(leftType, llvm::dyn_cast<LoadInst>(leftValue)->getPointerOperand(), {zero, zero});
        llvm::Value *cast = irBuilder.CreateBitCast(memAddr, llvm::PointerType::getUnqual(memType));
        return irBuilder.CreateLoad(memType, cast);
    }
}

llvm::Value *CodeGen::VisitPostMemberArrowExpr(PostMemberArrowExpr *postMemberArrow)
{
    // ptr
    llvm::Value *leftValue = postMemberArrow->left->Accept(this);
    llvm::Type *leftType = postMemberArrow->left->ty->Accept(this);
    CPointType *cLeftPointerType = llvm::dyn_cast<CPointType>(postMemberArrow->left->ty.get());

    CRecordType *cLeftType = llvm::dyn_cast<CRecordType>(cLeftPointerType->GetBaseType().get());
    TagKind tagKind = cLeftType->GetTagKind();

    llvm::Type *memType = postMemberArrow->member.ty->Accept(this);

    llvm::Value *zero = irBuilder.getInt32(0);
    if (tagKind == TagKind::kStruct)
    {
        llvm::Value *next = irBuilder.getInt32(postMemberArrow->member.elemIdx);
        llvm::Value *memAddr = irBuilder.CreateInBoundsGEP(cLeftPointerType->GetBaseType()->Accept(this), leftValue, {zero, next});
        return irBuilder.CreateLoad(memType, memAddr);
    }
    else
    {
        llvm::Value *memAddr = irBuilder.CreateInBoundsGEP(cLeftPointerType->GetBaseType()->Accept(this), leftValue, {zero, zero});
        llvm::Value *cast = irBuilder.CreateBitCast(memAddr, llvm::PointerType::getUnqual(memType));
        return irBuilder.CreateLoad(memType, cast);
    }
}
llvm::Value *CodeGen::VisitPostFuncCall(PostFuncCall *expr)
{
    llvm::Value *funcAddr = expr->left->Accept(this);
    llvm::FunctionType *funcTy = llvm::dyn_cast<llvm::FunctionType>(expr->left->ty->Accept(this));

    CFuncType *cFuncTy = llvm::dyn_cast<CFuncType>(expr->left->ty.get());
    const auto &param = cFuncTy->GetParams();

    int i = 0;
    llvm::SmallVector<llvm::Value *> args;
    for (const auto &arg : expr->args)
    {
        llvm::Value *val = arg->Accept(this);
        CastValue(val, param[i].type->Accept(this));
        args.push_back(val);
        i++;
    }
    return irBuilder.CreateCall(funcTy, funcAddr, args);
}

llvm::Value *CodeGen::VisitVariableDecl(VariableDecl *decl)
{
    llvm::Type *ty = decl->ty->Accept(this);

    // CreateAlloca 是在「栈上为变量分配一块内存」
    /*
        ty — 要分配的类型。这里是 getInt32Ty(),即 i32(32 位整数)。
        nullptr — 分配的元素个数(数组长度)。nullptr 表示 1 个。
        decl->name — 给生成的 IR 变量起的名字(只是为了可读性)。
        返回值是「指向这块内存的指针」。所以 value 的类型不是 i32 值本身,而是 i32*(那块内存的地址)。
    */
    /*
        操作	        指令	在这份代码里对应
        分配变量空间	alloca	VisitVariableDecl(当前这个函数)
        给变量赋值	    store	VisitAssignExpr(还没实现,返回 nullptr)
        读取变量	    load	VisitVariableAccessExpr(还没实现)
    */
    llvm::StringRef text(decl->tok.ptr, decl->tok.len);

    if (decl->isGloabl)
    {
        auto GetInitValueByOffset = [&](const std::vector<int> &offset) -> std::shared_ptr<VariableDecl::InitValue>
        {
            const auto &initVals = decl->initValues;
            for (const auto &n : initVals)
            {
                if (n->offsetList.size() != offset.size())
                {
                    continue;
                }
                bool find = true;
                for (int i = 0; i < offset.size(); i++)
                {
                    if (n->offsetList[i] != offset[i])
                    {
                        find = false;
                        break;
                    }
                }
                if (find)
                {
                    return n;
                }
            }
            return nullptr;
        };

        auto GetInitialValue = [&](llvm::Type *ty, auto &&func, std::vector<int> offset) -> llvm::Constant *
        {
            if (ty->isIntegerTy())
            {
                std::shared_ptr<VariableDecl::InitValue> init = GetInitValueByOffset(offset);
                if (init)
                {
                    auto *c = init->value->Accept(this);
                    CastValue(c, ty);
                    return llvm::dyn_cast<llvm::Constant>(c);
                }
                return irBuilder.getInt32(0);
            }
            else if (ty->isPointerTy())
            {
                std::shared_ptr<VariableDecl::InitValue> init = GetInitValueByOffset(offset);
                if (init)
                {
                    auto *c = init->value->Accept(this);
                    CastValue(c, ty);
                    return llvm::dyn_cast<llvm::Constant>(c);
                }
                return llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(ty));
            }
            else if (ty->isStructTy())
            {
                llvm::StructType *structTy = llvm::dyn_cast<llvm::StructType>(ty);
                int size = structTy->getStructNumElements();
                llvm::SmallVector<llvm::Constant *> elemConstantVal;
                for (int i = 0; i < size; i++)
                {
                    offset.push_back(i);
                    elemConstantVal.push_back(func(structTy->getStructElementType(i), func, offset));
                    offset.pop_back();
                }
                return llvm::ConstantStruct::get(structTy, elemConstantVal);
            }
            else if (ty->isArrayTy())
            {
                llvm::ArrayType *arrTy = llvm::dyn_cast<llvm::ArrayType>(ty);
                int size = arrTy->getArrayNumElements();
                llvm::SmallVector<llvm::Constant *> elemConstantVal;
                for (int i = 0; i < size; i++)
                {
                    offset.push_back(i);
                    elemConstantVal.push_back(func(arrTy->getArrayElementType(), func, offset));
                    offset.pop_back();
                }
                return llvm::ConstantArray::get(arrTy, elemConstantVal);
            }
            else
            {
                return nullptr;
            }
        };

        llvm::GlobalVariable *globalVar = new llvm::GlobalVariable(*module, ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, text);
        globalVar->setAlignment(llvm::Align(decl->ty->GetAlign()));
        globalVar->setInitializer(GetInitialValue(ty, GetInitialValue, {0}));
        AddGlobalVarToMap(globalVar, ty, text);
        return globalVar;
    }
    else
    {
        llvm::IRBuilder<> tmp(&curFunc->getEntryBlock(), curFunc->getEntryBlock().begin());
        auto *alloc = tmp.CreateAlloca(ty, nullptr, text);
        alloc->setAlignment(llvm::Align(decl->ty->GetAlign()));
        AddLocalVarToMap(alloc, ty, text);

        if (decl->initValues.size() > 0)
        {
            if (decl->initValues.size() == 1)
            {
                llvm::Value *initValue = decl->initValues[0]->value->Accept(this);
                CastValue(initValue, decl->initValues[0]->declType->Accept(this));
                irBuilder.CreateStore(initValue, alloc);
            }
            else
            {
                if (llvm::ArrayType *arrType = llvm::dyn_cast<llvm::ArrayType>(ty))
                {
                    for (const auto &initValue : decl->initValues)
                    {

                        llvm::SmallVector<llvm::Value *> vec;
                        for (auto &offset : initValue->offsetList)
                        {
                            vec.push_back(irBuilder.getInt32(offset));
                        }
                        llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, alloc, vec);
                        llvm::Value *v = initValue->value->Accept(this);
                        CastValue(v, initValue->declType->Accept(this));
                        irBuilder.CreateStore(v, addr);
                    }
                }
                else if (llvm::StructType *structType = llvm::dyn_cast<llvm::StructType>(ty))
                {
                    CRecordType *cStructType = llvm::dyn_cast<CRecordType>(decl->ty.get());
                    TagKind tagKind = cStructType->GetTagKind();

                    if (tagKind == TagKind::kStruct)
                    {
                        for (const auto &initValue : decl->initValues)
                        {
                            llvm::SmallVector<llvm::Value *> vec;
                            for (auto &offset : initValue->offsetList)
                            {
                                vec.push_back(irBuilder.getInt32(offset));
                            }
                            llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, alloc, vec);
                            llvm::Value *v = initValue->value->Accept(this);
                            CastValue(v, initValue->declType->Accept(this));
                            irBuilder.CreateStore(v, addr);
                        }
                    }
                    else
                    {
                        assert(decl->initValues.size() == 1);
                        llvm::SmallVector<llvm::Value *> vec;
                        const auto &initValue = decl->initValues[0];
                        for (auto &offset : initValue->offsetList)
                        {
                            vec.push_back(irBuilder.getInt32(offset));
                        }
                        llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, alloc, vec);
                        llvm::Value *v = initValue->value->Accept(this);
                        CastValue(v, initValue->declType->Accept(this));
                        irBuilder.CreateStore(v, addr);
                    }
                }
                else
                {
                    assert(0);
                }
            }
        }
        return alloc;
    }
}

llvm::Value *CodeGen::VisitFuncDecl(FuncDecl *decl)
{
    ClearVarScope();
    CFuncType *cFuncTy = llvm::dyn_cast<CFuncType>(decl->ty.get());
    const auto &params = cFuncTy->GetParams();
    llvm::Function *func = module->getFunction(cFuncTy->GetName());

    if (!func)
    {
        // 第一个参数是返回类型
        // 第二个参数是参数类型
        llvm::FunctionType *funcTy = llvm::dyn_cast<llvm::FunctionType>(decl->ty->Accept(this));
        func = Function::Create(funcTy, GlobalValue::ExternalLinkage, cFuncTy->GetName(), module.get());
        AddGlobalVarToMap(func, funcTy, cFuncTy->GetName());

        int i = 0;
        for (auto &arg : func->args())
        {
            arg.setName(params[i++].name);
        }
    }

    /// 只有声明没有函数体, 到此为止, 不用生成 entry 块
    if (!decl->blockStmt)
    {
        return nullptr;
    }

    BasicBlock *entryBB = BasicBlock::Create(context, "entry", func);
    irBuilder.SetInsertPoint(entryBB);

    curFunc = func;

    PushScope();

    // 存放变量的分配
    int i = 0;
    for (auto &arg : func->args())
    {
        auto *alloc = irBuilder.CreateAlloca(arg.getType(), nullptr, params[i].name);
        alloc->setAlignment(llvm::Align(params[i].type->GetAlign()));
        irBuilder.CreateStore(&arg, alloc);

        AddLocalVarToMap(alloc, arg.getType(), params[i].name);
        i++;
    }

    // 当前函数
    curFunc = func;

    decl->blockStmt->Accept(this);
    /*
        把一个 C 字符串常量放进模块的全局区,然后返回指向这段字符串首字符的指针(i8*)。
        C 里像 "expr val: %d\n" 这样的字符串字面量,需要:
            有一块只读的全局内存存放这些字符(以及结尾的 \0);
            传给 printf 的其实是指向这块内存开头的指针,而不是字符串本身。

        CreateGlobalStringPtr 一次帮你把这两件事都做了。

        它背后生成的 IR
        调用 CreateGlobalStringPtr("expr val: %d\n") 大致会生成:

        ; 一个全局常量,存放字符串内容 + 结尾的 \00
        @0 = private unnamed_addr constant [14 x i8] c"expr val: %d\0A\00"
        而它的返回值就是指向 @0 第一个字节的 i8* 指针。这个指针正好就是 printf 第一个参数(格式字符串)需要的类型。
    */

    /// 函数体生成完之后, 当前基本块若还缺终结指令, 补一条默认的 return
    llvm::BasicBlock *curBB = irBuilder.GetInsertBlock();
    if (curBB && (curBB->empty() || !curBB->back().isTerminator()))
    {
        if (cFuncTy->GetRetType()->GetKind() == CType::TY_Void)
        {
            irBuilder.CreateRetVoid();
        }
        else
        {
            irBuilder.CreateRet(irBuilder.getInt32(0));
        }
    }

    PopScope();

    // return instruction
    verifyFunction(*func);

    if (verifyModule(*module, &llvm::outs()))
    {
        module->print(llvm::outs(), nullptr);
    }

    return nullptr;
}

// a = 3; 这是一个右值表达式，并不是左值表达式
// llvm::Value *CodeGen::VisitAssignExpr(AssignExpr *expr)
// {
//     auto left = expr->left;
//     VariableAccessExpr *varAccessExpr = (VariableAccessExpr *)left.get();
//     llvm::StringRef text(varAccessExpr->tok.ptr, varAccessExpr->tok.len);
//     std::pair pair = varAddrTypeMap[text];
//     llvm::Value *leftValAddr = pair.first;
//     llvm::Type *ty = pair.second;
//     llvm::Value *rightValue = expr->right->Accept(this);
//     // CreateStore — 把值「写入」变量的内存
//     /*
//         第一个 rightValue — 要存的值(比如 i32 的 10)。
//         第二个 leftValAddr — 存到哪里,也就是之前 alloca 返回的地址(i32*)。
//     */
//     irBuilder.CreateStore(rightValue, leftValAddr);
//     return irBuilder.CreateLoad(ty, leftValAddr, text);
// }

llvm::Value *CodeGen::VisitVariableAccessExpr(VariableAccessExpr *expr)
{
    llvm::StringRef text(expr->tok.ptr, expr->tok.len);
    const auto &[addr, ty] = GetVarByName(text);

    if (ty->isFunctionTy())
    {
        return addr;
    }

    // CreateLoad — 从变量的内存「读出」值
    /*
        ty — 读出来的值是什么类型(这里 i32)。
        varAddr — 从哪里读,同样是 alloca 的那个地址。
        factorExpr->name — 给读出结果的 IR 寄存器起个名字(可读性)。

        store 时,要存的值本身已经带着类型(rightValue 知道自己是 i32),所以不用再单独告诉它类型。
        load 时,只给一个地址,新版 LLVM 的指针 i32* 已经退化成不带类型的 ptr,光看地址不知道该读几个字节、按什么类型解释,所以必须显式传 ty 说明「按 i32 读」。
    */
    return irBuilder.CreateLoad(ty, addr, text);
}

llvm::Type *CodeGen::VisitPrimaryType(CPrimaryType *ty)
{
    if (ty->GetKind() == CType::TY_Int)
    {
        return irBuilder.getInt32Ty();
    }
    else if (ty->GetKind() == CType::TY_Void)
    {
        return irBuilder.getVoidTy();
    }
    assert(0);
    return nullptr;
}

llvm::Type *CodeGen::VisitPointType(CPointType *ty)
{
    llvm::Type *baseType = ty->GetBaseType()->Accept(this);
    return llvm::PointerType::getUnqual(baseType);
}

llvm::Type *CodeGen::VisitArrayType(CArrayType *ty)
{
    llvm::Type *elementType = ty->GetElementType()->Accept(this);
    return llvm::ArrayType::get(elementType, ty->GetElementCount());
}

llvm::Type *CodeGen::VisitRecordType(CRecordType *ty)
{
    llvm::StructType *structType = nullptr;
    structType = llvm::StructType::getTypeByName(context, ty->GetName());
    if (structType)
    {
        return structType;
    }
    structType = llvm::StructType::create(context, ty->GetName());
    // structType->setName(ty->GetName());

    TagKind tagKind = ty->GetTagKind();
    if (tagKind == TagKind::kStruct)
    {
        llvm::SmallVector<llvm::Type *> vec;
        for (const auto &m : ty->GetMembers())
        {
            vec.push_back(m.ty->Accept(this));
        }
        structType->setBody(vec);
    }
    else
    {
        llvm::SmallVector<llvm::Type *> vec;
        const auto &m = ty->GetMembers();
        int idx = ty->GetMaxElementIdx();
        structType->setBody(m[idx].ty->Accept(this));
    }

    return structType;
}
llvm::Type *CodeGen::VisitFuncType(CFuncType *ty)
{
    llvm::Type *retTy = ty->GetRetType()->Accept(this);
    llvm::SmallVector<llvm::Type *> argsType;
    for (const auto &arg : ty->GetParams())
    {
        argsType.push_back(arg.type->Accept(this));
    }
    return llvm::FunctionType::get(retTy, argsType, false);
}

void CodeGen::AddLocalVarToMap(llvm::Value *addr, llvm::Type *ty, llvm::StringRef name)
{
    localVarMap.back().insert({name, {addr, ty}});
}

void CodeGen::AddGlobalVarToMap(llvm::Value *addr, llvm::Type *ty, llvm::StringRef name)
{
    globalVarMap.insert({name, {addr, ty}});
}

std::pair<llvm::Value *, llvm::Type *> CodeGen::GetVarByName(llvm::StringRef name)
{
    for (auto it = localVarMap.rbegin(); it != localVarMap.rend(); it++)
    {
        if (it->find(name) != it->end())
        {
            return (*it)[name];
        }
    }
    assert(globalVarMap.find(name) != globalVarMap.end());
    return globalVarMap[name];
}

void CodeGen::PushScope()
{
    localVarMap.emplace_back();
}

void CodeGen::PopScope()
{
    localVarMap.pop_back();
}

void CodeGen::ClearVarScope()
{
    localVarMap.clear();
}

void CodeGen::CastValue(llvm::Value *&val, llvm::Type *destTy)
{
    if (val->getType() != destTy)
    {
        if (val->getType()->isIntegerTy())
        {
            if (destTy->isPointerTy())
            {
                val = irBuilder.CreateIntToPtr(val, destTy);
            }
        }
        else if (val->getType()->isPointerTy())
        {
            if (destTy->isIntegerTy())
            {
                val = irBuilder.CreatePtrToInt(val, destTy);
            }
        }
        else if (val->getType()->isArrayTy())
        {
            if (destTy->isPointerTy())
            {
                auto *load = llvm::dyn_cast<llvm::LoadInst>(val);
                val = load->getPointerOperand();
            }
        }
    }
}
