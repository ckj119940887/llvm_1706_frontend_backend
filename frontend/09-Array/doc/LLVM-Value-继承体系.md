# LLVM `Value` 继承体系详解

以 `llvm::Value` 为根，几乎所有"能被别的东西使用的东西"都是 `Value`——一个 `Value` 代表一个可以被其他指令引用的 SSA 值。

---

## 一、顶层：`Value` 的直接子类

```
Value
├── Argument                  函数形参
├── BasicBlock                基本块
├── User                      "会使用其他 Value 的值"（最大的一支）
├── MemoryAccess              (MemorySSA 分析用，非 IR 核心)
│   ├── MemoryUse
│   ├── MemoryDef
│   └── MemoryPhi
├── InlineAsm                 内联汇编
├── MetadataAsValue           把 Metadata 包装成 Value 用
└── (其它内部/GPU 相关的少数类)
```

真正庞大的是 `User` 这一支。

---

## 二、两个关键抽象：`Value` 和 `User`

这是最容易困惑、也最重要的一点：

- **`Value`**：我是一个值，**别人可以使用我**（别人可以引用我作为操作数）。
- **`User`**：我会**使用别的值**（我有 operands 操作数）。

一个 `Instruction`（比如 `%c = add %a, %b`）**同时是两者**：

- 它是 `User`——它使用了 `%a` 和 `%b`（operands）。
- 它是 `Value`——它产生了 `%c`，`%c` 可以被后面的指令使用。

这套 Value/User 关系用 **use-def 链**（use-def chain）连接起来，构成了 LLVM IR 的图结构：

- `V->users()` 遍历"谁用了我"
- `I->operands()` 遍历"我用了谁"

---

## 三、第一大支：`User → Constant`

常量体系。**特点：编译期已知、无副作用、去重唯一化（uniqued）**。

```
User
└── Constant
    ├── ConstantData                    不含操作数的简单常量
    │   ├── ConstantInt                 整数常量 (i32 42, i1 true)
    │   ├── ConstantFP                  浮点常量 (double 3.14)
    │   ├── ConstantPointerNull         null 指针
    │   ├── ConstantTokenNone           token 类型的 none
    │   ├── UndefValue                  undef
    │   │   └── PoisonValue             poison (undef 的更强版本)
    │   ├── ConstantAggregateZero       全零聚合 (zeroinitializer)
    │   └── ConstantData{Array,Vector}  紧凑存储的数组/向量常量
    │
    ├── ConstantAggregate                含操作数的聚合常量
    │   ├── ConstantArray               [4 x i32] [...]
    │   ├── ConstantStruct              {i32, double} {...}
    │   └── ConstantVector              <4 x i32> <...>
    │
    ├── ConstantExpr                     常量表达式 (getelementptr、位运算等的常量形式)
    │   └── (按 opcode 细分: GetElementPtrConstantExpr,
    │        BinaryConstantExpr, CastConstantExpr ...)
    │
    ├── BlockAddress                     blockaddress(@f, %bb)
    ├── DSOLocalEquivalent               PLT/直接调用等价符号
    ├── NoCFIValue                       no_cfi 符号
    ├── ConstantPtrAuth                  指针认证 (AArch64 PAuth)
    │
    └── GlobalValue                      全局符号 (有链接属性、地址)
        ├── GlobalObject
        │   ├── Function                函数 (本身是常量/值!)
        │   └── GlobalVariable          全局变量 @g
        ├── GlobalAlias                 全局别名
        └── GlobalIFunc                 ifunc (运行时解析的符号)
```

**要点：**

- **`Function` 和 `GlobalVariable` 都是 `Constant`**——因为它们的"地址"在编译期就固定了，是编译期常量。
- `ConstantData` vs `ConstantAggregate` 的区别是**有没有真正的 operand**。`ConstantInt` 不引用任何 Value，所以在 `ConstantData` 下；`ConstantArray [i32 %a, i32 %b]` 引用了其他常量，所以在 `ConstantAggregate` 下。
- `PoisonValue` 继承自 `UndefValue`，这是个真实的继承关系（poison 是 undef 的特化）。

---

## 四、第二大支：`User → Instruction`

指令体系，**所有 IR 指令的基类**。LLVM 按"操作数个数模式"先分成几个中间基类，再落到具体指令。

```
User
└── Instruction
    ├── UnaryInstruction              (单操作数指令的共同基类)
    │   ├── UnaryOperator             fneg
    │   ├── CastInst                  各种类型转换
    │   │   ├── TruncInst / ZExtInst / SExtInst
    │   │   ├── FPTruncInst / FPExtInst
    │   │   ├── FPToUIInst / FPToSIInst / UIToFPInst / SIToFPInst
    │   │   ├── PtrToIntInst / IntToPtrInst
    │   │   ├── BitCastInst / AddrSpaceCastInst
    │   ├── LoadInst                  load
    │   ├── VAArgInst                 va_arg
    │   ├── ExtractValueInst          从聚合里取字段
    │   ├── AllocaInst                栈上分配 (codegen 变量常用!)
    │   └── FreezeInst                freeze
    │
    ├── BinaryOperator                二元运算 add/sub/mul/and/or/shl/fadd...
    │
    ├── CmpInst                       比较指令 (基类)
    │   ├── ICmpInst                  整数比较 icmp
    │   └── FCmpInst                  浮点比较 fcmp
    │
    ├── StoreInst                     store
    ├── GetElementPtrInst             getelementptr (地址计算)
    ├── PHINode                       phi 节点 (SSA 合流)
    ├── SelectInst                    select cond, a, b
    ├── ExtractElementInst            向量取元素
    ├── InsertElementInst             向量插元素
    ├── ShuffleVectorInst             向量 shuffle
    ├── InsertValueInst               往聚合里塞字段
    ├── LandingPadInst                异常 landing pad
    ├── AtomicCmpXchgInst             cmpxchg
    ├── AtomicRMWInst                 atomicrmw
    ├── FenceInst                     fence
    │
    ├── CallBase                      "调用类"指令的基类 (有参数列表)
    │   ├── CallInst                  call
    │   │   └── (IntrinsicInst)       intrinsic 调用的包装视图
    │   │        ├── DbgInfoIntrinsic, MemIntrinsic (memcpy/memset...),
    │   │        └── ... 各种 llvm.* intrinsic
    │   ├── InvokeInst                invoke (可能抛异常的调用)
    │   └── CallBrInst                callbr (带跳转目标的调用, 如 asm goto)
    │
    └── Terminator 类 (基本块结尾，通过 isTerminator() 归类)
        ├── ReturnInst               ret
        ├── BranchInst               br (条件/无条件跳转)
        ├── SwitchInst               switch
        ├── IndirectBrInst           indirectbr
        ├── ResumeInst               resume (异常传播)
        ├── CatchSwitchInst
        ├── CatchReturnInst
        ├── CleanupReturnInst
        └── UnreachableInst          unreachable
```

