#include "parser.h"

// prog : stmt*
// stmt : decl-stmt | expr-stmt | null-stmt | if-stmt
// null-stmt : ";"
// decl-stmt : "int" identifier ("," identifier (= expr)?)* ";"
// if-stmt : "if" "(" expr ")" stmt ( "else" stmt )?
// expr-stmt : expr ";"
// expr : assign-expr | add-expr
// assign-expr: identifier "=" expr
// add-expr : mult-expr (("+" | "-") mult-expr)*
// mult-expr : primary-expr (("*" | "/") primary-expr)*
// primary-expr : identifier | number | "(" expr ")"
// number: ([0-9])+
// identifier : (a-zA-Z_)(a-zA-Z0-9_)*
std::shared_ptr<Program> Parser::ParseProgram()
{
    std::vector<std::shared_ptr<AstNode>> nodeVec;
    while (tok.tokenType != TokenType::eof)
    {
        auto stmt = ParseStmt();
        if (stmt)
            nodeVec.push_back(stmt);
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
    program->nodeVec = std::move(nodeVec);
    return program;
}

// stmt : decl-stmt | expr-stmt | null-stmt | if-stmt | block-stmt | for-stmt
std::shared_ptr<AstNode> Parser::ParseStmt()
{
    // 准备下个表达式
    // null-stmt : ";"
    if (tok.tokenType == TokenType::semi)
    {
        Advance();
        return nullptr;
    }
    // decl-stmt
    if (IsTypeName())
    {
        return ParseDeclStmt();
    }
    // if-stmt
    else if (tok.tokenType == TokenType::kw_if)
    {
        return ParseIfStmt();
    }
    // block-stmt
    else if (tok.tokenType == TokenType::l_brace)
    {
        return ParseBlockStmt();
    }
    // for-stmt
    else if (tok.tokenType == TokenType::kw_for)
    {
        return ParseForStmt();
    }
    // break-stmt
    else if (tok.tokenType == TokenType::kw_break)
    {
        return ParseBreakStmt();
    }
    // continue-stmt
    else if (tok.tokenType == TokenType::kw_continue)
    {
        return ParseContinueStmt();
    }
    // expr-stmt
    else
    {
        return ParseExprStmt();
    }
}

// block-stmt : "{" stmt* "}"
std::shared_ptr<AstNode> Parser::ParseBlockStmt()
{
    sema.EnterScope();

    auto blockStmt = std::make_shared<BlockStmt>();

    Consume(TokenType::l_brace);
    while (tok.tokenType != TokenType::r_brace)
    {
        blockStmt->nodeVec.push_back(ParseStmt());
    }
    Consume(TokenType::r_brace);

    sema.ExitScope();

    return blockStmt;
}

// decl-stmt : "int" identifier ("," identifier (= expr)?)* ";"
std::shared_ptr<AstNode> Parser::ParseDeclStmt()
{
    Consume(TokenType::kw_int);
    CType *baseTy = CType::GetIntTy();

    auto decl = std::make_shared<DeclStmt>();

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
        decl->nodeVec.push_back(variableDecl);

        Consume(TokenType::identifier);

        if (tok.tokenType == TokenType::equal)
        {
            Token opTok = tok;
            Advance();

            auto left = sema.SemaVariableAccessNode(tmp);
            auto right = ParseExpr();
            auto assignExpr = sema.SemaAssignExprNode(left, right, opTok);

            decl->nodeVec.push_back(assignExpr);
        }
    }

    Consume(TokenType::semi);

    return decl;
}

std::shared_ptr<AstNode> Parser::ParseExprStmt()
{
    auto expr = ParseExpr();
    Consume(TokenType::semi);
    return expr;
}

