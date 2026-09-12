# ZUI API Reference

> This document covers the complete public API of the ZUI framework: core types, signals and slots, fonts, elements, layout, windows, basic controls, and data views.
> Companion reading: [Project description and build](../README.md).

###chapter: Conventions | Namespace, units, lifetime, and default-value conventions

- Everything lives in `namespace ZUI`.
- Coordinates and sizes are in **DIP** (device-independent pixels); before rendering they are snapped to physical pixels by `Snap()`.
- Controls are held via `std::shared_ptr<T>` and attached through a layout's `AddChild()`.
- Events are bound with `Connect(signal, slot)`, which returns a `Connection` that is automatically disconnected when its owner is destroyed.
- The `inline static` defaults of each class can be changed globally through the corresponding `static SetDefault*()`, affecting instances created afterward.


###chapter: Basic Types | Color, Rect, Thickness, Size

## Color

```cpp
struct Color {
    float r, g, b, a;
    Color(float r = 0, float g = 0, float b = 0, float a = 1.0f);
    static Color FromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b);
    D2D1_COLOR_F ToD2D() const;
    static Color Lerp(const Color& c1, const Color& c2, float t);
};
```

| Member | Description |
| --- | --- |
| `Color(r,g,b,a)` | Components range over `[0,1]`; defaults to opaque black |
| `FromArgb(a,r,g,b)` | Constructs from components in the 0–255 range |
| `ToD2D()` | Converts to a Direct2D color structure |
| `Lerp(c1,c2,t)` | Linear interpolation |

## Rect

```cpp
struct Rect {
    float x, y, width, height;
    Rect(float x = 0, float y = 0, float w = 0, float h = 0);
    bool Contains(float px, float py) const;
    D2D1_RECT_F ToD2D() const;
};
```

## Thickness

```cpp
struct Thickness {
    float left, top, right, bottom;
    Thickness(float l = 0, float t = 0, float r = 0, float b = 0);
};
```

## Size

```cpp
struct Size {
    float width, height;
    Size(float w = 0, float h = 0);
};
```


###chapter: Global Functions and DPI | clamp, DPI scaling, and pixel snapping

```cpp
template<typename T> T clamp(T value, T low, T high);
inline float& GlobalDpiScaleRef();
inline void SetGlobalDpiScale(float scale);
inline float GetGlobalDpiScale();
inline float Snap(float dip);
```

| Function | Description |
| --- | --- |
| `clamp(v, low, high)` | Clamps to the interval (note that the argument order is reversed compared with `std::clamp`) |
| `SetGlobalDpiScale(s)` | Sets the global DPI scale; `s<=0` falls back to `1.0f` |
| `GetGlobalDpiScale()` | Reads the current scale, defaults to `1.0f` |
| `Snap(dip)` | Snaps a DIP value to the nearest physical pixel |


###chapter: Signals and Connections | ZSignal, Connection, ConnectionGroup, and global signals

## ConnectionThread

```cpp
enum class ConnectionThread {
    CurrentThread,   // 在触发线程直接执行（默认）
    NewThread,       // 每次触发新建分离线程
    UIThread         // 投递到 UI 线程消息循环
};
```

## Connection

```cpp
class Connection {
    Connection();
    Connection(std::shared_ptr<detail::ConnectionState> state);
    ~Connection();                         // 析构自动断开
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;
    void disconnect();
    bool isConnected() const;
};
```

## ConnectionGroup

```cpp
class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    ConnectionGroup();
    ~ConnectionGroup();                    // 析构时断开全部
    void disconnectAll();
    size_t size() const;
    void addState(const std::shared_ptr<detail::ConnectionState>& state);
};
```

## ZSignal

```cpp
template <typename... TArgs>
class ZSignal {
    using SlotType = std::function<void(TArgs...)>;
    Connection connect(SlotType slot,
                       ConnectionThread thread = ConnectionThread::CurrentThread,
                       std::shared_ptr<ConnectionGroup> group = nullptr);
    void Fire(TArgs... targs) const;
    void operator()(TArgs... targs) const;   // Fire 的语法糖
};
```

###block_green_start
**Recommended usage**: controls already provide `UIElement::Connect(signal, slot)`, which attaches the connection to that control's `ConnectionGroup` and disconnects it automatically when the control is destroyed. When using `signal.connect(...)` manually, pay attention to the lifetime of the connection.
###block_green_end

