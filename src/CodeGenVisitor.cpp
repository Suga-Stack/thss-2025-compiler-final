#include "CodeGenVisitor.h"
#include "IR.h"
#include <iostream>

using namespace antlr4;

// 构造函数：初始化模块、构建器和内置库函数
CodeGenVisitor::CodeGenVisitor()
{
  module = std::make_unique<Module>("moudle");
  builder = std::make_unique<IRBuilder>(module.get());

  // 注册 SysY 运行时库函数
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

// 入口点：开始遍历 AST
void CodeGenVisitor::run(ParserRuleContext *root)
{
  auto *comp = dynamic_cast<SysYParser::CompUnitContext *>(root);
  if (!comp)
    return;
  genCompUnit(comp);
}

namespace
{
  // 常量求值器：用于在编译期计算常量表达式的值
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
          // 查找常量符号的值
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
      // 处理一元运算
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
      // 处理乘除模
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
      // 处理加减
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

// 生成编译单元 (全局变量声明和函数定义)
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

// 生成函数定义
void CodeGenVisitor::genFuncDef(SysYParser::FuncDefContext *ctx)
{
  TypePtr retTy = (ctx->funcType()->getText() == "void") ? VoidType::get() : IntType::get();
  std::string name = ctx->IDENT()->getText();
  funcRet[name] = retTy;

  auto *fn = builder->createFunction(name, retTy);

  // 处理函数参数
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
        // 处理数组参数，解析维度
        std::vector<int> dims;
        for (auto c : p->constExp())
        {
          dims.push_back(evalConstExp(c));
        }

        // 检查第一维是否为空 (例如 int a[][3])
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
        // 数组参数退化为指针
        pty = std::make_shared<PointerType>(elemTy);
      }

      fn->addParam(pname, pty);
    }
  }
  BasicBlock *entry = builder->createBasicBlock(fn, name + "Entry");
  builder->setInsertPoint(entry);
  symtab.enterScope();

  // 为参数创建 alloca 并存储初始值 (支持参数在函数体内被修改)
  for (const auto &param : fn->getParams())
  {
    std::string pname = param.name;
    TypePtr pty = param.type;
    std::string allocaPtr = builder->createAlloca(pty, pname);
    std::string paramName = "%" + pname;
    builder->createStore(paramName, allocaPtr, pty);
    symtab.add(pname, SymbolInfo{pty, allocaPtr});
  }

  // 生成函数体
  genBlock(ctx->block(), fn);

  // 确保函数有返回指令 (处理 void 函数末尾没有 return 的情况)
  if (!builder->isTerminated())
  {
    if (retTy->getID() == Type::VoidTy)
      builder->createRet("", retTy, false);
    else
      builder->createRet("0", retTy, true);
  }

  symtab.leaveScope();
}

// 生成代码块
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

