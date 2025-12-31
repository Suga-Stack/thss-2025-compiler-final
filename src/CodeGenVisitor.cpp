#include "CodeGenVisitor.h"
#include "IR.h"
#include <iostream>

using namespace antlr4;

CodeGenVisitor::CodeGenVisitor()
{
  module = std::make_unique<Module>("moudle");
  builder = std::make_unique<IRBuilder>(module.get());

  // Register library functions
  auto voidTy = VoidType::get();
  auto intTy = IntType::get();
  auto intPtrTy = std::make_shared<PointerType>(intTy);

  // int getint()
  auto getint = builder->createFunction("getint", intTy, true);
  funcRet["getint"] = intTy;

  // int getch()
  auto getch = builder->createFunction("getch", intTy, true);
  funcRet["getch"] = intTy;

  // int getarray(int[])
  auto getarray = builder->createFunction("getarray", intTy, true);
  getarray->addParam("a", intPtrTy);
  funcRet["getarray"] = intTy;

  // void putint(int)
  auto putint = builder->createFunction("putint", voidTy, true);
  putint->addParam("a", intTy);
  funcRet["putint"] = voidTy;

  // void putch(int)
  auto putch = builder->createFunction("putch", voidTy, true);
  putch->addParam("a", intTy);
  funcRet["putch"] = voidTy;

  // void putarray(int, int[])
  auto putarray = builder->createFunction("putarray", voidTy, true);
  putarray->addParam("n", intTy);
  putarray->addParam("a", intPtrTy);
  funcRet["putarray"] = voidTy;

  // void _sysy_starttime(int)
  auto starttime = builder->createFunction("_sysy_starttime", voidTy, true);
  starttime->addParam("lineno", intTy);
  funcRet["_sysy_starttime"] = voidTy;

  // void _sysy_stoptime(int)
  auto stoptime = builder->createFunction("_sysy_stoptime", voidTy, true);
  stoptime->addParam("lineno", intTy);
  funcRet["_sysy_stoptime"] = voidTy;
}

void CodeGenVisitor::run(ParserRuleContext *root)
{
  auto *comp = dynamic_cast<SysYParser::CompUnitContext *>(root);
  if (!comp)
    return;
  genCompUnit(comp);
}

namespace
{
  struct ConstEvaluator
  {
    SymbolTable *symtab;
    ConstEvaluator(SymbolTable *st) : symtab(st) {}

    int run(antlr4::tree::ParseTree *node)
    {
      if (auto n = dynamic_cast<SysYParser::NumberContext *>(node))
      {
        return std::stoi(n->getText(), nullptr, 0);
      }
      if (auto p = dynamic_cast<SysYParser::PrimaryExpContext *>(node))
      {
        if (p->number())
          return run(p->number());
        if (p->exp())
          return run(p->exp());
        if (p->lVal())
        {
          std::string name = p->lVal()->IDENT()->getText();
          if (auto sym = symtab->lookup(name))
          {
            if (sym->isConst)
              return sym->constValue;
          }
          return 0;
        }
        return 0;
      }
      if (auto u = dynamic_cast<SysYParser::UnaryExpContext *>(node))
      {
        if (u->primaryExp())
          return run(u->primaryExp());
        if (u->unaryOp())
        {
          int v = run(u->unaryExp());
          if (u->unaryOp()->getText() == "-")
            return -v;
          if (u->unaryOp()->getText() == "!")
            return !v;
          if (u->unaryOp()->getText() == "+")
            return v;
        }
      }
      if (auto m = dynamic_cast<SysYParser::MulExpContext *>(node))
      {
        if (m->mulExp())
        {
          int l = run(m->mulExp());
          int r = run(m->unaryExp());
          std::string op = m->children[1]->getText();
          if (op == "*")
            return l * r;
          if (op == "/")
            return r ? l / r : 0;
          if (op == "%")
            return r ? l % r : 0;
        }
        return run(m->unaryExp());
      }
      if (auto a = dynamic_cast<SysYParser::AddExpContext *>(node))
      {
        if (a->addExp())
        {
          int l = run(a->addExp());
          int r = run(a->mulExp());
          std::string op = a->children[1]->getText();
          if (op == "+")
            return l + r;
          if (op == "-")
            return l - r;
        }
        return run(a->mulExp());
      }
      if (auto e = dynamic_cast<SysYParser::ExpContext *>(node))
        return run(e->addExp());
      if (auto c = dynamic_cast<SysYParser::ConstExpContext *>(node))
        return run(c->addExp());
      if (node->children.size() == 1)
        return run(node->children[0]);
      return 0;
    }
  };
}

