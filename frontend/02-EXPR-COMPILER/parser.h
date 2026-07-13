#pragma once
#include "lexer.h"
#include "ast.h"

class Parser
{
private:
    Lexer &lexer;

public:
    Parser(Lexer &lexer) : lexer(lexer)
    {
        Advance();
    }
    std::shared_ptr<Program> ParserProgram();

private:
    std::shared_ptr<Expr> ParserExpr();
    std::shared_ptr<Expr> ParserTerm();
    std::shared_ptr<Expr> ParserFactor();

    // 检测当前token是否是该类型，不消费
    bool Expect(TokenType tokenType);
    // 检测当前token是否是该类型，消费
    bool Consume(TokenType tokenType);
    // 前进一个token
    void Advance();

    Token tok;
};