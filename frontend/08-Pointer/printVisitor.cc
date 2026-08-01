#include "printVisitor.h"

PrintVisitor::PrintVisitor(std::shared_ptr<Program> program)
{
    // 返回的是它内部管理的裸指针 Program*
    VisitProgram(program.get());
}

llvm::Value *PrintVisitor::VisitProgram(Program *p)
{
    p->node->Accept(this);
    return nullptr;
}

llvm::Value *PrintVisitor::VisitBlockStmt(BlockStmt *p)
{
    llvm::outs() << "{\n";
    for (auto &node : p->nodeVec)
    {
        node->Accept(this);
        llvm::outs() << "\n";
    }
    llvm::outs() << "}";
    return nullptr;
}

llvm::Value *PrintVisitor::VisitDeclStmt(DeclStmt *p)
{
    for (auto &node : p->nodeVec)
    {
        node->Accept(this);
    }
    return nullptr;
}

llvm::Value *PrintVisitor::VisitIfStmt(IfStmt *p)
{
    llvm::outs() << "if(";
    p->condNode->Accept(this);
    llvm::outs() << ")\n";
    p->thenNode->Accept(this);
    if (p->elseNode)
    {
        llvm::outs() << "\nelse\n";
        p->elseNode->Accept(this);
    }
    return nullptr;
}

llvm::Value *PrintVisitor::VisitForStmt(ForStmt *p)
{
    llvm::outs() << "for (";

    if (p->initNode)
    {
        p->initNode->Accept(this);
    }
    llvm::outs() << ";";

    if (p->condNode)
    {
        p->condNode->Accept(this);
    }
    llvm::outs() << ";";

    if (p->incNode)
    {
        p->incNode->Accept(this);
    }
    llvm::outs() << ")\n";

    if (p->bodyNode)
    {
        p->bodyNode->Accept(this);
    }

    return nullptr;
}

llvm::Value *PrintVisitor::VisitBreakStmt(BreakStmt *p)
{
    llvm::outs() << "break ;";
    return nullptr;
}

llvm::Value *PrintVisitor::VisitContinueStmt(ContinueStmt *p)
{
    llvm::outs() << "continue ;";
    return nullptr;
}

llvm::Value *PrintVisitor::VisitVariableDecl(VariableDecl *decl)
{
    decl->ty->Accept(this);

    llvm::StringRef text(decl->tok.ptr, decl->tok.len);
    llvm::outs() << text;

    if(decl->init) {
        llvm::outs() << "=";
        decl->init->Accept(this);
    }

    return nullptr;
}

llvm::Value *PrintVisitor::VisitBinaryExpr(BinaryExpr *binaryExpr)
{
    // 中序遍历
    binaryExpr->left->Accept(this);

    switch (binaryExpr->op)
    {
    case BinaryOp::add:
    {
        llvm::outs() << " + ";
        break;
    }
    case BinaryOp::sub:
    {
        llvm::outs() << " - ";
        break;
    }
    case BinaryOp::mul:
    {
        llvm::outs() << " * ";
        break;
    }
    case BinaryOp::div:
    {
        llvm::outs() << " / ";
        break;
    }
    case BinaryOp::equal:
    {
        llvm::outs() << " == ";
        break;
    }
    case BinaryOp::not_equal:
    {
        llvm::outs() << " != ";
        break;
    }
    case BinaryOp::less:
    {
        llvm::outs() << " < ";
        break;
    }
    case BinaryOp::less_equal:
    {
        llvm::outs() << " <= ";
        break;
    }
    case BinaryOp::greater:
    {
        llvm::outs() << " > ";
        break;
    }
    case BinaryOp::greater_equal:
    {
        llvm::outs() << " >= ";
        break;
    }
    case BinaryOp::bitwise_and:
    {
        llvm::outs() << " & ";
        break;
    }
    case BinaryOp::bitwise_or:
    {
        llvm::outs() << " | ";
        break;
    }
    case BinaryOp::bitwise_xor:
    {
        llvm::outs() << " ^ ";
        break;
    }
    case BinaryOp::logical_and:
    {
        llvm::outs() << " && ";
        break;
    }
    case BinaryOp::logical_or:
    {
        llvm::outs() << " || ";
        break;
    }
    case BinaryOp::mod:
    {
        llvm::outs() << " % ";
        break;
    }
    case BinaryOp::left_shift:
    {
        llvm::outs() << " << ";
        break;
    }
    case BinaryOp::right_shift:
    {
        llvm::outs() << " >> ";
        break;
    }
    case BinaryOp::comma:
    {
        llvm::outs() << ",";
        break;
    }
    case BinaryOp::assign:
    {
        llvm::outs() << "=";
        break;
    }
    case BinaryOp::add_assign:
    {
        llvm::outs() << "+=";
        break;
    }
    case BinaryOp::sub_assign:
    {
        llvm::outs() << "-=";
        break;
    }
    case BinaryOp::mul_assign:
    {
        llvm::outs() << "*=";
        break;
    }
    case BinaryOp::div_assign:
    {
        llvm::outs() << "/=";
        break;
    }
    case BinaryOp::mod_assign:
    {
        llvm::outs() << "%=";
        break;
    }
    case BinaryOp::bitwise_or_assign:
    {
        llvm::outs() << "|=";
        break;
    }
    case BinaryOp::bitwise_xor_assign:
    {
        llvm::outs() << "^=";
        break;
    }
    case BinaryOp::bitwise_and_assign:
    {
        llvm::outs() << "&=";
        break;
    }
    case BinaryOp::left_shift_assign:
    {
        llvm::outs() << "<<=";
        break;
    }
    case BinaryOp::right_shift_assign:
    {
        llvm::outs() << ">>=";
        break;
    }
    default:
        break;
    }

    binaryExpr->right->Accept(this);

    return nullptr;
}

