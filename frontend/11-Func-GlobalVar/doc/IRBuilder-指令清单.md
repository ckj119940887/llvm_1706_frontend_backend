# codegen.cc 中的 IR 构建调用清单

对应 `07-Logical-Bit` 阶段的 `codegen.cc`。分三部分：

1. 已用到的调用 —— 含义、生成的 IR、出处
2. 尚未用到的 IR 指令 —— 按类别列出，标注为什么没用到
3. 已知问题

---

## 一、已用到的调用

### 1.1 模块与函数骨架（不产生指令）

| 调用 | 含义 | 生成物 | 出处 |
|---|---|---|---|
| `FunctionType::get(retTy, {argTys}, isVarArg)` | 构造函数类型。第三个参数 `true` 表示变参 | `i32 (ptr, ...)` | codegen.cc:10, 14 |
| `Function::Create(fnTy, linkage, name, module)` | 在模块中建函数。无 body 时打印为 `declare`，有 body 为 `define` | `declare i32 @printf` / `define i32 @main` | codegen.cc:11, 15 |
| `BasicBlock::Create(ctx, name, parentFunc)` | 建基本块。传 `parentFunc` 则立刻挂到函数末尾；传 `nullptr` 则先游离 | `entry:` `cond:` `for.body:` … | 多处 |
| `bb->insertInto(curFunc)` | 把之前游离的基本块补挂到函数末尾 | 控制块在 IR 里的排列顺序 | codegen.cc:271, 275, 304, 308 |
| `irBuilder.SetInsertPoint(bb)` | 把「后续指令写到哪个块」的游标移到 bb 末尾 | —— | 多处 |
| `irBuilder.GetInsertBlock()` | 读取当前插入点所在的块 | —— | codegen.cc:269, 302 |
| `CreateGlobalStringPtr("...")` | 把 C 字符串放进全局只读区，返回指向首字符的 `i8*` | `@0 = private unnamed_addr constant [14 x i8] c"..."` | codegen.cc:44, 46 |
| `verifyFunction(*f)` | 校验函数 IR 合法性（每块有终结指令、SSA 支配关系等） | —— | codegen.cc:51 |
| `module->print(outs(), nullptr)` | 打印整个模块的 .ll 文本 | —— | codegen.cc:53 |

类型/常量工厂，同样不产生指令：

| 调用 | 含义 |
|---|---|
| `irBuilder.getInt32Ty()` | 类型 `i32` |
| `irBuilder.getInt8PtrTy()` | 类型 `i8*`（新版 LLVM 打印为不透明的 `ptr`） |
| `irBuilder.getInt32(n)` | 常量 `i32 n`，返回的是 `ConstantInt`，直接内联进指令，不占一行 IR |

### 1.2 内存类指令

| 调用 | 含义 | 生成的 IR | 出处 |
|---|---|---|---|
| `CreateAlloca(ty, arraySize, name)` | 在**栈帧**上分配一块 `ty` 大小的空间，返回**地址**（不是值）。`arraySize` 传 `nullptr` 表示 1 个元素 | `%a = alloca i32, align 4` | VisitVariableDecl, codegen.cc:403 |
| `CreateStore(val, addr)` | 把值写进地址。值自带类型，故不必再传 `ty` | `store i32 3, ptr %a` | VisitAssignExpr, codegen.cc:423 |
| `CreateLoad(ty, addr, name)` | 从地址读出值。新版 LLVM 指针是不透明的 `ptr`，光看地址不知道读几字节，所以**必须**显式传 `ty` | `%a1 = load i32, ptr %a` | VisitAssignExpr codegen.cc:424、VisitVariableAccessExpr codegen.cc:442 |

三者的分工：`alloca` 开空间 → `store` 写 → `load` 读。局部变量在前端一律走这套「内存化」方案，后续 mem2reg pass 会把它提升成寄存器 SSA。

### 1.3 算术与位运算（全部在 VisitBinaryExpr）

`nsw` = No Signed Wrap，`nuw` = No Unsigned Wrap。带上后，一旦真的溢出，结果是 undef —— 这是给优化器的承诺，让它可以做 `a+1 > a ⇒ true` 之类的化简。C 的有符号溢出本就是 UB，所以前端加 `nsw` 是合规的。

