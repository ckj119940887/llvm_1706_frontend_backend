#include "parser.h"

// prog : (expr? ";")*
// expr : term (("+" | "-") term)* ;
// term : factor (("*" | "/") factor)* ;
// factor : number | "(" expr ")" ;
// number: ([0-9])+ ;
std::shared_ptr<Program> Parser::ParserProgram()
{
    std::vector<std::shared_ptr<Expr>> exprVec;
    while (tok.tokenType != TokenType::eof)
    {
        // 准备下个表达式
        if (tok.tokenType == TokenType::semi)
        {
            Advance();
            continue;
        }
        auto expr = ParserExpr();
        exprVec.push_back(expr);
    }
    auto program = std::make_shared<Program>();
    program->exprVec = std::move(exprVec);
    return program;
}

std::shared_ptr<Expr> Parser::ParserExpr()
{
    auto left = ParserTerm();
    // 左结合
    while (tok.tokenType == TokenType::plus || tok.tokenType == TokenType::minus)
    {
        OpCode op;
        if (tok.tokenType == TokenType::plus)
        {
            op = OpCode::add;
        }
        else
        {
            op = OpCode::sub;
        }

        Advance();

        auto binaryExpr = std::make_shared<BinaryExpr>();
        binaryExpr->left = left;
        binaryExpr->op = op;
        binaryExpr->right = ParserTerm();

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

std::shared_ptr<Expr> Parser::ParserTerm()
{
    auto left = ParserFactor();
    // 左结合
    while (tok.tokenType == TokenType::mul || tok.tokenType == TokenType::div)
    {
        OpCode op;
        if (tok.tokenType == TokenType::div)
        {
            op = OpCode::div;
        }
        else
        {
            op = OpCode::mul;
        }

        Advance();

        auto binaryExpr = std::make_shared<BinaryExpr>();
        binaryExpr->left = left;
        binaryExpr->op = op;
        binaryExpr->right = ParserFactor();

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

std::shared_ptr<Expr> Parser::ParserFactor()
{
    if (tok.tokenType == TokenType::l_parent)
    {
        Advance();
        auto expr = ParserExpr();
        assert(Expect(TokenType::r_parent));
        Advance();
        return expr;
    }
    else
    {
        auto factor = std::make_shared<FactorExpr>();
        factor->number = tok.value;
        Advance();
        return factor;
    }
}

// 检测当前token是否是该类型，不消费
bool Parser::Expect(TokenType tokenType)
{
    return tok.tokenType == tokenType;
}
// 检测当前token是否是该类型，消费
bool Parser::Consume(TokenType tokenType)
{
    if (Expect(tokenType))
    {
        Advance();
        return true;
    }
    else
    {
        return false;
    }
}
// 前进一个token
void Parser::Advance()
{
    lexer.NextToken(tok);
}