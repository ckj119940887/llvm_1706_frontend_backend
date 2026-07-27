#pragma once
#include <memory>
#include <vector>
#include "llvm/IR/Value.h"
#include "type.h"
#include "lexer.h"

class Program;
class VariableDecl;
class AssignExpr;
class NumberExpr;
class BinaryExpr;
class VariableAccessExpr;

class Visitor
{
public:
    virtual ~Visitor() {}
    virtual llvm::Value *VisitProgram(Program *p) = 0;
    virtual llvm::Value *VisitVariableDecl(VariableDecl *decl) = 0;
    virtual llvm::Value *VisitAssignExpr(AssignExpr *expr) = 0;
    virtual llvm::Value *VisitNumberExpr(NumberExpr *expr) = 0;
    virtual llvm::Value *VisitBinaryExpr(BinaryExpr *binaryExpr) = 0;
    virtual llvm::Value *VisitVariableAccessExpr(VariableAccessExpr *factorExpr) = 0;
};

class AstNode
{
public:
    enum Kind
    {
        ND_VariableDecl,
        ND_BinaryExpr,
        ND_NumberExpr,
        ND_VariableAccessExpr,
        ND_AssignExpr
    };

private:
    const Kind kind;

public:
    AstNode(Kind kind) : kind(kind) {}
    /*
        GetKind() 末尾的 const 表示这是 const 成员函数，承诺不修改对象，
        因此可以被 const 对象（如 classof 中的 const AstNode*）调用；
        它与私有成员 kind 是否为 const 无关，只是为了让 const 对象也能
        安全调用此方法。（返回值前的 const 对 enum 值类型意义不大，可省略。）
    */
    // 返回值前的 const Kind 其实可有可无、意义不大。
    const Kind GetKind() const { return kind; }
    virtual ~AstNode() {}
    CType *ty;
    Token tok;
    virtual llvm::Value *Accept(Visitor *v) { return nullptr; }
};

class VariableDecl : public AstNode
{
public:
    VariableDecl() : AstNode(ND_VariableDecl) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitVariableDecl(this);
    }
    /*
        静态方法是属于类本身、而不属于某个对象实例的成员函数。特点：
        不需要对象就能调用，用 类名::方法名() 调用
        没有 this 指针，所以不能访问非静态成员（因为非静态成员依附于某个具体对象）
        只能访问类的静态成员，或者通过参数传进来的对象
    */
    /*
        这几个 classof是 LLVM 的 RTTI 机制（llvm::isa<> / llvm::dyn_cast<>）要求的固定接口。

        它的作用是回答一个问题："这个 AstNode* 究竟是不是某个具体子类？"

        static bool classof
        {
            return node->GetKind() == ND_VariableDecl;   // 靠 kind 字段判断
        }
        当你写：

        if (llvm::isa<VariableDecl>(node)) { ... }
        auto *decl = llvm::dyn_cast<VariableDecl>(node);
        LLVM 内部实际上就是调用了 VariableDecl::classof(node)。

        它必须是静态的，原因：
        判断"node 是不是 VariableDecl"这个动作发生在你还不知道 node 是不是 VariableDecl 的时候。如果 classof 是普通成员函数，就得先有一个 VariableDecl 对象才能调用 —— 这就矛盾了（你正是要判断它是不是）。
        所以它把"待检查的对象"当成参数传进来（const AstNode *node），用类里的 kind 枚举来判断，而不依赖 this。
    */
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_VariableDecl;
    }
};

enum class OpCode
{
    add,
    sub,
    mul,
    div
};

class BinaryExpr : public AstNode
{
public:
    OpCode op;
    std::shared_ptr<AstNode> left;
    std::shared_ptr<AstNode> right;
    BinaryExpr() : AstNode(ND_BinaryExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitBinaryExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_BinaryExpr;
    }
};

class NumberExpr : public AstNode
{
public:
    NumberExpr() : AstNode(ND_NumberExpr) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitNumberExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_NumberExpr;
    }
};

class VariableAccessExpr : public AstNode
{
public:
    VariableAccessExpr() : AstNode(ND_VariableAccessExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitVariableAccessExpr(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_VariableAccessExpr;
    }
};

class AssignExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> left;
    std::shared_ptr<AstNode> right;
    AssignExpr() : AstNode(ND_AssignExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitAssignExpr(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_AssignExpr;
    }
};

class Program
{
public:
    std::vector<std::shared_ptr<AstNode>> exprVec;
};