| 调用 | 含义 | 生成的 IR | 对应 OpCode |
|---|---|---|---|
| `CreateNSWAdd(l, r)` | 有符号加，假定不溢出 | `add nsw i32` | add |
| `CreateNSWSub(l, r)` | 有符号减，假定不溢出 | `sub nsw i32` | sub |
| `CreateNSWMul(l, r)` | 有符号乘，假定不溢出 | `mul nsw i32` | mul |
| `CreateSDiv(l, r)` | 有符号除（向零取整） | `sdiv i32` | div |
| `CreateSRem(l, r)` | 有符号取余，符号跟被除数走 | `srem i32` | mod |
| `CreateAnd(l, r)` | 按位与 | `and i32` | bitAnd |
| `CreateOr(l, r)` | 按位或 | `or i32` | bitOr |
| `CreateXor(l, r)` | 按位异或 | `xor i32` | bitXor |
| `CreateShl(l, r)` | 左移 | `shl i32` | leftShift |
| `CreateAShr(l, r)` | **算术**右移，高位补符号位（`-8 >> 1 == -4`） | `ashr i32` | rightShift |

右移选 `ashr` 而不是 `lshr`，是因为操作数类型是有符号的 `int`。将来支持 `unsigned` 时要按类型分派到 `lshr`。

### 1.4 比较指令

六个关系运算符各用一条 `icmp`，结果都是 **`i1`**，再转回 `i32`：

| 调用 | 含义 | 生成的 IR | 对应 OpCode |
|---|---|---|---|
| `CreateICmpEQ(l, r)` | 相等 | `icmp eq i32` | equal_equal |
| `CreateICmpNE(l, r)` | 不等 | `icmp ne i32` | not_equal |
| `CreateICmpSLT(l, r)` | 有符号 `<`（S = Signed） | `icmp slt i32` | less |
| `CreateICmpSLE(l, r)` | 有符号 `<=` | `icmp sle i32` | less_equal |
| `CreateICmpSGT(l, r)` | 有符号 `>` | `icmp sgt i32` | greater |
| `CreateICmpSGE(l, r)` | 有符号 `>=` | `icmp sge i32` | greater_equal |

另外 `CreateICmpNE(val, getInt32(0))` 被复用为「C 真值转换」——把任意 `i32` 压成 `i1`，用在 4 个地方：

- if 条件（codegen.cc:90）
- for 条件（codegen.cc:140）
- `&&` 的左、右操作数（codegen.cc:257, 263）
- `||` 的左、右操作数（codegen.cc:290, 296）

### 1.5 类型转换

| 调用 | 含义 | 生成的 IR | 出处 |
|---|---|---|---|
| `CreateZExt(v, ty)` | 零扩展，高位补 0。`i1 1 → i32 1` | `zext i1 ... to i32` | `&&`/`||` 的右操作数，codegen.cc:265, 298 |
| `CreateIntCast(v, ty, isSigned)` | 通用整型转换，按位宽自动选 `trunc`/`zext`/`sext`。这里 `isSigned=true` 且位宽变宽，实际选中的是 **`sext`** | `sext i1 ... to i32` | 六个关系运算符收尾，codegen.cc:333, 340, 347, 354, 361, 368 |

⚠️ 两处对 `i1 → i32` 用了**不同**的扩展方式，见第三节。

### 1.6 控制流

| 调用 | 含义 | 生成的 IR | 出处 |
|---|---|---|---|
| `CreateBr(bb)` | 无条件跳转。同时充当基本块的**终结指令** | `br label %bb` | if/for/break/continue/短路求值 |
| `CreateCondBr(cond, trueBB, falseBB)` | 条件跳转，`cond` 必须是 `i1` | `br i1 %c, label %t, label %f` | codegen.cc:93, 105, 141, 259, 292 |
| `CreatePHI(ty, numReserved)` | SSA 的 φ 节点：值取决于「从哪个前驱块跳进来」。必须位于块首 | `phi i32 [ %a, %bb1 ], [ 0, %bb2 ]` | 仅 `&&` codegen.cc:277、`||` codegen.cc:310 |
| `phi->addIncoming(val, bb)` | 登记一条「从 bb 来则取 val」 | 上面方括号里的一项 | codegen.cc:278-279, 311-312 |
| `CreateCall(callee, {args})` | 函数调用 | `call i32 (ptr, ...) @printf(ptr @0, i32 %x)` | codegen.cc:44, 46 |
| `CreateRet(v)` | 带值返回，终结指令 | `ret i32 0` | codegen.cc:49 |

