# ZufyUI 优化与修复：成果 / 待办 / 雷区

> 状态日期：2026-09-20 · 当前提交 `b1d7876` · tag `phase6-done` · 工作区干净
> 说明：本文是唯一权威清单。**已做**、**待办**、**雷区**三部分。级别：🔴严重 / 🟡中等 / 🟢轻微。

---

## 0. 结论

- 优化整体已完成，**当前版本为实测最优**（切页内存峰值 300–400MB → **~30MB 封顶**、停止切页后数秒回落；CPU 为各版本中最低）。
- CPU% 用任务管理器测低占用进程**噪声很大**，同一版本会有 0.2 vs 0.8 的跳动；已用 tag 二分确认"每版都有、且当前版最低"，**不存在回归**。

---

## 一、已完成（按主题）

### 1. 内存 / 缓存（切页峰值 300–400MB → ~30MB）
- **M1** `PageHost::NavigateTo` 去掉 `InvalidateLayout()`（切页不再触发全量重排）。
- **M3** `PageHost` 可见性同步改用 `SetVisibleNoInvalidate`（page 填满 host，可见性不影响布局）。
- **M2** OnPaint 布局分支**删除 `ClearAllCaches()`**；不再"任何布局失效就重建所有缓存"。
- **修2** `PageHost` 资源释放时机：由"过渡完成那一帧(`justFinished`)"改为**"页面可见→不可见那一帧"**。
  （高频切换时 `animProgress_` 被反复重置 → `justFinished` 永不触发 → 隐藏页缓存无限累积；改后 ~30MB 封顶。）
- **缓存语义更正**：缓存是**逐控件**的（`ComposeImpl` 对缓存元素画完自身位图后**仍会递归画子元素**）→ 父缓存只含父自身绘制，**子变化不作废父缓存**。

### 2. 布局机制（第 2 期核心）
- `UIElement` 加 **DesiredSize 缓存**：`Measure` 为非虚包装器（`MeasureOverride` 为子类纯虚实现）+ `GetDesiredSize()`。
- `Arrange` 包装器（`ArrangeOverride` 子类实现），并：
  - 只在**约束变了**时重测（C2）；
  - `cacheValid_ = false` **只由"尺寸变化"驱动**（位置变化位图照 blit）；
  - `Measure` 里 `cacheValid_ = false` 只由 **`desiredSize_` 真的变了**驱动。
- **两级脏位**（拆分原 `arrangeDirty_` 被塞进的三个语义）：
  - `measureDirty_`：自身/子树需重测 → **冒泡到根**；
  - `selfArrangeDirty_`：自身要重跑 ArrangeOverride（只置自身）；
  - `subtreeDirty_`：子树有脏节点（沿祖先链置位，让父 ArrangeOverride 跑起来到达脏节点）。
- `InvalidateLayout`：`measureDirty_+selfArrangeDirty_` 置自身；祖先只 `measureDirty_+subtreeDirty_`；**冒泡到根，不做"遇脏即停"**（C1）。
- 22 个类 `Measure→MeasureOverride` / `Arrange→ArrangeOverride`（`MeasureOverride` 纯虚做安全网，漏改即编译失败）。
- `ColumnBox`/`RowBox`：去掉 Arrange 内 `Measure`，改 `GetDesiredSize()`；`MeasureOverride` 与 Arrange 用**同一约束**。

### 3. 渲染 / CPU
- **X1** `WM_SIZE` 尺寸守卫（客户区没变直接跳过；拖动/`SetWindowPos` 空触发）。
- **拖动短路**：`inSizeMove_` 期间 `WM_NCHITTEST` 直接返回 `HTCAPTION`、`WM_NCMOUSEMOVE` 直接 return（免每帧整树 HitTest + 按钮 hover 抖动）。
- **HasRenderWork** 不再每 tick 做全树 `HasActiveAnimation()`，改用上次合成的 `activeAnimScratch_`。
- **CollectDragRegions / CollectNonParticipating** 由"每帧全树"改为**仅重排后**（`dragRegions_` / `npBefore_`,`npAfter_`）。
- **lastActiveAnimElements_** 由 hashset 深拷贝改 **swap**。
- **M5** `PageHost::Draw` 空实现（页面背景+子树由 `GetChildren`+`GetChildRenderTransform` 通道递归绘制，原来同帧重复画一遍背景）。
- **A6** `PageHost` 过渡不再每帧对两页 `RequestRepaint()`（过渡只是变换，页面缓存应保持有效）。
- **③** `ComposeImpl`/`EnsureCache` 直接用 `dpi_`，去掉**每元素每帧** `renderTarget_->GetDpi()`。

