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

## Changelog

### 2026-09-13 — Major update: interaction states, shadows / tooltips, controls and data views

**Core**

- `UIElement` gained: enabled/disabled (`SetEnabled/IsEnabled/IsEffectivelyEnabled` — disabled state inherits from the parent chain, blocks mouse/keyboard input, and controls grey themselves), generic tooltips (`SetToolTip/GetToolTip`), shadows (`SetShadow/SetShadowColor/SetShadowBlur/SetShadowOffset/SetShadowCornerRadius/GetShadowExtent`), and a context-menu hook `OnContextMenu`.
- `Window`: element shadows are composited into the offscreen cache; a shared tooltip overlay (anchored at a fixed offset above the mouse position when shown, fades in after a 0.5s hover, white background with black text and a soft shadow, dismissed as soon as the mouse moves); Tab focus traversal (the focus ring is shown only for Tab navigation, not for mouse clicks).
- Shadows rewritten as **layered Gaussian CDF** (`DrawSoftShadow`): per-layer alpha is derived from the Gaussian distribution so the composite approximates `targetA·(1-Φ(d/σ))`; the alpha of `SetShadowColor` now means the *visible edge* opacity (the interior is roughly 2×).
- Frame-time clamping: `deltaTime` in `OnPaint` is capped at `0.033s`, fixing animations that jumped straight to their end after an idle period or a restore from minimize.
- `PageHost`: fixed a bug where, during a page transition, the same page was updated twice per frame, doubling the speed of animations nested inside it.
- Timer precision: `timeBeginPeriod(1)` on window creation and `timeEndPeriod(1)` on destruction reduce animation jitter.
- Debug output is now controlled by the `ZUI_DEBUG` macro (off by default; define it to enable); Release hot paths no longer build debug strings.
- Performance / memory: `GetChildren()` now returns `const&` (reused buffer, no per-frame allocation); `ZSignal::Fire` uses a thread-local snapshot; `Compose` culls subtrees entirely outside the clip; the active-animation set reuses a buffer; `DrawSoftShadow` uses fixed-size arrays to avoid per-frame heap allocation.

**Basic controls**

- `Button`: disabled state, Enter/Space activation, checkable mode (`SetCheckable/SetChecked/IsChecked/Toggled`), text alignment / padding, auto-repeat.
- `CheckBox`: hover halo animation, text label and color, hover box color, keyboard, disabled.
- `ToggleSwitch`: disabled, text label, keyboard, `SetSize`, indeterminate state, custom colors.
- `Label`: padding, line spacing, max lines (with ellipsis), `GetDesiredSize`, disabled color.
- `ProgressBar`: `ValueChanged`, range, percentage text with text color, disabled.
- `Slider`: `SliderReleased`, stepping / snapping (`SetStep/SetSnapToStep`), arrow keys / Home / End, disabled.
- `ScrollViewer`: scrollbar visibility policy (`Auto/Always/Hidden`), `ScrollChanged`, `GetScrollOffset`, per-instance colors, `ContentMargin`.
- `TextBox`: read-only (`SetReadOnly`), input filter (`SetInputFilter`), `ReturnPressed`, public selection / undo / copy-paste / select-all, placeholder color, password reveal (`SetRevealPassword`); fixed Shift and mouse-drag selection not accumulating (independent anchor).
- `ComboBox`: data add/remove/query, placeholder, per-item disabled, max visible items, open/close signals (`DropDownOpened/DropDownClosed`), **editable + input filtering** (`SetEditable/SetFilterEnabled/SetEditText`, with a blinking caret and click positioning).

**Data views**

- `ListView`: new `None` selection mode; type-ahead; sort comparator + indicator; per-item disabled / text color / tooltip (**bound to the item itself, so they survive sorting**); keyboard navigation skips disabled items.
- `TableView`: per-column sorting + indicator (row-level metadata is remapped when sorting or inserting/removing rows/columns); cell text color / tooltip; per-row disabled; column hiding; column alignment; per-row height; keyboard navigation skips disabled rows.
- `TreeView`: filter / search, `GetNodePath`, default expand depth; per-node `tooltip` wired to the shared tooltip in the base class.

**Docs**

- Added this changelog; the API docs were rewritten from a "list of signatures" into a more detailed "implementation notes + pitfalls" style; added a warning about signal/object lifetime reference cycles.

### 2026-09-13 (cont.) — Multi-window support (`Application`)

- New `ZUI::Application`: `app.CreateWindow(...)` to create windows and `app.Run()` for a single shared message loop, Qt style; windows are truly independent.
- Repaint/layout are routed per owning window (`UIElement::GetWindow()`); the DPI scale is now **thread-local** (set per window before drawing); `Window` gained instance signals `Activated` / `Deactivated` / `Closed`, and `UIZSignals::WindowDeactivated` / `GlobalMouseDown` now carry a `Window*`.
- Closing one window no longer quits the app; it quits when the **last** window closes. The `ID2D1Factory` and `timeBeginPeriod` are shared by the app core.
- All window-related global signals now carry a `Window*` (`DrawOverlay` / `GlobalMouseDown` / `WindowDeactivated`, plus capture and repaint fallbacks); subscribers filter with `GetWindow()`. This fixes the cross-talk where window A's popup was drawn onto window B or clicking B wrongly collapsed A. Added `Window::SetPosition/SetSize/IsValid`.
- Fixed ComboBoxes holding the thread-wide system mouse capture after expanding (which made other windows unusable): Win32 capture is now held only while the mouse button is down and released on mouse-up (element-level logical capture is unaffected); also handled `WM_CAPTURECHANGED`.
- Added modal and owned windows: `Window::SetOwner` / `GetOwner`, `Window::RunModal(owner)`, `Application::CreateWindow(..., owner)`; while modal, clicking the disabled owner **flashes** the modal window.
- Robustness: elements store the owning window as an **id** (instead of a raw pointer); after the window is destroyed `GetWindow()` returns nullptr, removing crashes from dangling window pointers.
- Backward compatible: `Window win; win.Create(...); win.Run();` still works. `#undef CreateWindow` avoids the Win32 macro clash.

## Documentation

- Online documentation: <https://zhc9968.github.io/ZUI/>
- API Reference (English, 13 chapters): <https://zhc9968.github.io/ZUI/docs/API.en.html>
- API 参考（中文）: <https://zhc9968.github.io/ZUI/docs/API.html>

## License

This project is licensed under the **MIT** License; see [LICENSE](LICENSE) for details.