**PHI 的关键细节**（codegen.cc:269, 302）：

```cpp
nextBB = irBuilder.GetInsertBlock();
```

`right` 求值完后，当前所在的块**不一定**还是最初创建的 `nextBB` —— 右子树里若嵌套了 `&&`/`||`，会引入新的块。PHI 登记的前驱必须是**实际跳来的那个块**，所以这里重新取一次。这是短路求值代码生成最容易写错的地方。

### 1.7 各 Visit 函数生成的结构

| 函数 | 生成的 IR 结构 |
|---|---|
| `VisitProgram` | `declare printf` + `define main` + entry 块 + 末尾 `call printf` + `ret i32 0` |
| `VisitBlockStmt` / `VisitDeclStmt` | 不产生指令，只是遍历子节点，返回最后一个值 |
| `VisitIfStmt` | `cond` / `then` / `else`(可选) / `last` 四块；`icmp ne` + `condbr`，两个分支各一条 `br last` |
| `VisitForStmt` | `for.init` / `for.cond` / `for.inc` / `for.body` / `for.last` 五块；把 `lastBB`/`incBB` 登记进 `breakBBs`/`continueBBs`，退出时 erase |
| `VisitBreakStmt` | `br` 到该循环的 `for.last`，随后建一个 `for.break.death` 死块承接后续指令 |
| `VisitContinueStmt` | `br` 到该循环的 `for.inc`，随后建 `for.continue.death` 死块 |
| `VisitBinaryExpr` | 见 1.3–1.5；`&&`/`||` 额外生成 `nextBB`/`falseBB`(或 `trueBB`)/`mergeBB` + PHI |
| `VisitNumberExpr` | 不产生指令，返回 `ConstantInt` |
| `VisitVariableDecl` | 一条 `alloca`，并把 `{名字 → (地址, 类型)}` 记入 `varAddrTypeMap` |
| `VisitAssignExpr` | `store` + `load`（因为 `a = 3` 整体还是个有值的表达式，值为赋完之后的 a） |
| `VisitVariableAccessExpr` | 一条 `load` |

### 1.8 `&&` / `||` 的短路结构

以 `a && b` 为例（codegen.cc:250-282）：

```llvm
  %l = ...                    ; 求 a
  %c = icmp ne i32 %l, 0
  br i1 %c, label %nextBB, label %falseBB

nextBB:                       ; a 为真才求 b
  %r  = ...                   ; 求 b（可能又展开出若干块！）
  %r1 = icmp ne i32 %r, 0
  %r2 = zext i1 %r1 to i32
  br label %mergeBB

falseBB:                      ; a 为假，b 根本不求值
  br label %mergeBB

mergeBB:
  %v = phi i32 [ %r2, %nextBB ], [ 0, %falseBB ]
```

`||` 结构对称：左为真直接跳 `trueBB`，PHI 的常量项是 `1`。

`falseBB`/`trueBB`/`mergeBB` 创建时**不**传 `curFunc`，等填完 `nextBB` 再 `insertInto` —— 目的是让 IR 里的块顺序符合执行顺序，可读性更好。

---

## 二、尚未用到的 IR 指令

### 2.1 同类中被跳过的变体

| 指令 | 说明 | 为什么没用到 |
|---|---|---|
| `CreateAdd/Sub/Mul` | 不带 `nsw` 的版本 | 统一用了 nsw 版 |
| `CreateNUWAdd/Sub/Mul` | 带 `nuw` | 无 `unsigned` 类型 |
| `CreateExactSDiv` | 承诺整除无余数 | —— |
| `CreateUDiv` / `CreateURem` | 无符号除/余 | 无 `unsigned` 类型 |
| `CreateLShr` | **逻辑**右移，高位补 0 | 无 `unsigned` 类型；有了以后要按类型在 `ashr`/`lshr` 之间分派 |
| `CreateICmpULT/ULE/UGT/UGE` | 无符号比较 | 同上 |
| `CreateSExt` / `CreateTrunc` | 显式符号扩展 / 截断 | 只有 `i32` 一种宽度；`sext` 目前是被 `CreateIntCast` 间接触发的 |