## Global Signals `UIZSignals`

| Signal | Parameters | Firing condition |
| --- | --- | --- |
| `DrawOverlay` | `ID2D1RenderTarget*` | Overlay drawing after all UI drawing has completed |
| `GlobalMouseDown` | `float, float` | Global mouse-down (DIP coordinates) |
| `WindowDeactivated` | — | Window deactivated/minimized |
| `ElementCaptureRequest` | `UIElement*` | A control requests mouse capture |
| `ElementCaptureRelease` | `UIElement*` | A control releases mouse capture |
| `RepaintRequest` | `UIElement*` | A control requests a repaint |
| `LayoutInvalidated` | — | Global layout invalidation |


###chapter: Fonts | FontSpec and FontManager

## FontSpec

```cpp
struct FontSpec {
    std::wstring familyName = L"Segoe UI";
    float size = 14.0f;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
    std::wstring locale = L"en-us";
    bool operator==(const FontSpec& other) const;
    bool operator!=(const FontSpec& other) const;
};
```

## FontManager (singleton)

| Method | Description |
| --- | --- |
| `static FontManager& Instance()` | Gets the singleton |
| `IDWriteFactory* GetFactory()` | Lazily loaded shared DWrite factory |
| `IDWriteTextFormat* GetFormat(const FontSpec& spec)` | Gets a shared format for a font spec (with caching) |
| `void SetGlobalFont(const FontSpec& spec)` | Sets the global font and fires `GlobalFontChanged` |
| `const FontSpec& GetGlobalFont() const` | Reads the global font |
| `ZSignal<> GlobalFontChanged` | Global font-changed signal |


###chapter: Core Element UIElement | Sizing, layout, drawing, events, and font interfaces

The base class of all visual elements. Custom controls must inherit from it and implement the pure virtual `Measure` and `Draw`.

## Layout properties

| Method | Description |
| --- | --- |
| `void SetMinWidth(float)` / `SetMinHeight(float)` | Minimum size |
| `void SetMaxWidth(float)` / `SetMaxHeight(float)` | Maximum size |
| `void SetMinSize(float w, float h)` / `SetMaxSize(float w, float h)` | Sets both at once |
| `float GetMinWidth()/GetMinHeight()/GetMaxWidth()/GetMaxHeight() const` | Reads them |
| `void SetFillWidth(bool)` / `SetFillHeight(bool)` | Whether to fill the parent container |
| `bool GetFillWidth()/GetFillHeight() const` | Reads them |
| `void SetWidth(float)` / `SetHeight(float)` | Fixed size |
| `float GetWidth()/GetHeight() const` | Reads them |
| `void SetMargin(const Thickness&)` / `Thickness GetMargin() const` | Outer margin |
| `void SetStretchWeights(float h, float v)` | Sets both horizontal/vertical stretch weights at once |
| `void SetHorizontalStretchWeight(float)` / `SetVerticalStretchWeight(float)` | Sets them individually |
| `float GetHorizontalStretchWeight()/GetVerticalStretchWeight() const` | Reads them |
| `void InvalidateLayout()` / `bool IsLayoutDirty() const` / `ClearLayoutDirty()` | Layout invalidation |

## Measure, arrange, and draw

| Method | Description |
| --- | --- |
| `virtual Size Measure(const Size& availableSize) = 0` | Measures (**must be implemented**) |
| `virtual void Arrange(const Rect& finalRect)` | Arranges |
| `Rect GetArrangedRect() const` | The arrange result |
| `virtual void Draw(ID2D1RenderTarget* rt) = 0` | Draws (**must be implemented**) |
| `virtual std::vector<UIElement*> GetChildren() const` | Child element list |
| `virtual bool UseCache() const` / `SetUseCache(bool)` | Whether to use an off-screen cache |
| `virtual std::optional<D2D1_RECT_F> GetClipRect() const` | Child clipping region |
| `void RequestRepaint()` | Requests a repaint |

## Event virtual functions (overridable)