void CodeGenVisitor::genCompUnit(SysYParser::CompUnitContext *ctx)
{
  for (auto child : ctx->children)
  {
    if (auto decl = dynamic_cast<SysYParser::DeclContext *>(child))
    {
      if (decl->varDecl())
        genGlobalVarDecl(decl->varDecl());
      else if (decl->constDecl())
        genGlobalConstDecl(decl->constDecl());
    }
    else if (auto func = dynamic_cast<SysYParser::FuncDefContext *>(child))
    {
      genFuncDef(func);
    }
  }
}

void CodeGenVisitor::genFuncDef(SysYParser::FuncDefContext *ctx)
{
  TypePtr retTy = (ctx->funcType()->getText() == "void") ? VoidType::get() : IntType::get();
  std::string name = ctx->IDENT()->getText();
  funcRet[name] = retTy;

  auto *fn = builder->createFunction(name, retTy);

  // params
  if (ctx->funcFParams())
  {
    for (auto p : ctx->funcFParams()->funcFParam())
    {
      std::string pname = p->IDENT()->getText();

      bool isArray = !p->L_BRACKET().empty();
      bool isPtr = (p->MUL() != nullptr);

      TypePtr pty = IntType::get();

      if (isPtr)
      {
        pty = std::make_shared<PointerType>(IntType::get());
      }
      else if (isArray)
      {
        std::vector<int> dims;
        for (auto c : p->constExp())
        {
          dims.push_back(evalConstExp(c));
        }

        bool firstBracketEmpty = false;
        for (size_t i = 0; i < p->children.size(); ++i)
        {
          if (p->children[i]->getText() == "[")
          {
            if (i + 1 < p->children.size() && p->children[i + 1]->getText() == "]")
            {
              firstBracketEmpty = true;
            }
            break;
          }
        }

        int startIdx = firstBracketEmpty ? 0 : 1;
        std::vector<int> elementDims;
        for (size_t i = startIdx; i < dims.size(); ++i)
        {
          elementDims.push_back(dims[i]);
        }

        TypePtr elemTy = IntType::get();
        if (!elementDims.empty())
        {
          elemTy = buildArrayType(elementDims);
        }
        pty = std::make_shared<PointerType>(elemTy);
      }

      fn->addParam(pname, pty);
    }
  }
  BasicBlock *entry = builder->createBasicBlock(fn, name + "Entry");
  builder->setInsertPoint(entry);
  symtab.enterScope();

  // Create allocas for parameters to support mutable parameters (SysY semantics)
  for (const auto &param : fn->getParams())
  {
    std::string pname = param.name;
    TypePtr pty = param.type;
    std::string allocaPtr = builder->createAlloca(pty, pname);
    std::string paramName = "%" + pname;
    builder->createStore(paramName, allocaPtr, pty);
    symtab.add(pname, SymbolInfo{pty, allocaPtr});
  }

  // body
  genBlock(ctx->block(), fn);

  if (!builder->isTerminated())
  {
    if (retTy->getID() == Type::VoidTy)
      builder->createRet("", retTy, false);
    else
      builder->createRet("0", retTy, true);
  }

  symtab.leaveScope();
}

void CodeGenVisitor::genBlock(SysYParser::BlockContext *ctx, Function *fn)
{
  symtab.enterScope();
  for (auto item : ctx->blockItem())
  {
    if (auto d = item->decl())
    {
      if (d->varDecl())
        genVarDecl(d->varDecl(), fn);
      else if (d->constDecl())
        genConstDecl(d->constDecl(), fn);
    }
    else if (auto s = item->stmt())
    {
      genStmt(s, fn);
    }
  }
  symtab.leaveScope();
}

