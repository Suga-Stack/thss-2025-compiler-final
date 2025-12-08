#pragma once

#include <string>
#include <memory>
#include "IR.h"

class IRBuilder {
public:
  explicit IRBuilder(Module *m) : module(m) {}
  Function *createFunction(const std::string &name, TypePtr retType);
  BasicBlock *createBasicBlock(Function *f, const std::string &name);
  void setInsertPoint(BasicBlock *bb) { insertBB = bb; }

  // instruction helpers (return textual names where appropriate)
  std::string createAlloca(TypePtr ty, const std::string &hint);
  void createStore(const std::string &val, const std::string &ptr, TypePtr ty);
  std::string createLoad(const std::string &ptr, TypePtr ty, const std::string &hint);
  void createRet(const std::string &val, TypePtr retType, bool hasVal);
  std::string createBinary(const std::string &op, const std::string &lhs, const std::string &rhs, const std::string &hint);
  void createBr(const std::string &targetLabel);
  void createCondBr(const std::string &cond, const std::string &tLabel, const std::string &fLabel);
  std::string createICmp(const std::string &pred, const std::string &lhs, const std::string &rhs, const std::string &hint);
  std::string createCall(const std::string &callee, const std::vector<std::pair<std::string, TypePtr>> &args, TypePtr retTy, const std::string &hint);
  std::string createGEP(TypePtr elemTy, const std::string &basePtr, const std::vector<std::string> &indices, const std::string &hint);

private:
  Module *module;
  BasicBlock *insertBB = nullptr;
  unsigned tmpCounter = 0;
  unsigned blockCounter = 0;
  std::string uniqueTmp(const std::string &hint);
  std::string uniqueBlock(const std::string &hint);
};
