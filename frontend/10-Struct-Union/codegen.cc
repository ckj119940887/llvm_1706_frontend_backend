#include "codegen.h"
using namespace llvm;
#include "llvm/IR/Verifier.h"

llvm::Value *CodeGen::VisitProgram(Program *p)
{
    // printf
    // 第一个参数是返回类型
    // 第二个参数是参数类型
    auto printfFuncType = FunctionType::get(irBuilder.getInt32Ty(), {irBuilder.getInt8PtrTy()}, true);
    auto printfFunc = Function::Create(printfFuncType, GlobalValue::ExternalLinkage, "printf", module.get());

    // main function
    auto mFuncType = FunctionType::get(irBuilder.getInt32Ty(), false);
    auto mFunc = Function::Create(mFuncType, GlobalValue::ExternalLinkage, "main", module.get());

    BasicBlock *entryBB = BasicBlock::Create(context, "entry", mFunc);
    irBuilder.SetInsertPoint(entryBB);

    // 当前函数
    curFunc = mFunc;

    llvm::Value *lastVal = nullptr;
    lastVal = p->node->Accept(this);
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
    // if (lastVal)
    //     irBuilder.CreateCall(printfFunc, {irBuilder.CreateGlobalStringPtr("expr val: %d\n"), lastVal});
    // else
    //     irBuilder.CreateCall(printfFunc, {irBuilder.CreateGlobalStringPtr("last inst is not expr")});

    // return instruction
    llvm::Value *ret = irBuilder.CreateRet(lastVal);
    verifyFunction(*mFunc);

    if (verifyModule(*module, &llvm::outs()))
    {
        module->print(llvm::outs(), nullptr);
    }

    return ret;
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
    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(context, "then", curFunc);
    llvm::BasicBlock *elseBB = nullptr;
    if (p->elseNode)
        elseBB = llvm::BasicBlock::Create(context, "else", curFunc);
    llvm::BasicBlock *lastBB = llvm::BasicBlock::Create(context, "last", curFunc);

    irBuilder.CreateBr(condBB);
    irBuilder.SetInsertPoint(condBB);
    llvm::Value *val = p->condNode->Accept(this);
    // 整型比较指令
    llvm::Value *condVal = irBuilder.CreateICmpNE(val, irBuilder.getInt32(0));
    if (p->elseNode)
    {
        irBuilder.CreateCondBr(condVal, thenBB, elseBB);

        irBuilder.SetInsertPoint(thenBB);
        p->thenNode->Accept(this);
        irBuilder.CreateBr(lastBB);

        irBuilder.SetInsertPoint(elseBB);
        p->elseNode->Accept(this);
        irBuilder.CreateBr(lastBB);
    }
    else
    {
        irBuilder.CreateCondBr(condVal, thenBB, lastBB);

        irBuilder.SetInsertPoint(thenBB);
        p->thenNode->Accept(this);
        irBuilder.CreateBr(lastBB);
    }

    irBuilder.SetInsertPoint(lastBB);

    return nullptr;
}