void CodeGenVisitor::genVarDecl(SysYParser::VarDeclContext *ctx, Function *fn)
{
  (void)fn;
  if (!ctx)
    return;
  for (auto def : ctx->varDef())
  {
    std::string name = def->IDENT()->getText();
    std::vector<int> dims;
    for (auto c : def->constExp())
      dims.push_back(evalConstExp(c));
    TypePtr ty = dims.empty() ? IntType::get() : buildArrayType(dims);
    std::string ptr = builder->createAlloca(ty, name);
    symtab.add(name, SymbolInfo{ty, ptr});
    if (def->initVal())
    {
      if (dims.empty())
      {
        if (def->initVal()->exp())
        {
          std::string val = genExp(def->initVal()->exp());
          builder->createStore(val, ptr, ty);
        }
      }
      else
      {
        int total = 1;
        for (int d : dims)
          total *= d;
        std::vector<std::string> flat(total, "0");
        int cursor = 0;

        processInitVal(def->initVal(), dims, 0, cursor, flat);

        for (int idx = 0; idx < total; ++idx)
        {
          // Optimization: skip zero stores if we assume alloca is uninitialized (SysY doesn't guarantee zero init for locals)
          // But for correctness with partial init, we must store 0s if explicit init was provided.
          // Since we filled 'flat' with "0", we store everything.
          int rem = idx;
          std::vector<std::string> gepIdx = {"0"};
          int tempTotal = total;
          for (int d : dims)
          {
            int cur = rem / (tempTotal / d);
            rem = rem % (tempTotal / d);
            tempTotal /= d;
            gepIdx.push_back(std::to_string(cur));
          }
          std::string elemPtr = builder->createGEP(ty, ptr, gepIdx, name + "_init");
          builder->createStore(flat[idx], elemPtr, IntType::get());
        }
      }
    }
  }
}

void CodeGenVisitor::genConstDecl(SysYParser::ConstDeclContext *ctx, Function *fn)
{
  (void)fn;
  if (!ctx)
    return;
  for (auto def : ctx->constDef())
  {
    std::string name = def->IDENT()->getText();
    std::vector<int> dims;
    for (auto c : def->constExp())
      dims.push_back(evalConstExp(c));
    TypePtr ty = dims.empty() ? IntType::get() : buildArrayType(dims);
    std::string ptr = builder->createAlloca(ty, name);

    if (dims.empty())
    {
      int val = 0;
      if (def->constInitVal()->constExp())
      {
        val = evalConstExp(def->constInitVal()->constExp());
        builder->createStore(std::to_string(val), ptr, ty);
      }
      symtab.add(name, SymbolInfo{ty, ptr, true, val});
    }
    else
    {
      symtab.add(name, SymbolInfo{ty, ptr, true, 0});

      int total = 1;
      for (int d : dims)
        total *= d;
      std::vector<std::string> flat(total, "0");
      int cursor = 0;

      processConstInitVal(def->constInitVal(), dims, 0, cursor, flat);

      for (int idx = 0; idx < total; ++idx)
      {
        int rem = idx;
        std::vector<std::string> gepIdx = {"0"};
        int tempTotal = total;
        for (int d : dims)
        {
          int cur = rem / (tempTotal / d);
          rem = rem % (tempTotal / d);
          tempTotal /= d;
          gepIdx.push_back(std::to_string(cur));
        }
        std::string elemPtr = builder->createGEP(ty, ptr, gepIdx, name + "_init");
        builder->createStore(flat[idx], elemPtr, IntType::get());
      }
    }
  }
}

void CodeGenVisitor::genStmt(SysYParser::StmtContext *ctx, Function *fn)
{
  if (!ctx)
    return;
  if (auto as = dynamic_cast<SysYParser::AssignStmtContext *>(ctx))
  {
    std::string ptr = getLValPtr(as->lVal());
    std::string val = genExp(as->exp());
    builder->createStore(val, ptr, IntType::get());
  }
  else if (auto rs = dynamic_cast<SysYParser::ReturnStmtContext *>(ctx))
  {
    if (rs->exp())
    {
      std::string val = genExp(rs->exp());
      builder->createRet(val, fn->getReturnType(), true);
    }
    else
    {
      builder->createRet("", fn->getReturnType(), false);
    }
  }
  else if (auto es = dynamic_cast<SysYParser::ExpStmtContext *>(ctx))
  {
    if (es->exp())
      genExp(es->exp());
  }
  else if (auto bs = dynamic_cast<SysYParser::BlockStmtContext *>(ctx))
  {
    genBlock(bs->block(), fn);
  }
  else if (auto ifs = dynamic_cast<SysYParser::IfStmtContext *>(ctx))
  {
    BasicBlock *thenBB = builder->createBasicBlock(fn, "if.then");
    BasicBlock *elseBB = builder->createBasicBlock(fn, "if.else");
    BasicBlock *endBB = builder->createBasicBlock(fn, "if.end");
    genCond(ifs->cond(), thenBB, elseBB, fn);

    builder->setInsertPoint(thenBB);
    genStmt(ifs->stmt(0), fn);
    if (!builder->isTerminated())
      builder->createBr(endBB->getName());

    builder->setInsertPoint(elseBB);
    if (ifs->stmt().size() > 1)
      genStmt(ifs->stmt(1), fn);
    if (!builder->isTerminated())
      builder->createBr(endBB->getName());

    builder->setInsertPoint(endBB);
  }
  else if (auto ws = dynamic_cast<SysYParser::WhileStmtContext *>(ctx))
  {
    BasicBlock *condBB = builder->createBasicBlock(fn, "while.cond");
    BasicBlock *bodyBB = builder->createBasicBlock(fn, "while.body");
    BasicBlock *endBB = builder->createBasicBlock(fn, "while.end");

    if (!builder->isTerminated())
      builder->createBr(condBB->getName());

    builder->setInsertPoint(condBB);
    genCond(ws->cond(), bodyBB, endBB, fn);

    builder->setInsertPoint(bodyBB);
    breakLabels.push_back(endBB->getName());
    continueLabels.push_back(condBB->getName());
    genStmt(ws->stmt(), fn);
    if (!builder->isTerminated())
      builder->createBr(condBB->getName());

    breakLabels.pop_back();
    continueLabels.pop_back();

    builder->setInsertPoint(endBB);
  }
  else if (dynamic_cast<SysYParser::BreakStmtContext *>(ctx))
  {
    if (!breakLabels.empty())
      builder->createBr(breakLabels.back());
  }
  else if (dynamic_cast<SysYParser::ContinueStmtContext *>(ctx))
  {
    if (!continueLabels.empty())
      builder->createBr(continueLabels.back());
  }
  else
  {
  }
}

