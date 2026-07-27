#pragma once
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/FormatVariadic.h"

/*
预处理结果:
namespace diag { enum { err_unknown_char, }; };   // err_unknown_char = 0
static const char *diag_msg[] = {
    "unknown char '{0}'",                          // 下标 0
};
static llvm::SourceMgr::DiagKind diag_kind[] = {
    llvm::SourceMgr::DK_Error,                     // 下标 0
};
*/
namespace diag
{
    enum
    {
#define DIAG(ID, KIND, MSG) ID,
#include "diag.inc"
    };
};

/*
编译器里到处都要报错，如果每处都写 printf("line %d: unknown char ...")，会有三个问题:

错误文案散落各处，无法统一管理/国际化;
没有源码位置信息，用户看不到出错的那一行和 ^ 标记;
错误等级（error/warning/note）无法统一控制。
DiagEngine 就是把这三件事收敛到一个地方: 一处声明诊断，全局统一格式化和输出。它是 Clang 里 DiagnosticsEngine 的极简仿制品。
*/
class DiagEngine
{
private:
    // llvm::SourceMgr 负责: 持有源文件缓冲区、把一个裸指针换算成「第几行第几列」、打印带上下文和 ^~~~ 波浪线的漂亮错误
    llvm::SourceMgr &mgr; // LLVM 的源码管理器
    llvm::SourceMgr::DiagKind GetDiagKind(unsigned id);
    const char *GetDiagMsg(unsigned id);

public:
    DiagEngine(llvm::SourceMgr &mgr) : mgr(mgr) {}

    template <typename... Args>
    void Report(llvm::SMLoc loc, unsigned diagId, Args... args)
    {
        auto kind = GetDiagKind(diagId);
        const char *fmt = GetDiagMsg(diagId);
        auto f = llvm::formatv(fmt, std::forward<Args>(args)...).str();
        mgr.PrintMessage(loc, kind, f);

        if (kind == llvm::SourceMgr::DK_Error)
        {
            exit(0);
        }
    }
};