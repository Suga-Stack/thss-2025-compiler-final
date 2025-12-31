#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Type.h"

struct SymbolInfo
{
  TypePtr type;
  // IR name for the storage (e.g., "%a0")
  std::string irName;
  bool isConst = false;
  int constValue = 0;
};

class SymbolTable
{
public:
  void enterScope();
  void leaveScope();
  bool add(const std::string &name, SymbolInfo info);
  const SymbolInfo *lookup(const std::string &name) const;

private:
  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
};