// 生成局部变量声明
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

    // 处理初始化
    if (def->initVal())
    {
      if (dims.empty())
      {
        // 标量初始化
        if (def->initVal()->exp())
        {
          std::string val = genExp(def->initVal()->exp());
          builder->createStore(val, ptr, ty);
        }
      }
      else
      {
        // 数组初始化：展平并逐个元素存储
        int total = 1;
        for (int d : dims)
          total *= d;
        std::vector<std::string> flat(total, "0");
        int cursor = 0;

        processInitVal(def->initVal(), dims, 0, cursor, flat);

        for (int idx = 0; idx < total; ++idx)
        {
          // 计算多维数组的 GEP 索引
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

// 生成局部常量声明
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
      // 记录常量值到符号表，以便后续常量折叠
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

// 生成语句
void CodeGenVisitor::genStmt(SysYParser::StmtContext *ctx, Function *fn)
{
  if (!ctx)
    return;
  // 赋值语句
  if (auto as = dynamic_cast<SysYParser::AssignStmtContext *>(ctx))
  {
    std::string ptr = getLValPtr(as->lVal());
    std::string val = genExp(as->exp());
    builder->createStore(val, ptr, IntType::get());
  }
  // 返回语句
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
  // 表达式语句
  else if (auto es = dynamic_cast<SysYParser::ExpStmtContext *>(ctx))
  {
    if (es->exp())
      genExp(es->exp());
  }
  // 块语句
  else if (auto bs = dynamic_cast<SysYParser::BlockStmtContext *>(ctx))
  {
    genBlock(bs->block(), fn);
  }
  // If 语句
  else if (auto ifs = dynamic_cast<SysYParser::IfStmtContext *>(ctx))
  {
    BasicBlock *thenBB = builder->createBasicBlock(fn, "if.then");
    BasicBlock *elseBB = builder->createBasicBlock(fn, "if.else");
    BasicBlock *endBB = builder->createBasicBlock(fn, "if.end");
    // 生成条件跳转
    genCond(ifs->cond(), thenBB, elseBB, fn);

    // 生成 Then 块
    builder->setInsertPoint(thenBB);
    genStmt(ifs->stmt(0), fn);
    if (!builder->isTerminated())
      builder->createBr(endBB->getName());

    // 生成 Else 块
    builder->setInsertPoint(elseBB);
    if (ifs->stmt().size() > 1)
      genStmt(ifs->stmt(1), fn);
    if (!builder->isTerminated())
      builder->createBr(endBB->getName());

    builder->setInsertPoint(endBB);
  }
  // While 语句
  else if (auto ws = dynamic_cast<SysYParser::WhileStmtContext *>(ctx))
  {
    BasicBlock *condBB = builder->createBasicBlock(fn, "while.cond");
    BasicBlock *bodyBB = builder->createBasicBlock(fn, "while.body");
    BasicBlock *endBB = builder->createBasicBlock(fn, "while.end");

    if (!builder->isTerminated())
      builder->createBr(condBB->getName());

    // 生成条件判断
    builder->setInsertPoint(condBB);
    genCond(ws->cond(), bodyBB, endBB, fn);

    // 生成循环体
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
  // Break 语句
  else if (dynamic_cast<SysYParser::BreakStmtContext *>(ctx))
  {
    if (!breakLabels.empty())
      builder->createBr(breakLabels.back());
  }
  // Continue 语句
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

// 生成加减表达式
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

// 生成乘除模表达式
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

// 生成一元表达式
std::string CodeGenVisitor::genUnary(SysYParser::UnaryExpContext *ctx)
{
  if (ctx->primaryExp())
    return genPrimary(ctx->primaryExp());
  // 函数调用
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
  // 一元运算符
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
      // 逻辑非: (v == 0) ? 1 : 0
      std::string cmp = builder->createICmp("eq", v, "0", "not");
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

// 生成左值表达式 (加载值)
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

    // 数组退化或部分访问
    if (ctx->exp().size() < (size_t)(dims + (isPtr ? 1 : 0)))
    {
      if (isPtr && ctx->exp().empty())
      {
        std::string val = builder->createLoad(sym->irName, sym->type, name + "_ptr");
        // 加载的是指针本身
        tmpValueTypes[val] = sym->type;
        return val;
      }

      std::string ptr = getLValPtr(ctx);

      // 确定指针指向的类型
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
        // 退化为指向第一个元素的指针
        std::string decayed = builder->createGEP(currentTy, ptr, {"0", "0"}, name + "_decay");
        TypePtr elemTy = std::static_pointer_cast<ArrayType>(currentTy)->getElementType();
        tmpValueTypes[decayed] = std::make_shared<PointerType>(elemTy);
        return decayed;
      }

      // 已经是指向元素的指针
      tmpValueTypes[ptr] = std::make_shared<PointerType>(currentTy);
      return ptr;
    }
  }

  // 标量或完全索引的数组元素：加载值
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

// 生成全局变量声明
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

// 生成全局常量声明
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

// 获取左值的地址 (指针)
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
    // 对于指针类型 (如数组参数)，符号存储的是指针变量的地址 (i32**)
    // 必须先加载指针值 (i32*)
    std::string ptr = builder->createLoad(base, sym->type, name + "_ptr");
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

// 逻辑或 (短路求值)
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

// 逻辑与 (短路求值)
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

// 相等性比较
void CodeGenVisitor::genEq(SysYParser::EqExpContext *ctx, BasicBlock *tBB, BasicBlock *fBB, Function *fn)
{
  if (ctx->eqExp() && ctx->relExp())
  {
    std::string lhs = genEqVal(ctx);
    std::string cmp = builder->createICmp("ne", lhs, "0", "cond");
    builder->createCondBr(cmp, tBB->getName(), fBB->getName());
  }
  else
  {
    genRel(ctx->relExp(), tBB, fBB, fn);
  }
}

// 关系比较
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
    // 基本情况：RelExp -> AddExp
    // 计算 AddExp，如果不为 0 则跳转到 true，否则跳转到 false
    std::string val = genAdd(ctx->addExp());
    std::string cmp = builder->createICmp("ne", val, "0", "cond");
    builder->createCondBr(cmp, tBB->getName(), fBB->getName());
  }
}

// 辅助函数：生成比较表达式的值 (0/1)
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
      // 对齐到下一个元素边界
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

      // 确保填满当前元素 (如果需要则补零)
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
