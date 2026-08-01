#include "parser.h"

// prog       ::= block-stmt
// block-stmt ::= stmt*
// stmt       ::= decl-stmt | expr-stmt | null-stmt | if-stmt | block-stmt | for-stmt | break-stmt | continue-stmt
// decl-stmt  ::= decl-spec init-declarator-list? ";"
// init-declarator-list ::= declarator (= assign)? ("," declarator (= assign)?)*
// decl-spec  ::= "int"
// declarator ::= "*"* direct-declarator
// direct-declarator ::= identifier

// null-stmt     ::= ";"
// if-stmt       ::= "if" "(" expr ")" stmt ( "else" stmt )?
// for-stmt      ::= "for" "(" expr? ";" expr? ";" expr? ")" stmt
// 							    "for" "(" decl-stmt expr? ";" expr? ")" stmt
// block-stmt    ::= "{" stmt* "}"
// break-stmt    ::= "break" ";"
// continue-stmt ::= "continue" ";"
// expr-stmt     ::= expr ";"

// expr         ::= assign (, assign )*
// assign ::= conditional ("="|"+="|"-="|"*="|"/="|"%="|"<<="|">>="|"&="|"^="|"|=" asign)?
// conditional ::= logor  ("?" expr ":" conditional)?
// logor       ::= logand ("||" logand)*
// logand      ::= bitor  ("&&" bitor)*
// bitor       ::= bitxor ("|" bitxor)*
// bitxor      ::= bitand ("^" bitand)*
// bitand      ::= equal ("&" equal)*
// equal       ::= relational ("==" | "!=" relational)*
// relational  ::= shift ("<"|">"|"<="|">=" shift)*
// shift       ::= add ("<<" | ">>" add)*
// add         ::= mult ("+" | "-" mult)*
// mult        ::= cast ("*" | "/" | "%" cast)*
// cast        ::= unary | "(" type-name ")" cast
// unary       ::= postfix | ("++"|"--"|"&"|"*"|"+"|"-"|"~"|"!"|"sizeof") unary
//                 | "sizeof" "(" type-name ")"
// postfix     ::= primary ("++"|"--")*
// primary     ::= identifier | number | "(" expr ")"
// number      ::= ([0-9])+
// identifier  ::= (a-zA-Z_)(a-zA-Z0-9_)*
std::shared_ptr<Program> Parser::ParseProgram()
{
    auto program = std::make_shared<Program>();
    if (tok.tokenType != TokenType::eof)
    {
        program->node = ParseBlockStmt();
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
    /*
        如果直接写 program->exprVec = exprVec;，会拷贝整个 vector——逐个复制里面每个元素（引用计数增减），有开销。
        std::move 的作用是把 exprVec 标记为「我不再需要它了，可以把内部资源抢过来」。于是 vector 内部的那块数组内存直接转交给 program->exprVec，不发生元素逐个拷贝，只是几个指针的搬移，非常快。
        代价：被 move 之后的 exprVec 变成「空壳」（有效但内容已被掏空），你不该再用它。这里正好第 40 行就 return 了，exprVec 也随之销毁，所以完全安全。
    */
    Expect(TokenType::eof);
    return program;
}

// stmt       ::= decl-stmt | expr-stmt | null-stmt | if-stmt | block-stmt | for-stmt | break-stmt | continue-stmt
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
    if (IsTypeName(tok.tokenType))
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

// block-stmt ::= stmt*
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
    auto baseTy = ParseDeclSpec();

    // int ;
    if (tok.tokenType == TokenType::semi)
    {
        Consume(TokenType::semi);
        return nullptr;
    }

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

        decl->nodeVec.push_back(Declarator(baseTy));
    }

    Consume(TokenType::semi);

    return decl;
}

// declarator ::= "*"* direct-declarator
std::shared_ptr<AstNode> Parser::Declarator(std::shared_ptr<CType> baseType)
{
    while (tok.tokenType == TokenType::star)
    {
        Consume(TokenType::star);
        // 循环迭代
        baseType = std::make_shared<CPointType>(baseType);
    }

    Expect(TokenType::identifier);
    auto decl = sema.SemaVariableDeclNode(tok, baseType);
    Consume(TokenType::identifier);

    if (tok.tokenType == TokenType::equal)
    {
        Advance();
        VariableDecl *varDecl = llvm::dyn_cast<VariableDecl>(decl.get());
        varDecl->init = ParseAssignExpr();
    }

    return decl;
}

// decl-spec  ::= "int"
std::shared_ptr<CType> Parser::ParseDeclSpec()
{
    if (tok.tokenType == TokenType::kw_int)
    {
        Consume(TokenType::kw_int);
        return CType::IntType;
    }
    GetDiagEngine().Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_type);
    return nullptr;
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

    if (IsTypeName(tok.tokenType))
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