std::string CodeGenVisitor::genExp(SysYParser::ExpContext *ctx)
{
  return genAdd(ctx->addExp());
}

std::string CodeGenVisitor::genAdd(SysYParser::AddExpContext *ctx)
{
  if (ctx->addExp() && ctx->mulExp())
  {
    std::string lhs = genAdd(ctx->addExp());
    std::string rhs = genMul(ctx->mulExp());
    std::string op = ctx->children[1]->getText();
    std::string res = builder->createBinary(op, lhs, rhs, "add");
    tmpValueTypes[res] = IntType::get();
    return res;
  }
  return genMul(ctx->mulExp());
}

std::string CodeGenVisitor::genMul(SysYParser::MulExpContext *ctx)
{
  if (ctx->mulExp() && ctx->unaryExp())
  {
    std::string lhs = genMul(ctx->mulExp());
    std::string rhs = genUnary(ctx->unaryExp());
    std::string op = ctx->children[1]->getText();
    std::string res = builder->createBinary(op, lhs, rhs, "mul");
    tmpValueTypes[res] = IntType::get();
    return res;
  }
  return genUnary(ctx->unaryExp());
}

std::string CodeGenVisitor::genUnary(SysYParser::UnaryExpContext *ctx)
{
  if (ctx->primaryExp())
    return genPrimary(ctx->primaryExp());
  if (ctx->IDENT())
  {
    std::string callee = ctx->IDENT()->getText();
    std::vector<std::pair<std::string, TypePtr>> args;
    if (ctx->funcRParams())
    {
      for (auto e : ctx->funcRParams()->exp())
      {
        std::string v = genExp(e);
        TypePtr argTy = IntType::get();
        if (tmpValueTypes.count(v))
          argTy = tmpValueTypes[v];
        args.push_back({v, argTy});
      }
    }
    TypePtr ret = funcRet.count(callee) ? funcRet[callee] : IntType::get();
    std::string res = builder->createCall(callee, args, ret, callee + "_call");
    if (ret->getID() != Type::VoidTy)
      tmpValueTypes[res] = ret;
    return res;
  }
  if (ctx->unaryOp())
  {
    std::string op = ctx->unaryOp()->getText();
    std::string v = genUnary(ctx->unaryExp());
    if (op == "-")
    {
      std::string res = builder->createBinary("-", "0", v, "neg");
      tmpValueTypes[res] = IntType::get();
      return res;
    }
    else if (op == "+")
    {
      return v;
    }
    else if (op == "!")
    {
      // Logical NOT: (v == 0) ? 1 : 0
      // 1. Compare v with 0. Result is i1.
      std::string cmp = builder->createICmp("eq", v, "0", "not");
      // 2. Zero-extend i1 result to i32.
      std::string res = builder->createZExt(cmp, BoolType::get(), IntType::get(), "not_ext");
      tmpValueTypes[res] = IntType::get();
      return res;
    }
  }
  if (ctx->IDENT())
  {
  }
  return "0";
}