### 4. 文本
- **① FontManager 全局文本布局缓存**（跨所有控件共享）：key=`文本+fmt指针+量化宽高(0.5 DIP)+noWrap+mode`；mode 0=原始布局、1=显示布局（按**原文本**缓存截断结果）；有界 FIFO（400，一次淘汰 1/4）；`Store` 防重。
- `DrawTextWithEllipsis`：命中显示缓存 → 直接画（跳过测量+二分）；未命中 → 原始布局走缓存 + 二分截断（`reserve/assign` 消临时分配）+ 缓存显示布局。
- **②** `Label::Draw` 的 Ellipsis 由**逐字符 O(n)** 改**二分**。
- D4：`DrawTextWithEllipsis` 未截断时复用测量布局。

### 5. 其它 bug / 健壮性（第 1 期）
- 逻辑：**L1–L12**（死代码、父勾选动画、Multi 重复触发、`SetCurrentCell` -1 语义、排序补发 `SelectionChanged`、`TextBox` 撤销栈、VK_DELETE 重复赋值、`SetValue` 空触发、`ComboBox::ApplyFilter` 保留选中、`justExpanded_` 生效、`CaptionButton` 移出不清 pressed）、**D6**（ListView firstVisible 与 itemRect 统一 Snap）。
- 绘制/事件/资源：**E1**（`OnMouseDown` 过滤 disabled）、**E2**（`ZSignal::Fire` 释放 `tlSnapshot`）、**D3/D5**（滚动条除零守卫）、**R2**（图像缓存 key 加设备 epoch）、**P1**（`ListView` 批量增删 `BeginUpdate/EndUpdate`）、**P3**（`TreeView::GetVisibleIndex` 惰性索引）。
- **④ `childrenDirty_`**：`GetChildren()` 结果缓存（`SetParent` 置脏；`PageHost` 动画状态变化 / `Label::ClearChildren` 也置脏）。

---

## 二、待办（TODO）

| 优先级 | 项 | 说明 | 风险 |
|---|---|---|---|
| 随时可做 | **D6 注释止血** | 给 `SetItem(shared_ptr<Label>)` 系列加注释："仅 `GetText()` 被读取，其余属性被忽略；单项样式请用 `SetItemTextColor` 等" | 零 |
| 下一阶段 | **D1/D2 数据控件 Label 化** | `ListView/TableView` 的 `GetChildren` 返回项 Label + `ArrangeOverride` 里 Arrange + `GetClipRect` 返回自身范围 + `UpdateAnimation/HasActiveAnimation/AttachWindowRecursive` 递归 + 加项时 `label->SetUseCache(false)`；删 `Draw` 里的 `DrawTextWithEllipsis`（改由合成递归画） | 低 |
| 下一阶段 | **D3 接口收敛** | `SetItemTextColor` 等转发给 Label，删 `itemTextColors_`/`cellTextColors_` | 中（破坏源码兼容） |
| 下一阶段 | **D4 TreeView 重构** | `TreeNode.columns` 由 `std::wstring` 改 `std::shared_ptr<Label>` | 中（破坏性） |
| 下一阶段 | **D5 Label 布局缓存** | `Label::Draw` 会 `SetTextAlignment/SetWordWrapping/SetLineSpacing/Trimming` **修改** layout，不能共用全局缓存 → 需按 `text+宽高+fmt+对齐+换行+overflow+maxLines` 建独立 keyed cache | 中 |
| 按需 | **M7 缓存预算/LRU** | 给离屏缓存加"总像素预算 + 淘汰"，大容器才不爆 | 中 |
| 按需 | **R3/R4/L13** | `TreeNode::parent` 弱引用；`checkAnim_` key 改 nodeId；`SetParent` 幽灵节点（需基类虚 `RemoveChild` + 各容器实现） | 中 |
| 按需 | **M8/M9/M10** | 单元素缓存像素上限；`ReleaseDeviceResources` 是否递归；多页时懒构建/卸载 | 中 |
| 可选 | **layout 缓存 key 零拷贝** | 现在每次调用构造 key（含一次文本拷贝）。若要再压：`wstring_view` + 自定义 hash（注意生命周期） | 低 |
| 可选 | **C4 `PrepareMeasure()`** | 排查 `MeasureOverride` 里的可变副作用（`ComboBox::RecalcItemWidths`、`Label::measuredIconW_`），外提到显式入口 | 中 |
| 可选 | **C3 GridLayout 定向重测** | Grid 是二维 stretch，保留"最终 cell 尺寸 ≠ 上轮 availableSize 才重测单个子" | 中 |
| 可选 | **P2/P4** | `TreeView::FindNode` 建 id→node 映射；`BuildVisibleList` 增量 | 中 |
| 可选 | **P6/P7** | `MenuWindow` 10ms 定时器提高步长/改 DWM 动画 | 低 |