`HitTest(x,y)`, `OnMouseEnter/Leave/Move/Down/Up`, `OnMouseWheel(dx,dy)`, `OnKeyDown/Up(key,lParam)`, `OnChar(ch)`, `OnFocus`, `OnBlur`, `UpdateAnimation(dt)`, `HasActiveAnimation()`, `IsFocusable()`, `IsTextInput()`, `ReleaseDeviceResources()`, `GetImeCandidateRect()`, `SetCompositionText(text, hasComposition, cursorPos)`, `GetChildRenderTransform(child)`, `OnFontChanged()`.

## Callback members (`std::function`)

`MouseEnterHandler`, `MouseLeaveHandler`, `MouseMoveHandler`, `MouseDownHandler`, `MouseUpHandler`, `KeyDownHandler`, `KeyUpHandler`, `CharHandler`, `FocusHandler`, `BlurHandler`.

## Parent/child, visibility, and menus

| Method | Description |
| --- | --- |
| `void SetParent(UIElement*)` / `UIElement* GetParent() const` | Parent element |
| `void SetVisible(bool)` / `bool IsVisible() const` | Visibility |
| `void SetContextMenu(std::shared_ptr<Menu>)` / `GetContextMenu()` | Context menu |
| `void SetBleed(float)` / `float GetBleed() const` | Bleed size (default `4.0f`) |
| `template<typename Signal, typename Slot> auto Connect(Signal&, Slot&&)` | Connects a signal (manages lifetime automatically) |

## Font interfaces

| Method | Description |
| --- | --- |
| `static void SetGlobalFont(const FontSpec&)` | Global font |
| `static void SetGlobalFontFamily(const std::wstring&)` | Global font family |
| `static void SetGlobalFontSize(float)` | Global font size |
| `static FontSpec GetGlobalFont()` | Reads the global font |
| `void SetFont(const FontSpec&)` / `SetFontFamily(const std::wstring&)` / `SetFontSize(float)` / `SetFontWeight(DWRITE_FONT_WEIGHT)` | Instance-level override |
| `void ClearFont()` / `bool HasFontOverride() const` | Clears/queries the override |
| `FontSpec GetEffectiveFontSpec() const` | Resolution: instance override → type default → global default |
| `IDWriteTextFormat* GetFontFormat() const` | Shared text format |


###chapter: Layout | Layout, ColumnBox, RowBox, GridLayout, LayoutHost, Card, Page, PageHost

## Layout (base class)

```cpp
class Layout : public UIElement {
    virtual ~Layout() = default;
    bool UseCache() const override;   // 布局容器默认不用缓存
};
```

## ColumnBox (vertical layout)

```cpp
class ColumnBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
    // 默认横向拉伸权重 1.0，纵向 0.0
};
```

## RowBox (horizontal layout)

```cpp
class RowBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
    // 默认横向拉伸权重 0.0，纵向 1.0
};
```

## GridLayout (grid layout)

```cpp
class GridLayout : public Layout {
    enum class Alignment { Start, Center, End };
    struct GridItem { std::shared_ptr<UIElement> element; int row, col, rowSpan, colSpan; };

    void AddChild(std::shared_ptr<UIElement> child, int row, int col, int rowSpan = 1, int colSpan = 1);
    void SetSpacing(float horizontal, float vertical);
    void SetColumnStretch(int col, float weight);
    void SetRowStretch(int row, float weight);
    void SetHorizontalAlignment(Alignment align);
    void SetVerticalAlignment(Alignment align);
};
```

Default horizontal and vertical spacing is `0`, alignment is `Start`, and both the default horizontal and vertical stretch weights are `1.0`.

## LayoutHost (swappable layout container)

```cpp
class LayoutHost : public UIElement {
    std::shared_ptr<UIElement> GetLayout() const;
    void SetLayout(std::shared_ptr<UIElement> layout);
    template<typename T> std::shared_ptr<T> GetLayoutAs() const;
};
```

With `GridLayout` held by default on construction. `GetLayoutAs<GridLayout>()` is the usual way to obtain the layout.

## Card

```cpp
class Card : public LayoutHost {
    static float DefaultPadding;           // 12.0f
    static float DefaultCornerRadius;      // 8.0f
    static Color DefaultBgColor;           // 白
    static Color DefaultBorderColor;       // 200,200,200

    void SetPadding(float);
    void SetCornerRadius(float);
    void SetBackgroundColor(Color);
    void SetBorderColor(Color);
    // 另有对应的 static SetDefault*
};
```

## Page

