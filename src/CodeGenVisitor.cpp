#include "CodeGenVisitor.h"
#include "IR.h"
#include <iostream>

using namespace antlr4;

CodeGenVisitor::CodeGenVisitor() {
  module = std::make_unique<Module>("moudle");
  builder = std::make_unique<IRBuilder>(module.get());
}

void CodeGenVisitor::run(ParserRuleContext *root) {
  auto *comp = dynamic_cast<SysYParser::CompUnitContext *>(root);
  if (!comp) return;
  genCompUnit(comp);
}

void CodeGenVisitor::genCompUnit(SysYParser::CompUnitContext *ctx) {
  for (auto decl : ctx->decl()) {
    if (decl->varDecl()) {
      genGlobalVarDecl(decl->varDecl());
    }
  }
  // Then process functions
  for (auto fn : ctx->funcDef()) genFuncDef(fn);
}

void CodeGenVisitor::genFuncDef(SysYParser::FuncDefContext *ctx) {
  TypePtr retTy = (ctx->funcType()->getText() == "void") ? VoidType::get() : IntType::get();
  std::string name = ctx->IDENT()->getText();

  auto *fn = builder->createFunction(name, retTy);

  // params
  if (ctx->funcFParams()) {
    for (auto p : ctx->funcFParams()->funcFParam()) {
      std::string pname = p->IDENT()->getText();
      fn->addParam(pname, IntType::get());
    }
  }
  BasicBlock *entry = builder->createBasicBlock(fn, name + "Entry");
  builder->setInsertPoint(entry);
  symtab.enterScope();

  size_t idx = 0;
  if (ctx->funcFParams()) {
    for (auto p : ctx->funcFParams()->funcFParam()) {
      std::string pname = p->IDENT()->getText();
      std::string allocaPtr = builder->createAlloca(IntType::get(), pname);
  
      std::string paramName = "%" + pname;
      builder->createStore(paramName, allocaPtr, IntType::get());
      symtab.add(pname, SymbolInfo{IntType::get(), allocaPtr});
      ++idx;
    }
  }

  // body
  genBlock(ctx->block(), fn);

  symtab.leaveScope();
}

void CodeGenVisitor::genBlock(SysYParser::BlockContext *ctx, Function *fn) {
  symtab.enterScope();
  for (auto item : ctx->blockItem()) {
    if (auto d = item->decl()) {
      if (d->varDecl()) genVarDecl(d->varDecl(), fn);
    } else if (auto s = item->stmt()) {
      genStmt(s, fn);
    }
  }
  symtab.leaveScope();
}

void CodeGenVisitor::genVarDecl(SysYParser::VarDeclContext *ctx, Function *fn) {
  (void)fn;
  if (!ctx) return;
  for (auto def : ctx->varDef()) {
    std::string name = def->IDENT()->getText();
    std::string ptr = builder->createAlloca(IntType::get(), name);
    symtab.add(name, SymbolInfo{IntType::get(), ptr});

    if (def->initVal()) {
      auto *expCtx = def->initVal()->exp();
      if (expCtx) {
        std::string val = genExp(expCtx);
        builder->createStore(val, ptr, IntType::get());
      }
    }
  }
}

void CodeGenVisitor::genStmt(SysYParser::StmtContext *ctx, Function *fn) {
  if (!ctx) return;
  if (auto as = dynamic_cast<SysYParser::AssignStmtContext *>(ctx)) {
    std::string ptr = getLValPtr(as->lVal());
    std::string val = genExp(as->exp());
    builder->createStore(val, ptr, IntType::get());
  } else if (auto rs = dynamic_cast<SysYParser::ReturnStmtContext *>(ctx)) {
    if (rs->exp()) {
      std::string val = genExp(rs->exp());
      builder->createRet(val, fn->getReturnType(), true);
    } else {
      builder->createRet("", fn->getReturnType(), false);
    }
  } else if (auto es = dynamic_cast<SysYParser::ExpStmtContext *>(ctx)) {
    if (es->exp()) genExp(es->exp());
  } else if (auto bs = dynamic_cast<SysYParser::BlockStmtContext *>(ctx)) {
    genBlock(bs->block(), fn);
  } else {
  }
}

