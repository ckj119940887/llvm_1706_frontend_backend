#include <iostream>
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Verifier.h"

using namespace llvm;

// llvm hello world
// source -> llvm ir -> run -> output "hello"

// .ll
// .ll -> module -> {global*, function*}
// function -> {basicBlock+}, at least one basic block (entryBlock)

// gen -> module -> main -> puts("hello")

int main()
{
    // 通过标准C++的输出
    // std::cout << "hello" << std::endl;

    // ir 输出
    LLVMContext context;
    Module m("helloworld", context);
    IRBuilder<> irBuilder(context);

    // int puts(char *)
    FunctionType *putsFuncType = FunctionType::get(irBuilder.getInt32Ty(), {irBuilder.getInt8PtrTy()}, false);
    Function *putsFunc = Function::Create(putsFuncType, GlobalValue::LinkageTypes::ExternalLinkage, "puts", m);

    llvm::Constant *c = ConstantDataArray::getString(context, "hello, world");
    GlobalVariable *gvar = new GlobalVariable(m, c->getType(), true, GlobalValue::LinkageTypes::PrivateLinkage, c, "gStr");

    FunctionType *mainFunctionType = FunctionType::get(irBuilder.getInt32Ty(), false);
    Function *mainFunc = Function::Create(mainFunctionType, GlobalValue::LinkageTypes::ExternalLinkage, "main", m);
    BasicBlock *entryBB = BasicBlock::Create(context, "entry", mainFunc);
    irBuilder.SetInsertPoint(entryBB);

    llvm::Value *gep = irBuilder.CreateGEP(gvar->getType(), gvar, {irBuilder.getInt64(0), irBuilder.getInt64(0)});

    irBuilder.CreateCall(putsFunc, {gep}, "call_puts");
    irBuilder.CreateRet(irBuilder.getInt32(0));

    verifyFunction(*mainFunc, &errs());
    verifyModule(m, &errs());
    m.print(outs(), nullptr);

    return 0;
}