std::string CodeGenVisitor::genPrimary(SysYParser::PrimaryExpContext *ctx)
{
  if (ctx->exp())
    return genExp(ctx->exp());
  if (ctx->lVal())
    return genLVal(ctx->lVal());
  if (ctx->number())
    return std::to_string(std::stoi(ctx->number()->getText(), nullptr, 0));
  return "0";
}

std::string CodeGenVisitor::genLVal(SysYParser::LValContext *ctx)
{
  std::string name = ctx->IDENT()->getText();
  auto sym = symtab.lookup(name);
  if (!sym)
  {
    std::cerr << "Undefined variable: " << name << "\n";
    return "0";
  }

  bool isArray = (sym->type->getID() == Type::ArrayTy);
  bool isPtr = (sym->type->getID() == Type::PointerTy);

  if (isArray || isPtr)
  {
    int dims = 0;
    TypePtr t = sym->type;
    if (isPtr)
      t = std::static_pointer_cast<PointerType>(t)->getPointee();
    while (t->getID() == Type::ArrayTy)
    {
      dims++;
      t = std::static_pointer_cast<ArrayType>(t)->getElementType();
    }

    // Array decay or partial access
    if (ctx->exp().size() < (size_t)(dims + (isPtr ? 1 : 0)))
    {
      if (isPtr && ctx->exp().empty())
      {
        std::string val = builder->createLoad(sym->irName, sym->type, name + "_ptr");
        // FIX: The loaded value is the pointer itself (e.g. i32*), so its type is sym->type.
        tmpValueTypes[val] = sym->type;
        return val;
      }

      std::string ptr = getLValPtr(ctx);

      // Determine the type pointed to by ptr
      TypePtr currentTy = sym->type;
      if (isPtr)
        currentTy = std::static_pointer_cast<PointerType>(currentTy)->getPointee();
      for (size_t i = 0; i < ctx->exp().size(); ++i)
      {
        if (currentTy->getID() == Type::ArrayTy)
        {
          currentTy = std::static_pointer_cast<ArrayType>(currentTy)->getElementType();
        }
      }

      if (currentTy->getID() == Type::ArrayTy)
      {
        // Decay to pointer to first element
        std::string decayed = builder->createGEP(currentTy, ptr, {"0", "0"}, name + "_decay");
        TypePtr elemTy = std::static_pointer_cast<ArrayType>(currentTy)->getElementType();
        tmpValueTypes[decayed] = std::make_shared<PointerType>(elemTy);
        return decayed;
      }

      // Already a pointer to the element (e.g. int* for int a[])
      tmpValueTypes[ptr] = std::make_shared<PointerType>(currentTy);
      return ptr;
    }
  }

  std::string ptr = getLValPtr(ctx);
  std::string res = builder->createLoad(ptr, IntType::get(), name + "_val");
  tmpValueTypes[res] = IntType::get();
  return res;
}

std::string CodeGenVisitor::getOrCreateVar(const std::string &name, Function *fn)
{
  auto sym = symtab.lookup(name);
  if (sym)
    return sym->irName;
  std::string ptr = builder->createAlloca(IntType::get(), name);
  symtab.add(name, SymbolInfo{IntType::get(), ptr});
  return ptr;
}

void CodeGenVisitor::genGlobalVarDecl(SysYParser::VarDeclContext *ctx)
{
  if (!ctx)
    return;
  for (auto def : ctx->varDef())
  {
    std::string name = def->IDENT()->getText();
    std::vector<int> dims;
    for (auto c : def->constExp())
      dims.push_back(evalConstExp(c));

    TypePtr ty = dims.empty() ? IntType::get() : buildArrayType(dims);

    std::string initStr = "zeroinitializer";
    if (dims.empty())
    {
      initStr = "0";
      if (def->initVal() && def->initVal()->exp())
      {
        ConstEvaluator eval(&symtab);
        int val = eval.run(def->initVal()->exp());
        initStr = std::to_string(val);
      }
    }
    else if (def->initVal())
    {
      int total = 1;
      for (int d : dims)
        total *= d;
      std::vector<std::string> flat(total, "0");
      int cursor = 0;
      processGlobalInitVal(def->initVal(), dims, 0, cursor, flat);

      int buildCursor = 0;
      initStr = buildGlobalInitString(dims, 0, flat, buildCursor, false);
    }

    auto gvar = std::make_unique<GlobalVariable>(name, ty, initStr);
    module->addGlobal(std::move(gvar));
    symtab.add(name, SymbolInfo{ty, "@" + name});
  }
}

