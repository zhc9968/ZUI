# ZUI

> A C++ native Win32 custom-drawn UI framework based on Direct2D, with layout, controls, signals/slots, and high-DPI adaptation.

ZUI is a **header-only** Windows desktop UI framework built directly on Direct2D / DirectWrite / DWM. It does not depend on Qt, MFC, or any third-party library; it packs controls, layout, animation, fonts, data views, and signals/slots into a handful of `.h` files, making it suitable for native C++ applications that want a lightweight, controllable, modern look.

- Repository: <https://gitee.com/zhc9968/zui>
- License: MIT
- Full API documentation: [docs/API.md](docs/API.md)

## Features

###block_list_start
###block_item_start
**Direct2D custom drawing**
Hardware-accelerated rendering, Acrylic background, rounded corners, and shadows for a modern look.
###block_item_end
###block_item_start
**Complete layout system**
`ColumnBox` / `RowBox` / `GridLayout`, supporting spacing, stretch weight, fill, alignment, and row/column spanning.
###block_item_end
###block_item_start
**Signals and slots**
Built-in `ZSignal` / `Connection`, supporting `Connect` with automatic lifetime management, plus three dispatch strategies: current thread / new thread / UI thread.
###block_item_end
###block_item_start
**Rich controls**
Labels, buttons, text boxes, combo boxes, toggle switches, scroll containers, progress bars, sliders, and three data views: list / table / tree.
###block_item_end
###block_item_start
**Complete animation and transitions**
Hover, expand, indicator bar, and page switching (`PageHost`) all have built-in animations, and scrolling supports smooth scrolling.
###block_item_end
###block_item_start
**High-DPI adaptation**
Automatically aware of DPI; `Snap()` snaps drawing to physical pixels to avoid blurriness.
###block_item_end
###block_item_start
**IME compatibility**
Text boxes support Chinese IME composition input and candidate window positioning.
###block_item_end
###block_item_start
**Offscreen caching**
Elements can automatically cache their drawing results to reduce repeated drawing overhead.
###block_item_end
###block_list_end

## Requirements

| Item | Requirement |
| --- | --- |
| Operating system | Windows 10 / 11 |
| Compiler | Visual Studio (requires the **v145** toolset), C++17 or later (Release x64 uses C++20) |
| Windows SDK | 10.0 or later |
| Runtime libraries | System-provided `d2d1` / `dwrite` / `dwmapi` / `imm32` (no additional installation required) |
| Third-party dependencies | None |

## Build

1. Open the solution `ZUI.slnx` with Visual Studio;
2. Select the **Release | x64** configuration;
3. Build the solution (Build Solution);
4. The output is located at `x64\Release\ZUI.exe`; run it directly.

Command-line build (requires MSBuild on PATH):

```bash
msbuild ZUI.slnx /p:Configuration=Release /p:Platform=x64
```

###block_orange_start
`ZUI.cpp` is a comprehensive demo program (left-side navigation + multiple test pages) that also serves as the framework's "smoke test". When building your own application, you only need to include the headers and write your own `WinMain`.
###block_orange_end

## Project structure

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

## Quick start

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

Key points:

- `Window::Create` creates the window, initializes Direct2D, applies the Acrylic background, and generates a default root `ColumnBox`;
- Create controls with `std::make_shared<T>()` and attach them to the layout with `AddChild`;
- Bind events with `Connect(signal, slot)`; connections are automatically disconnected when the control is destroyed;
- Finally, call `win.Run()` to enter the message loop.

## Documentation

- Detailed API (every class, method, signal, and default value): [docs/API.md](docs/API.md)

## License

This project is licensed under the **MIT** License; see [LICENSE](LICENSE) for details.