```cpp
class Page : public LayoutHost {
    static float DefaultPadding;     // 10.0f
    void SetPadding(float);
    void SetBackgroundColor(Color);
};
```

## PageHost (page host / transition animation)

```cpp
class PageHost : public UIElement {
    enum class TransitionDirection { Left, Right, Up, Down };

    void AddPage(std::shared_ptr<Page> page);
    void NavigateTo(int index);
    void SetTransitionDirection(TransitionDirection dir);
    void SetAnimationDuration(float seconds);   // 下限 0.01s，默认 0.3s
    int GetCurrentIndex() const;
    std::shared_ptr<Page> GetCurrentPage() const;
};
```


###chapter: Menus | MenuItem, Menu, and MenuWindow

## MenuItem

```cpp
class MenuItem {
    enum class Type { Normal, Separator, Submenu };
    std::wstring text;
    std::function<void()> callback;
    std::shared_ptr<Menu> submenu;
    Type type = Type::Normal;
    bool enabled = true;
    std::shared_ptr<Label> icon;
};
```

## Menu

```cpp
class Menu : public std::enable_shared_from_this<Menu> {
    void AddItem(const std::wstring& text, std::function<void()> callback = nullptr);
    void AddSeparator();
    void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu);
    std::vector<std::shared_ptr<MenuItem>> items;
};
```

## MenuWindow (popup menu window, generally used internally by the framework)

```cpp
class MenuWindow {
    MenuWindow(std::shared_ptr<Menu> menu, HWND owner, int x, int y);
    void Show(int x, int y);
    void Hide();
    void CloseAll();
};
```


###chapter: Window | Creation, background, title bar, and root layout

```cpp
class Window {
    static WindowBackdrop DefaultBackdrop;        // AcrylicBlurBehind
    static DWORD DefaultBackdropColor;            // 0x80FFFFFF
    static Color DefaultBackgroundColor;          // 透明

    bool Create(int width, int height, const std::wstring& title);
    void Run();

    void SetRootLayout(std::shared_ptr<Layout> layout);
    std::shared_ptr<Layout> GetRootLayout() const;
    std::shared_ptr<ColumnBox> GetRootColumnBox() const;

    void SetBackdrop(WindowBackdrop backdrop, DWORD color = 0x80FFFFFF);
    void SetBackgroundColor(Color color);
    void SetContextMenu(std::shared_ptr<Menu> menu);

    void SetTitleBarColors(COLORREF caption, COLORREF text, COLORREF border);
    void SetCaptionColor(COLORREF color);
    void SetTitleTextColor(COLORREF color);
    void SetBorderColor(COLORREF color);

    void SetMinSize(int width, int height);
    void SetMouseCapture(UIElement* elem);
    void ReleaseMouseCapture(UIElement* elem);
};
```

`WindowBackdrop` values: `None`, `Gradient`, `TransparentGradient`, `BlurBehind`, `AcrylicBlurBehind`.

The default root layout is a `ColumnBox` with `margin 20` and `spacing 10`.


###chapter: Basic Controls | Label, Button, TextBox, ComboBox, ToggleSwitch, ScrollViewer, ProgressBar, Slider

## Helper function

```cpp
inline void DrawTextWithEllipsis(ID2D1RenderTarget* rt,
    const std::wstring& text, const D2D1_RECT_F& rect,
    const D2D1_COLOR_F& color, const FontSpec& spec,
    ComPtr<ID2D1SolidColorBrush>& textBrush,
    IDWriteTextFormat* textFormat = nullptr, bool forceNoWrap = false);
```

## Label

```cpp
enum class TextOverflow { Wrap, Ellipsis };
enum class HAlign { Left, Center, Right };
enum class VAlign { Top, Center, Bottom };

class Label : public UIElement {
    Label(const std::wstring& text = L"Label");

    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    void SetTextColor(Color color);
    void SetTextOverflow(TextOverflow mode);
    TextOverflow GetTextOverflow() const;
    void SetAlignment(HAlign hAlign, VAlign vAlign);
    HAlign GetHorizontalAlignment() const;
    VAlign GetVerticalAlignment() const;

    static void SetDefaultTextColor(Color);
    static void SetDefaultFontSize(float);
    static void SetDefaultOverflow(TextOverflow);
    static void SetDefaultAlignment(HAlign, VAlign);
};
```