void CodeGenVisitor::genGlobalConstDecl(SysYParser::ConstDeclContext *ctx)
{
  if (!ctx)
    return;
  for (auto def : ctx->constDef())
  {
    std::string name = def->IDENT()->getText();
    std::vector<int> dims;
    for (auto c : def->constExp())
      dims.push_back(evalConstExp(c));

    TypePtr ty = dims.empty() ? IntType::get() : buildArrayType(dims);

    std::string initStr = "zeroinitializer";
    int val = 0;
    if (dims.empty())
    {
      if (def->constInitVal()->constExp())
      {
        val = evalConstExp(def->constInitVal()->constExp());
        initStr = std::to_string(val);
      }
    }
    else
    {
      int total = 1;
      for (int d : dims)
        total *= d;
      std::vector<std::string> flat(total, "0");
      int cursor = 0;
      processConstInitVal(def->constInitVal(), dims, 0, cursor, flat);

      int buildCursor = 0;
      initStr = buildGlobalInitString(dims, 0, flat, buildCursor, false);
    }

    auto gvar = std::make_unique<GlobalVariable>(name, ty, initStr);
    module->addGlobal(std::move(gvar));
    symtab.add(name, SymbolInfo{ty, "@" + name, true, val});
  }
}

std::string CodeGenVisitor::getLValPtr(SysYParser::LValContext *ctx)
{
  std::string name = ctx->IDENT()->getText();
  auto sym = symtab.lookup(name);
  if (!sym)
  {
    std::cerr << "Undefined variable: " << name << "\n";
    return "";
  }
  std::string base = sym->irName;
  std::vector<std::string> indices;
  for (auto e : ctx->exp())
    indices.push_back(genExp(e));

  if (sym->type->getID() == Type::PointerTy)
  {
    // For pointer types (e.g. array parameters), the symbol stores the address of the pointer variable (i32**).
    // We must load the pointer value (i32*) first.
    std::string ptr = builder->createLoad(base, sym->type, name + "_ptr");
    // FIX: The loaded value is the pointer itself.
    tmpValueTypes[ptr] = sym->type;

    if (indices.empty())
      return base;

    return builder->createGEP(std::static_pointer_cast<PointerType>(sym->type)->getPointee(), ptr, indices, name + "_idx");
  }

  TypePtr arrTy = sym->type;
  std::vector<std::string> gepIdx = {"0"};
  gepIdx.insert(gepIdx.end(), indices.begin(), indices.end());
  return builder->createGEP(arrTy, base, gepIdx, name + "_idx");
}

// 条件与比较
void CodeGenVisitor::genCond(SysYParser::CondContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn)
{
  genLOr(ctx->lOrExp(), tBB, fBB, fn);
}

void CodeGenVisitor::genLOr(SysYParser::LOrExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn)
{
  if (ctx->lOrExp() && ctx->lAndExp())
  {
    BasicBlock *rhs = builder->createBasicBlock(fn, "lor.rhs");
    genLOr(ctx->lOrExp(), tBB, rhs, fn);
    builder->setInsertPoint(rhs);
    genLAnd(ctx->lAndExp(), tBB, fBB, fn);
  }
  else
  {
    genLAnd(ctx->lAndExp(), tBB, fBB, fn);
  }
}

void CodeGenVisitor::genLAnd(SysYParser::LAndExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn)
{
  if (ctx->lAndExp() && ctx->eqExp())
  {
    BasicBlock *rhs = builder->createBasicBlock(fn, "land.rhs");
    genLAnd(ctx->lAndExp(), rhs, fBB, fn);
    builder->setInsertPoint(rhs);
    genEq(ctx->eqExp(), tBB, fBB, fn);
  }
  else
  {
    genEq(ctx->eqExp(), tBB, fBB, fn);
  }
}

