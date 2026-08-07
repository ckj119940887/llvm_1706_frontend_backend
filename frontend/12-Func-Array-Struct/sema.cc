#include "sema.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

std::shared_ptr<AstNode> Sema::SemaVariableDeclNode(Token tok, std::shared_ptr<CType> ty, bool isGlobal)
{
    // 检查是否出现重定义
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindObjSymbolInCurEnv(text);
    /// Skip 模式下是前瞻试探性解析, 符号既不会入表, 也不该报重定义
    if (symbol && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_redefined, text);
    }

    if (mode == Mode::Normal)
    {
        // 添加到符号表
        scope.AddObjSymbol(ty, text);
    }

    auto variableDecl = std::make_shared<VariableDecl>();
    variableDecl->tok = tok;
    variableDecl->ty = ty;
    variableDecl->isLValue = true;
    variableDecl->isGloabl = isGlobal;

    return variableDecl;
}

std::shared_ptr<AstNode> Sema::SemaVariableAccessNode(Token tok)
{
    // 这个查找要在全部的env中进行查找
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindObjSymbol(text);
    if (symbol == nullptr && mode == Mode::Normal)
    {
        // 没有找到
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_undefined, text);
    }

    auto expr = std::make_shared<VariableAccessExpr>();
    expr->tok = tok;
    expr->ty = symbol->GetTy();
    expr->isLValue = true;
    return expr;
}

std::shared_ptr<AstNode> Sema::SemaNumberExprNode(Token tok, std::shared_ptr<CType> ty)
{
    auto factor = std::make_shared<NumberExpr>();
    factor->tok = tok;
    factor->ty = ty;

    return factor;
}

std::shared_ptr<AstNode> Sema::SemaBinaryExprNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode> right, BinaryOp op)
{
    auto binaryExpr = std::make_shared<BinaryExpr>();
    binaryExpr->left = left;
    binaryExpr->op = op;
    binaryExpr->right = right;
    binaryExpr->ty = left->ty;

    if (op == BinaryOp::add || op == BinaryOp::sub || op == BinaryOp::add_assign || op == BinaryOp::sub_assign)
    {
        // int a = 3; int *p = &a; 3+p;
        if ((left->ty->GetKind() == CType::TY_Int) && (right->ty->GetKind() == CType::TY_Point))
        {
            binaryExpr->ty = right->ty;
        }
    }

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

std::shared_ptr<AstNode> Sema::SemaUnaryExprNode(std::shared_ptr<AstNode> unary, UnaryOp op, Token tok)
{
    auto node = std::make_shared<UnaryExpr>();
    node->op = op;
    node->node = unary;

    switch (op)
    {
    case UnaryOp::positive:
    case UnaryOp::negative:
    case UnaryOp::logical_not:
    case UnaryOp::bitwise_not:
    {
        if (unary->ty->GetKind() != CType::TY_Int && mode == Mode::Normal)
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_type, "int type");
        }
        node->ty = unary->ty;
        break;
    }
    case UnaryOp::addr:
    {
        if (!unary->isLValue && mode == Mode::Normal)
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_lvalue);
        }
        node->ty = std::make_shared<CPointType>(unary->ty);
        break;
    }
    case UnaryOp::deref:
    {
        // *a
        // 一定要是指针类型
        if (unary->ty->GetKind() != CType::TY_Point && mode == Mode::Normal)
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_type, "pointer type");
        }
        // if (!unary->isLValue)
        // {
        //     diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_lvalue);
        // }
        CPointType *pty = llvm::dyn_cast<CPointType>(unary->ty.get());
        node->ty = pty->GetBaseType();
        node->isLValue = true;
        break;
    }
    case UnaryOp::inc:
    case UnaryOp::dec:
    {
        if (!unary->isLValue && mode == Mode::Normal)
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_lvalue);
        }
        node->ty = unary->ty;
        break;
    }
    default:
        break;
    }

    return node;
}

std::shared_ptr<AstNode> Sema::SemaThreeExprNode(std::shared_ptr<AstNode> cond, std::shared_ptr<AstNode> then, std::shared_ptr<AstNode> els, Token tok)
{
    auto node = std::make_shared<ThreeExpr>();
    node->cond = cond;
    node->then = then;
    node->els = els;
    if (then->ty->GetKind() != els->ty->GetKind() && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_same_type);
    }
    node->ty = then->ty;
    return node;
}