llvm::Value *PrintVisitor::VisitThreeExpr(ThreeExpr *threeExpr)
{
    threeExpr->cond->Accept(this);
    llvm::outs() << "?";
    threeExpr->then->Accept(this);
    llvm::outs() << ":";
    threeExpr->els->Accept(this);
    return nullptr;
}

llvm::Value *PrintVisitor::VisitUnaryExpr(UnaryExpr *unaryExpr)
{
    switch (unaryExpr->op)
    {
    case UnaryOp::positive:
    {
        llvm::outs() << "+";
        break;
    }
    case UnaryOp::negative:
    {
         llvm::outs() << "-";
        break;
    }
    case UnaryOp::deref:
    {
         llvm::outs() << "*";
        break;
    }
    case UnaryOp::addr:
    {
         llvm::outs() << "&";
        break;
    }
    case UnaryOp::inc:
    {
         llvm::outs() << "++";
        break;
    }
    case UnaryOp::dec:
    {
         llvm::outs() << "--";
        break;
    }
    case UnaryOp::logical_not:
    {
         llvm::outs() << "!";
        break;
    }
    case UnaryOp::bitwise_not:
    {
        llvm::outs() << "~";
        break;
    }
    default:
        break;
    }

    unaryExpr->node->Accept(this);
    return nullptr;
}

llvm::Value *PrintVisitor::VisitSizeOfExpr(SizeOfExpr *sizeOfExpr)
{
    llvm::outs() << "sizeof";
    if(sizeOfExpr->type){
        llvm::outs() << "(";
        sizeOfExpr->type->Accept(this);
        llvm::outs() << ")";
    } else {
        sizeOfExpr->type->Accept(this);
    }
    return nullptr;
}

llvm::Value *PrintVisitor::VisitPostIncExpr(PostIncExpr *postIncExpr)
{
    postIncExpr->left->Accept(this);
    llvm::outs() << "++";
    return nullptr;
}

llvm::Value *PrintVisitor::VisitPostDecExpr(PostDecExpr *postDecExpr)
{
    postDecExpr->left->Accept(this);
    llvm::outs() << "--";
    return nullptr;
}

llvm::Value *PrintVisitor::VisitNumberExpr(NumberExpr *numberExpr)
{
    llvm::outs() << llvm::StringRef(numberExpr->tok.ptr, numberExpr->tok.len);
    return nullptr;
}

llvm::Value *PrintVisitor::VisitVariableAccessExpr(VariableAccessExpr *variableAccessExpr)
{
    llvm::outs() << llvm::StringRef(variableAccessExpr->tok.ptr, variableAccessExpr->tok.len);
    return nullptr;
}


llvm::Type *PrintVisitor::VisitPrimaryType(CPrimaryType *ty) 
{
    if(ty->GetKind() == CType::TY_Int) {
        llvm::outs() << "int ";
    }

    return nullptr;
}

llvm::Type *PrintVisitor::VisitPointType(CPointType *ty) 
{
    // int **p
    ty->GetBaseType()->Accept(this);
    llvm::outs() << "*";

    return nullptr;
}