---

## 三、雷区（不要做 / 已证明是坑）

### A. 明确"不做"的设计（做了会更糟）
1. **页级"整平"缓存**（把子树合成到一张位图）——❌ 不做。单控件动画会导致整张位图更新、与子控件内容重复、内存巨大、收益低。
2. **事件驱动 `CollectActiveAnimations`**（pull→push）——❌ 不做。"是否在动"是**派生量**不是独立状态；要在每个 setter/动画点发通知，条件触发的 bug（卡住/幽灵/悬垂）极难测；收益仅 1%~3%。
3. **合并 `UpdateAnimation` + `CollectActiveAnimations`**——❌ 不做。容器（ColumnBox/RowBox/GridLayout/Page/LayoutHost/Card/PageHost/ScrollViewer）的 `UpdateAnimation` **自带递归**，外部再遍历会让**子元素 UpdateAnimation 被调两次 → 线性动画速度翻倍**。
4. **让数据控件里的 Label 走缓存（`cacheRT_`）**——❌ 负优化。项多、滚动频繁、内容变化多 → 每项一张 GPU 位图，创建成本 > 收益。数据控件应保持 `SetUseCache(false)` + 直接绘制（靠全局 layout 缓存降本）。

### B. 已经踩过的认知坑（务必按"对"的那列做）
| 错误做法 | 正确认知 |
|---|---|
| 子元素变化 → 作废**父链**缓存 | 缓存是**逐控件**的；父缓存只含父自身绘制，**子变化不动父缓存** |
| `Arrange` 里无条件 `cacheValid_ = false` | 只由**尺寸变化**驱动；纯位置变化不清缓存 |
| 用一个 `arrangeDirty_` 承担"自身脏/子树脏/缓存失效"三个语义 | 拆成 `selfArrangeDirty_` + `subtreeDirty_`；缓存失效独立 |
| `InvalidateLayout` "遇脏即停" | **冒泡到根**（Measure 过程会打破不变式，遇脏即停会布局卡死） |
| 手动云母拖动会"重新采样" | 只是 **translate 变换**，合成器不重采样 |
| 拖动时照常做 hover 命中 | 拖动/缩放循环里 **`WM_NCHITTEST`→HTCAPTION、`WM_NCMOUSEMOVE`→return** |
| 用"未 Snap 的偏移"算 firstVisible | 与 itemRect 统一用 **Snap 后的值** |

