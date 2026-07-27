#include "parser.h"

// prog : (decl-stmt | expr-stmt)*
// decl-stmt : "int" identifier ("," identifier (= expr)?)* ";"
// expr-stmt : expr? ";"
// expr : assign-expr | add-expr
// assign-expr: identifier "=" expr
// add-expr : mult-expr (("+" | "-") mult-expr)*
// mult-expr : primary-expr (("*" | "/") primary-expr)*
// primary-expr : identifier | number | "(" expr ")"
// number: ([0-9])+
// identifier : (a-zA-Z_)(a-zA-Z0-9_)*
std::shared_ptr<Program> Parser::ParseProgram()
{
    std::vector<std::shared_ptr<AstNode>> exprVec;
    while (tok.tokenType != TokenType::eof)
    {
        // 准备下个表达式
        if (tok.tokenType == TokenType::semi)
        {
            Advance();
            continue;
        }
        if (tok.tokenType == TokenType::kw_int)
        {
            const auto &exprs = ParseDecl();
            for (auto &expr : exprs)
            {
                exprVec.push_back(expr);
            }
        }
        else
        {
            auto expr = ParseExpr();
            exprVec.push_back(expr);
        }
    }
    /*
    shared_ptr<T>	一个智能指针类型	共享所有权（引用计数）
    unique_ptr<T>	另一个智能指针类型	独占所有权（不能拷贝，只能 move）
    */
    /*
        std::make_shared<Program>()创建一个 Program 对象，
        并返回一个管理它的 shared_ptr（共享智能指针）。

        它做了两件事：在堆上 new 一个 Program + 用引用计数包裹它。
        好处：你不用手动 delete。当最后一个指向它的 shared_ptr 消失时，对象自动释放。
        为什么用 make_shared 而不是 shared_ptr<Program>(new Program())？——它把「对象」和「引用计数控制块」一次性分配在同一块内存，更快、更省一次内存分配，也更异常安全。

        在你的 AST 里到处用 shared_ptr，是因为一个节点可能被多个地方引用（比如 parser 建树、后面 codegen 遍历），共享所有权最省心。
    */
    auto program = std::make_shared<Program>();
    /*
        如果直接写 program->exprVec = exprVec;，会拷贝整个 vector——逐个复制里面每个元素（引用计数增减），有开销。
        std::move 的作用是把 exprVec 标记为「我不再需要它了，可以把内部资源抢过来」。于是 vector 内部的那块数组内存直接转交给 program->exprVec，不发生元素逐个拷贝，只是几个指针的搬移，非常快。
        代价：被 move 之后的 exprVec 变成「空壳」（有效但内容已被掏空），你不该再用它。这里正好第 40 行就 return 了，exprVec 也随之销毁，所以完全安全。
    */
    program->exprVec = std::move(exprVec);
    return program;
}

// decl-stmt : "int" identifier ("," identifier (= expr)?)* ";"
std::vector<std::shared_ptr<AstNode>> Parser::ParseDecl()
{
    Consume(TokenType::kw_int);
    CType *baseTy = CType::GetIntTy();

    std::vector<std::shared_ptr<AstNode>> astArr;

    // int a,b=3; => a,b=3;
    int i = 0;
    while (tok.tokenType != TokenType::semi)
    {
        // 判断当前是不是第一个变量,用来决定要不要吃掉一个逗号。
        if (i++ > 0)
        {
            assert(Consume(TokenType::comma));
        }

        auto variableName = tok.content;
        // int a = 3; => int a; a = 3;
        // 一条证明语句转换成VariableDecl和AssignExpr
        auto variableDecl = sema.SemaVariableDeclNode(variableName, baseTy);
        astArr.push_back(variableDecl);

        Consume(TokenType::identifier);

        if (tok.tokenType == TokenType::equal)
        {
            Advance();

            auto left = sema.SemaVariableAccessNode(variableName);
            auto right = ParseExpr();
            auto assignExpr = sema.SemaAssignExprNode(left, right);

            astArr.push_back(assignExpr);
        }
    }

    Consume(TokenType::semi);

    return astArr;
}

// add-expr : mult-expr (("+" | "-") mult-expr)*
std::shared_ptr<AstNode> Parser::ParseExpr()
{
    auto left = ParseTerm();
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

        auto right = ParseTerm();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// mult-expr : primary-expr (("*" | "/") primary-expr)*
std::shared_ptr<AstNode> Parser::ParseTerm()
{
    auto left = ParseFactor();
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

        auto right = ParseFactor();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// primary-expr : identifier | number | "(" expr ")"
// number: ([0-9])+
// identifier : (a-zA-Z_)(a-zA-Z0-9_)*
std::shared_ptr<AstNode> Parser::ParseFactor()
{
    if (tok.tokenType == TokenType::l_parent)
    {
        Advance();
        auto expr = ParseExpr();
        assert(Expect(TokenType::r_parent));
        Advance();
        return expr;
    }
    else if (tok.tokenType == TokenType::identifier)
    {
        auto expr = sema.SemaVariableAccessNode(tok.content);
        Advance();
        return expr;
    }
    else
    {
        auto factor = sema.SemaNumberExprNode(tok.value, tok.ty);
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