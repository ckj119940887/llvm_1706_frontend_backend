#pragma once
#include "ast.h"
#include "parser.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ADT/DenseMap.h"

class CodeGen : public Visitor, public TypeVisitor
{
public:
    CodeGen(std::shared_ptr<Program> p)
    {
        module = std::make_unique<llvm::Module>("expr", context);
        VisitProgram(p.get());
    };

    std::unique_ptr<llvm::Module> &GetModule()
    {
        return module;
    }

    llvm::Value *VisitProgram(Program *p) override;
    llvm::Value *VisitBlockStmt(BlockStmt *p) override;
    llvm::Value *VisitDeclStmt(DeclStmt *p) override;
    llvm::Value *VisitIfStmt(IfStmt *p) override;
    llvm::Value *VisitForStmt(ForStmt *p) override;
    llvm::Value *VisitBreakStmt(BreakStmt *p) override;
    llvm::Value *VisitContinueStmt(ContinueStmt *p) override;
    llvm::Value *VisitVariableDecl(VariableDecl *decl) override;
    llvm::Value *VisitNumberExpr(NumberExpr *expr) override;
    llvm::Value *VisitThreeExpr(ThreeExpr *threeExpr) override;
    llvm::Value *VisitUnaryExpr(UnaryExpr *unaryExpr) override;
    llvm::Value *VisitSizeOfExpr(SizeOfExpr *sizeOfExpr) override;
    llvm::Value *VisitPostIncExpr(PostIncExpr *postIncExpr) override;
    llvm::Value *VisitPostDecExpr(PostDecExpr *postDecExpr) override;
    llvm::Value *VisitPostSubscript(PostSubscript *postSubscript) override;
    llvm::Value *VisitBinaryExpr(BinaryExpr *binaryExpr) override;
    llvm::Value *VisitVariableAccessExpr(VariableAccessExpr *factorExpr) override;

    llvm::Type *VisitPrimaryType(CPrimaryType *ty) override;
    llvm::Type *VisitPointType(CPointType *ty) override;
    llvm::Type *VisitArrayType(CArrayType *ty) override;

private:
    llvm::LLVMContext context;
    llvm::IRBuilder<> irBuilder{context};
    std::unique_ptr<llvm::Module> module;

    llvm::DenseMap<AstNode *, llvm::BasicBlock *> breakBBs;
    llvm::DenseMap<AstNode *, llvm::BasicBlock *> continueBBs;

    // 记录当前函数
    llvm::Function *curFunc{nullptr};

    /*
        关键点：Type 是根，不继承 Value
        llvm::Type 没有基类，它和 Value 是两套独立的体系。
        常见误解是以为 Type : public Value，实际上是 Value 里 持有 一个 Type*。

        继承树
        Type                      (无基类)
        ├── IntegerType           i1 / i8 / i32 / i64 ... 任意位宽
        ├── FunctionType          返回值类型 + 参数类型列表 + isVarArg
        ├── StructType            { i32, double }，可命名/可 opaque/可 packed
        ├── ArrayType             [10 x i32]
        ├── PointerType           ptr（LLVM 15+ 起为不透明指针，只带 AddressSpace）
        ├── VectorType            抽象基类，不能直接实例化
        │   ├── FixedVectorType       <4 x float>
        │   └── ScalableVectorType    <vscale x 4 x float>
        ├── TargetExtType         target("...") 目标特定扩展类型
        └── TypedPointerType      仅个别 GPU 后端用（在 TypedPointerType.h 里）
        而这些 primitive type 没有对应的子类，它们就是 Type 本身，仅靠 TypeID 区分：

        HalfTyID / BFloatTyID / FloatTyID / DoubleTyID / X86_FP80TyID / FP128TyID / PPC_FP128TyID / VoidTyID / LabelTyID / MetadataTyID / X86_MMXTyID / X86_AMXTyID / TokenTyID

        所以 Type::getDoubleTy(ctx) 返回的是一个 Type*，没有 DoubleType 这个类。
    */
    llvm::StringMap<std::pair<llvm::Value *, llvm::Type *>> varAddrTypeMap;
};