// for-stmt : "for" "(" expr? ";" expr? ";" expr? ")" stmt
// 		"for" "(" decl-stmt expr? ";" expr? ")" stmt
std::shared_ptr<AstNode> Parser::ParseForStmt()
{
    Consume(TokenType::kw_for);
    Consume(TokenType::l_parent);

    sema.EnterScope();

    auto stmt = std::make_shared<ForStmt>();

    breakNodes.push_back(stmt);
    continueNodes.push_back(stmt);

    std::shared_ptr<AstNode> initNode = nullptr;
    std::shared_ptr<AstNode> condNode = nullptr;
    std::shared_ptr<AstNode> incNode = nullptr;
    std::shared_ptr<AstNode> bodyNode = nullptr;

    if (IsTypeName())
    {
        initNode = ParseDeclStmt();
    }
    else
    {
        if (tok.tokenType != TokenType::semi)
        {
            initNode = ParseExpr();
        }
        Consume(TokenType::semi);
    }

    if (tok.tokenType != TokenType::semi)
    {
        condNode = ParseExpr();
    }
    Consume(TokenType::semi);

    if (tok.tokenType != TokenType::r_parent)
    {
        incNode = ParseExpr();
    }
    Consume(TokenType::r_parent);

    bodyNode = ParseStmt();

    stmt->initNode = initNode;
    stmt->condNode = condNode;
    stmt->incNode = incNode;
    stmt->bodyNode = bodyNode;

    breakNodes.pop_back();
    continueNodes.pop_back();

    sema.ExitScope();

    return stmt;
}

// break-stmt: "break" ";"
std::shared_ptr<AstNode> Parser::ParseBreakStmt()
{
    if (breakNodes.size() == 0)
    {
        GetDiagEngine().Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_break_stmt);
    }
    Consume(TokenType::kw_break);
    auto node = std::make_shared<BreakStmt>();
    node->target = breakNodes.back();
    Consume(TokenType::semi);
    return node;
}

// continue-stmt: "continue" ";"
std::shared_ptr<AstNode> Parser::ParseContinueStmt()
{
    if (continueNodes.size() == 0)
    {
        GetDiagEngine().Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_continue_stmt);
    }
    Consume(TokenType::kw_continue);
    auto node = std::make_shared<ContinueStmt>();
    node->target = continueNodes.back();
    Consume(TokenType::semi);
    return node;
}

// if-stmt : "if" "(" expr ")" stmt ( "else" stmt )?
std::shared_ptr<AstNode> Parser::ParseIfStmt()
{
    Consume(TokenType::kw_if);
    Consume(TokenType::l_parent);
    auto condExpr = ParseExpr();
    Consume(TokenType::r_parent);
    auto thenStmt = ParseStmt();
    std::shared_ptr<AstNode> elseStmt = nullptr;
    if (tok.tokenType == TokenType::kw_else)
    {
        Consume(TokenType::kw_else);
        elseStmt = ParseStmt();
    }
    return sema.SemaIfStmtNode(condExpr, thenStmt, elseStmt);
}

// expr : assign-expr | logor-expr
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

    return ParseLogOrExpr();
}

