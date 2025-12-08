# 编译器架构设计说明


---

## 目录
- 概览
- 项目布局与建议文件
- 词法分析 & 语法分析
- 类型系统设计
- IRBuilder / Instruction 类接口
- `main` 与算术表达式 IR 生成示例
- 符号表设计
- 局部变量与全局变量的翻译
- 控制流语句的翻译（if / while / break / continue）
- 函数的翻译
- 数组的翻译
- 控制流图与机器无关优化（Optional）
- 测试用例与剩余工作

---

## 概览

目标：在现有仓库基础上提供清晰的接口与示例，使得任何熟悉 C++ 与编译原理的开发者能接手并完成代码生成与中间表示（IR）层的实现。

约定：
- 语言：C++（与现有代码风格保持一致）
- IR 取向：基于三地址/指令 + 基本块 + 函数的表示（类似 LLVM 风格，简化版）

---

## 项目布局与建议文件

现有仓库已有 `src/CodeGenVisitor.*`、`include/IR.h`、`src/IRBuilder.cpp` 等文件。建议新增/修改位置：

- `include/IR.h`：IR 抽象基类与 `Value`、`Instruction`、`BasicBlock`、`Function` 声明
- `include/IRBuilder.h`：`IRBuilder` 声明（创建指令、管理当前基本块）
- `src/IRBuilder.cpp`：实现 `IRBuilder`
- `src/CodeGenVisitor.cpp`：实现 parse tree 到 IR 的翻译，调用 `IRBuilder`
- `include/SymbolTable.h` / `src/SymbolTable.cpp`：符号表实现
- `tests/`：放置单元/集成测试样例（表达式、控制流、函数、数组）

---

## 词法分析 & 语法分析

说明：ANTLR 已在仓库中，词法/语法工作通常由生成的 `Lexer`/`Parser` 完成。下游模块以 parse tree 或 visitor 回调为接口。

接口建议：

- `parse(const std::string &input) -> ParseTree*`：接收源代码字符串，返回 Parser 生成的根节点（或直接返回 `Parser::CompUnitContext*`）。
- 使用 Visitor：现有 `CodeGenVisitor` 继承自 ANTLR 的 visitor 基类，建议在 `CodeGenVisitor` 中注入 `IRBuilder &builder` 与 `SymbolTable &symtab`。

示例构造（伪代码）：

```cpp
// CodeGenVisitor.h
class CodeGenVisitor : public SysYParserBaseVisitor {
 public:
  CodeGenVisitor(IRBuilder &b, SymbolTable &s);
  antlrcpp::Any visitFuncDef(SysYParser::FuncDefContext *ctx) override;
  // 更多 visitXXX
};
```

调用流程：
1. 使用 ANTLR 构造 `Parser` 并得到 `CompUnitContext* root`。
2. 创建 `IRBuilder builder(module)`、`SymbolTable symtab`。
3. `CodeGenVisitor visitor(builder, symtab)` -> `visitor.visitCompUnit(root)`。

---

## 类型系统设计

目标：提供类型表示与比较、兼容性检查、布局信息（如数组元素尺寸）等。

核心类：

```cpp
// include/Type.h
class Type {
 public:
  enum Kind { INT, VOID, PTR, ARRAY, FUNC };
  virtual Kind kind() const = 0;
  virtual bool equals(const Type* other) const = 0;
  virtual std::string str() const = 0;
  virtual ~Type() = default;
};

class IntType : public Type { ... };
class VoidType : public Type { ... };
class PointerType : public Type { Type *pointee; ... };
class ArrayType : public Type { Type *elem; size_t length; ... };
class FunctionType : public Type { Type *ret; std::vector<Type*> params; ... };
```

API 要点：
- `equals`：精确类型相等检查
- `isAssignableFrom(dest, src)`：可选，检查赋值兼容性
- 提供 `sizeInBytes()`（整型返回固定 4）用于 GEP / 内存布局

---

## IRBuilder / Instruction 类接口

设计目标：封装 IR 创建逻辑，将高阶代码生成从指令细节中解耦。

关键概念：`Module`（函数集合、全局变量）、`Function`、`BasicBlock`、`Instruction`、`Value`。

基本接口建议（头文件示例）：

