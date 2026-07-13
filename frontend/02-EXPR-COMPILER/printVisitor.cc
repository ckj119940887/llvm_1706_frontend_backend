#include "printVisitor.h"

PrintVisitor::PrintVisitor(std::shared_ptr<Program> program)
{
    VisitProgram(program.get());
}

void PrintVisitor::VisitProgram(Program *p)
{
    for (auto &expr : p->exprVec)
    {
        expr->Accept(this);
        llvm::outs() << "\n";
    }
}
void PrintVisitor::VisitBinaryExpr(BinaryExpr *binaryExpr)
{
    // 后序遍历
    binaryExpr->left->Accept(this);
    binaryExpr->right->Accept(this);

    switch (binaryExpr->op)
    {
    case OpCode::add:
    {
        llvm::outs() << " + ";
        break;
    }
    case OpCode::sub:
    {
        llvm::outs() << " - ";
        break;
    }
    case OpCode::mul:
    {
        llvm::outs() << " * ";
        break;
    }
    case OpCode::div:
    {
        llvm::outs() << " / ";
        break;
    }
    default:
        break;
    }
}
void PrintVisitor::VisitFactorExpr(FactorExpr *factorExpr)
{
    llvm::outs() << factorExpr->number;
}