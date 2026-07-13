#include "lexer.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/ErrorOr.h"
#include "parser.h"
#include "printVisitor.h"
#include "codegen.h"

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

    std::unique_ptr<llvm::MemoryBuffer> memBuf = std::move(*buf);
    Lexer lex(memBuf->getBuffer());

    // Token tok;
    // while (true)
    // {
    //     lex.NextToken(tok);
    //     if (tok.tokenType != TokenType::eof)
    //         tok.Dump();
    //     else
    //         break;
    // }

    Parser p(lex);
    auto program = p.ParserProgram();

    // PrintVisitor printVisitor(program);
    CodeGen codeGen(program);

    return 0;
}