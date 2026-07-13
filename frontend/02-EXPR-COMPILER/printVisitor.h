#pragma once
#include "ast.h"
#include "parser.h"

class PrintVisitor : public Visitor
{
public:
    PrintVisitor(std::shared_ptr<Program> program);

    void VisitProgram(Program *p) override;
    void VisitBinaryExpr(BinaryExpr *binaryExpr) override;
    void VisitFactorExpr(FactorExpr *factorExpr) override;
};