void CodeGenVisitor::genEq(SysYParser::EqExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn)
{
  if (ctx->eqExp() && ctx->relExp())
  {
    // For EqExp, we need values, not control flow branching yet unless it's the top level condition.
    // However, the current structure passes tBB/fBB down.
    // The issue described is about precedence and value calculation like (1 < 8) != (7 % 2).
    // This requires evaluating sub-expressions to values (0/1) instead of branching.

    // We need a helper to evaluate an expression to an i32 value (0 or 1 for boolean results).
    // But genAdd/genMul/genUnary return string names of i32 values.
    // genRel/genEq currently take BBs, implying they generate branches.
    // This design is suitable for control flow (short-circuit), but not for value calculation in conditions like (a<b) == (c<d).

    // Refactoring strategy:
    // 1. If we are in a condition context (tBB/fBB not null), we might still need values for sub-expressions.
    // 2. SysY/C defines comparison operators as returning int 0 or 1.
    // 3. We should change genRel/genEq to return a string (value name) representing the result i32.
    // 4. The top-level genCond will then take that i32 result and generate the final branch.

    // Wait, genCond calls genLOr -> genLAnd -> genEq -> genRel.
    // Short-circuiting is only for && and ||.
    // ==, !=, <, >, etc. are standard binary operators returning 0/1.

    // Let's implement genEq/genRel to return std::string (i32 value) and NOT take BBs.
    // I need to update the header file as well? The user provided CodeGenVisitor.h in previous turn.
    // I will assume I can modify .h in the next step or I will provide it if I can.
    // Wait, the prompt says "Always make changes to these files".
    // I will modify CodeGenVisitor.cpp to implement `genRelVal` and `genEqVal` helpers,
    // and make `genRel` / `genEq` (the void ones) use them to get a value and then branch.

    // But wait, `genEq` is recursive: `eqExp (EQ | NEQ) relExp`.
    // If we have `a == b == c`, it parses as `(a == b) == c`.
    // `a == b` must return 0/1.

    std::string lhs = genEqVal(ctx);
    std::string cmp = builder->createICmp("ne", lhs, "0", "cond");
    builder->createCondBr(cmp, tBB->getName(), fBB->getName());
  }
  else
  {
    genRel(ctx->relExp(), tBB, fBB, fn);
  }
}

void CodeGenVisitor::genRel(SysYParser::RelExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn)
{
  if (ctx->relExp() && ctx->addExp())
  {
    std::string lhs = genRelVal(ctx);
    std::string cmp = builder->createICmp("ne", lhs, "0", "cond");
    builder->createCondBr(cmp, tBB->getName(), fBB->getName());
  }
  else
  {
    // Base case: RelExp -> AddExp
    // Just evaluate AddExp. If it's != 0, jump to true, else false.
    std::string val = genAdd(ctx->addExp());
    std::string cmp = builder->createICmp("ne", val, "0", "cond");
    builder->createCondBr(cmp, tBB->getName(), fBB->getName());
  }
}

// Helpers to generate value (0/1) for Eq/Rel expressions
std::string CodeGenVisitor::genEqVal(SysYParser::EqExpContext *ctx)
{
  if (ctx->eqExp() && ctx->relExp())
  {
    std::string lhs = genEqVal(ctx->eqExp());
    std::string rhs = genRelVal(ctx->relExp());
    std::string op = ctx->children[1]->getText();
    std::string pred = (op == "==" ? "eq" : "ne");
    std::string cmp = builder->createICmp(pred, lhs, rhs, "eq");
    return builder->createZExt(cmp, BoolType::get(), IntType::get(), "eq_ext");
  }
  return genRelVal(ctx->relExp());
}

std::string CodeGenVisitor::genRelVal(SysYParser::RelExpContext *ctx)
{
  if (ctx->relExp() && ctx->addExp())
  {
    std::string lhs = genRelVal(ctx->relExp());
    std::string rhs = genAdd(ctx->addExp());
    std::string op = ctx->children[1]->getText();
    std::string pred;
    if (op == "<")
      pred = "slt";
    else if (op == ">")
      pred = "sgt";
    else if (op == "<=")
      pred = "sle";
    else if (op == ">=")
      pred = "sge";

    std::string cmp = builder->createICmp(pred, lhs, rhs, "rel");
    return builder->createZExt(cmp, BoolType::get(), IntType::get(), "rel_ext");
  }
  return genAdd(ctx->addExp());
}

// 常量与数组
int CodeGenVisitor::evalConstExp(SysYParser::ConstExpContext *ctx)
{
  if (!ctx || !ctx->addExp())
    return 0;
  ConstEvaluator evaluator(&symtab);
  return evaluator.run(ctx);
}

TypePtr CodeGenVisitor::buildArrayType(const std::vector<int> &dims)
{
  TypePtr cur = IntType::get();
  for (auto it = dims.rbegin(); it != dims.rend(); ++it)
  {
    cur = std::make_shared<ArrayType>(cur, *it);
  }
  return cur;
}

void CodeGenVisitor::collectInitVals(SysYParser::InitValContext *ctx, std::vector<std::string> &vals)
{
  if (!ctx)
    return;
  if (ctx->exp())
  {
    vals.push_back(genExp(ctx->exp()));
    return;
  }
  for (auto child : ctx->initVal())
  {
    collectInitVals(child, vals);
  }
}

