# ZUI

> A C++ native Win32 custom-drawn UI framework based on Direct2D, with layout, controls, signals/slots, and high-DPI adaptation.

> Language: **English** · [简体中文](README.md)

ZUI is a **header-only** Windows desktop UI framework built directly on Direct2D / DirectWrite / DWM. It does not depend on Qt, MFC, or any third-party library; it packs controls, layout, animation, fonts, data views, and signals/slots into a handful of `.h` files, making it suitable for native C++ applications that want a lightweight, controllable, modern look.

- Repository: GitHub <https://github.com/zhc9968/ZUI> · Gitee <https://gitee.com/zhc9968/zui>
- License: MIT
- Online docs: <https://zhc9968.github.io/ZUI/>
- API Reference (English): <https://zhc9968.github.io/ZUI/docs/API.en.html>

## Features

- **Direct2D custom drawing**: hardware-accelerated rendering, Acrylic background, rounded corners, and shadows for a modern look.
- **Complete layout system**: `ColumnBox` / `RowBox` / `GridLayout`, supporting spacing, stretch weight, fill, alignment, and row/column spanning.
- **Signals and slots**: built-in `ZSignal` / `Connection`, supporting `Connect` with automatic lifetime management, plus three dispatch strategies: current thread / new thread / UI thread.
- **Rich controls**: labels, buttons, text boxes, combo boxes, toggle switches, scroll containers, progress bars, sliders, and three data views: list / table / tree.
- **Complete animation and transitions**: hover, expand, indicator bar, and page switching (`PageHost`) all have built-in animations, and scrolling supports smooth scrolling.
- **High-DPI adaptation**: automatically aware of DPI; `Snap()` snaps drawing to physical pixels to avoid blurriness.
- **IME compatibility**: text boxes support Chinese IME composition input and candidate window positioning.
- **Offscreen caching**: elements can automatically cache their drawing results to reduce repeated drawing overhead.

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

> `ZUI.cpp` is a comprehensive demo program (left-side navigation + multiple test pages) that also serves as the framework's "smoke test". When building your own application, you only need to include the headers and write your own `WinMain`.

## Project structure

```text
ZUI/
├── ZUI.h              # Core: types / signals & slots / fonts / elements / layout / menus / window
├── ZUIWidgets.h       # Basic controls: Label / Button / TextBox / ComboBox / ToggleSwitch / ScrollViewer / ProgressBar / Slider
├── ZDataViewer.h      # Data views: ListView / TableView / TreeView
├── ZUI.cpp            # Demo program entry point (WinMain)
├── ZUI.slnx           # Solution
├── ZUI.vcxproj        # Project file
└── docs/
    └── API.en.md      # Full API documentation
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

    auto btn = std::make_shared<Button>(L"Click me");
    btn->Connect(btn->Clicked, []() {
        MessageBoxW(nullptr, L"Hello ZUI!", L"Info", MB_OK);
    });

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto state = std::make_shared<Label>(L"Switch: off");
    toggle->Connect(toggle->Toggled, [state](bool on) {
        state->SetText(on ? L"Switch: on" : L"Switch: off");
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

- Online documentation: <https://zhc9968.github.io/ZUI/>
- API Reference (English, 12 chapters): <https://zhc9968.github.io/ZUI/docs/API.en.html>
- API 参考（中文）: <https://zhc9968.github.io/ZUI/docs/API.html>

## License

This project is licensed under the **MIT** License; see [LICENSE](LICENSE) for details.
