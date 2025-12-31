#include "IR.h"
#include <sstream>

// BasicBlock
// 将指令添加到基本块的最前端 (用于 alloca 优化)
void BasicBlock::addInstFront(InstPtr inst)
{
  insts.insert(insts.begin(), std::move(inst));
}

std::string BasicBlock::toString() const
{
  std::ostringstream oss;
  oss << name << ":\n"; // 打印基本块标签
  for (const auto &i : insts)
  {
    oss << "  " << i->toString() << "\n"; // 打印每条指令
  }
  return oss.str();
}

// Function
BasicBlock *Function::appendBlock(const std::string &name)
{
  blocks.emplace_back(std::make_unique<BasicBlock>(name));
  return blocks.back().get();
}

std::string Function::toString() const
{
  std::ostringstream oss;
  if (isDeclaration)
  {
    oss << "declare " << retType->toString() << " @" << name << "(";
  }
  else
  {
    oss << "define " << retType->toString() << " @" << name << "(";
  }

  for (size_t i = 0; i < params.size(); ++i)
  {
    oss << params[i].type->toString();
    if (!isDeclaration)
    {
      oss << " %" << params[i].name;
    }
    if (i + 1 < params.size())
      oss << ", ";
  }
  oss << ")";

  if (isDeclaration)
  {
    oss << "\n";
  }
  else
  {
    oss << " {\n";
    for (const auto &b : blocks)
    {
      oss << b->toString();
    }
    oss << "}\n";
  }
  return oss.str();
}

// Module
Function *Module::addFunction(std::unique_ptr<Function> f)
{
  functions.emplace_back(std::move(f));
  return functions.back().get();
}

GlobalVariable *Module::addGlobal(std::unique_ptr<GlobalVariable> g)
{
  globals.emplace_back(std::move(g));
  return globals.back().get();
}

std::string Module::toString() const
{
  std::ostringstream oss;
  oss << "; ModuleID = '" << name << "'\n";
  oss << "source_filename = \"" << name << "\"\n\n";
  for (const auto &g : globals)
  {
    oss << g->toString() << "\n";
  }
  if (!globals.empty())
    oss << "\n";
  for (const auto &f : functions)
  {
    oss << f->toString() << "\n";
  }
  return oss.str();
}

// Instructions
std::string AllocaInst::toString() const
{
  std::ostringstream oss;
  oss << "%" << dest << " = alloca " << ty->toString() << ", align 4";
  return oss.str();
}

std::string StoreInst::toString() const
{
  std::ostringstream oss;
  oss << "store " << ty->toString() << " " << val << ", " << ty->toString() << "* " << ptr << ", align 4";
  return oss.str();
}

std::string LoadInst::toString() const
{
  std::ostringstream oss;
  oss << "%" << dest << " = load " << ty->toString() << ", " << ty->toString() << "* " << ptr << ", align 4";
  return oss.str();
}

std::string ReturnInst::toString() const
{
  std::ostringstream oss;
  if (hasValue)
  {
    oss << "ret " << retType->toString() << " " << val;
  }
  else
  {
    oss << "ret " << retType->toString();
  }
  return oss.str();
}

std::string BinaryInst::toString() const
{
  std::ostringstream oss;
  std::string llvmop = "add";
  if (op == "+")
    llvmop = "add";
  else if (op == "-")
    llvmop = "sub";
  else if (op == "*")
    llvmop = "mul";
  else if (op == "/")
    llvmop = "sdiv";
  else if (op == "%")
    llvmop = "srem";
  oss << "%" << dest << " = " << llvmop << " i32 " << lhs << ", " << rhs;
  return oss.str();
}

std::string BrInst::toString() const
{
  std::ostringstream oss;
  oss << "br label %" << target;
  return oss.str();
}

std::string CondBrInst::toString() const
{
  std::ostringstream oss;
  oss << "br i1 " << cond << ", label %" << tlabel << ", label %" << flabel;
  return oss.str();
}

std::string ICmpInst::toString() const
{
  std::ostringstream oss;
  oss << "%" << dest << " = icmp " << pred << " i32 " << lhs << ", " << rhs;
  return oss.str();
}

std::string CallInst::toString() const
{
  std::ostringstream oss;
  if (hasResult)
    oss << "%" << dest << " = ";
  oss << "call " << retTy->toString() << " @" << callee << "(";
  for (size_t i = 0; i < args.size(); ++i)
  {
    oss << args[i].second->toString() << " " << args[i].first;
    if (i + 1 < args.size())
      oss << ", ";
  }
  oss << ")";
  return oss.str();
}

std::string GetElementPtrInst::toString() const
{
  std::ostringstream oss;
  oss << "%" << dest << " = getelementptr inbounds " << elemTy->toString() << ", " << elemTy->toString() << "* " << base;
  for (const auto &idx : indices)
  {
    oss << ", i32 " << idx;
  }
  return oss.str();
}

std::string GlobalVariable::toString() const
{
  std::ostringstream oss;
  oss << "@" << name << " = global " << ty->toString() << " " << init << ", align 4";
  return oss.str();
}

std::string ZextInst::toString() const
{
  std::ostringstream oss;
  oss << "%" << dest << " = zext " << fromTy->toString() << " " << src << " to " << toTy->toString();
  return oss.str();
}