### 2.2 一元运算符（本阶段最明显的缺口）

| 指令 | 对应 C 语法 | 惯用写法 |
|---|---|---|
| `CreateNot(v)` | `~x` | 展开成 `xor x, -1` |
| `CreateNeg(v)` | 一元 `-x` | 展开成 `sub 0, x` |
| （无专用指令） | `!x` | `icmp eq x, 0` + `zext` |

`ast.h:228-248` 的 OpCode 枚举里 18 个二元运算符**全部**在 `VisitBinaryExpr` 中实现了，`default` 分支实际走不到。缺的是一元运算符这一整类 —— 词法/语法层还没有对应节点，不只是 codegen 的事。

### 2.3 控制流

| 指令 | 说明 | 备注 |
|---|---|---|
| `CreateSwitch` | 多路分支 | `switch` 语句未实现 |
| `CreateIndirectBr` | 跳到运行期计算出的标签 | `goto`/计算跳转未实现 |
| `CreateSelect(c, a, b)` | 三目 `c ? a : b`，**无分支** | `&&`/`||` 这里用 PHI + 基本块实现是**必须**的（要短路，右操作数不能提前求值）；但将来实现 `?:` 且两边无副作用时，`select` 是更短的写法 |
| `CreateUnreachable` | 标记不可达 | `for.break.death` / `for.continue.death` 两个死块目前留空等后续指令填；语义上更准确的做法是直接放 `unreachable` |
| `CreateRetVoid` | `ret void` | 只有返回 `i32` 的 main |
| `CreateInvoke` / `CreateLandingPad` / `CreateResume` | 异常处理 | C 语言不需要 |

### 2.4 内存 / 聚合 / 指针

| 指令 | 说明 |
|---|---|
| `CreateGEP` / `CreateInBoundsGEP` | 地址计算。数组下标、结构体成员、指针运算全靠它 —— 下一阶段的核心 |
| `CreateExtractValue` / `CreateInsertValue` | 读写**寄存器里**的聚合值字段 |
| `CreateExtractElement` / `CreateInsertElement` / `CreateShuffleVector` | 向量操作 |
| `CreateBitCast` | 同尺寸位重解释 |
| `CreatePtrToInt` / `CreateIntToPtr` / `CreateAddrSpaceCast` | 指针与整数互转 |
| `CreateAtomicRMW` / `CreateAtomicCmpXchg` / `CreateFence` | 原子操作与内存屏障 |
| `CreateVAArg` | 取变参 |
| `new GlobalVariable(...)` | 全局变量。目前全局区只有 `CreateGlobalStringPtr` 生成的字符串常量 |

### 2.5 浮点全套

`CreateFAdd` / `CreateFSub` / `CreateFMul` / `CreateFDiv` / `CreateFRem` / `CreateFNeg` / `CreateFCmpO*` / `CreateFCmpU*` / `CreateSIToFP` / `CreateFPToSI` / `CreateUIToFP` / `CreateFPToUI` / `CreateFPExt` / `CreateFPTrunc`。

类型系统里只有 `CType::GetIntTy()`（type.h），`VisitVariableDecl` 也只处理这一种类型。

---

## 三、已知问题：关系运算符返回 -1 而非 1

六个关系运算符统一用：

```cpp
return irBuilder.CreateIntCast(val, irBuilder.getInt32Ty(), true);
//                                                         ^^^^ isSigned
```

`isSigned=true` 且目标位宽更大 ⇒ `CastInst::getCastOpcode` 选中 **`SExt`**。而 `sext i1 1 to i32` 得到的是全 1，即 **-1**。

实测 `int a; a = 3; a == 3;`：

```llvm
%0 = icmp eq i32 %a2, 3
%1 = sext i1 %0 to i32        ; true → 0xFFFFFFFF = -1
%2 = call i32 (ptr, ...) @printf(ptr @0, i32 %1)
```

输出 `expr val: -1`，而 C 标准规定关系运算符产出 `0` 或 `1`。

- 用作条件判断时**无害**：`icmp ne -1, 0` 仍为真
- 参与算术就会错：`1 + (a == 3)` 得 `0` 而不是 `2`

修法：改成 `CreateZExt(val, irBuilder.getInt32Ty())`，或把第三个参数传 `false`，与 `&&`/`||` 分支里的 `CreateZExt` 保持一致。