```cpp
// include/IR.h (核心类型)
class Value { Type *type(); virtual std::string name() const = 0; };
class Instruction : public Value { BasicBlock *parent(); };
class BasicBlock {
 public:
  std::string name();
  Function *parent();
  std::vector<Instruction*> insts();
};

// include/IRBuilder.h
class IRBuilder {
 public:
  IRBuilder(Module &m);
  void setInsertPoint(BasicBlock *bb);
  BasicBlock* createBasicBlock(const std::string &name, Function *parent=nullptr);
  Instruction* createAdd(Value *lhs, Value *rhs, const std::string &name="");
  Instruction* createSub(...);
  Instruction* createMul(...);
  Instruction* createDiv(...);
  Instruction* createAlloca(Type *ty, const std::string &name="");
  Instruction* createLoad(Value *ptr, const std::string &name="");
  Instruction* createStore(Value *val, Value *ptr);
  Instruction* createRet(Value *val);
  Instruction* createBr(BasicBlock *dest);
  Instruction* createCondBr(Value *cond, BasicBlock *thenBB, BasicBlock *elseBB);
  Instruction* createCall(Function *callee, const std::vector<Value*> &args);
  Instruction* createGEP(Value *ptr, const std::vector<Value*> &indices, const std::string &name="");
};
```

Instruction 子类建议：
- `BinaryInst`（opcode, lhs, rhs）
- `AllocaInst`、`LoadInst`、`StoreInst`
- `CallInst`、`RetInst`
- `BranchInst`、`CondBranchInst`
- `GetElementPtrInst`（用于数组/结构访问）

实现细节：
- 指令包含对操作数 `Value*` 的引用
- `IRBuilder` 负责将新指令插入到当前基本块尾部

---

## `main` 与算术表达式 IR 生成示例

`main` 的职责：
1. 读取/接收源代码或文件名
2. 调用 ANTLR 解析器生成 parse tree
3. 创建 `Module/IRBuilder/SymbolTable`
4. 调用 `CodeGenVisitor` 生成 IR
5. （可选）运行 IR 验证、优化并导出到文件（.ll/.s）或直接交给后端

示例 `main` 伪代码：

```cpp
int main(int argc, char** argv) {
  std::string source = readFile(argv[1]);
  auto tree = parse(source);
  Module module("m");
  IRBuilder builder(module);
  SymbolTable symtab;
  CodeGenVisitor visitor(builder, symtab);
  visitor.visit(tree);
  module.dump("out.ll");
}
```

算术表达式示例：输入 `1 + 2 * 3` 的 IR（伪 IR）：

```
%c1 = const 1
%c2 = const 2
%c3 = const 3
%t1 = mul %c2, %c3
%t2 = add %c1, %t1
ret %t2
```

在 `CodeGenVisitor::visitAddExp` 中：
1. 递归生成 `lhs` 与 `rhs` 的 `Value*`
2. 根据操作符调用 `builder.createAdd(lhs, rhs)` 或 `createMul` 等

---

## 符号表设计

需求：支持嵌套作用域、函数/变量声明、区分全局/局部、存储类型信息与 IR 对应 Value*

建议接口：

```cpp
// include/SymbolTable.h
struct SymbolEntry { Type *type; Value *value; bool isGlobal; };

class SymbolTable {
 public:
  void pushScope();
  void popScope();
  bool declare(const std::string &name, const SymbolEntry &entry); // 返回 false 表示重复声明
  SymbolEntry* lookupLocal(const std::string &name); // 仅当前作用域
  SymbolEntry* lookup(const std::string &name); // 向上查找
};
```

实现要点：
- 内部使用 `std::vector<std::unordered_map<std::string, SymbolEntry>> scopes_` 或 `std::stack`
- 在生成局部变量时，将 `AllocaInst` 的 `Value*` 放入对应 entry 的 `value` 字段
- 在全局变量声明时，创建 Module-level GlobalVar，并记录为 `isGlobal=true`

---

## 局部变量与全局变量的翻译

全局变量：
- 在 Module 层创建 `GlobalVariable`（带初始值或默认 0），符号表中标记为 `isGlobal`。
- 访问全局变量直接使用 `GlobalVariable*`，读写使用 `load`/`store`。

局部变量：
- 在函数入口单独插入 `alloca`（或在需要时插入）分配空间：`%a = alloca i32`。
- 变量初始化 -> `store initVal, %a`。
- 读取 -> `load %a`。

示例：

```cpp
// 生成局部 int x = 5;
Value *ptr = builder.createAlloca(IntType::get(), "x");
builder.createStore(builder.constantInt(5), ptr);
symtab.declare("x", {IntType::get(), ptr, false});
```

注意：为避免在中途插入 `alloca` 导致不便，常见做法是在函数的入口 basic block 中集中创建所有局部 `alloca`。

---

## 控制流语句的翻译（if / while / break / continue）

通用思路：把控制流结构拆成基本块（BasicBlock），使用 `condbr`/`br` 连接。

if (有 else) 形式：

1. 生成条件表达式 -> `Value* cond`
2. 创建 `thenBB`, `elseBB`, `mergeBB`
3. `createCondBr(cond, thenBB, elseBB)`
4. 在 thenBB 尾部生成 `br mergeBB`（除非 then 已返回）
5. 在 elseBB 尾部生成 `br mergeBB`（除非 else 已返回）

