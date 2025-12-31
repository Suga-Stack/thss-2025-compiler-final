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

// 代码生成访问者：遍历 AST 并生成 LLVM IR
class CodeGenVisitor
{
public:
  CodeGenVisitor();

  // 入口点：遍历 AST 生成 IR
  void run(antlr4::ParserRuleContext *root);

  // 获取生成的模块
  Module *getModule() { return module.get(); }

private:
  // 表达式生成辅助函数
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

  // 比较值生成辅助函数 (返回 i32 0/1)
  std::string genEqVal(SysYParser::EqExpContext *ctx);
  std::string genRelVal(SysYParser::RelExpContext *ctx);

  // 数组与常量
  int evalConstExp(SysYParser::ConstExpContext *ctx);
  TypePtr buildArrayType(const std::vector<int> &dims);

  // 结构化初始化辅助函数
  int getStride(const std::vector<int> &dims, int level);
  void processInitVal(SysYParser::InitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result);
  void processConstInitVal(SysYParser::ConstInitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result);
  void processGlobalInitVal(SysYParser::InitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result);
  std::string buildGlobalInitString(const std::vector<int> &dims, int level, const std::vector<std::string> &flat, int &cursor, bool showType = true);

  // 展平辅助函数 (用于内部处理或简单情况)
  void collectInitVals(SysYParser::InitValContext *ctx, std::vector<std::string> &vals);
  void collectConstInitVals(SysYParser::ConstInitValContext *ctx, std::vector<std::string> &vals);

  // 遍历函数
  void genCompUnit(SysYParser::CompUnitContext *ctx);
  void genFuncDef(SysYParser::FuncDefContext *ctx);
  void genBlock(SysYParser::BlockContext *ctx, Function *fn);
  void genStmt(SysYParser::StmtContext *ctx, Function *fn);
  void genVarDecl(SysYParser::VarDeclContext *ctx, Function *fn);
  void genConstDecl(SysYParser::ConstDeclContext *ctx, Function *fn);
  void genGlobalVarDecl(SysYParser::VarDeclContext *ctx);
  void genGlobalConstDecl(SysYParser::ConstDeclContext *ctx);

  // 工具函数
  std::string getOrCreateVar(const std::string &name, Function *fn);

  std::unique_ptr<Module> module;
  std::unique_ptr<IRBuilder> builder;
  SymbolTable symtab;

  std::unordered_map<std::string, TypePtr> funcRet;       // 函数返回类型表
  std::unordered_map<std::string, TypePtr> tmpValueTypes; // 跟踪临时值的类型
  std::vector<std::string> breakLabels;                   // break 跳转目标栈
  std::vector<std::string> continueLabels;                // continue 跳转目标栈
};
