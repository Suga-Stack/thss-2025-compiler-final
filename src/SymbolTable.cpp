#include "SymbolTable.h"

// 进入新的作用域：压入一个新的符号表层级
void SymbolTable::enterScope() { scopes.emplace_back(); }

// 离开当前作用域：弹出一个符号表层级
void SymbolTable::leaveScope()
{
  if (!scopes.empty())
    scopes.pop_back();
}

// 在当前作用域添加符号
bool SymbolTable::add(const std::string &name, SymbolInfo info)
{
  if (scopes.empty())
    enterScope();
  auto &top = scopes.back();
  if (top.find(name) != top.end())
    return false; // 重复定义
  top.emplace(name, std::move(info));
  return true;
}

// 查找符号：从最内层作用域向外查找
const SymbolInfo *SymbolTable::lookup(const std::string &name) const
{
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
  {
    auto f = it->find(name);
    if (f != it->end())
      return &f->second;
  }
  return nullptr;
}