llvm::Value *CodeGen::VisitForStmt(ForStmt *p)
{
    llvm::BasicBlock *initBB = llvm::BasicBlock::Create(context, "for.init", curFunc);
    llvm::BasicBlock *condBB = llvm::BasicBlock::Create(context, "for.cond", curFunc);
    llvm::BasicBlock *incBB = llvm::BasicBlock::Create(context, "for.inc", curFunc);
    llvm::BasicBlock *bodyBB = llvm::BasicBlock::Create(context, "for.body", curFunc);
    llvm::BasicBlock *lastBB = llvm::BasicBlock::Create(context, "for.last", curFunc);

    breakBBs.insert({p, lastBB});
    continueBBs.insert({p, incBB});

    irBuilder.CreateBr(initBB);
    irBuilder.SetInsertPoint(initBB);
    if (p->initNode)
    {
        p->initNode->Accept(this);
    }

    irBuilder.CreateBr(condBB);
    irBuilder.SetInsertPoint(condBB);
    if (p->condNode)
    {
        llvm::Value *val = p->condNode->Accept(this);
        llvm::Value *condVal = irBuilder.CreateICmpNE(val, irBuilder.getInt32(0));
        irBuilder.CreateCondBr(condVal, bodyBB, lastBB);
    }
    else
    {
        irBuilder.CreateBr(bodyBB);
    }

    irBuilder.SetInsertPoint(bodyBB);
    if (p->bodyNode)
    {
        p->bodyNode->Accept(this);
    }

    irBuilder.CreateBr(incBB);
    irBuilder.SetInsertPoint(incBB);
    if (p->incNode)
    {
        p->incNode->Accept(this);
    }

    irBuilder.CreateBr(condBB);

    breakBBs.erase(p);
    continueBBs.erase(p);

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
    llvm::Type *ty = nullptr;
    if (sizeOfExpr->type)
    {
        ty = sizeOfExpr->type->Accept(this);
    }
    else
    {
        ty = sizeOfExpr->node->ty->Accept(this);
    }

    if (ty->isPointerTy())
    {
        return irBuilder.getInt32(8);
    }
    else if (ty->isIntegerTy())
    {
        return irBuilder.getInt32(4);
    }
    else
    {
        assert(0);
        return nullptr;
    }
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
    llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, llvm::dyn_cast<LoadInst>(left)->getPointerOperand(), {offset});
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

    llvm::IRBuilder<> tmp(&curFunc->getEntryBlock(), curFunc->getEntryBlock().begin());
    llvm::Value *value = tmp.CreateAlloca(ty, nullptr, text);
    varAddrTypeMap.insert({text, {value, ty}});

    if (decl->initValues.size() > 0)
    {
        if (decl->initValues.size() == 1)
        {
            llvm::Value *initValue = decl->initValues[0]->value->Accept(this);
            irBuilder.CreateStore(initValue, value);
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
                    llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, value, vec);
                    llvm::Value *v = initValue->value->Accept(this);
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
                        llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, value, vec);
                        llvm::Value *v = initValue->value->Accept(this);
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
                    llvm::Value *addr = irBuilder.CreateInBoundsGEP(ty, value, vec);
                    llvm::Value *v = initValue->value->Accept(this);
                    irBuilder.CreateStore(v, addr);
                }
            }
            else
            {
                assert(0);
            }
        }
    }

    return value;
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
    std::pair pair = varAddrTypeMap[text];
    llvm::Value *varAddr = pair.first;
    llvm::Type *ty = pair.second;
    // CreateLoad — 从变量的内存「读出」值
    /*
        ty — 读出来的值是什么类型(这里 i32)。
        varAddr — 从哪里读,同样是 alloca 的那个地址。
        factorExpr->name — 给读出结果的 IR 寄存器起个名字(可读性)。

        store 时,要存的值本身已经带着类型(rightValue 知道自己是 i32),所以不用再单独告诉它类型。
        load 时,只给一个地址,新版 LLVM 的指针 i32* 已经退化成不带类型的 ptr,光看地址不知道该读几个字节、按什么类型解释,所以必须显式传 ty 说明「按 i32 读」。
    */
    return irBuilder.CreateLoad(ty, varAddr, text);
}

llvm::Type *CodeGen::VisitPrimaryType(CPrimaryType *ty)
{
    if (ty->GetKind() == CType::TY_Int)
    {
        return irBuilder.getInt32Ty();
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
        for (const auto &m : ty->GetMember())
        {
            vec.push_back(m.ty->Accept(this));
        }
        structType->setBody(vec);
    }
    else
    {
        llvm::SmallVector<llvm::Type *> vec;
        const auto &m = ty->GetMember();
        int idx = ty->GetMaxElementIdx();
        structType->setBody(m[idx].ty->Accept(this));
    }

    return structType;
}