#include "lexer.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ErrorOr.h"
#include "parser.h"
#include "printVisitor.h"
#include "codegen.h"
#include "sema.h"
#include "diag_engine.h"

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("input your filename\n");
        return 0;
    }

    const char *fileName = argv[1];
    static llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buf = llvm::MemoryBuffer::getFile(fileName);
    if (!buf)
    {
        llvm::errs() << "can't open file !!!\n";
        return -1;
    }

    llvm::SourceMgr mgr;                                    // LLVM 的源码管理器
    DiagEngine diagEngine(mgr);                             // 引擎持有 mgr 的引用
    mgr.AddNewSourceBuffer(std::move(*buf), llvm::SMLoc()); // 把整个文件内容交给 mgr

    Lexer lex(mgr, diagEngine);

    // Token tok;
    // while (true)
    // {
    //     lex.NextToken(tok);
    //     if (tok.tokenType != TokenType::eof)
    //         tok.Dump();
    //     else
    //         break;
    // }

    Sema sema(diagEngine);
    Parser p(lex, sema);
    auto program = p.ParseProgram();

    // PrintVisitor printVisitor(program);
    CodeGen codeGen(program);

    return 0;
}