#pragma once
#include <memory>
#include <vector>
#include "llvm/ADT/StringMap.h"
#include "type.h"

enum class SymbolKind
{
    obj, // var, func
    tag  // struct, union
};

class Symbol
{
private:
    SymbolKind kind;
    std::shared_ptr<CType> ty;
    llvm::StringRef name;

public:
    Symbol(SymbolKind kind, std::shared_ptr<CType> ty, llvm::StringRef name) : kind(kind), ty(ty), name(name) {}
    std::shared_ptr<CType> GetTy() { return ty; }
};

class Env
{
public:
    // 符号信息
    // llvm::StringMap 是 LLVM 自己实现的一个哈希表容器，专门用「字符串」当键。
    // 相当于 std::unordered_map<std::string, V>，但针对编译器场景做了优化。
    llvm::StringMap<std::shared_ptr<Symbol>> objSymbolTable;
    // struct & union
    llvm::StringMap<std::shared_ptr<Symbol>> tagSymbolTable;
};

class Scope
{
private:
    std::vector<std::shared_ptr<Env>> envs;

public:
    Scope();
    void EnterScope();
    void ExitScope();
    std::shared_ptr<Symbol> FindObjSymbol(llvm::StringRef name);
    std::shared_ptr<Symbol> FindObjSymbolInCurEnv(llvm::StringRef name);
    void AddObjSymbol(std::shared_ptr<CType> ty, llvm::StringRef name);

    std::shared_ptr<Symbol> FindTagSymbol(llvm::StringRef name);
    std::shared_ptr<Symbol> FindTagSymbolInCurEnv(llvm::StringRef name);
    void AddTagSymbol(std::shared_ptr<CType> ty, llvm::StringRef name);
};