std::string CodeGenVisitor::genExp(SysYParser::ExpContext *ctx) {
  return genAdd(ctx->addExp());
}

std::string CodeGenVisitor::genAdd(SysYParser::AddExpContext *ctx) {
  if (ctx->addExp() && ctx->mulExp()) {
    std::string lhs = genAdd(ctx->addExp());
    std::string rhs = genMul(ctx->mulExp());
    std::string op = ctx->children[1]->getText();
    return builder->createBinary(op, lhs, rhs, "add");
  }
  return genMul(ctx->mulExp());
}

std::string CodeGenVisitor::genMul(SysYParser::MulExpContext *ctx) {
  if (ctx->mulExp() && ctx->unaryExp()) {
    std::string lhs = genMul(ctx->mulExp());
    std::string rhs = genUnary(ctx->unaryExp());
    std::string op = ctx->children[1]->getText();
    return builder->createBinary(op, lhs, rhs, "mul");
  }
  return genUnary(ctx->unaryExp());
}

std::string CodeGenVisitor::genUnary(SysYParser::UnaryExpContext *ctx) {
  if (ctx->primaryExp()) return genPrimary(ctx->primaryExp());
  if (ctx->unaryOp()) {
    std::string op = ctx->unaryOp()->getText();
    std::string v = genUnary(ctx->unaryExp());
    if (op == "-") {
      return builder->createBinary("-", "0", v, "neg");
    } else if (op == "+") {
      return v;
    } else if (op == "!") {
      // naive logical not: icmp eq v, 0 then zext to i32 via select? Here produce icmp result used as i1 string
      return builder->createICmp("eq", v, "0", "not");
    }
  }
  if (ctx->IDENT()) {
  }
  return "0";
}

std::string CodeGenVisitor::genPrimary(SysYParser::PrimaryExpContext *ctx) {
  if (ctx->exp()) return genExp(ctx->exp());
  if (ctx->lVal()) return genLVal(ctx->lVal());
  if (ctx->number()) return ctx->number()->getText();
  return "0";
}

std::string CodeGenVisitor::genLVal(SysYParser::LValContext *ctx) {
  std::string name = ctx->IDENT()->getText();
  auto sym = symtab.lookup(name);
  if (!sym) {
    std::cerr << "Undefined variable: " << name << "\n";
    return "0";
  }
  std::string loaded = builder->createLoad(sym->irName, sym->type, name + "_val");
  return loaded;
}

std::string CodeGenVisitor::getOrCreateVar(const std::string &name, Function *fn) {
  auto sym = symtab.lookup(name);
  if (sym) return sym->irName;
  std::string ptr = builder->createAlloca(IntType::get(), name);
  symtab.add(name, SymbolInfo{IntType::get(), ptr});
  return ptr;
}

void CodeGenVisitor::genGlobalVarDecl(SysYParser::VarDeclContext *ctx) {
  if (!ctx) return;
  for (auto def : ctx->varDef()) {
    std::string name = def->IDENT()->getText();
    std::string initVal = "0";
    
    if (def->initVal() && def->initVal()->exp()) {
      auto *expCtx = def->initVal()->exp();
      if (expCtx->addExp() && expCtx->addExp()->mulExp() && 
          expCtx->addExp()->mulExp()->unaryExp() &&
          expCtx->addExp()->mulExp()->unaryExp()->primaryExp() &&
          expCtx->addExp()->mulExp()->unaryExp()->primaryExp()->number()) {
        initVal = expCtx->addExp()->mulExp()->unaryExp()->primaryExp()->number()->getText();
      }
    }
    
    auto gvar = std::make_unique<GlobalVariable>(name, IntType::get(), initVal);
    module->addGlobal(std::move(gvar));
    symtab.add(name, SymbolInfo{IntType::get(), "@" + name});
  }
}

std::string CodeGenVisitor::getLValPtr(SysYParser::LValContext *ctx) {
  std::string name = ctx->IDENT()->getText();
  auto sym = symtab.lookup(name);
  if (!sym) {
    std::cerr << "Undefined variable: " << name << "\n";
    return "";
  }
  return sym->irName;
}