if (无 else)：类似，但 elseBB 可以直接作为 `mergeBB`。

while 翻译示例：

1. 创建 `condBB`, `bodyBB`, `afterBB`。
2. `br condBB`（从前导块跳到条件检查）
3. 在 `condBB` 生成 `cond`，`condbr(cond, bodyBB, afterBB)`。
4. `bodyBB` 生成主体语句，尾部 `br condBB`。

break / continue：
- 在翻译循环体时维护一个 loop context（包含 `breakTarget` 和 `continueTarget`）。
- `break` 生成 `br breakTarget`；`continue` 生成 `br continueTarget`。

示例伪代码：

```cpp
BasicBlock *condBB = builder.createBasicBlock("while.cond", func);
BasicBlock *bodyBB = builder.createBasicBlock("while.body", func);
BasicBlock *afterBB = builder.createBasicBlock("while.end", func);
builder.createBr(condBB);
builder.setInsertPoint(condBB);
Value *cond = visit(ctx->cond());
builder.createCondBr(cond, bodyBB, afterBB);
builder.setInsertPoint(bodyBB);
// translate body ...
builder.createBr(condBB);
builder.setInsertPoint(afterBB);
```

---

## 函数的翻译

要求：支持函数声明与定义，参数传递，返回值处理，递归与调用。

策略：

- 在 Module 中创建 `Function`（包含 `FunctionType`）
- 为函数创建入口 `BasicBlock`（entry），并在 entry 中为每个参数分配一个 `alloca` 并 `store` 参数到该地址（便于后续按地址访问）
- 翻译函数体时用新的符号表作用域
- `return` 生成 `ret` 指令；如果函数无返回值，生成 `ret void`

调用：

```cpp
Value *result = builder.createCall(calleeFunc, {arg1, arg2});
```

注意：约定参数传递在 IR 层是按值传递（若为数组/大结构体，使用指针或引用语义）。

---

## 数组的翻译

表示：多维数组可视为嵌套数组或扁平化存储，常用 GEP（GetElementPtr）进行索引计算。

基本思路：

- 定义 ArrayType(elemType, size)
- 对局部数组：在入口处做 `alloca ArrayType` 或按元素 `alloca`（更常见是单个数组对象）
- 访问 a[i][j]：先得到数组基址 `ptr`，然后 `gep ptr, 0, i, j`（第一个 0 表示基址偏移）

示例：访问 `int a[10][20]; a[i][j] = 5;`

```cpp
Value *base = symtab.lookup("a")->value; // pointer to 10x20 array
Value *idx0 = visit(i); Value *idx1 = visit(j);
Value *elemPtr = builder.createGEP(base, {builder.constantInt(0), idx0, idx1});
builder.createStore(builder.constantInt(5), elemPtr);
```

边界检查：可选，在前端或 IR 插入分支检查越界并报错。

---

## 控制流图与机器无关优化（Optional）

若你想在 IR 产生后做优化，建议先构造 CFG（BasicBlock 节点，边为 `br`/`condbr` 目标），再实现一些标准的机器无关优化：

- 常量折叠（Constant Folding）
- 死代码消除（Dead Code Elimination）
- 公共子表达式消除（CSE）/复制传播
- 简单的局部代数简化

接口提示：

```cpp
class Pass {
 public:
  virtual bool runOnFunction(Function &f) = 0; // 返回是否修改
};

class ModulePassManager { void run(Module &m); };
```

注意：如果实现 SSA 会使某些优化更容易，但初期可基于非-SSA 的局部优化先行。

---

## 测试用例与剩余工作

测试建议：

- 单元测试：类型系统比较、GEP 索引生成、指令构造器行为
- 集成测试：输入小型 SysY 程序 -> 生成 IR -> 验证 IR 输出与手写参考一致
- 功能测试：控制流（嵌套 if/while）、函数递归、数组索引、全局/局部变量

示例测试文件夹：`test/functional/`（仓库已有类似样例），为每个特性添加 `.sy`、`.ll` 和 `.output` 文件。

剩余工作（优先级建议）：

1. 在 `include/IR.h` 和 `include/IRBuilder.h` 中完善类声明并实现基本指令
2. 在 `include/SymbolTable.h` 中实现符号表并在 `CodeGenVisitor` 中注入
3. 在 `src/CodeGenVisitor.cpp` 中实现表达式/语句到 IR 的翻译（从最简单的算术开始）
4. 添加若干单元与集成测试
5. （可选）实现简单优化 passes

---

## 小结与交接指引

- 关键文件：`include/IR.h`、`include/IRBuilder.h`、`include/SymbolTable.h`、`src/CodeGenVisitor.cpp`。
- 实现顺序建议：类型系统 -> IR 基础（Value/Instruction/Block）-> IRBuilder -> 符号表 -> CodeGen visitor。

