# ZUI

> 基于 Direct2D 的 C++ 原生 Win32 自绘 UI 框架，含布局、控件、信号槽与高 DPI 适配。

> 语言：**简体中文** · [English](README.en.md)

ZUI 是一个**纯头文件**的 Windows 桌面 UI 框架，直接建立在 Direct2D / DirectWrite / DWM 之上。它不依赖 Qt、MFC 或任何第三方库，把控件、布局、动画、字体、数据视图和信号槽都装进几个 `.h` 里，适合想要轻量、可控、现代观感的原生 C++ 应用。

- 仓库：GitHub <https://github.com/zhc9968/ZUI> · Gitee <https://gitee.com/zhc9968/zui>
- 许可证：MIT
- 在线文档：<https://zhc9968.github.io/ZUI/>
- API 参考：<https://zhc9968.github.io/ZUI/docs/API.html>

## 特性

- **Direct2D 自绘**：硬件加速渲染，亚克力（Acrylic）背景、圆角、阴影，观感现代。
- **完整布局系统**：`ColumnBox` / `RowBox` / `GridLayout`，支持间距、拉伸权重、填充、对齐与跨行跨列。
- **信号槽**：内置 `ZSignal` / `Connection`，支持 `Connect` 自动管理生命周期，以及当前线程 / 新线程 / UI 线程三种分发策略。
- **丰富控件**：标签、按钮、文本框、下拉框、开关、滚动容器、进度条、滑块，以及列表 / 表格 / 树三种数据视图。
- **完整动画与转场**：悬停、展开、指示条、页面切换（`PageHost`）都有内置动画，滚动支持平滑滚动。
- **高 DPI 适配**：自动感知 DPI，`Snap()` 把绘制吸附到物理像素，避免模糊。
- **IME 兼容**：文本框支持中文输入法组合输入与候选框定位。
- **离屏缓存**：元素可自动缓存绘制结果，减少重复绘制开销。

## 环境要求

| 项 | 要求 |
| --- | --- |
| 操作系统 | Windows 10 / 11 |
| 编译器 | Visual Studio（需 **v145** 工具集），C++17 及以上（Release x64 使用 C++20） |
| Windows SDK | 10.0 及以上 |
| 运行库 | 系统自带 `d2d1` / `dwrite` / `dwmapi` / `imm32`（无需额外安装） |
| 第三方依赖 | 无 |

## 构建

1. 用 Visual Studio 打开解决方案 `ZUI.slnx`；
2. 选择配置 **Release | x64**；
3. 生成解决方案（Build Solution）；
4. 产物位于 `x64\Release\ZUI.exe`，直接运行即可。

命令行构建（需要 MSBuild 在 PATH 中）：

```bash
msbuild ZUI.slnx /p:Configuration=Release /p:Platform=x64
```

> `ZUI.cpp` 是一个综合演示程序（左侧导航 + 多个测试页），同时也充当框架的“冒烟测试”。真正要做自己的应用时，只需要包含头文件并写自己的 `WinMain`。

## 项目结构

```text
ZUI/
├── ZUI.h              # 核心：类型 / 信号槽 / 字体 / 元素 / 布局 / 菜单 / 窗口
├── ZUIWidgets.h       # 基础控件：Label / Button / TextBox / ComboBox / ToggleSwitch / ScrollViewer / ProgressBar / Slider
├── ZDataViewer.h      # 数据视图：ListView / TableView / TreeView
├── ZUI.cpp            # 综合演示程序入口（WinMain）
├── ZUI.slnx           # 解决方案
├── ZUI.vcxproj        # 工程文件
└── docs/
    └── API.md         # 完整 API 文档
```

## 快速开始

```cpp
#include "ZUIWidgets.h"
using namespace ZUI;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Window win;
    if (!win.Create(1000, 700, L"ZUI Demo"))
        return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

    auto row = std::make_shared<RowBox>();
    row->SetSpacing(10);

    auto btn = std::make_shared<Button>(L"点我");
    btn->Connect(btn->Clicked, []() {
        MessageBoxW(nullptr, L"Hello ZUI!", L"提示", MB_OK);
    });

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto state = std::make_shared<Label>(L"开关：关");
    toggle->Connect(toggle->Toggled, [state](bool on) {
        state->SetText(on ? L"开关：开" : L"开关：关");
    });

    row->AddChild(btn);
    row->AddChild(toggle);
    row->AddChild(state);
    root->AddChild(row);

    win.Run();
    return 0;
}
```

要点：

- `Window::Create` 会创建窗口、初始化 Direct2D、应用亚克力背景并生成一个默认的根 `ColumnBox`；
- 用 `std::make_shared<T>()` 创建控件，`AddChild` 挂到布局上；
- 用 `Connect(信号, 槽)` 绑定事件，连接会随控件析构自动断开；
- 最后调用 `win.Run()` 进入消息循环。

## 更新日志

### 2026-09-13 — 大优化：交互状态、阴影 / ToolTip、控件与数据视图增强

**核心框架**

