// 最小化 IR 核心声明
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Type.h"

// 核心 Value/Instruction/Module 层次结构 (基于字符串的最小化实现)
class Instruction
{
public:
  virtual ~Instruction() = default;
  virtual std::string toString() const = 0;           // 生成该指令的 LLVM IR 字符串
  virtual bool isTerminator() const { return false; } // 是否为终结指令 (如 ret, br)
};

using InstPtr = std::unique_ptr<Instruction>;

// 基本块：包含一系列指令
class BasicBlock
{
public:
  explicit BasicBlock(std::string name) : name(std::move(name)) {}
  void addInst(InstPtr inst) { insts.emplace_back(std::move(inst)); }
  void addInstFront(InstPtr inst); // 将指令添加到块的开头 (用于 alloca 优化)
  const std::string &getName() const { return name; }
  std::string toString() const;
  bool isTerminated() const { return !insts.empty() && insts.back()->isTerminator(); }

private:
  std::string name;
  std::vector<InstPtr> insts;
};

using BBPtr = std::unique_ptr<BasicBlock>;

struct FunctionParam
{
  std::string name;
  TypePtr type;
};

// 函数：包含参数和基本块
class Function
{
public:
  Function(std::string name, TypePtr retType, bool isDecl = false)
      : name(std::move(name)), retType(std::move(retType)), isDeclaration(isDecl) {}
  BasicBlock *appendBlock(const std::string &name);
  BasicBlock *getEntryBlock() { return blocks.empty() ? nullptr : blocks.front().get(); }
  void addParam(const std::string &n, TypePtr t) { params.push_back({n, t}); }
  const std::string &getName() const { return name; }
  const std::vector<FunctionParam> &getParams() const { return params; }
  std::string toString() const;
  TypePtr getReturnType() const { return retType; }
  bool isDecl() const { return isDeclaration; }

private:
  std::string name;
  TypePtr retType;
  std::vector<FunctionParam> params;
  std::vector<BBPtr> blocks;
  bool isDeclaration; // 是否仅为声明 (如库函数)
};

using FunctionPtr = std::unique_ptr<Function>;

// 全局变量
class GlobalVariable
{
public:
  GlobalVariable(std::string name, TypePtr ty, std::string init)
      : name(std::move(name)), ty(std::move(ty)), init(std::move(init)) {}
  std::string toString() const;
  const std::string &getName() const { return name; }

private:
  std::string name;
  TypePtr ty;
  std::string init; // 初始化的文本表示 (如 "zeroinitializer" 或 "123")
};

using GlobalPtr = std::unique_ptr<GlobalVariable>;

// 模块：包含全局变量和函数
class Module
{
public:
  explicit Module(std::string name) : name(std::move(name)) {}
  Function *addFunction(std::unique_ptr<Function> f);
  GlobalVariable *addGlobal(std::unique_ptr<GlobalVariable> g);
  std::string toString() const;

private:
  std::string name;
  std::vector<FunctionPtr> functions;
  std::vector<GlobalPtr> globals;
};

// 具体的指令实现类
class AllocaInst : public Instruction
{
public:
  AllocaInst(std::string dest, TypePtr t) : dest(std::move(dest)), ty(std::move(t)) {}
  std::string toString() const override;
  std::string dest;
  TypePtr ty;
};

class StoreInst : public Instruction
{
public:
  StoreInst(std::string val, std::string ptr, TypePtr t) : val(std::move(val)), ptr(std::move(ptr)), ty(std::move(t)) {}
  std::string toString() const override;
  std::string val;
  std::string ptr;
  TypePtr ty;
};

class LoadInst : public Instruction
{
public:
  LoadInst(std::string dest, std::string ptr, TypePtr t) : dest(std::move(dest)), ptr(std::move(ptr)), ty(std::move(t)) {}
  std::string toString() const override;
  std::string dest;
  std::string ptr;
  TypePtr ty;
};

class ReturnInst : public Instruction
{
public:
  ReturnInst(std::string v, TypePtr rt, bool hasVal) : val(std::move(v)), retType(std::move(rt)), hasValue(hasVal) {}
  std::string toString() const override;
  bool isTerminator() const override { return true; }
  std::string val;
  TypePtr retType;
  bool hasValue;
};

class BinaryInst : public Instruction
{
public:
  BinaryInst(std::string dest, std::string op, std::string lhs, std::string rhs)
      : dest(std::move(dest)), op(std::move(op)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  std::string toString() const override;
  std::string dest, op, lhs, rhs;
};

class BrInst : public Instruction
{
public:
  explicit BrInst(std::string target) : target(std::move(target)) {}
  std::string toString() const override;
  bool isTerminator() const override { return true; }

private:
  std::string target;
};

class CondBrInst : public Instruction
{
public:
  CondBrInst(std::string cond, std::string t, std::string f)
      : cond(std::move(cond)), tlabel(std::move(t)), flabel(std::move(f)) {}
  std::string toString() const override;
  bool isTerminator() const override { return true; }

private:
  std::string cond, tlabel, flabel;
};

class ICmpInst : public Instruction
{
public:
  ICmpInst(std::string dest, std::string pred, std::string lhs, std::string rhs)
      : dest(std::move(dest)), pred(std::move(pred)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  std::string toString() const override;

private:
  std::string dest, pred, lhs, rhs;
};

class CallInst : public Instruction
{
public:
  CallInst(std::string dest, std::string callee, TypePtr retTy, std::vector<std::pair<std::string, TypePtr>> args, bool hasResult)
      : dest(std::move(dest)), callee(std::move(callee)), retTy(std::move(retTy)), args(std::move(args)), hasResult(hasResult) {}
  std::string toString() const override;

private:
  std::string dest;
  std::string callee;
  TypePtr retTy;
  std::vector<std::pair<std::string, TypePtr>> args;
  bool hasResult;
};

class GetElementPtrInst : public Instruction
{
public:
  GetElementPtrInst(std::string dest, TypePtr elemTy, std::string base, std::vector<std::string> indices)
      : dest(std::move(dest)), elemTy(std::move(elemTy)), base(std::move(base)), indices(std::move(indices)) {}
  std::string toString() const override;

private:
  std::string dest;
  TypePtr elemTy;
  std::string base;
  std::vector<std::string> indices;
};

class ZextInst : public Instruction
{
public:
  ZextInst(std::string dest, std::string src, TypePtr fromTy, TypePtr toTy)
      : dest(std::move(dest)), src(std::move(src)), fromTy(std::move(fromTy)), toTy(std::move(toTy)) {}
  std::string toString() const override;

private:
  std::string dest, src;
  TypePtr fromTy, toTy;
};
