#include "sema.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

std::shared_ptr<AstNode> Sema::SemaVariableDeclNode(Token tok, CType *ty)
{
    // 检查是否出现重定义
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindVarSymbolInCurEnv(text);
    if (symbol)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_redefined, text);
    }

    // 添加到符号表
    scope.AddSymbol(SymbolKind::LocalVariable, ty, text);

    auto variableDecl = std::make_shared<VariableDecl>();
    variableDecl->tok = tok;
    variableDecl->ty = ty;

    return variableDecl;
}

std::shared_ptr<AstNode> Sema::SemaVariableAccessNode(Token tok)
{
    // 这个查找要在全部的env中进行查找
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindVarSymbol(text);
    if (symbol == nullptr)
    {
        // 没有找到
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_undefined, text);
    }
    else
    {
        auto expr = std::make_shared<VariableAccessExpr>();
        expr->tok = tok;
        expr->ty = symbol->GetTy();
        return expr;
    }
}

std::shared_ptr<AstNode> Sema::SemaNumberExprNode(Token tok, CType *ty)
{
    auto factor = std::make_shared<NumberExpr>();
    factor->tok = tok;
    factor->ty = ty;

    return factor;
}

std::shared_ptr<AstNode> Sema::SemaAssignExprNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode> right, Token tok)
{
    assert(left && right);

    if (!llvm::isa<VariableAccessExpr>(left.get()))
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_lvalue);
    }

    auto assignExpr = std::make_shared<AssignExpr>();
    assignExpr->tok = tok;
    assignExpr->left = left;
    assignExpr->right = right;
    return assignExpr;
}

std::shared_ptr<AstNode> Sema::SemaBinaryExprNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode> right, OpCode op)
{
    auto binaryExpr = std::make_shared<BinaryExpr>();
    binaryExpr->left = left;
    binaryExpr->op = op;
    binaryExpr->right = right;
    return binaryExpr;
}

std::shared_ptr<AstNode> Sema::SemaIfStmtNode(std::shared_ptr<AstNode> condNode, std::shared_ptr<AstNode> thenNode, std::shared_ptr<AstNode> elseNode)
{
    auto ifStmt = std::make_shared<IfStmt>();
    ifStmt->condNode = condNode;
    ifStmt->thenNode = thenNode;
    ifStmt->elseNode = elseNode;
    return ifStmt;
}

void Sema::EnterScope()
{
    scope.EnterScope();
}

void Sema::ExitScope()
{
    scope.ExitScope();
}