std::shared_ptr<AstNode> Sema::SemaSizeOfExprNode(std::shared_ptr<AstNode> unary, std::shared_ptr<CType> ty)
{
    auto node = std::make_shared<SizeOfExpr>();
    node->type = ty;
    node->node = unary;
    node->ty = CType::IntType;
    return node;
}

std::shared_ptr<AstNode> Sema::SemaPostIncExprNode(std::shared_ptr<AstNode> left, Token tok)
{
    if (!left->isLValue && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_lvalue);
    }
    auto node = std::make_shared<PostIncExpr>();
    node->left = left;
    node->ty = left->ty;
    return node;
}

std::shared_ptr<AstNode> Sema::SemaPostDecExprNode(std::shared_ptr<AstNode> left, Token tok)
{
    if (!left->isLValue && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_lvalue);
    }
    auto node = std::make_shared<PostDecExpr>();
    node->left = left;
    node->ty = left->ty;
    return node;
}

// a[1] -> *(a + offset(1 * elementSize))
std::shared_ptr<AstNode> Sema::SemaPostSubscriptNode(std::shared_ptr<AstNode> left, std::shared_ptr<AstNode> node, Token tok)
{
    if (left->ty->GetKind() != CType::TY_Array && left->ty->GetKind() != CType::TY_Point && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_expected_type, "array or point");
    }
    auto post = std::make_shared<PostSubscript>();
    post->left = left;
    post->node = node;
    if (left->ty->GetKind() == CType::TY_Array)
    {
        CArrayType *arrType = llvm::dyn_cast<CArrayType>(left->ty.get());
        post->ty = arrType->GetElementType();
    }
    else if (left->ty->GetKind() == CType::TY_Point)
    {
        CPointType *pointType = llvm::dyn_cast<CPointType>(left->ty.get());
        post->ty = pointType->GetBaseType();
    }
    /// a[i] 是左值
    post->isLValue = true;
    return post;
}

std::shared_ptr<AstNode> Sema::SemaPostMemberDotNode(std::shared_ptr<AstNode> left, Token iden, Token dotTok)
{
    if (left->ty->GetKind() != CType::TY_Record && (mode == Mode::Normal))
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(dotTok.ptr), diag::err_expected_type, "struct or union type");
    }

    CRecordType *recordType = llvm::dyn_cast<CRecordType>(left->ty.get());
    const auto &members = recordType->GetMembers();

    bool found = false;
    Member curMember;
    for (const auto &member : members)
    {
        if (member.name == llvm::StringRef(iden.ptr, iden.len))
        {
            found = true;
            curMember = member;
            break;
        }
    }

    if (!found)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(iden.ptr), diag::err_miss, "struct or union miss field");
    }

    auto node = std::make_shared<PostMemberDotExpr>();
    node->tok = dotTok;
    node->ty = curMember.ty;
    node->left = left;
    node->member = curMember;
    /// a.b 是左值, 可以取地址 / 自增自减
    node->isLValue = true;

    return node;
}

std::shared_ptr<AstNode> Sema::SemaPostMemberArrowNode(std::shared_ptr<AstNode> left, Token iden, Token arrowTok)
{
    if (left->ty->GetKind() != CType::TY_Point)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(arrowTok.ptr), diag::err_expected_type, "pointer type");
    }

    CPointType *pRecordType = llvm::dyn_cast<CPointType>(left->ty.get());
    if (pRecordType->GetBaseType()->GetKind() != CType::TY_Record)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(arrowTok.ptr), diag::err_expected_type, "pointer to struct or union type");
    }

    CRecordType *recordType = llvm::dyn_cast<CRecordType>(pRecordType->GetBaseType().get());
    const auto &members = recordType->GetMembers();

    bool found = false;
    Member curMember;
    for (const auto &member : members)
    {
        if (member.name == llvm::StringRef(iden.ptr, iden.len))
        {
            found = true;
            curMember = member;
            break;
        }
    }

    if (!found)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(iden.ptr), diag::err_miss, "struct or union miss field");
    }

    auto node = std::make_shared<PostMemberArrowExpr>();
    node->tok = arrowTok;
    node->ty = curMember.ty;
    node->left = left;
    node->member = curMember;
    /// a->b 同样是左值
    node->isLValue = true;

    return node;
}

std::shared_ptr<VariableDecl::InitValue> Sema::SemaInitValue(std::shared_ptr<CType> declType, std::shared_ptr<AstNode> value, std::vector<int> &offsetList, Token tok)
{
    // if (declType->GetKind() != value->ty->GetKind() && mode == Mode::Normal)
    // {
    //     diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_miss, "same type");
    // }

    auto initValue = std::make_shared<VariableDecl::InitValue>();
    initValue->declType = declType;
    initValue->value = value;
    initValue->offsetList = offsetList;

    return initValue;
}

