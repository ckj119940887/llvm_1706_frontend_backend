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

    llvm::Value *lastVal = nullptr;
    for (auto &expr : p->exprVec)
    {
        lastVal = expr->Accept(this);
    }
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
    irBuilder.CreateCall(printfFunc, {irBuilder.CreateGlobalStringPtr("expr val: %d\n"), lastVal});

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

    /*
        nsw	No Signed Wrap	假定不发生有符号溢出
        nuw	No Unsigned Wrap	假定不发生无符号溢出
    */
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

llvm::Value *CodeGen::VisitNumberExpr(NumberExpr *numberExpr)
{
    return irBuilder.getInt32(numberExpr->number);
}

llvm::Value *CodeGen::VisitVariableDecl(VariableDecl *decl)
{
    llvm::Type *ty = nullptr;
    if (decl->ty == CType::GetIntTy())
    {
        ty = irBuilder.getInt32Ty();
    }

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
    llvm::Value *value = irBuilder.CreateAlloca(ty, nullptr, decl->name);
    varAddrMap.insert({decl->name, value});
    return value;
}

llvm::Value *CodeGen::VisitAssignExpr(AssignExpr *expr)
{
    auto left = expr->left;
    VariableAccessExpr *varAccessExpr = (VariableAccessExpr *)left.get();
    llvm::Value *leftValAddr = varAddrMap[varAccessExpr->name];
    llvm::Value *rightValue = expr->right->Accept(this);
    // CreateStore — 把值「写入」变量的内存
    /*
        第一个 rightValue — 要存的值(比如 i32 的 10)。
        第二个 leftValAddr — 存到哪里,也就是之前 alloca 返回的地址(i32*)。
    */
    return irBuilder.CreateStore(rightValue, leftValAddr);
}

llvm::Value *CodeGen::VisitVariableAccessExpr(VariableAccessExpr *factorExpr)
{
    llvm::Value *varAddr = varAddrMap[factorExpr->name];
    llvm::Type *ty = nullptr;
    if (factorExpr->ty == CType::GetIntTy())
    {
        ty = irBuilder.getInt32Ty();
    }
    // CreateLoad — 从变量的内存「读出」值
    /*
        ty — 读出来的值是什么类型(这里 i32)。
        varAddr — 从哪里读,同样是 alloca 的那个地址。
        factorExpr->name — 给读出结果的 IR 寄存器起个名字(可读性)。

        store 时,要存的值本身已经带着类型(rightValue 知道自己是 i32),所以不用再单独告诉它类型。
        load 时,只给一个地址,新版 LLVM 的指针 i32* 已经退化成不带类型的 ptr,光看地址不知道该读几个字节、按什么类型解释,所以必须显式传 ty 说明「按 i32 读」。
    */
    return irBuilder.CreateLoad(ty, varAddr, factorExpr->name);
}