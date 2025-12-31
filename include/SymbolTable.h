#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "Type.h"

// 符号信息结构体
struct SymbolInfo
{
  TypePtr type;
  // 存储的 IR 名称 (例如 "%a0", "@global_var")，用于生成指令时引用
  std::string irName;
  bool isConst = false; // 是否为常量
  int constValue = 0;   // 如果是常量，存储其编译期求值结果
};

// 符号表类，支持嵌套作用域
class SymbolTable
{
public:
  void enterScope();                                       // 进入新的作用域
  void leaveScope();                                       // 离开当前作用域
  bool add(const std::string &name, SymbolInfo info);      // 在当前作用域添加符号
  const SymbolInfo *lookup(const std::string &name) const; // 查找符号 (从内层向外层)

private:
  // 作用域栈，每个元素是一个名称到符号信息的映射
  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
};