**要点：**

- **`AllocaInst`、`LoadInst`、`ExtractValueInst`、`CastInst` 等都在 `UnaryInstruction` 下**——它们都恰好只有一个操作数（alloca 的操作数是"数组大小"）。
- **`CallInst`、`InvokeInst`、`CallBrInst` 共享 `CallBase`**——这样处理"调用参数、被调函数、attribute"的代码可以统一写。想拿"调用了哪个函数"用 `CallBase::getCalledFunction()`。
- `IntrinsicInst` 及其子类**不是新的对象类型**，而是 `CallInst` 的"视图/包装"——一个 `call @llvm.memcpy` 本质是 `CallInst`，但可以 `dyn_cast<MemCpyInst>` 用更方便的接口访问。
- **Terminator（终结指令）**在新版 LLVM 里没有单独的 `TerminatorInst` 基类了（历史上有过，后来删了），改用 `Instruction::isTerminator()` 判断。每个基本块必须以一条终结指令结尾。

---

## 五、第三支：`Argument` 和 `BasicBlock`

这两个直接挂在 `Value` 下，**不经过 `User`**：

- **`Argument`**：函数形参。它是 `Value`（函数体里能用它），但它不"使用"别的东西，所以不是 `User`。
- **`BasicBlock`**：基本块。它是 `Value`（`br`/`switch` 要引用它作跳转目标），但它本身不是 `User`。注意基本块**内部装着**一串 `Instruction`，但这是"包含关系"（`ilist`），不是继承关系。

---

## 六、三个正交的、**不属于** Value 体系的类

初学最容易搞混的，是这三套独立体系：

| 体系 | 根类 | 作用 | 与 Value 的关系 |
|------|------|------|----------------|
| **类型** | `Type` | `i32`, `double`, `[4 x i32]`, 指针… | 每个 `Value` 有一个 `getType()`，但 Type 不是 Value |
| **元数据** | `Metadata` | 调试信息 `!dbg`、TBAA、循环信息… | 靠 `MetadataAsValue` 才能当 Value 用 |
| **属性** | `Attribute` | `noalias`, `nonnull`, `sext`… | 挂在函数/参数上，完全独立 |

`Type` 自己也有继承体系（`IntegerType`、`PointerType`、`StructType`、`FunctionType`、`ArrayType`…），但那是另一棵树。

---

## 七、LLVM 特色的 RTTI

LLVM **不用** C++ 的 `dynamic_cast`（为了性能，编译时关掉了 RTTI）。它自己实现了一套：

```cpp
if (auto *CI = dyn_cast<ConstantInt>(V)) {   // 转不成返回 nullptr
    int64_t x = CI->getSExtValue();
}
if (isa<Instruction>(V)) { ... }             // 只判断类型
Instruction *I = cast<Instruction>(V);       // 确信是，直接转（失败会 assert）
```

- `isa<T>` —— 判断是不是。
- `cast<T>` —— 确定是，直接转（debug 下失败 assert）。
- `dyn_cast<T>` —— 试着转，失败返回 `nullptr`（最常用，配合 `if` 用）。

底层靠每个类里的 `getValueID()` 和 `classof()` 静态方法实现，比 `dynamic_cast` 快得多。

---

## 八、和写 codegen 的关系

`IRBuilder` 的方法基本都返回 `Value*`：

```cpp
Value *L = ...;                        // 表达式左值
Value *R = ...;
Value *sum = Builder.CreateAdd(L, R);  // 返回 Value* (其实是个 Instruction*)
AllocaInst *slot = Builder.CreateAlloca(...);  // 变量的栈槽，AllocaInst 也是 Value
Builder.CreateStore(sum, slot);
Value *v = Builder.CreateLoad(ty, slot);
```

所以 AST codegen 函数几乎都统一返回 `llvm::Value*`——因为不管子表达式是常量、变量读取、还是运算结果，它们**都是 `Value`**，这正是这套继承体系带来的统一性。

---

## 九、一句话记住主干

> **`Value`** = "能被引用的东西" → 其中 **`User`** = "还会引用别人的东西" → `User` 下面就两大块：**`Constant`**（编译期常量，含全局符号和函数）和 **`Instruction`**（运行期指令）。剩下 `Argument`、`BasicBlock` 直接挂在 `Value` 下。

**权威清单**（想确认某个版本的精确枚举）：

- `llvm/include/llvm/IR/Value.def` —— `ValueTy` 枚举，这棵树的权威清单。
- `llvm/include/llvm/IR/Instruction.def` —— 所有指令 opcode 的清单。