// expr         ::= assign (, assign )*
std::shared_ptr<AstNode> Parser::ParseExpr()
{
    auto left = ParseAssignExpr();
    while (tok.tokenType == TokenType::comma)
    {
        Consume(TokenType::comma);
        auto right = ParseAssignExpr();

        left = sema.SemaBinaryExprNode(left, right, BinaryOp::comma);
    }
    return left;
}

// equal-expr : relational-expr (("==" | "!=") relational-expr)*
std::shared_ptr<AstNode> Parser::ParseEqualExpr()
{
    auto left = ParseRelationalExpr();
    // 左结合
    while (tok.tokenType == TokenType::equal_equal || tok.tokenType == TokenType::not_equal)
    {
        BinaryOp op;
        if (tok.tokenType == TokenType::equal_equal)
        {
            op = BinaryOp::equal;
        }
        else
        {
            op = BinaryOp::not_equal;
        }
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseRelationalExpr(), op);
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
        BinaryOp op;
        if (tok.tokenType == TokenType::less)
        {
            op = BinaryOp::less;
        }
        else if (tok.tokenType == TokenType::less_equal)
        {
            op = BinaryOp::less_equal;
        }
        else if (tok.tokenType == TokenType::greater)
        {
            op = BinaryOp::greater;
        }
        else
        {
            op = BinaryOp::greater_equal;
        }
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseShiftExpr(), op);
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
        BinaryOp op;
        if (tok.tokenType == TokenType::plus)
        {
            op = BinaryOp::add;
        }
        else
        {
            op = BinaryOp::sub;
        }
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseMutliExpr(), op);
    }
    return left;
}

// conditional ::= logor  ("?" expr ":" conditional)?
std::shared_ptr<AstNode> Parser::ParseConditionalExpr()
{
    auto left = ParseLogOrExpr();
    if (tok.tokenType != TokenType::question)
    {
        return left;
    }
    Token tmp = tok;
    Consume(TokenType::question);
    auto then = ParseExpr();
    Consume(TokenType::colon);
    auto els = ParseConditionalExpr();

    return sema.SemaThreeExprNode(left, then, els, tmp);
}

// assign-expr: identifier ("=" expr)+
// 支持a = b = 3;
std::shared_ptr<AstNode> Parser::ParseAssignExpr()
{
    auto left = ParseConditionalExpr();

    if (!IsAssignOperator())
    {
        return left;
    }

    BinaryOp op;
    switch (tok.tokenType)
    {
    case TokenType::equal:
        op = BinaryOp::assign;
        break;
    case TokenType::plus_equal:
        op = BinaryOp::add_assign;
        break;
    case TokenType::minus_equal:
        op = BinaryOp::sub_assign;
        break;
    case TokenType::star_equal:
        op = BinaryOp::mul_assign;
        break;
    case TokenType::slash_equal:
        op = BinaryOp::div_assign;
        break;
    case TokenType::percent_equal:
        op = BinaryOp::mod_assign;
        break;
    case TokenType::less_less_equal:
        op = BinaryOp::left_shift_assign;
        break;
    case TokenType::greater_greater_equal:
        op = BinaryOp::right_shift_assign;
        break;
    case TokenType::amp_equal:
        op = BinaryOp::bitwise_and_assign;
        break;
    case TokenType::pipe_equal:
        op = BinaryOp::bitwise_or_assign;
        break;
    case TokenType::caret_equal:
        op = BinaryOp::bitwise_xor_assign;
        break;
    }

    Advance();
    return sema.SemaBinaryExprNode(left, ParseAssignExpr(), op);
}

// logor-expr: logand-expr ("||" logand-expr)*
std::shared_ptr<AstNode> Parser::ParseLogOrExpr()
{
    auto left = ParseLogAndExpr();
    // 左结合
    while (tok.tokenType == TokenType::pipepipe)
    {
        BinaryOp op = BinaryOp::logical_or;
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseLogAndExpr(), op);
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
        BinaryOp op = BinaryOp::logical_and;
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseBitOrExpr(), op);
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
        BinaryOp op = BinaryOp::bitwise_or;
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseBitXorExpr(), op);
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
        BinaryOp op = BinaryOp::bitwise_and;
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseEqualExpr(), op);
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
        BinaryOp op = BinaryOp::bitwise_xor;
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseBitAndExpr(), op);
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
        BinaryOp op;
        if (tok.tokenType == TokenType::less_less)
        {
            op = BinaryOp::left_shift;
        }
        else
        {
            op = BinaryOp::right_shift;
        }
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseAddExpr(), op);
    }
    return left;
}