void CodeGenVisitor::collectConstInitVals(SysYParser::ConstInitValContext *ctx, std::vector<std::string> &vals)
{
  if (!ctx)
    return;
  if (ctx->constExp())
  {
    vals.push_back(std::to_string(evalConstExp(ctx->constExp())));
    return;
  }
  for (auto child : ctx->constInitVal())
  {
    collectConstInitVals(child, vals);
  }
}

int CodeGenVisitor::getStride(const std::vector<int> &dims, int level)
{
  if (level >= dims.size())
    return 1;
  int p = 1;
  for (size_t i = level + 1; i < dims.size(); ++i)
  {
    p *= dims[i];
  }
  return p;
}

void CodeGenVisitor::processInitVal(SysYParser::InitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result)
{
  if (ctx->exp())
  {
    if (cursor < (int)result.size())
    {
      result[cursor] = genExp(ctx->exp());
      cursor++;
    }
    return;
  }

  int step = getStride(dims, level);
  int start = cursor;

  for (auto child : ctx->initVal())
  {
    if (child->exp())
    {
      processInitVal(child, dims, level, cursor, result);
    }
    else
    {
      // Align to next element boundary
      int current_offset = cursor - start;
      int rem = current_offset % step;
      if (rem != 0)
      {
        cursor += (step - rem);
      }

      int next_boundary = cursor + step;
      if (cursor >= (int)result.size())
        break;

      processInitVal(child, dims, level + 1, cursor, result);

      // Ensure we finished the element (pad with zeros if needed)
      if (cursor < next_boundary)
      {
        cursor = next_boundary;
      }
    }
  }
}

void CodeGenVisitor::processConstInitVal(SysYParser::ConstInitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result)
{
  if (ctx->constExp())
  {
    if (cursor < (int)result.size())
    {
      result[cursor] = std::to_string(evalConstExp(ctx->constExp()));
      cursor++;
    }
    return;
  }

  int step = getStride(dims, level);
  int start = cursor;

  for (auto child : ctx->constInitVal())
  {
    if (child->constExp())
    {
      processConstInitVal(child, dims, level, cursor, result);
    }
    else
    {
      int current_offset = cursor - start;
      int rem = current_offset % step;
      if (rem != 0)
      {
        cursor += (step - rem);
      }

      int next_boundary = cursor + step;
      if (cursor >= (int)result.size())
        break;

      processConstInitVal(child, dims, level + 1, cursor, result);

      if (cursor < next_boundary)
      {
        cursor = next_boundary;
      }
    }
  }
}

void CodeGenVisitor::processGlobalInitVal(SysYParser::InitValContext *ctx, const std::vector<int> &dims, int level, int &cursor, std::vector<std::string> &result)
{
  if (ctx->exp())
  {
    if (cursor < (int)result.size())
    {
      ConstEvaluator eval(&symtab);
      result[cursor] = std::to_string(eval.run(ctx->exp()));
      cursor++;
    }
    return;
  }

  int step = getStride(dims, level);
  int start = cursor;

  for (auto child : ctx->initVal())
  {
    if (child->exp())
    {
      processGlobalInitVal(child, dims, level, cursor, result);
    }
    else
    {
      int current_offset = cursor - start;
      int rem = current_offset % step;
      if (rem != 0)
      {
        cursor += (step - rem);
      }

      int next_boundary = cursor + step;
      if (cursor >= (int)result.size())
        break;

      processGlobalInitVal(child, dims, level + 1, cursor, result);

      if (cursor < next_boundary)
      {
        cursor = next_boundary;
      }
    }
  }
}

std::string CodeGenVisitor::buildGlobalInitString(const std::vector<int> &dims, int level, const std::vector<std::string> &flat, int &cursor, bool showType)
{
  if (level == (int)dims.size())
  {
    if (cursor >= (int)flat.size())
      return "i32 0";
    return "i32 " + flat[cursor++];
  }

  std::string res = "";
  if (showType)
  {
    TypePtr currentType = IntType::get();
    for (int i = (int)dims.size() - 1; i >= level; --i)
    {
      currentType = std::make_shared<ArrayType>(currentType, dims[i]);
    }
    res += currentType->toString() + " ";
  }

  res += "[";

  int count = dims[level];
  for (int i = 0; i < count; ++i)
  {
    if (i > 0)
      res += ", ";
    res += buildGlobalInitString(dims, level + 1, flat, cursor, true);
  }
  res += "]";
  return res;
}