Defaults: black text, `Ellipsis`, left-aligned, vertically centered, font size `16`.

## Button

```cpp
class Button : public UIElement {
    ZSignal<> Clicked;

    Button(const std::wstring& text = L"Button");
    void SetText(const std::wstring& text);
    std::wstring GetText() const;
    void SetColors(Color normal, Color hover, Color pressed);
    void SetTextColor(Color color);
    void SetCornerRadius(float radius);
    void SetHoverAnimationSpeed(float speed);

    static void SetDefaultColors(Color, Color, Color);
    static void SetDefaultSize(float width, float height);
    // ...
};
```

Default size `120×36`, corner radius `4`, white text on a blue background (normal state `0,120,212`). `Clicked` fires when the mouse is released inside the button.

## TextBox

```cpp
class TextBox : public UIElement {
    ZSignal<const std::wstring&> TextChanged;

    TextBox();
    std::wstring GetText() const;
    void SetText(const std::wstring& text);
    void SetPlaceholder(const std::wstring& placeholder);
    void SetPasswordMode(bool mode);
    void SetMaxLength(int maxLength);        // -1 不限
    int  GetMaxLength() const;
    bool IsFocused() const;
    void Focus();
    void Blur();

    void SetTextColor(Color);
    void SetBackgroundColor(Color);
    void SetBorderColor(Color);
    void SetSelectionColor(Color);
    void SetHoverBackgroundColor(Color);
    void SetHoverBorderColor(Color);
    void SetCursorBlinkInterval(float);
};
```

Default size `160×30`, font size `14`, cursor blink `0.5s`. Supports Ctrl+C/X/V/A/Z/Y, arrow keys, Home/End, and IME composition input.

## ComboBox

```cpp
class ComboBox : public UIElement {
    ZSignal<int> SelectionChanged;

    ComboBox();
    void AddItem(const std::wstring& item);
    void SetItems(const std::vector<std::wstring>& items);
    void SetSelectedIndex(int index);
    int GetSelectedIndex() const;
    std::wstring GetSelectedText() const;
    bool IsExpanded() const;
    void Collapse();

    void SetNormalBgColor(D2D1_COLOR_F);
    void SetHoverBgColor(D2D1_COLOR_F);
    void SetBorderColor(D2D1_COLOR_F);
    void SetIndicatorColor(D2D1_COLOR_F);
    void SetListItemHeight(float);
};
```

Default size `160×30`, list item height `24`, indicator bar width `3` with a height ratio of `0.6`.

## ToggleSwitch

```cpp
class ToggleSwitch : public UIElement {
    ZSignal<bool> Toggled;

    ToggleSwitch(bool initialState = false);
    void SetOn(bool on);
    bool IsOn() const;
    void SetColors(Color on, Color off, Color knob);
    void SetAnimationSpeed(float speed);
};
```

Default size `50×24`, on `0,120,212`, off `200,200,200`, white knob.

## ScrollViewer

```cpp
class ScrollViewer : public UIElement {
    ScrollViewer();
    void SetContent(std::shared_ptr<UIElement> content);
    std::shared_ptr<UIElement> GetContent() const;
    void SetVerticalScrollEnabled(bool enabled);
    void SetHorizontalScrollEnabled(bool enabled);
    void SetScrollBarWidth(float width);
    void SetScrollWheelStep(float step);
    void SetAnimationSpeed(float speed);

    void ScrollTo(float offsetX, float offsetY, bool animated = true);
    void ScrollBy(float deltaX, float deltaY, bool animated = true);
};
```

Scroll bar width `8`, minimum length `20`, wheel step `30`. The internal `ScrollBar` class generally does not need to be used directly.

## ProgressBar

```cpp
class ProgressBar : public UIElement {
    ProgressBar();
    void SetValue(float value);        // [0,1]，会关闭不确定模式
    float GetValue() const;
    void SetIndeterminate(bool indeterminate);
    bool IsIndeterminate() const;
    void SetTrackColor(Color);
    void SetFillColor(Color);
    void SetBorderColor(Color);
    void SetIndeterminateBlockWidth(float);
    void SetIndeterminateSpeed(float);
};
```

Default size `200×20`, indeterminate block width `40`, speed `100`.

## Slider

