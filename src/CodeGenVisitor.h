#pragma once

#include "IRBuilder.h"
#include "SymbolTable.h"
#include "Type.h"
#include "antlr4-runtime.h"
#include "SysYParser.h"

#include <memory>
#include <string>


class CodeGenVisitor {
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


  void genCompUnit(SysYParser::CompUnitContext *ctx);
  void genFuncDef(SysYParser::FuncDefContext *ctx);
  void genBlock(SysYParser::BlockContext *ctx, Function *fn);
  void genStmt(SysYParser::StmtContext *ctx, Function *fn);
  void genVarDecl(SysYParser::VarDeclContext *ctx, Function *fn);
  void genGlobalVarDecl(SysYParser::VarDeclContext *ctx);

  // utilities
  std::string getOrCreateVar(const std::string &name, Function *fn);

  std::unique_ptr<Module> module;
  std::unique_ptr<IRBuilder> builder;
  SymbolTable symtab;
};