// equal-expr : relational-expr (("==" | "!=") relational-expr)*
std::shared_ptr<AstNode> Parser::ParseEqualExpr()
{
    auto left = ParseRelationalExpr();
    // 左结合
    while (tok.tokenType == TokenType::equal_equal || tok.tokenType == TokenType::not_equal)
    {
        OpCode op;
        if (tok.tokenType == TokenType::equal_equal)
        {
            op = OpCode::equal_equal;
        }
        else
        {
            op = OpCode::not_equal;
        }

        Advance();

        auto right = ParseRelationalExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// relational-expr: shift-expr (("<"|">"|"<="|">=") shift-expr)*
std::shared_ptr<AstNode> Parser::ParseRelationalExpr()
{
    auto left = ParseShiftExpr();
    // 左结合
    while (tok.tokenType == TokenType::less ||
           tok.tokenType == TokenType::less_equal ||
           tok.tokenType == TokenType::greater ||
           tok.tokenType == TokenType::greater_equal)
    {
        OpCode op;
        if (tok.tokenType == TokenType::less)
        {
            op = OpCode::less;
        }
        else if (tok.tokenType == TokenType::less_equal)
        {
            op = OpCode::less_equal;
        }
        else if (tok.tokenType == TokenType::greater)
        {
            op = OpCode::greater;
        }
        else
        {
            op = OpCode::greater_equal;
        }

        Advance();

        auto right = ParseShiftExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// add-expr : mult-expr (("+" | "-") mult-expr)*
std::shared_ptr<AstNode> Parser::ParseAddExpr()
{
    auto left = ParseMutliExpr();
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

        auto right = ParseMutliExpr();
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

// logor-expr: logand-expr ("||" logand-expr)*
std::shared_ptr<AstNode> Parser::ParseLogOrExpr()
{
    auto left = ParseLogAndExpr();
    // 左结合
    while (tok.tokenType == TokenType::pipepipe)
    {
        OpCode op = OpCode::logOr;

        Advance();

        auto right = ParseLogAndExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// logand-expr: bitor-expr ("&&" bitor-expr)*
std::shared_ptr<AstNode> Parser::ParseLogAndExpr()
{
    auto left = ParseBitOrExpr();
    // 左结合
    while (tok.tokenType == TokenType::ampamp)
    {
        OpCode op = OpCode::logAnd;

        Advance();

        auto right = ParseBitOrExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// bitor-expr: bitxor-expr ("|" bitxor-expr)*
std::shared_ptr<AstNode> Parser::ParseBitOrExpr()
{
    auto left = ParseBitXorExpr();
    // 左结合
    while (tok.tokenType == TokenType::pipe)
    {
        OpCode op = OpCode::bitOr;

        Advance();

        auto right = ParseBitXorExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// bitand-expr: equal-expr ("&" equal-expr)*
std::shared_ptr<AstNode> Parser::ParseBitAndExpr()
{
    auto left = ParseEqualExpr();
    // 左结合
    while (tok.tokenType == TokenType::amp)
    {
        OpCode op = OpCode::bitAnd;

        Advance();

        auto right = ParseEqualExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// bitxor-expr: bitand-expr ("^" bitand-expr)*
std::shared_ptr<AstNode> Parser::ParseBitXorExpr()
{
    auto left = ParseBitAndExpr();
    // 左结合
    while (tok.tokenType == TokenType::caret)
    {
        OpCode op = OpCode::bitXor;

        Advance();

        auto right = ParseBitAndExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// shift-expr: add-expr (("<<" | ">>") add-expr)*
std::shared_ptr<AstNode> Parser::ParseShiftExpr()
{
    auto left = ParseAddExpr();
    // 左结合
    while (tok.tokenType == TokenType::less_less || tok.tokenType == TokenType::greater_greater)
    {
        OpCode op;
        if (tok.tokenType == TokenType::less_less)
        {
            op = OpCode::leftShift;
        }
        else
        {
            op = OpCode::rightShift;
        }

        Advance();

        auto right = ParseAddExpr();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// mult-expr : primary-expr (("*" | "/") primary-expr)*
std::shared_ptr<AstNode> Parser::ParseMutliExpr()
{
    auto left = ParsePrimary();
    // 左结合
    while (
        tok.tokenType == TokenType::mul ||
        tok.tokenType == TokenType::div ||
        tok.tokenType == TokenType::percent)
    {
        OpCode op;
        if (tok.tokenType == TokenType::div)
        {
            op = OpCode::div;
        }
        else if (tok.tokenType == TokenType::percent)
        {
            op = OpCode::mod;
        }
        else
        {
            op = OpCode::mul;
        }

        Advance();

        auto right = ParsePrimary();
        auto binaryExpr = sema.SemaBinaryExprNode(left, right, op);

        // 要继续处理下一个Expr,当前Expr变成left child
        left = binaryExpr;
    }
    return left;
}

// primary-expr : identifier | number | "(" expr ")"
// number: ([0-9])+
// identifier : (a-zA-Z_)(a-zA-Z0-9_)*
std::shared_ptr<AstNode> Parser::ParsePrimary()
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

bool Parser::IsTypeName()
{
    if (tok.tokenType == TokenType::kw_int)
    {
        return true;
    }
    return false;
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