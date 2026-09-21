# ZUI 优化与修复清单（三期）

> 版本：2026-09-20 · 依据三方评审 + 源码逐条核对。
> 说明：所有行号为当前（tag `pre-optimization-2026-09-20`）源码行号。级别：🔴 严重 / 🟡 中等 / 🟢 轻微。
> 承诺：**每一期结束 = 一次 git commit + 一次物理备份**；先做低风险、后做高风险；布局机制重构采用"保守版"（正确优先于性能）。

---

## 0. 总览表

| 编号 | 位置 | 问题 | 级别 | 期次 |
|---|---|---|---|---|
| X1 | `Window::WndProc` WM_SIZE | 窗口拖动/SetWindowPos 触发 WM_SIZE 但客户区未变 → 空全量重排 | 🟡 | 1 |
| M1 | `PageHost::NavigateTo` | 切页调 `InvalidateLayout()` → 全树重建 | 🔴 | 1 |
| M3 | `PageHost::UpdateAnimation` | 每帧 `SetVisible` → 可见性变化又 `InvalidateLayout()` | 🔴 | 1 |
| L1..L12, D6 | 见 §2 | 逻辑 bug 一批 | 🟡/🟢 | 1 |
| E1,E2,D3,D4,D5,R2,R3,R4 | 见 §3 | 绘制/事件/资源一批 | 🟡/🟢 | 1 |
| 布局机制 | `UIElement` + 22 个类 + `Window::OnPaint` | Measure/Arrange 无缓存、Arrange 内重复 Measure、Invalidate 粒度粗、ClearLayoutDirty 语义乱 | 🔴 | 2 |
| M5 | `PageHost::Draw` + `GetChildren` | 同帧页面被画两次（双通道） | 🔴 | 2 |
| P6/A6 | `Window::OnPaint` 每帧遍历 | CollectDragRegions / CollectActiveAnimations 全树每帧跑 | 🔴 | 3 |
| P1..P4, P7 | 见 §5 | 性能细节一批 | 🟡/🟢 | 3 |
| M8/M9/M10 | `EnsureCache` / 释放粒度 | 已推迟（不在本清单内做） | — | 推迟 |
| 5.3 约束传递 | `ColumnBox::Measure` | 传"剩余高度"是行为变更，明确不在本清单 | — | 不做 |

---

# 第 1 期：低风险 bug + 小改（先做，独立提交验收益）

## X1：WM_SIZE 空触发守卫（🟡）

**位置**：`ZUI.h` `case WM_SIZE:`（约 3527）

**问题**：窗口拖动或 `SetWindowPos` 会发 `WM_SIZE` 但客户区尺寸未变，当前仍无条件 `layoutNeeded_=layoutInvalidated_=true` + `InvalidateRect` + `ResizeSwapChain`。

**改法**：新增成员 `UINT lastClientW_ = 0, lastClientH_ = 0;`（`Window` 私有区），WM_SIZE 里先比较：

```cpp
case WM_SIZE: {
    UpdateTimerState();
    if (wParam == SIZE_MINIMIZED) { /* 原最小化分支不变 */ return 0; }
    if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) { /* 原还原分支不变 */ }
    RECT rc; GetClientRect(hwnd_, &rc);
    UINT nw = (UINT)(rc.right - rc.left), nh = (UINT)(rc.bottom - rc.top);
    if (nw == lastClientW_ && nh == lastClientH_) return 0;   // ★ 尺寸没变直接跳过
    lastClientW_ = nw; lastClientH_ = nh;
    clientWidthDip_  = nw * 96.0f / dpi_;
    clientHeightDip_ = nh * 96.0f / dpi_;
    if (swapChain_) ResizeSwapChain(nw, nh);
    layoutNeeded_ = true; layoutInvalidated_ = true;
    InvalidateRect(hwnd_, nullptr, FALSE);
    return 0;
}
```

> 创建窗口时把 `lastClientW_/H_` 初始化为当前客户区尺寸，避免首帧误判。

## M1：NavigateTo 去掉 InvalidateLayout（🔴）

**位置**：`ZUI.h:1757`