std::shared_ptr<CType> Sema::SemaTagAccess(Token tok)
{
    // 这个查找要在全部的env中进行查找
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindTagSymbol(text);
    if (symbol)
    {
        return symbol->GetTy();
    }
    else
    {
        return nullptr;
    }
}

std::shared_ptr<CType> Sema::SemaTagDecl(Token tok, const std::vector<Member> &members, TagKind tagKind)
{
    // 检查是否出现重定义
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindTagSymbolInCurEnv(text);
    if (symbol && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_redefined, text);
    }

    auto recordTy = std::make_shared<CRecordType>(text, members, tagKind);

    if (mode == Mode::Normal)
    {
        // 添加到符号表
        scope.AddTagSymbol(recordTy, text);
    }

    return recordTy;
}

std::shared_ptr<CType> Sema::SemaTagDecl(Token tok, std::shared_ptr<CType> type)
{
    // 检查是否出现重定义
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindTagSymbolInCurEnv(text);
    if (symbol && mode == Mode::Normal)
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_redefined, text);
    }

    if (mode == Mode::Normal)
    {
        // 添加到符号表
        scope.AddTagSymbol(type, text);
    }

    return type;
}

std::shared_ptr<CType> Sema::SemaAnnoyTagDecl(const std::vector<Member> &members, TagKind tagKind)
{
    llvm::StringRef text = CType::GenAnnoyRecordName(tagKind);
    auto recordTy = std::make_shared<CRecordType>(text, members, tagKind);

    if (mode == Mode::Normal)
    {
        // 添加到符号表
        scope.AddTagSymbol(recordTy, text);
    }

    return recordTy;
}

std::shared_ptr<AstNode> Sema::SemaFuncDecl(Token tok, std::shared_ptr<CType> type, std::shared_ptr<AstNode> blockStmt)
{
    CFuncType *funTy = llvm::dyn_cast<CFuncType>(type.get());
    funTy->hasBody = blockStmt ? true : false;

    // 1. 检测是否出现重定义, 只有带函数体的才算定义
    llvm::StringRef text(tok.ptr, tok.len);
    std::shared_ptr<Symbol> symbol = scope.FindObjSymbolInCurEnv(text);
    if (symbol)
    {
        auto symTy = symbol->GetTy();
        if (symTy->GetKind() != CType::TY_Func && mode == Mode::Normal)
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_redefined, text);
        }
        CFuncType *symbolFunTy = llvm::dyn_cast<CFuncType>(symTy.get());
        if (symbolFunTy && symbolFunTy->hasBody && funTy->hasBody && blockStmt && mode == Mode::Normal)
        {
            diagEngine.Report(llvm::SMLoc::getFromPointer(tok.ptr), diag::err_redefined, text);
        }
    }

    // 2. 函数名加到当前(全局)符号表, 后面才能被调用
    if ((symbol == nullptr || funTy->hasBody) && (mode == Mode::Normal))
    {
        scope.AddObjSymbol(type, text);
    }

    auto funcDecl = std::make_shared<FuncDecl>();
    funcDecl->ty = type;
    funcDecl->blockStmt = blockStmt;
    funcDecl->tok = tok;
    return funcDecl;
}

std::shared_ptr<AstNode> Sema::SemaFuncCall(std::shared_ptr<AstNode> left, std::vector<std::shared_ptr<AstNode>> &args)
{
    Token iden = left->tok;
    if (left->ty->GetKind() != CType::TY_Func && (mode == Mode::Normal))
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(iden.ptr), diag::err_expected, "functype");
    }

    CFuncType *funcType = llvm::dyn_cast<CFuncType>(left->ty.get());
    if (funcType->GetParams().size() != args.size() && (mode == Mode::Normal))
    {
        diagEngine.Report(llvm::SMLoc::getFromPointer(iden.ptr), diag::err_miss, "arg count not match");
    }

    auto funcDecl = std::make_shared<PostFuncCall>();
    funcDecl->ty = funcType->GetRetType();
    funcDecl->left = left;
    funcDecl->args = args;
    funcDecl->tok = left->tok;
    return funcDecl;
}

void Sema::EnterScope()
{
    scope.EnterScope();
}

void Sema::ExitScope()
{
    scope.ExitScope();
}

void Sema::SetMode(Mode mode)
{
    this->mode = mode;
}