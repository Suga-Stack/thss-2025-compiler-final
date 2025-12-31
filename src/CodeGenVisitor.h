#pragma once

#include "IRBuilder.h"
#include "SymbolTable.h"
#include "Type.h"
#include "antlr4-runtime.h"
#include "SysYParser.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CodeGenVisitor
{
public:
  CodeGenVisitor();

  void run(antlr4::ParserRuleContext *root);

  // retrieve generated module
  Module *getModule() { return module.get(); }

private:
  std::string genExp(SysYParser::ExpContext *ctx);
  std::string genAdd(SysYParser::AddExpContext *ctx);
  std::string genMul(SysYParser::MulExpContext *ctx);
  std::string genUnary(SysYParser::UnaryExpContext *ctx);
  std::string genPrimary(SysYParser::PrimaryExpContext *ctx);
  std::string genLVal(SysYParser::LValContext *ctx);
  std::string getLValPtr(SysYParser::LValContext *ctx);

  // 条件与比较
  void genCond(SysYParser::CondContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn);
  void genLOr(SysYParser::LOrExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn);
  void genLAnd(SysYParser::LAndExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn);
  void genEq(SysYParser::EqExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn);
  void genRel(SysYParser::RelExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn);

  // Helpers for value generation of comparison (returning i32 0/1)
  std::string genEqVal(SysYParser::EqExpContext *ctx);
  std::string genRelVal(SysYParser::RelExpContext *ctx);

  // 数组与常量
  int evalConstExp(SysYParser::ConstExpContext *ctx);
  TypePtr buildArrayType(const std::vector<int> &dims);

  // New helpers for structured initialization
  int getStride(const std::vector<int> &dims, int level);
  void processInitVal(SysYParser::InitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result);
  void processConstInitVal(SysYParser::ConstInitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result);
  void processGlobalInitVal(SysYParser::InitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result);
  std::string buildGlobalInitString(const std::vector<int> &dims, int level, const std::vector<std::string> &flat, int &cursor, bool showType = true);

  // Flatten helpers (used by processInitVal/processConstInitVal internally or for simple cases)
  void collectInitVals(SysYParser::InitValContext *ctx, std::vector<std::string> &vals);
  void collectConstInitVals(SysYParser::ConstInitValContext *ctx, std::vector<std::string> &vals);

  void genCompUnit(SysYParser::CompUnitContext *ctx);
  void genFuncDef(SysYParser::FuncDefContext *ctx);
  void genBlock(SysYParser::BlockContext *ctx, Function *fn);
  void genStmt(SysYParser::StmtContext *ctx, Function *fn);
  void genVarDecl(SysYParser::VarDeclContext *ctx, Function *fn);
  void genConstDecl(SysYParser::ConstDeclContext *ctx, Function *fn);
  void genGlobalVarDecl(SysYParser::VarDeclContext *ctx);
  void genGlobalConstDecl(SysYParser::ConstDeclContext *ctx);

  // utilities
  std::string getOrCreateVar(const std::string &name, Function *fn);

  std::unique_ptr<Module> module;
  std::unique_ptr<IRBuilder> builder;
  SymbolTable symtab;

  std::unordered_map<std::string, TypePtr> funcRet;
  std::unordered_map<std::string, TypePtr> tmpValueTypes; // Track types of temporary values
  std::vector<std::string> breakLabels;
  std::vector<std::string> continueLabels;
};