```cpp
void NavigateTo(int index) {
    if (index < 0 || index >= (int)pages_.size() || index == currentIndex_) return;
    fromIndex_ = currentIndex_; toIndex_ = index;
    animating_ = true; animProgress_ = 0.0f;
    RequestRepaint();
    InvalidateLayout();   // ← 删掉此行
}
```

**依据**：所有 page 在 `PageHost::Arrange`(1802) 里每个布局周期都被 Arrange；任何内容变化都会通过 `InvalidateLayout→MarkLayoutInvalidated` 置窗口级标志，下一帧仍会布局。故删掉安全。

## M3：PageHost 用 SetVisibleNoInvalidate（🔴）

**位置**：`ZUI.h:1957-1962`

```cpp
// 前
pages_[i]->SetVisible(vis);
// 后（page 填满 host，可见性不影响 host 布局）
pages_[i]->SetVisibleNoInvalidate(vis);
```

> 已确认 `SetVisibleNoInvalidate`(824) 仍调 `OnVisibilityChanged` → ComboBox 自动收起等行为保留。

---

# 第 1 期续：逻辑 bug（批次 1）

## L1：HasActiveAnimation 死代码（🟡）
**位置**：`ZUI.h:1983`。`if (animating_) return true;`(1975) 之后的 `if (animating_) { from/to 检查 }` 整段删除。

## L2：UpdateAnimation 调试判断错误（🟢）
**位置**：`ZUI.h:1892`。`if (!animating_ && animProgress_ == 0.0f)` 恒真（初始值 0）。把 `justFinished` 提前定义（1924）并在调试块里用它判断"刚结束那一帧"。

## L3：TreeView 父勾选动画缺失（🟡）
**位置**：`ZDataViewer.h UpdateParentCheckState`。设完父 `checkState` 后补 `EnsureCheckAnim(parent);`。

## L4：SetSelectedNode Multi 重复触发（🟢）
**位置**：`ZDataViewer.h SetSelectedNode`（Multi 分支）。循环内加 `if (selectedNode_ == node) return;`。

## L5：SetCurrentCell row=-1 语义（🟡）
**位置**：`ZDataViewer.h SetCurrentCell`。开头 `if (row < 0) { 清空当前 cell; return; }`，别让 `row=-1, col=0` 混进后续判断。

## L6：Sort 后不发 SelectionChanged（🟡）
**位置**：`ZDataViewer.h ListView::Sort / TableView::SortByColumn`。排序使选中项复位/移动后，补发一次 `SelectionChanged`（或调用方同步）。

## L7：TextBox::SetText 撤销栈（🟡）
**位置**：`ZUIWidgets.h TextBox::SetText`。**只保留 `ClearUndoHistory();`**（删掉紧随的 `PushUndoState()`）。编程式 `SetText` 不产生撤销点（对齐 QTextEdit）。

## L8：VK_DELETE 重复赋值（🟢）
**位置**：`ZUIWidgets.h TextBox::OnKeyDown`。删掉重复的 `selectionAnchor_ = cursorPos_;`。

## L9：Slider/ProgressBar SetValue 空触发（🟢）
**位置**：`ZUIWidgets.h`。`SetValue` 开头 `if (value == value_) return;`（Slider 用 Snap 后的值比较）。

## L10：ComboBox ApplyFilter 重置选中（🟡）
**位置**：`ZUIWidgets.h ComboBox::ApplyFilter`。过滤后：原选中项若仍在结果里 → 保留其索引；否则才回 0。

## L11：ComboBox justExpanded_ 未生效（🟢）
**位置**：`ZUIWidgets.h ComboBox::OnMouseDown`。展开那一帧用 `justExpanded_` 跳过 `PlaceCaretFromX`。

## L12：CaptionButton 按下移出松手不 Click（🟢）
**位置**：`ZUIWindowTool.h CaptionButton::OnMouseLeave/OnMouseUp`。移出只清 `hovered_`（保留 `pressed_`）；`OnMouseUp` 里 `if (pressed_ && Contains(x,y)) Clicked.Fire();`。

