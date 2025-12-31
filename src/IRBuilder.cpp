#include "IRBuilder.h"
#include <sstream>

Function *IRBuilder::createFunction(const std::string &name, TypePtr retType, bool isDecl)
{
  auto f = std::make_unique<Function>(name, retType, isDecl);
  Function *fp = module->addFunction(std::move(f));
  curFunc = fp;
  return fp;
}

BasicBlock *IRBuilder::createBasicBlock(Function *f, const std::string &name)
{
  std::string n = uniqueBlock(name);
  BasicBlock *bb = f->appendBlock(n);
  return bb;
}

std::string IRBuilder::uniqueTmp(const std::string &hint)
{
  std::string prefix = hint;
  if (prefix.length() > 64)
    prefix = prefix.substr(0, 64);
  std::ostringstream oss;
  oss << prefix << tmpCounter++;
  return oss.str();
}

std::string IRBuilder::uniqueBlock(const std::string &hint)
{
  std::string prefix = hint;
  if (prefix.length() > 64)
    prefix = prefix.substr(0, 64);
  std::ostringstream oss;
  oss << prefix << "." << blockCounter++;
  return oss.str();
}

// 创建 alloca 指令 (栈内存分配)
// 优化：始终在当前函数的入口块(Entry Block)的前端插入 alloca，避免循环中栈空间爆炸
std::string IRBuilder::createAlloca(TypePtr ty, const std::string &hint)
{
  std::string name = uniqueTmp(hint.empty() ? "tmp" : hint);
  if (curFunc)
  {
    if (auto entry = curFunc->getEntryBlock())
    {
      entry->addInstFront(std::make_unique<AllocaInst>(name, ty));
      return "%" + name;
    }
  }
  if (!insertBB)
    return name;
  insertBB->addInst(std::make_unique<AllocaInst>(name, ty));
  return "%" + name;
}

void IRBuilder::createStore(const std::string &val, const std::string &ptr, TypePtr ty)
{
  if (!insertBB)
    return;
  insertBB->addInst(std::make_unique<StoreInst>(val, ptr, ty));
}

std::string IRBuilder::createLoad(const std::string &ptr, TypePtr ty, const std::string &hint)
{
  std::string name = uniqueTmp(hint.empty() ? "ld" : hint);
  if (!insertBB)
    return "%" + name;
  insertBB->addInst(std::make_unique<LoadInst>(name, ptr, ty));
  return "%" + name;
}

void IRBuilder::createRet(const std::string &val, TypePtr retType, bool hasVal)
{
  if (!insertBB)
    return;
  insertBB->addInst(std::make_unique<ReturnInst>(val, retType, hasVal));
}

std::string IRBuilder::createBinary(const std::string &op, const std::string &lhs, const std::string &rhs, const std::string &hint)
{
  std::string name = uniqueTmp(hint.empty() ? "tmp" : hint);
  if (!insertBB)
    return "%" + name;
  insertBB->addInst(std::make_unique<BinaryInst>(name, op, lhs, rhs));
  return "%" + name;
}

void IRBuilder::createBr(const std::string &targetLabel)
{
  if (!insertBB)
    return;
  insertBB->addInst(std::make_unique<BrInst>(targetLabel));
}

void IRBuilder::createCondBr(const std::string &cond, const std::string &tLabel, const std::string &fLabel)
{
  if (!insertBB)
    return;
  insertBB->addInst(std::make_unique<CondBrInst>(cond, tLabel, fLabel));
}

std::string IRBuilder::createICmp(const std::string &pred, const std::string &lhs, const std::string &rhs, const std::string &hint)
{
  std::string name = uniqueTmp(hint.empty() ? "cmp" : hint);
  if (!insertBB)
    return "%" + name;
  insertBB->addInst(std::make_unique<ICmpInst>(name, pred, lhs, rhs));
  return "%" + name;
}

std::string IRBuilder::createCall(const std::string &callee, const std::vector<std::pair<std::string, TypePtr>> &args, TypePtr retTy, const std::string &hint)
{
  std::string dest;
  bool hasResult = retTy->getID() != Type::VoidTy;
  if (hasResult)
  {
    dest = uniqueTmp(hint.empty() ? "call" : hint);
  }
  if (!insertBB)
  {
    return hasResult ? "%" + dest : "";
  }
  insertBB->addInst(std::make_unique<CallInst>(dest, callee, retTy, args, hasResult));
  return hasResult ? "%" + dest : "";
}

std::string IRBuilder::createGEP(TypePtr elemTy, const std::string &basePtr, const std::vector<std::string> &indices, const std::string &hint)
{
  std::string name = uniqueTmp(hint.empty() ? "gep" : hint);
  if (!insertBB)
    return "%" + name;
  insertBB->addInst(std::make_unique<GetElementPtrInst>(name, elemTy, basePtr, indices));
  return "%" + name;
}

std::string IRBuilder::createZExt(const std::string &val, TypePtr fromTy, TypePtr toTy, const std::string &hint)
{
  std::string name = uniqueTmp(hint.empty() ? "zext" : hint);
  if (!insertBB)
    return "%" + name;
  insertBB->addInst(std::make_unique<ZextInst>(name, val, fromTy, toTy));
  return "%" + name;
}