```cpp
class Slider : public UIElement {
    ZSignal<float> ValueChanged;

    Slider();
    void SetRange(float min, float max);
    void SetValue(float value);
    float GetValue() const;
    void SetTrackColor(Color);
    void SetFillColor(Color);
    void SetThumbColor(Color);
    void SetHoverThumbColor(Color);
    void SetThumbSize(float);
    void SetTrackHeight(float);
};
```

Default size `160×24`, default range `[0,100]`, track height `4`, thumb diameter `14`.


###chapter: Data Views | ListView, TableView, TreeNode, TreeView

## ListView

```cpp
class ListView : public UIElement {
    ZSignal<int> SelectionChanged;
    ZSignal<int> ItemClicked;

    ListView();
    void AddItem(std::shared_ptr<Label> label);
    void AddItem(const std::wstring& text);
    void InsertItem(int index, const std::wstring& text);
    void RemoveItem(int index);
    void Clear();
    void SetItem(int index, const std::wstring& text);
    std::shared_ptr<Label> GetItemLabel(int index) const;
    std::wstring GetItemText(int index) const;
    int GetItemCount() const;

    void SetSelectedIndex(int index);
    int GetSelectedIndex() const;
    std::wstring GetSelectedText() const;

    void SetButtonMode(bool enable);        // 按钮模式：圆角行 + 间距
    bool IsButtonMode() const;
    void SetButtonSpacing(float spacing);

    void SetItemHeight(float height);
    float GetItemHeight() const;
    void SetIndicatorWidth(float width);
    void SetIndicatorHeightRatio(float ratio);
    void SetIndicatorColor(Color);
    void SetBackgroundColor(Color);
    void SetTextColor(Color);
    void SetSelectedColor(Color);
    void SetHoverColor(Color);
    void SetBorderColor(Color);
};
```

Default size `200×200`, row height `28`, `buttonMode_=false`, `buttonSpacing_=4.0f`.

## TableView

```cpp
enum class SelectionMode { Cell, Row, Column, None };

class TableView : public UIElement {
    ZSignal<int, int> CellClicked;        // (row, col)
    ZSignal<int, int> SelectionChanged;

    TableView();
    void SetRowCount(int rows);
    void SetColumnCount(int cols);
    void SetItem(int row, int col, const std::wstring& text);
    void SetItem(int row, int col, std::shared_ptr<Label> label);
    std::wstring GetItemText(int row, int col) const;
    std::shared_ptr<Label> GetItemLabel(int row, int col) const;
    void SetHorizontalHeaderLabels(const std::vector<std::wstring>& labels);
    void SetColumnWidth(int col, float width);
    float GetColumnWidth(int col) const;
    void SetRowHeight(float height);
    void SetHeaderHeight(float height);

    void SetSelectionMode(SelectionMode mode);
    SelectionMode GetSelectionMode() const;
    void SetCurrentCell(int row, int col);
    int GetCurrentRow() const;
    int GetCurrentColumn() const;

    // 颜色：SetBackgroundColor / SetHeaderBackgroundColor / SetTextColor /
    //       SetHeaderTextColor / SetSelectedColor / SetHoverColor /
    //       SetGridLineColor / SetBorderColor / SetScrollBarColors
};
```

Default size `400×300`, header height `26`, row height `24`, minimum column width `40`, default selection mode `Cell`. Note: there is **no** public `GetRowCount()` / `GetColumnCount()`.

## TreeNode

```cpp
struct TreeNode {
    std::vector<std::wstring> columns;                 // 第 0 列为节点文本
    TreeNode* parent = nullptr;                        // 非拥有
    std::vector<std::shared_ptr<TreeNode>> children;
    bool expanded = false;
    int depth = 0;
    void* userData = nullptr;

    TreeNode(const std::wstring& text);
    TreeNode(const std::vector<std::wstring>& cols);
};
```

## TreeView