- `UIElement` 新增：启用/禁用（`SetEnabled/IsEnabled/IsEffectivelyEnabled`，禁用状态沿父链继承、拦截鼠标键盘、控件自行置灰）、通用 ToolTip（`SetToolTip/GetToolTip`）、阴影（`SetShadow/SetShadowColor/SetShadowBlur/SetShadowOffset/SetShadowCornerRadius/GetShadowExtent`）、右键菜单钩子 `OnContextMenu`。
- `Window`：元素阴影合成进离屏缓存；统一 ToolTip 浮层（锚定“显示时鼠标位置”上方固定偏移、悬停 0.5s 渐显、白底黑字带柔和阴影、鼠标移动即关闭）；Tab 焦点遍历（焦点环仅在 Tab 导航时显示，鼠标点击不显示）。
- 阴影重写为 **高斯 CDF 分层**（`DrawSoftShadow`）：按高斯分布分配每层 alpha，使叠加结果逼近 `targetA·(1-Φ(d/σ))`；`SetShadowColor` 的 alpha 语义为“边缘可见透明度”，内部约为其 2 倍。
- 帧时间钳制：`OnPaint` 的 `deltaTime` 上限 `0.033s`，修复空闲 / 最小化恢复后“动画一帧跳到终点”的问题。
- `PageHost` 修复切页动画期间同一页面每帧被 `UpdateAnimation` 两次、导致页面内嵌动画速度翻倍的问题。
- 定时器精度：窗口创建 `timeBeginPeriod(1)`、销毁 `timeEndPeriod(1)`，降低动画抖动。
- 调试输出统一由宏 `ZUI_DEBUG` 控制（默认关闭，定义后启用），Release 热路径不再有调试字符串构造。
- 性能 / 内存：`GetChildren()` 改为返回 `const&`（复用缓冲，消除每帧每节点分配）；`ZSignal::Fire` 用线程本地快照；`Compose` 增加裁剪剔除（完全在裁剪区外的子树直接跳过）；活跃动画集合复用缓冲；`DrawSoftShadow` 用定长数组避免每帧堆分配。

**基础控件**

- `Button`：禁用态、Enter/Space 键盘触发、可切换（`SetCheckable/SetChecked/IsChecked/Toggled`）、文字对齐 / 内边距、自动重复。
- `CheckBox`：悬停光晕动画、文字标签与颜色、悬停框色、键盘、禁用。
- `ToggleSwitch`：禁用、文字标签、键盘、`SetSize`、不确定态、自定义颜色。
- `Label`：内边距、行距、最大行数（超出省略）、`GetDesiredSize`、禁用色。
- `ProgressBar`：`ValueChanged`、范围、显示百分比文本与文字颜色、禁用。
- `Slider`：`SliderReleased`、步进 / 吸附（`SetStep/SetSnapToStep`）、方向键 / Home / End、禁用。
- `ScrollViewer`：滚动条可见性策略（`Auto/Always/Hidden`）、`ScrollChanged`、`GetScrollOffset`、实例颜色、`ContentMargin`。
- `TextBox`：只读（`SetReadOnly`）、输入过滤（`SetInputFilter`）、`ReturnPressed`、公开选区 / 撤销 / 复制粘贴 / 全选、占位符颜色、密码显隐（`SetRevealPassword`）；修复 Shift 与鼠标拖选“选区不累积”的问题（引入独立锚点）。
- `ComboBox`：数据增删查、占位符、每项禁用、最大可见项、开合信号（`DropDownOpened/DropDownClosed`）、**可编辑 + 输入过滤**（`SetEditable/SetFilterEnabled/SetEditText`，带闪烁光标与点击定位）。

**数据视图**

- `ListView`：选择模式新增 `None`；首字母定位（type-ahead）；排序回调 + 排序指示；每项禁用 / 单独文字色 / ToolTip（**按项目绑定，排序后仍然跟随原项目**）；键盘上下键跳过禁用项。
- `TableView`：按列排序 + 排序指示（排序 / 增删行列时行级元数据随行重映射）；单元格文字色 / ToolTip；按行禁用；列隐藏；列对齐；按行高度；键盘上下键跳过禁用行。
- `TreeView`：过滤 / 搜索、`GetNodePath`、默认展开深度；节点 `tooltip` 接入基础类的统一 ToolTip。

**文档**

- 新增本更新日志；API 文档从“定义罗列”改为更详细的“实现要点 + 易混点”风格；补充信号与对象生命周期的引用环警示。

### 2026-09-13（续）— 多窗口支持（Application）

- 新增 `ZUI::Application`：`app.CreateWindow(...)` 创建窗口、`app.Run()` 运行单一共享消息循环，Qt 风格；窗口之间真正独立。
- 重绘 / 布局按所属窗口路由（`UIElement::GetWindow()`）；DPI 缩放改为**线程本地**（每个窗口渲染前设置自己的缩放）；`Window` 新增实例信号 `Activated` / `Deactivated` / `Closed`，全局 `UIZSignals::WindowDeactivated` 与 `GlobalMouseDown` 增加 `Window*` 参数。
- 关闭单个窗口不再退出应用，**最后一个窗口关闭才退出**；`ID2D1Factory` 与 `timeBeginPeriod` 由应用核心共享。
- 向后兼容：单窗口 `Window win; win.Create(...); win.Run();` 仍可用。`#undef CreateWindow` 以避免与 Win32 宏冲突。

## 文档

- 在线文档站点：<https://zhc9968.github.io/ZUI/>
- API 参考（中文，13 章）：<https://zhc9968.github.io/ZUI/docs/API.html>
- API Reference (English)：<https://zhc9968.github.io/ZUI/docs/API.en.html>

## 许可证

本项目采用 **MIT** 许可证，详见 [LICENSE](LICENSE)。
