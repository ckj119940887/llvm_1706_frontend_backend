#include "scope.h"

Scope::Scope()
{
    // 主环境
    envs.push_back(std::make_shared<Env>());
}

void Scope::EnterScope()
{
    envs.push_back(std::make_shared<Env>());
}

void Scope::ExitScope()
{
    envs.pop_back();
}

std::shared_ptr<Symbol> Scope::FindVarSymbol(llvm::StringRef name)
{
    // 逆向查找
    for (auto it = envs.rbegin(); it != envs.rend(); ++it)
    {
        auto &table = (*it)->variableSymbolTable;
        if (table.count(name) > 0)
        {
            return table[name];
        }
    }

    return nullptr;
}

std::shared_ptr<Symbol> Scope::FindVarSymbolInCurEnv(llvm::StringRef name)
{
    auto &table = envs.back()->variableSymbolTable;
    if (table.count(name) > 0)
    {
        return table[name];
    }

    return nullptr;
}

void Scope::AddSymbol(SymbolKind kind, std::shared_ptr<CType> ty, llvm::StringRef name)
{
    auto symbol = std::make_shared<Symbol>(kind, ty, name);
    // 这行是往当前作用域的符号表里插入一条「变量名 → 符号」的映射。
    envs.back()->variableSymbolTable.insert({name, symbol});
}