```cpp
class TreeView : public UIElement {
    ZSignal<std::shared_ptr<TreeNode>> SelectionChanged;
    ZSignal<std::shared_ptr<TreeNode>> NodeClicked;
    ZSignal<std::shared_ptr<TreeNode>, bool> ExpandChanged;

    TreeView();
    void SetColumnCount(int count);
    void SetHeaderLabels(const std::vector<std::wstring>& labels);
    void SetColumnWidth(int col, float width);
    float GetColumnWidth(int col) const;

    std::shared_ptr<TreeNode> AddRoot(const std::wstring& text);
    std::shared_ptr<TreeNode> AddRoot(const std::vector<std::wstring>& columns);
    std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode> parent, const std::wstring& text);
    std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode> parent, const std::vector<std::wstring>& columns);
    void RemoveNode(std::shared_ptr<TreeNode> node);
    void Clear();

    void ExpandNode(std::shared_ptr<TreeNode> node, bool expand);
    void ToggleNode(std::shared_ptr<TreeNode> node);
    void ExpandNodeRecursive(std::shared_ptr<TreeNode> node, bool expand);
    bool IsExpanded(std::shared_ptr<TreeNode> node) const;

    void SetSelectedNode(std::shared_ptr<TreeNode> node);
    std::shared_ptr<TreeNode> GetSelectedNode() const;
    void ScrollToNode(std::shared_ptr<TreeNode> node);

    void SetHeaderVisible(bool visible);
    void SetRowHeight(float height);
    void SetIndent(float indent);
};
```

Default size `400×300`, row height `24`, indent `16`, header height `24`; one column by default, column width `40`, header `名称`.


###chapter: Appendix | Signal reference, default-value quick reference, and complete example
## Signal reference

| Class | Signal | Parameters |
| --- | --- | --- |
| `Button` | `Clicked` | — |
| `TextBox` | `TextChanged` | `const std::wstring&` |
| `ComboBox` | `SelectionChanged` | `int` |
| `ToggleSwitch` | `Toggled` | `bool` |
| `Slider` | `ValueChanged` | `float` |
| `ListView` | `SelectionChanged` / `ItemClicked` | `int` |
| `TableView` | `SelectionChanged` / `CellClicked` | `int, int` |
| `TreeView` | `SelectionChanged` / `NodeClicked` | `std::shared_ptr<TreeNode>` |
| `TreeView` | `ExpandChanged` | `std::shared_ptr<TreeNode>, bool` |
| `FontManager` | `GlobalFontChanged` | — |
| `UIZSignals` | `DrawOverlay` / `GlobalMouseDown` / ... | See section 3 |

## Common default values quick reference

| Item | Value |
| --- | --- |
| Global font | `Segoe UI` / `14.0f` |
| Window default background | `AcrylicBlurBehind` / `0x80FFFFFF` |
| Root layout margin / spacing | `20` / `10` |
| Label | Font size `16`, `Ellipsis`, vertically centered |
| Button | `120×36`, corner radius `4`, white text on blue |
| TextBox | `160×30`, font size `14` |
| ComboBox | `160×30`, list item height `24` |
| ToggleSwitch | `50×24` |
| ScrollViewer | Scroll bar width `8`, wheel step `30` |
| ProgressBar | `200×20` |
| Slider | `160×24`, range `[0,100]` |
| ListView | `200×200`, row height `28` |
| TableView | `400×300`, header `26`, row height `24` |
| TreeView | `400×300`, row height `24`, indent `16` |

## Complete example

```cpp
#include "ZDataViewer.h"
using namespace ZUI;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    Window win;
    if (!win.Create(900, 600, L"ZUI 示例")) return 1;

    auto root = win.GetRootColumnBox();
    root->SetSpacing(10);

    // 顶部：开关 + 滑块 + 进度条
    auto row = std::make_shared<RowBox>();
    row->SetSpacing(10);

    auto toggle = std::make_shared<ToggleSwitch>(false);
    auto slider = std::make_shared<Slider>();
    slider->SetRange(0.0f, 1.0f);
    auto bar = std::make_shared<ProgressBar>();

    toggle->Connect(toggle->Toggled, [bar](bool on) {
        bar->SetIndeterminate(on);
    });
    slider->Connect(slider->ValueChanged, [bar](float v) {
        bar->SetValue(v);
    });

    row->AddChild(toggle);
    row->AddChild(slider);
    row->AddChild(bar);
    root->AddChild(row);

    // 中间：列表
    auto list = std::make_shared<ListView>();
    list->SetFillHeight(true);
    for (int i = 1; i <= 20; ++i)
        list->AddItem(L"项目 " + std::to_wstring(i));
    list->SetSelectedIndex(0);
    root->AddChild(list);

    win.Run();
    return 0;
}
```