### C. 平台 / 工程坑（硬事实）
- **DComp 桌面窗口目标（`CreateDesktopWindowTarget`）的视觉坐标是物理像素**（该目标 DPI=96，不随窗口 200% 走）——不是 DIP。壁纸图层 `Offset = -窗口屏幕坐标`、`Size = 虚拟屏幕尺寸`。
- **`CreateSwapChainForComposition` + `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` 要求 `BufferCount >= 2`**；写 1 会**静默失败**（表现为窗口全透明）。
- **含中文的源文件必须带 UTF-8 BOM**；否则 MSVC 按 GBK 解析会报莫名的 `C2143/C2259/C2888`。
- `ID2D1BitmapRenderTarget` 的缓存**不是整平**的：父缓存不含子树。
- `Measure` 包装器缓存的安全前提：**`FontManager::formatCache_ 永不淘汰`**（同一 FontSpec 永远同一 `IDWriteTextFormat*`）——若将来给它加淘汰，必须同时清 `layoutCache_`。
- 缓存作废的**唯一可靠来源**：`RequestRepaint()`（→ `pendingRepaint_`）+ `EnsureCache` 的尺寸比对。**任何内容/size 变化的 setter 必须调 `RequestRepaint`/`InvalidateLayout`**，否则缓存会陈旧。
- `childrenDirty_`：**所有改子元素列表的路径都要置脏**；现靠 `SetParent` 覆盖"增"，`Label::ClearChildren`、`PageHost` 的动画状态变化另置。新增移除/替换 API 时务必补置脏（漏 → 子元素不显示/不刷新）。

---

## 四、每个阶段的提交与 tag（可回退）

| tag | commit 主题 |
|---|---|
| `pre-optimization-2026-09-20` | 优化前基线 |
| `phase1-done` | WM_SIZE 守卫 / M1 / M3 / 逻辑 bug L1–L12 / D6 / E1,E2,D4,D5 |
| `phase2-done` | 布局机制：DesiredSize 缓存 + 包装器 + 两级脏位 + 去 ClearAllCaches |
| `phase2b-done` | 撤销"作废祖先缓存"过度作废 |
| `phase2c-done` | ColumnBox/RowBox 去 Arrange 内 Measure |
| `phase3-done` | 拖动短路 + M5 + P1 + P3 + R2 |
| `phase3c-done` | A6（切页不每帧重画两页）+ CollectNonParticipating 缓存 |
| `phase4-done` | 拆 arrangeDirty_ + cacheValid_ 只由尺寸驱动 |
| `phase5-done` | FontManager 全局 layout 缓存 + Label 二分 + DPI + childrenDirty_ |
| `phase6-done` | PageHost 释放时机（内存封顶）+ Store 防重 + GetChildren 守卫整理 |

---

## 五、数据视图改造进度

### ✅ 第一项：ListView Label 化（D1/D2）
- 项 Label 现在**真正走 Window 流程**：`GetChildren()` 返回**可见项** Label（用"滚动位置比对"保证不陈旧，滚动/重排后自动重建）；`ArrangeOverride` 里把每项摆到文本矩形；`GetClipRect()` 返回自身范围；
- 加项时 `label->SetUseCache(false)`（避免每项一张 GPU 位图）；
- `Draw` 里**删掉逐项 `DrawTextWithEllipsis`**——文本由 Label 经合成递归画（因此天然在选中/悬停背景**之上**、天然不接收事件）；
- `SetItemTextColor` / `SetItemDisabled` **转发给 Label**（颜色/禁用由 Label 画）。
- **收益**：列表项可放**图标 + 内嵌 Label + 单项样式**；文本从"每帧每项重建 layout"变为 Label 正常绘制。
- **demo 新增测试按钮**（列表视图页）：`批量+50 (BeginUpdate)`、`富项 (图标+内嵌 Label)`。

### ⏳ 待办
- D2b：ListView 的 `UpdateAnimation`/`HasActiveAnimation` **递归可见项 Label**（富项里的动画子控件需要）；
- D2：`TableView` 同样 Label 化；
- D3：数据控件接口收敛（删 `itemTextColors_`/`cellTextColors_`，全走 Label）；
- D4：`TreeView` 重构（`TreeNode.columns` → `shared_ptr<Label>`）；
- D5：`Label` 自身的布局缓存（`Label::Draw` 会改 layout，需独立 keyed cache）。
