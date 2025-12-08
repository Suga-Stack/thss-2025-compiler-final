#include "SymbolTable.h"

void SymbolTable::enterScope() { scopes.emplace_back(); }
void SymbolTable::leaveScope() { if (!scopes.empty()) scopes.pop_back(); }

bool SymbolTable::add(const std::string &name, SymbolInfo info) {
  if (scopes.empty()) enterScope();
  auto &top = scopes.back();
  if (top.find(name) != top.end()) return false;
  top.emplace(name, std::move(info));
  return true;
}

const SymbolInfo *SymbolTable::lookup(const std::string &name) const {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto f = it->find(name);
    if (f != it->end()) return &f->second;
  }
  return nullptr;
}
