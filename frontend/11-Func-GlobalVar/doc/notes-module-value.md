# llvm::Module 与 llvm::Value 的关系

## 核心结论

`llvm::Module` 和 `llvm::Value` 之间**没有继承关系**,是**容器与被容纳者(组合/聚合)**的关系。

- `Value` 是"内容":在 IR 中可以被引用、可以作为操作数(operand)的东西的根类。
- `Module` 是"容器":装这些东西的盒子,它自己不会作为任何指令的操作数,也没有类型(`Type`)。

一句话:**Value 是"内容",Module 是"容器";容器不是它所装内容的一种。**

## 1. Module 不是 Value

```cpp
class Module { ... };   // 独立类,无基类
class Value  { ... };   // IR 值体系的根
```

## 2. Module 里装的都是 Value

```
Module (不是 Value)
 ├─ Function          ← 是 Value
 ├─ GlobalVariable    ← 是 Value
 ├─ GlobalAlias       ← 是 Value
 └─ GlobalIFunc       ← 是 Value
```

这些顶层实体都是 `GlobalValue`(继而是 `Value`)。

> **Module *拥有(owns)* 一批 Value,但 Module 本身 *不是* Value。**

## 3. 靠 LLVMContext 连在一起

`Module` 和 `Value` 共享同一个 `LLVMContext`:

```cpp
Module M("mymod", Context);
Value *V = ...;              // V->getContext() 和 M.getContext() 是同一个
```

- `LLVMContext` 负责 uniquing:类型 `i32`、常量 `ConstantInt 42` 全局唯一。
- `Module` 从属于一个 `Context`;`Module` 里所有 `Value` 也用这个 `Context`。

## 4. API 上体现的容器关系

```cpp
// 从 Module 拿 Value:
Function *F = M.getFunction("main");          // 返回 Value 的子类
GlobalVariable *G = M.getGlobalVariable("g"); // 返回 Value 的子类

// 从 Value 反查 Module:
Module *M2 = F->getParent();      // Function::getParent() → Module*
```

注意:`Value` 基类**没有** `getModule()`,因为不是所有 Value 都属于某个 Module
(比如游离的 `ConstantInt` 常量)。只有有归属的子类才提供:

```cpp
Module *GlobalValue::getParent();   // 全局值属于哪个 Module
Module *Instruction::getModule();   // 指令 → BasicBlock → Function → Module
```

## 5. 完整从属层级(容器视角)

```
LLVMContext                 (最外层,类型/常量唯一化,非 Value)
   │
   └─ Module                (编译单元,非 Value)
        │
        ├─ GlobalVariable   ┐
        │                   │
        └─ Function         ├─ 这些是 Value
             │              │
             └─ BasicBlock  (BasicBlock 也是 Value!)
                  │         │
                  └─ Instruction  ┘  (也是 Value)
```

注意 `BasicBlock` 和 `Instruction` 都是 `Value`(可被跳转/其他指令引用),
但 `Module` 和 `LLVMContext` 都**不是** `Value`。

## 附:Value 的继承体系(Module 内实体所在的家族)

```
Value
 └─ User
     └─ Constant
         └─ GlobalValue          // linkage、可见性
             ├─ GlobalObject      // section、alignment
             │   ├─ Function
             │   └─ GlobalVariable
             ├─ GlobalAlias
             └─ GlobalIFunc
```
