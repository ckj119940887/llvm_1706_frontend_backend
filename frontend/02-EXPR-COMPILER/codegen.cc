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

    for (auto &expr : p->exprVec)
    {
        llvm::Value *p = expr->Accept(this);
        irBuilder.CreateCall(printfFunc, {irBuilder.CreateGlobalStringPtr("expr val: %d\n"), p});
    }

    // return instruction
    llvm::Value *ret = irBuilder.CreateRet(irBuilder.getInt32(0));

    verifyFunction(*mFunc);

    module->print(llvm::outs(), nullptr);
    return ret;
}

llvm::Value *CodeGen::VisitBinaryExpr(BinaryExpr *binaryExpr)
{
    llvm::Value *left = binaryExpr->left->Accept(this);
    llvm::Value *right = binaryExpr->right->Accept(this);

    switch (binaryExpr->op)
    {
    case OpCode::add:
    {
        return irBuilder.CreateNSWAdd(left, right);
    }
    case OpCode::sub:
    {
        return irBuilder.CreateNSWSub(left, right);
    }
    case OpCode::mul:
    {
        return irBuilder.CreateNSWMul(left, right);
    }
    case OpCode::div:
    {
        return irBuilder.CreateSDiv(left, right);
    }
    default:
        break;
    }
    return nullptr;
}

llvm::Value *CodeGen::VisitFactorExpr(FactorExpr *factorExpr)
{
    return irBuilder.getInt32(factorExpr->number);
}