## D6：ListView::Draw 偏移不一致（🟢）
**位置**：`ZDataViewer.h ListView::Draw`。`firstVisible` 用 `Snap(scrollOffsetY_)` 计算，与 itemRect 的 Snap 偏移统一。

---

# 第 1 期续：绘制/事件/资源（批次 2）

## E1：OnMouseDown 未过滤 disabled（🟡）
**位置**：`ZUI.h Window::OnMouseDown`。命中后 `if (!hit->IsEffectivelyEnabled()) { 交给父/跳过; }`，与 `UpdateHover` 保持一致。

## E2：ZSignal::Fire tlSnapshot 常驻（🟢）
**位置**：`ZUI.h ZSignal::Fire`(~450)。Fire 结束 `tlSnapshot.clear();`（释放最后一次快照）。

## D3：表头分隔线画两次（🟢）
**位置**：`ZDataViewer.h TableView::Draw / TreeView::Draw`。删末尾重复画的分隔线。

## D4：DrawTextWithEllipsis 每次建 layout（🟡）
**位置**：`ZUIWidgets.h DrawTextWithEllipsis`。加一个小 LRU 缓存（key = `文本 + width + fontSize`，容量 64），命中直接 `DrawTextLayout`。

## D5：DrawScrollBar 除零（🟡）
**位置**：`ZDataViewer.h` 三处 `DrawScrollBar`。进入前 `if (maxScrollY_ <= 0.0f) return;`（X 同理）。

## R2：ImageDeviceCache key 裸指针（🟡）
**位置**：`ZUIImages.h ImageDeviceCache`。key 由 `ID2D1RenderTarget*` 改为 `{deviceEpoch_, renderTarget*}`；`DeviceReset` 时 `++deviceEpoch_` 并清空。

## R3：TreeNode::parent 裸指针（🟡）
**位置**：`ZDataViewer.h TreeNode`。`TreeNode* parent` → `std::weak_ptr<TreeNode>`（或节点持 id + map 查找）。

## R4：TreeView::checkAnim_ 悬垂 key（🟡）
**位置**：`ZDataViewer.h TreeView`。`checkAnim_` 的 key `TreeNode*` → `nodeId`；或 `TreeNode` 析构时从 map erase 自身。

### 剔除/降级
- **R1（MenuWindow 资源）**：非真 bug，剔除。
- **A5（GetChildren 返回可变成员）**：单线程无实际风险，剔除。
- **P7（100Hz 定时器）**：MenuWindow 专用，影响小，降级 🟢（第 3 期顺手）。

---

# 第 2 期：布局机制重构（高风险，一次到位）

## 目标
把"全量重排 + Arrange 内重复 Measure"改为 **DesiredSize 缓存 + MeasureOverride/ArrangeOverride + 惰性脏标记**。这是第 1 期之后、收益最大但风险最高的一期。

## 2.1 UIElement 新增字段与包装器（收口 A1/A2/A4/A5）

```cpp
// 成员（替换旧 layoutDirty_）
Size  desiredSize_{};
Size  previousAvailableSize_{ -1, -1 };
bool  measureDirty_ = true;
bool  arrangeDirty_ = true;

Size Measure(const Size& avail) {                    // 非虚；子类实现 MeasureOverride
    if (!measureDirty_ && avail.width  == previousAvailableSize_.width
                       && avail.height == previousAvailableSize_.height) return desiredSize_;
    desiredSize_ = MeasureOverride(avail);
    previousAvailableSize_ = avail;
    measureDirty_ = false;
    return desiredSize_;
}
Size GetDesiredSize() const { return desiredSize_; }
bool NeedsLayout() const { return measureDirty_ || arrangeDirty_; }

void Arrange(const Rect& r) {
    bool rectChanged = !(r.x==arrangedRect_.x && r.y==arrangedRect_.y
                      && r.width==arrangedRect_.width && r.height==arrangedRect_.height);
    if (!arrangeDirty_ && !rectChanged) return;      // ★ A1：绝不无条件标子脏
    if (r.width != desiredSize_.width || r.height != desiredSize_.height)
        Measure(Size(r.width, r.height));            // Fill/Stretch/换行 → 按最终尺寸重测（命中缓存 O(1)）
    ArrangeOverride(r);
    arrangedRect_ = r; arrangeDirty_ = false;
    cacheValid_ = false;                             // ★ A2：真重排 → 自身缓存作废
}
```

