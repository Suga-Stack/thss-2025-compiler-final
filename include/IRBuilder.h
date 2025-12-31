#pragma once

#include <string>
#include <memory>
#include "IR.h"

// IR 构建器，用于简化指令生成过程
class IRBuilder
{
public:
  explicit IRBuilder(Module *m) : module(m) {}

  // 创建函数定义
  Function *createFunction(const std::string &name, TypePtr retType, bool isDecl = false);
  // 创建基本块
  BasicBlock *createBasicBlock(Function *f, const std::string &name);
  // 设置当前插入点 (后续指令将插入到此基本块)
  void setInsertPoint(BasicBlock *bb) { insertBB = bb; }
  // 检查当前基本块是否已终结 (已有 ret 或 br)
  bool isTerminated() const { return insertBB && insertBB->isTerminated(); }

  // 指令生成辅助函数 (返回生成的临时变量名，如 "%tmp1")

  // 栈内存分配 (alloca)
  std::string createAlloca(TypePtr ty, const std::string &hint);
  // 内存存储 (store)
  void createStore(const std::string &val, const std::string &ptr, TypePtr ty);
  // 内存加载 (load)
  std::string createLoad(const std::string &ptr, TypePtr ty, const std::string &hint);
  // 函数返回 (ret)
  void createRet(const std::string &val, TypePtr retType, bool hasVal);
  // 二元运算 (add, sub, mul, sdiv, srem)
  std::string createBinary(const std::string &op, const std::string &lhs, const std::string &rhs, const std::string &hint);
  // 无条件跳转 (br label)
  void createBr(const std::string &targetLabel);
  // 条件跳转 (br i1 cond, label t, label f)
  void createCondBr(const std::string &cond, const std::string &tLabel, const std::string &fLabel);
  // 整数比较 (icmp)
  std::string createICmp(const std::string &pred, const std::string &lhs, const std::string &rhs, const std::string &hint);
  // 函数调用 (call)
  std::string createCall(const std::string &callee, const std::vector<std::pair<std::string, TypePtr>> &args, TypePtr retTy, const std::string &hint);
  // 获取元素指针 (getelementptr) - 用于数组访问
  std::string createGEP(TypePtr elemTy, const std::string &basePtr, const std::vector<std::string> &indices, const std::string &hint);
  // 零扩展 (zext) - 用于 i1 转 i32
  std::string createZExt(const std::string &val, TypePtr fromTy, TypePtr toTy, const std::string &hint);

private:
  Module *module;
  Function *curFunc = nullptr;    // 当前正在构建的函数
  BasicBlock *insertBB = nullptr; // 当前插入的基本块
  unsigned tmpCounter = 0;        // 临时变量计数器
  unsigned blockCounter = 0;      // 基本块计数器

  // 生成唯一的临时变量名
  std::string uniqueTmp(const std::string &hint);
  // 生成唯一的块名
  std::string uniqueBlock(const std::string &hint);
};