// mult        ::= cast ("*" | "/" | "%" cast)*
std::shared_ptr<AstNode> Parser::ParseMutliExpr()
{
    auto left = ParseUnaryExpr();
    // 左结合
    while (
        tok.tokenType == TokenType::star ||
        tok.tokenType == TokenType::div ||
        tok.tokenType == TokenType::percent)
    {
        BinaryOp op;
        if (tok.tokenType == TokenType::div)
        {
            op = BinaryOp::div;
        }
        else if (tok.tokenType == TokenType::percent)
        {
            op = BinaryOp::mod;
        }
        else
        {
            op = BinaryOp::mul;
        }
        Advance();
        // 要继续处理下一个Expr,当前Expr变成left child
        left = sema.SemaBinaryExprNode(left, ParseUnaryExpr(), op);
    }
    return left;
}

// unary       ::= postfix | ("++"|"--"|"&"|"*"|"+"|"-"|"~"|"!"|"sizeof") unary
//               | "sizeof" "(" type-name ")"
std::shared_ptr<AstNode> Parser::ParseUnaryExpr()
{
    if (!IsUnaryOperator())
    {
        return ParsePostFixExpr();
    }

    if (tok.tokenType == TokenType::kw_sizeof)
    {
        bool isTypeName = false;
        Consume(TokenType::kw_sizeof);

        if (tok.tokenType == TokenType::l_parent)
        {
            lexer.SaveState();
            Token nextTok;
            lexer.NextToken(nextTok);
            if (IsTypeName(nextTok.tokenType))
            {
                isTypeName = true;
            }
            lexer.RestoreState();
        }

        if (isTypeName)
        {
            Consume(TokenType::l_parent);
            auto type = ParseType();
            Consume(TokenType::r_parent);
            return sema.SemaSizieOfExprNode(nullptr, type);
        }
        else
        {
            auto node = ParseUnaryExpr();
            return sema.SemaSizieOfExprNode(node, nullptr);
        }
    }

    UnaryOp op;
    switch (tok.tokenType)
    {
    case TokenType::plus_plus:
        op = UnaryOp::inc;
        break;
    case TokenType::minus_minus:
        op = UnaryOp::dec;
        break;
    case TokenType::plus:
        op = UnaryOp::positive;
        break;
    case TokenType::minus:
        op = UnaryOp::negative;
        break;
    case TokenType::star:
        op = UnaryOp::deref;
        break;
    case TokenType::amp:
        op = UnaryOp::addr;
        break;
    case TokenType::exclaim:
        op = UnaryOp::logical_not;
        break;
    case TokenType::tilde:
        op = UnaryOp::bitwise_not;
        break;
    }

    Advance(); // 吃掉一元运算符

    Token tmp = tok;

    auto node = ParseUnaryExpr();
    return sema.SemaUnaryExprNode(node, op, tok);
}

// postfix     ::= primary ("++"|"--")*
std::shared_ptr<AstNode> Parser::ParsePostFixExpr()
{
    auto left = ParsePrimary();
    while (true)
    {
        if (tok.tokenType == TokenType::plus_plus)
        {
            Token tmp = tok;
            Consume(TokenType::plus_plus);
            left = sema.SemaPostIncExprNode(left, tmp);
            continue;
        }
        else if (tok.tokenType == TokenType::minus_minus)
        {
            Token tmp = tok;
            Consume(TokenType::minus_minus);
            left = sema.SemaPostDecExprNode(left, tmp);
            continue;
        }
        break;
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

bool Parser::IsAssignOperator()
{
    return tok.tokenType == TokenType::equal ||
           tok.tokenType == TokenType::plus_equal ||
           tok.tokenType == TokenType::minus_equal ||
           tok.tokenType == TokenType::star_equal ||
           tok.tokenType == TokenType::slash_equal ||
           tok.tokenType == TokenType::percent_equal ||
           tok.tokenType == TokenType::less_less_equal ||
           tok.tokenType == TokenType::greater_greater_equal ||
           tok.tokenType == TokenType::amp_equal ||
           tok.tokenType == TokenType::pipe_equal ||
           tok.tokenType == TokenType::caret_equal;
}

bool Parser::IsUnaryOperator()
{
    return tok.tokenType == TokenType::plus_plus ||
           tok.tokenType == TokenType::minus_minus ||
           tok.tokenType == TokenType::plus ||
           tok.tokenType == TokenType::minus ||
           tok.tokenType == TokenType::star ||
           tok.tokenType == TokenType::amp ||
           tok.tokenType == TokenType::exclaim ||
           tok.tokenType == TokenType::tilde ||
           tok.tokenType == TokenType::kw_sizeof;
}

std::shared_ptr<CType> Parser::ParseType()
{
    std::shared_ptr<CType> baseType = nullptr;
    if (tok.tokenType == TokenType::kw_int)
    {
        baseType = CType::IntType;
        Consume(TokenType::kw_int);
    }
    assert(baseType);
    while (tok.tokenType == TokenType::star)
    {
        baseType = std::make_shared<CPointType>(baseType);
        Consume(TokenType::star);
    }
    return baseType;
}

bool Parser::IsTypeName(TokenType tokenType)
{
    if (tokenType == TokenType::kw_int)
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