#include "parser.h"

// prog : stmt*
// stmt : decl-stmt | expr-stmt | null-stmt
// null-stmt : ";"
// decl-stmt : "int" identifier ("," identifier (= expr)?)* ";"
// expr-stmt : expr ";"
// expr : assign-expr | add-expr
// assign-expr: identifier ("=" expr)+
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
        // null-stmt : ";"
        if (tok.tokenType == TokenType::semi)
        {
            Advance();
            continue;
        }
        // decl-stmt
        if (tok.tokenType == TokenType::kw_int)
        {
            const auto &exprs = ParseDeclStmt();
            for (auto &expr : exprs)
            {
                exprVec.push_back(expr);
            }
        }
        // expr-stmt
        else
        {
            auto expr = ParseExprStmt();
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
std::vector<std::shared_ptr<AstNode>> Parser::ParseDeclStmt()
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
            Consume(TokenType::comma);
        }

        Token tmp = tok;
        // int a = 3; => int a; a = 3;
        // 一条证明语句转换成VariableDecl和AssignExpr
        auto variableDecl = sema.SemaVariableDeclNode(tok, baseTy);
        astArr.push_back(variableDecl);

        Consume(TokenType::identifier);

        if (tok.tokenType == TokenType::equal)
        {
            Token opTok = tok;
            Advance();

            auto left = sema.SemaVariableAccessNode(tmp);
            auto right = ParseExpr();
            auto assignExpr = sema.SemaAssignExprNode(left, right, opTok);

            astArr.push_back(assignExpr);
        }
    }

    Consume(TokenType::semi);

    return astArr;
}

std::shared_ptr<AstNode> Parser::ParseExprStmt()
{
    auto expr = ParseExpr();
    Consume(TokenType::semi);
    return expr;
}

// expr : assign-expr | add-expr
// assign-expr: identifier ("=" expr)+
// add-expr : mult-expr (("+" | "-") mult-expr)*
std::shared_ptr<AstNode> Parser::ParseExpr()
{
    // 判断到底是assign-expr,还是add-expr
    bool isAssignExpr = false;
    lexer.SaveState();
    if (tok.tokenType == TokenType::identifier)
    {
        Token tmp;
        lexer.NextToken(tmp);
        if (tmp.tokenType == TokenType::equal)
        {
            isAssignExpr = true;
        }
    }
    lexer.RestoreState();

    // assign-expr: identifier "=" expr
    if (isAssignExpr)
    {
        return ParseAssignExpr();
    }

    // add-expr : mult-expr (("+" | "-") mult-expr)*
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

// assign-expr: identifier ("=" expr)+
// 支持a = b = 3;
std::shared_ptr<AstNode> Parser::ParseAssignExpr()
{
    Expect(TokenType::identifier);
    Token tmp = tok;
    Advance();
    auto expr = sema.SemaVariableAccessNode(tmp);
    Token opTok = tok;
    Consume(TokenType::equal);
    return sema.SemaAssignExprNode(expr, ParseExpr(), opTok);
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
        Consume(TokenType::r_parent);
        return expr;
    }
    else if (tok.tokenType == TokenType::identifier)
    {
        auto expr = sema.SemaVariableAccessNode(tok);
        Advance();
        return expr;
    }
    else
    {
        Expect(TokenType::number);
        auto factor = sema.SemaNumberExprNode(tok, tok.ty);
        Advance();
        return factor;
    }
}

// 检测当前token是否是该类型，不消费；不匹配则在当前token处报错
bool Parser::Expect(TokenType tokenType)
{
    if (tok.tokenType == tokenType)
    {
        return true;
    }

    GetDiagEngine().Report(
        llvm::SMLoc::getFromPointer(tok.ptr),
        diag::err_expected,
        Token::GetSpellingText(tokenType),
        llvm::StringRef(tok.ptr, tok.len));
    return false;
}

// 检测当前token是否是该类型，消费
bool Parser::Consume(TokenType tokenType)
{
    if (Expect(tokenType))
    {
        Advance();
        return true;
    }
    return false;
}

// 前进一个token
void Parser::Advance()
{
    lexer.NextToken(tok);
}