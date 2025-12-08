// Minimal IR core declarations
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "Type.h"

// Core Value/Instruction/Module hierarchy (minimal and string-based)
class Instruction {
public:
  virtual ~Instruction() = default;
  virtual std::string toString() const = 0;
};

using InstPtr = std::unique_ptr<Instruction>;

class BasicBlock {
public:
  explicit BasicBlock(std::string name) : name(std::move(name)) {}
  void addInst(InstPtr inst) { insts.emplace_back(std::move(inst)); }
  const std::string &getName() const { return name; }
  std::string toString() const;
private:
  std::string name;
  std::vector<InstPtr> insts;
};

using BBPtr = std::unique_ptr<BasicBlock>;

struct FunctionParam {
  std::string name;
  TypePtr type;
};

class Function {
public:
  Function(std::string name, TypePtr retType) : name(std::move(name)), retType(std::move(retType)) {}
  BasicBlock *appendBlock(const std::string &name);
  void addParam(const std::string &n, TypePtr t) { params.push_back({n, t}); }
  const std::string &getName() const { return name; }
  const std::vector<FunctionParam> &getParams() const { return params; }
  std::string toString() const;
  TypePtr getReturnType() const { return retType; }
private:
  std::string name;
  TypePtr retType;
  std::vector<FunctionParam> params;
  std::vector<BBPtr> blocks;
};

using FunctionPtr = std::unique_ptr<Function>;

class GlobalVariable {
public:
  GlobalVariable(std::string name, TypePtr ty, std::string init)
      : name(std::move(name)), ty(std::move(ty)), init(std::move(init)) {}
  std::string toString() const;
  const std::string &getName() const { return name; }
private:
  std::string name;
  TypePtr ty;
  std::string init; // textual initializer
};

using GlobalPtr = std::unique_ptr<GlobalVariable>;

class Module {
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

// Simple instruction implementations
class AllocaInst : public Instruction {
public:
  AllocaInst(std::string dest, TypePtr t) : dest(std::move(dest)), ty(std::move(t)) {}
  std::string toString() const override;
  std::string dest;
  TypePtr ty;
};

class StoreInst : public Instruction {
public:
  StoreInst(std::string val, std::string ptr, TypePtr t) : val(std::move(val)), ptr(std::move(ptr)), ty(std::move(t)) {}
  std::string toString() const override;
  std::string val; std::string ptr; TypePtr ty;
};

class LoadInst : public Instruction {
public:
  LoadInst(std::string dest, std::string ptr, TypePtr t) : dest(std::move(dest)), ptr(std::move(ptr)), ty(std::move(t)) {}
  std::string toString() const override;
  std::string dest; std::string ptr; TypePtr ty;
};

class ReturnInst : public Instruction {
public:
  ReturnInst(std::string v, TypePtr rt, bool hasVal) : val(std::move(v)), retType(std::move(rt)), hasValue(hasVal) {}
  std::string toString() const override;
  std::string val; TypePtr retType; bool hasValue;
};

class BinaryInst : public Instruction {
public:
  BinaryInst(std::string dest, std::string op, std::string lhs, std::string rhs)
      : dest(std::move(dest)), op(std::move(op)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  std::string toString() const override;
  std::string dest, op, lhs, rhs;
};

class BrInst : public Instruction {
public:
  explicit BrInst(std::string target) : target(std::move(target)) {}
  std::string toString() const override;
private:
  std::string target;
};

class CondBrInst : public Instruction {
public:
  CondBrInst(std::string cond, std::string t, std::string f)
      : cond(std::move(cond)), tlabel(std::move(t)), flabel(std::move(f)) {}
  std::string toString() const override;
private:
  std::string cond, tlabel, flabel;
};

class ICmpInst : public Instruction {
public:
  ICmpInst(std::string dest, std::string pred, std::string lhs, std::string rhs)
      : dest(std::move(dest)), pred(std::move(pred)), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
  std::string toString() const override;
private:
  std::string dest, pred, lhs, rhs;
};

class CallInst : public Instruction {
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

class GetElementPtrInst : public Instruction {
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
