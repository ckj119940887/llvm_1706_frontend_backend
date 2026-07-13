#pragma once
#include <memory>
#include <vector>

class Program;
class Expr;
class BinaryExpr;
class FactorExpr;

class Visitor
{
public:
    virtual ~Visitor() {}
    virtual void VisitProgram(Program *p) = 0;
    virtual void VisitExpr(Expr *expr) {}
    virtual void VisitBinaryExpr(BinaryExpr *binaryExpr) = 0;
    virtual void VisitFactorExpr(FactorExpr *factorExpr) = 0;
};

class Expr
{
public:
    virtual ~Expr() {}
    virtual void Accept(Visitor *v) {}
};

enum class OpCode
{
    add,
    sub,
    mul,
    div
};

class BinaryExpr : public Expr
{
public:
    OpCode op;
    std::shared_ptr<Expr> left;
    std::shared_ptr<Expr> right;

    void Accept(Visitor *v) override
    {
        v->VisitBinaryExpr(this);
    }
};

class FactorExpr : public Expr
{
public:
    int number;
    void Accept(Visitor *v) override
    {
        v->VisitFactorExpr(this);
    }
};

class Program
{
public:
    std::vector<std::shared_ptr<Expr>> exprVec;
};