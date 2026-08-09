#pragma once
#include <memory>
#include <vector>
#include "llvm/IR/Value.h"
#include "type.h"
#include "lexer.h"

class Program;
class VariableDecl;
class FuncDecl;
class NumberExpr;
class StringExpr;
class BinaryExpr;
class ThreeExpr;
class UnaryExpr;
class SizeOfExpr;
class PostIncExpr;
class PostDecExpr;
class PostSubscript;
class VariableAccessExpr;
class IfStmt;
class DeclStmt;
class BlockStmt;
class ForStmt;
class BreakStmt;
class ReturnStmt;
class ContinueStmt;
class PostMemberDotExpr;
class PostMemberArrowExpr;
class PostFuncCall;
class DoWhileStmt;
class SwitchStmt;
class CaseStmt;
class DefaultStmt;

class Visitor
{
public:
    virtual ~Visitor() {}
    virtual llvm::Value *VisitProgram(Program *p) = 0;
    virtual llvm::Value *VisitBlockStmt(BlockStmt *p) = 0;
    virtual llvm::Value *VisitDeclStmt(DeclStmt *p) = 0;
    virtual llvm::Value *VisitIfStmt(IfStmt *p) = 0;
    virtual llvm::Value *VisitForStmt(ForStmt *p) = 0;
    virtual llvm::Value *VisitBreakStmt(BreakStmt *p) = 0;
    virtual llvm::Value *VisitContinueStmt(ContinueStmt *p) = 0;
    virtual llvm::Value *VisitReturnStmt(ReturnStmt *p) = 0;
    virtual llvm::Value *VisitDoWhileStmt(DoWhileStmt *p) = 0;
    virtual llvm::Value *VisitSwitchStmt(SwitchStmt*p) = 0;
    virtual llvm::Value *VisitCaseStmt(CaseStmt *p) = 0;
    virtual llvm::Value *VisitDefaultStmt(DefaultStmt *p) = 0;
    virtual llvm::Value *VisitVariableDecl(VariableDecl *decl) = 0;
    virtual llvm::Value *VisitFuncDecl(FuncDecl *decl) = 0;
    virtual llvm::Value *VisitNumberExpr(NumberExpr *expr) = 0;
    virtual llvm::Value *VisitStringExpr(StringExpr *expr) = 0;
    virtual llvm::Value *VisitBinaryExpr(BinaryExpr *binaryExpr) = 0;
    virtual llvm::Value *VisitThreeExpr(ThreeExpr *threeExpr) = 0;
    virtual llvm::Value *VisitUnaryExpr(UnaryExpr *unaryExpr) = 0;
    virtual llvm::Value *VisitSizeOfExpr(SizeOfExpr *sizeOfExpr) = 0;
    virtual llvm::Value *VisitPostIncExpr(PostIncExpr *postIncExpr) = 0;
    virtual llvm::Value *VisitPostDecExpr(PostDecExpr *postDecExpr) = 0;
    virtual llvm::Value *VisitPostSubscript(PostSubscript *postSubscript) = 0;
    virtual llvm::Value *VisitPostMemberDotExpr(PostMemberDotExpr *postMemberDot) = 0;
    virtual llvm::Value *VisitPostMemberArrowExpr(PostMemberArrowExpr *postMemberArrow) = 0;
    virtual llvm::Value *VisitPostFuncCall(PostFuncCall *postMemberArrow) = 0;
    virtual llvm::Value *VisitVariableAccessExpr(VariableAccessExpr *factorExpr) = 0;
};

class AstNode
{
public:
    enum Kind
    {
        ND_ForStmt,
        ND_BreakStmt,
        ND_ContinueStmt,
        ND_BlockStmt,
        ND_DeclStmt,
        ND_IfStmt,
        ND_ReturnStmt,
        ND_DoWhileStmt,
        ND_SwitchStmt,
        ND_CaseStmt,
        ND_DefaultStmt,
        ND_VariableDecl,
        ND_FuncDecl,
        ND_BinaryExpr,
        ND_ThreeExpr,
        ND_UnaryExpr,
        ND_SizeOfExpr,
        ND_PostIncExpr,
        ND_PostDecExpr,
        ND_PostSubscript,
        ND_NumberExpr,
        ND_StringExpr,
        ND_VariableAccessExpr,
        ND_PostMemberDotExpr,
        ND_PostMemberArrowExpr,
        ND_PostFuncCall
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
    // 是否是左值
    bool isLValue{false};
    virtual ~AstNode() {}
    std::shared_ptr<CType> ty;
    Token tok;
    virtual llvm::Value *Accept(Visitor *v) { return nullptr; }
};

class BlockStmt : public AstNode
{
public:
    std::vector<std::shared_ptr<AstNode>> nodeVec;

public:
    BlockStmt() : AstNode(ND_BlockStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitBlockStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_BlockStmt;
    }
};

class DeclStmt : public AstNode
{
public:
    std::vector<std::shared_ptr<AstNode>> nodeVec;

public:
    DeclStmt() : AstNode(ND_DeclStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitDeclStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_DeclStmt;
    }
};

class ForStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> initNode;
    std::shared_ptr<AstNode> condNode;
    std::shared_ptr<AstNode> incNode;
    std::shared_ptr<AstNode> bodyNode;

public:
    ForStmt() : AstNode(ND_ForStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitForStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_ForStmt;
    }
};

class BreakStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> target;

public:
    BreakStmt() : AstNode(ND_BreakStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitBreakStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_BreakStmt;
    }
};

class ContinueStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> target;

