#include "sema.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

std::shared_ptr<AstNode> Sema::SemaVariableDeclNode(llvm::StringRef name, CType *ty)
{
    // 检查是否出现重定义
    std::shared_ptr<Symbol> symbol = scope.FindVarSymbolInCurEnv(name);
    if (symbol)
    {
        llvm::errs() << "re-defined variable " << name << "\n";
        return nullptr;
    }

    // 添加到符号表
    scope.AddSymbol(SymbolKind::LocalVariable, ty, name);

    auto variableDecl = std::make_shared<VariableDecl>();
    variableDecl->name = name;
    variableDecl->ty = ty;

    return variableDecl;
}

std::shared_ptr<AstNode> Sema::SemaVariableAccessNode(llvm::StringRef name)
{
    // 这个查找要在全部的env中进行查找
    std::shared_ptr<Symbol> symbol = scope.FindVarSymbol(name);
    if (symbol == nullptr)
    {
        // 没有找到
        llvm::errs() << "use undefined symbol: " << name << "\n";
        return nullptr;
    }
    else
    {
        auto expr = std::make_shared<VariableAccessExpr>();
        expr->name = name;
        expr->ty = symbol->GetTy();
        return expr;
    }
}

std::shared_ptr<AstNode> Sema::SemaNumberExprNode(int number, CType *ty)
{
    auto factor = std::make_shared<NumberExpr>();
    factor->number = number;
    factor->ty = ty;

    return factor;
}

std::shared_ptr<AstNode> Sema::SemaAssignExprNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode> right)
{
    if (!left || !right)
    {
        llvm::errs() << "left or right can't be nullptr\n";
        return nullptr;
    }

    if (!llvm::isa<VariableAccessExpr>(left.get()))
    {
        llvm::errs() << "must be left value\n";
        return nullptr;
    }

    auto assignExpr = std::make_shared<AssignExpr>();
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