- **A1**：删掉 `dirtySubtree_` 与"每帧标子脏"；"父变→子重测"由 `Arrange` 里的 `Measure(最终尺寸)` 承担（只有尺寸真的不同才测）。
- **A2**：缓存作废逐元素、在包装器内做，不需要集合/去重/`ClearAllCaches`（设备重建仍走 `ReleaseDeviceResources`）。
- **A4**：入口改用 `NeedsLayout()`；删 `IsLayoutDirty()`。
- **A5**：删 `ClearLayoutDirty()`（含递归版）与 `layoutDirty_`。

## 2.2 失效传播（收口 A3）

```cpp
void InvalidateMeasure() {
    for (UIElement* e = this; e; e = e->parent_) {
        if (e->measureDirty_) break;                 // ★ 已脏即停 → 均摊 O(1)
        e->measureDirty_ = true; e->arrangeDirty_ = true;
    }
    if (Window* w = GetWindow()) w->MarkLayoutInvalidated();
}
void InvalidateArrange() {
    for (UIElement* e = this; e; e = e->parent_) {
        if (e->arrangeDirty_) break;
        e->arrangeDirty_ = true;
    }
    if (Window* w = GetWindow()) w->MarkLayoutInvalidated();
}
void InvalidateLayout() { InvalidateMeasure(); }     // 过渡期统一按 Measure（安全）
```

- **A3**：`SetParent` 不传播（保持只设 `parent_`）；`AddChild/InsertChild/RemoveChild` 末尾 `if (GetWindow()) InvalidateMeasure();`（建树期零成本）；`AttachWindowRecursive` 成功后 `InvalidateMeasure();`。
- 不变式：`measureDirty_==true ⇒ 祖先也 true`（每次置脏都冒泡、测完自清）→ "遇脏即停"安全。

## 2.3 OnPaint 接入（收口 A2/A4）

```cpp
// ZUI.h:4184 附近
if (layoutInvalidated_ || layoutNeeded_ || (rootElement_ && rootElement_->NeedsLayout())) {
    if (rootElement_) { rootElement_->Measure(Size(availWidth, availHeight));
                        rootElement_->Arrange(Rect(left, top, availWidth, availHeight)); }
    layoutNeeded_ = false; layoutInvalidated_ = false;
    // ★ 不再 ClearAllCaches() / CollectVisibleCachedElements()
}
```

## 2.4 改名：22 个类 `Measure→MeasureOverride`（安全网：纯虚）

**防漏机制**：`Size MeasureOverride(const Size&) = 0;` 设为纯虚 → 任何漏改的类**编译失败**（不会静默走空实现返回 {0,0} 塌布局）。`ArrangeOverride` 给基类默认实现。

| 文件 | 类（Measure/Arrange 行） |
|---|---|
| ZUI.h | ColumnBox(1055/1069)、RowBox(1147/1161)、GridLayout(1277/1305)、Card(1570/1576)、Page(1677/1683)、PageHost(1790/1802) |
| ZUIWidgets.h | Label(186/250)、Button(483/485)、TextBox(722)、ToggleSwitch(2248/2238)、ScrollBar(2488/2493)、ScrollViewer(2709/2722)、ProgressBar(3080/3082)、Slider(3264)、CheckBox(3511/3521) |
| ZDataViewer.h | ListView(429/431)、TableView(1533/1535)、TreeView(3098/3102) |
| ZUIWindowTool.h | CaptionButton(74)、TitleBar(228/233) |

> 改名用机械替换：`Size Measure(const Size& availableSize) override` → `Size MeasureOverride(const Size& availableSize) override`；`void Arrange(const Rect& finalRect) override` → `void ArrangeOverride(const Rect& finalRect) override`。改完全量搜索 `Size Measure(` / `void Arrange(` 兜底。