public:
    ContinueStmt() : AstNode(ND_ContinueStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitContinueStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_ContinueStmt;
    }
};

class ReturnStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> expr{nullptr};

public:
    ReturnStmt() : AstNode(ND_ReturnStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitReturnStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_ReturnStmt;
    }
};

class DoWhileStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> body;
    std::shared_ptr<AstNode> cond;

public:
    DoWhileStmt() : AstNode(ND_DoWhileStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitDoWhileStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_DoWhileStmt;
    }
};

class SwitchStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> expr;
    std::shared_ptr<AstNode> stmt;
    std::shared_ptr<AstNode> defaultStmt{nullptr};

public:
    SwitchStmt() : AstNode(ND_SwitchStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitSwitchStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_SwitchStmt;
    }
};

class CaseStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> expr;
    std::shared_ptr<AstNode> stmt;

public:
    CaseStmt() : AstNode(ND_CaseStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitCaseStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_CaseStmt;
    }
};

class DefaultStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> stmt;

public:
    DefaultStmt() : AstNode(ND_DefaultStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitDefaultStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_DefaultStmt;
    }
};

class IfStmt : public AstNode
{
public:
    std::shared_ptr<AstNode> condNode;
    std::shared_ptr<AstNode> thenNode;
    std::shared_ptr<AstNode> elseNode;

public:
    IfStmt() : AstNode(ND_IfStmt) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitIfStmt(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_IfStmt;
    }
};

class VariableDecl : public AstNode
{
public:
    struct InitValue
    {
        std::shared_ptr<CType> declType;
        std::shared_ptr<AstNode> value;
        // a[2][4] = {{1,2}, {3,4}}
        // 1 --> {0, 0}
        // 2 --> {0, 1}
        // 3 --> {1, 0}
        // 4 --> {1, 1}
        std::vector<int> offsetList;
    };
    bool isGloabl{false};
    std::vector<std::shared_ptr<InitValue>> initValues;
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

class FuncDecl : public AstNode
{
public:
    std::shared_ptr<AstNode> blockStmt{nullptr};
    FuncDecl() : AstNode(ND_FuncDecl) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitFuncDecl(this);
    }

    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_FuncDecl;
    }
};

enum class BinaryOp
{
    add,
    sub,
    mul,
    div,
    mod,
    equal, // ==
    not_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    logical_or,
    logical_and,
    bitwise_or,
    bitwise_and,
    bitwise_xor,
    left_shift,
    right_shift,

    comma,

    assign,
    add_assign,
    sub_assign,
    mul_assign,
    div_assign,
    mod_assign,
    bitwise_or_assign,
    bitwise_xor_assign,
    bitwise_and_assign,
    left_shift_assign,
    right_shift_assign,
};

class BinaryExpr : public AstNode
{
public:
    BinaryOp op;
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

class ThreeExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> cond;
    std::shared_ptr<AstNode> then;
    std::shared_ptr<AstNode> els;

    ThreeExpr() : AstNode(ND_ThreeExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitThreeExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_ThreeExpr;
    }
};

enum class UnaryOp
{
    positive,
    negative,
    deref,
    addr,
    inc,
    dec,
    logical_not,
    bitwise_not
};

class UnaryExpr : public AstNode
{
public:
    UnaryOp op;
    std::shared_ptr<AstNode> node;

    UnaryExpr() : AstNode(ND_UnaryExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitUnaryExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_UnaryExpr;
    }
};

class SizeOfExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> node;
    std::shared_ptr<CType> type;

    SizeOfExpr() : AstNode(ND_SizeOfExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitSizeOfExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_SizeOfExpr;
    }
};

class PostIncExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> left;

    PostIncExpr() : AstNode(ND_PostIncExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitPostIncExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_PostIncExpr;
    }
};

class PostDecExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> left;

    PostDecExpr() : AstNode(ND_PostDecExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitPostDecExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_PostDecExpr;
    }
};

class PostSubscript : public AstNode
{
public:
    std::shared_ptr<AstNode> left;
    std::shared_ptr<AstNode> node; // index

    PostSubscript() : AstNode(ND_PostSubscript) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitPostSubscript(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_PostSubscript;
    }
};

class PostMemberDotExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> left;
    Member member;

    PostMemberDotExpr() : AstNode(ND_PostMemberDotExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitPostMemberDotExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_PostMemberDotExpr;
    }
};

class PostMemberArrowExpr : public AstNode
{
public:
    std::shared_ptr<AstNode> left;
    Member member;

    PostMemberArrowExpr() : AstNode(ND_PostMemberArrowExpr) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitPostMemberArrowExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_PostMemberArrowExpr;
    }
};

class PostFuncCall : public AstNode
{
public:
    std::shared_ptr<AstNode> left;
    std::vector<std::shared_ptr<AstNode>> args;

    PostFuncCall() : AstNode(ND_PostFuncCall) {}

    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitPostFuncCall(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_PostFuncCall;
    }
};

class NumberExpr : public AstNode
{
public:
    int value;
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

class StringExpr : public AstNode
{
public:
    StringExpr() : AstNode(ND_StringExpr) {}
    llvm::Value *Accept(Visitor *v) override
    {
        return v->VisitStringExpr(this);
    }
    static bool classof(const AstNode *node)
    {
        return node->GetKind() == ND_StringExpr;
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

class Program
{
public:
    llvm::StringRef fileName;
    std::vector<std::shared_ptr<AstNode>> externalDecls;
};