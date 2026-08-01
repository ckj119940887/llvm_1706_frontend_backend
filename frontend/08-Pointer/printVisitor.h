#pragma once
#include "ast.h"
#include "parser.h"

class PrintVisitor : public Visitor, public TypeVisitor
{
public:
    PrintVisitor(std::shared_ptr<Program> program);

    llvm::Value *VisitProgram(Program *p) override;
    llvm::Value *VisitBlockStmt(BlockStmt *p) override;
    llvm::Value *VisitDeclStmt(DeclStmt *p) override;
    llvm::Value *VisitIfStmt(IfStmt *p) override;
    llvm::Value *VisitForStmt(ForStmt *p) override;
    llvm::Value *VisitBreakStmt(BreakStmt *p) override;
    llvm::Value *VisitContinueStmt(ContinueStmt *p) override;
    llvm::Value *VisitVariableDecl(VariableDecl *decl) override;
    llvm::Value *VisitNumberExpr(NumberExpr *expr) override;
    llvm::Value *VisitBinaryExpr(BinaryExpr *binaryExpr) override;
    llvm::Value *VisitThreeExpr(ThreeExpr *threeExpr) override;
    llvm::Value *VisitUnaryExpr(UnaryExpr *unaryExpr) override;
    llvm::Value *VisitSizeOfExpr(SizeOfExpr *sizeOfExpr) override;
    llvm::Value *VisitPostIncExpr(PostIncExpr *postIncExpr) override;
    llvm::Value *VisitPostDecExpr(PostDecExpr *postDecExpr) override;
    llvm::Value *VisitVariableAccessExpr(VariableAccessExpr *factorExpr) override;

    llvm::Type *VisitPrimaryType(CPrimaryType *ty) override;
    llvm::Type *VisitPointType(CPointType *ty) override;
};