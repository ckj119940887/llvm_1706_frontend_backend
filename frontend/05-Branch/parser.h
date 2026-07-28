#pragma once
#include "lexer.h"
#include "ast.h"
#include "sema.h"

class Parser
{
private:
    Lexer &lexer;
    Sema &sema;

public:
    Parser(Lexer &lexer, Sema &sema) : lexer(lexer), sema(sema)
    {
        Advance();
    }
    std::shared_ptr<Program> ParseProgram();

private:
    std::shared_ptr<AstNode> ParseBlockStmt();
    std::shared_ptr<AstNode> ParseStmt();
    std::shared_ptr<AstNode> ParseDeclStmt();
    std::shared_ptr<AstNode> ParseIfStmt();
    std::shared_ptr<AstNode> ParseExprStmt();
    std::shared_ptr<AstNode> ParseExpr();
    std::shared_ptr<AstNode> ParseAssignExpr();
    std::shared_ptr<AstNode> ParseTerm();
    std::shared_ptr<AstNode> ParseFactor();

    // 检测当前token是否是该类型，不消费
    bool Expect(TokenType tokenType);
    // 检测当前token是否是该类型，消费
    bool Consume(TokenType tokenType);
    // 前进一个token
    void Advance();

    DiagEngine &GetDiagEngine()
    {
        return lexer.GetDiagEngine();
    }

    Token tok;
};