## 2.5 去掉 Arrange 内的 Measure（典型 3 例，其余照做）

**① ColumnBox（ZUI.h:1080）**
```cpp
// 前
Size childSize = child->Measure(Size(childW, FLT_MAX));
float childH = child->GetHeight() > 0 ? child->GetHeight() : childSize.height;
// 后
float childH = child->GetHeight() > 0 ? child->GetHeight() : child->GetDesiredSize().height;
```
**② GridLayout（ZUI.h:1319）**：删 `item.element->Measure(Size(FLT_MAX,FLT_MAX))`，用 Measure 阶段缓存的 `rowMinHeights_/colMinWidths_` 或 `GetDesiredSize()`。
**③ Card / ScrollViewer（ZUIWidgets.h:233 / 2711）**：同样删 Arrange 内 `Measure`，改 `GetDesiredSize()`。

> ⚠️ **5.3 约束传递（ColumnBox 传"剩余高度"）明确不做**，避免换行/滚动行为变化。

## 2.6 验证要点（第 2 期必做）
1. 全量重排一次后，动画/悬停应**不再**触发根 Measure/Arrange（用日志计数验证）。
2. 逐页目视：切页、resize、滚动、文本框输入、列表增删，布局不得塌陷/错位。
3. 内存：切页峰值应显著回落（对比第 1 期后基线）。

---

# 第 3 期：性能收尾

## M5：PageHost 双通道绘制（🔴）
**位置**：`ZUI.h:1807 PageHost::Draw` + `1843 GetChildren` + `1998 GetChildRenderTransform`。
**问题**：`Draw` 里手动 SetTransform 画两页，`ComposeImpl` 又通过 `GetChildren` 递归 + `GetChildRenderTransform` 再画一次 → 同帧双绘、变换叠加。
**改法**：`PageHost::Draw` 改为空实现（`void Draw(ID2D1RenderTarget*) override {}`），只靠 `GetChildRenderTransform` 通道；`Draw` 里的 `PushAxisAlignedClip` 保留到 `GetClipRect`(1862) 已返回 `arrangedRect_`，无需在 Draw 里做。

## P6/A6：OnPaint 每帧全树遍历事件化（🔴）
**位置**：`ZUI.h:4205 CollectDragRegions` / `4216-4228 CollectActiveAnimations` / `1938-1942 PageHost 每帧 RequestRepaint`。
**改法**：
- `CollectDragRegions`：只在 `WM_NCHITTEST` 按需收集；或尺寸/结构变化时重建一次并缓存。
- `CollectActiveAnimations`：控件动画开始/结束调用 `Window::RegisterActiveAnimation(elem)` / `Unregister`，OnPaint 直接遍历 `activeAnims_`（O(活跃数)）。
- `PageHost` 过渡期间对两页的 `RequestRepaint()` 改为只标真正变化的元素。

## 批次 3（性能细节）
- **P1**：`ListView::AddItem/InsertItem` 加 `BeginUpdate/EndUpdate` 批量挂起。
- **P2**：`TreeView::FindNode` 建 id→node 映射。
- **P3**：`TreeView::GetVisibleIndex` 文档内记 index。
- **P4**：`TreeView::BuildVisibleList` 增量更新。
- **P7**：`MenuWindow::HandleAnimationTimer`（10ms 定时器）提高步长或改 DWM 动画（🟢，顺手）。

---

# 附：自我校验结论（三方评审收口）

1. 保守版脏布局：**父 rect 变 → 子强制 Measure+Arrange**（已写入 2.1 的 `Arrange` 包装器）。
2. 前置：先重构 Layout 分离 Measure/Arrange（已列为第 2 期 2.1–2.5）。
3. `dirtySubtree_` 冗余，已删；`ClearLayoutDirty` 与 `layoutDirty_` 一并删。
4. 传播触发点：`InvalidateMeasure/Arrange` 冒泡 + 结构变化仅在"已挂窗口"时冒泡。
5. 5.3 约束传递不在本清单。
6. 改名以 `MeasureOverride = 0` 纯虚做安全网，防静默塌布局。
