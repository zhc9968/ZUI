# ZUI

> 基于 Direct2D 的 C++ 原生 Win32 自绘 UI 框架，含布局、控件、信号槽与高 DPI 适配。

> 语言：**简体中文** · [English](README.en.md)

ZUI 是一个**纯头文件**的 Windows 桌面 UI 框架，直接建立在 Direct2D / DirectWrite / DWM 之上。它不依赖 Qt、MFC 或任何第三方库，把控件、布局、动画、字体、数据视图和信号槽都装进几个 `.h` 里，适合想要轻量、可控、现代观感的原生 C++ 应用。

- 仓库：GitHub <https://github.com/zhc9968/ZUI> · Gitee <https://gitee.com/zhc9968/zui>
- 许可证：MIT
- 完整 API 文档：[docs/API.md](docs/API.md)

## 特性

###block_list_start
###block_item_start
**Direct2D 自绘**
硬件加速渲染，亚克力（Acrylic）背景、圆角、阴影，观感现代。
###block_item_end
###block_item_start
**完整布局系统**
`ColumnBox` / `RowBox` / `GridLayout`，支持间距、拉伸权重、填充、对齐与跨行跨列。
###block_item_end
###block_item_start
**信号槽**
内置 `ZSignal` / `Connection`，支持 `Connect` 自动管理生命周期，以及当前线程 / 新线程 / UI 线程三种分发策略。
###block_item_end
###block_item_start
**丰富控件**
标签、按钮、文本框、下拉框、开关、滚动容器、进度条、滑块，以及列表 / 表格 / 树三种数据视图。
###block_item_end
###block_item_start
**完整动画与转场**
悬停、展开、指示条、页面切换（`PageHost`）都有内置动画，滚动支持平滑滚动。
###block_item_end
###block_item_start
**高 DPI 适配**
自动感知 DPI，`Snap()` 把绘制吸附到物理像素，避免模糊。
###block_item_end
###block_item_start
**IME 兼容**
文本框支持中文输入法组合输入与候选框定位。
###block_item_end
###block_item_start
**离屏缓存**
元素可自动缓存绘制结果，减少重复绘制开销。
###block_item_end
###block_list_end

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

###block_orange_start
`ZUI.cpp` 是一个综合演示程序（左侧导航 + 多个测试页），同时也充当框架的“冒烟测试”。真正要做自己的应用时，只需要包含头文件并写自己的 `WinMain`。
###block_orange_end

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

## 文档

- 详细 API（每个类、方法、信号、默认值）：[docs/API.md](docs/API.md)

## 许可证

本项目采用 **MIT** 许可证，详见 [LICENSE](LICENSE)。
