# ZUI API 参考

> 本文档覆盖 ZUI 框架的全部公开 API：核心类型、信号槽、字体、元素、布局、窗口、基础控件与数据视图。
> 配套阅读：[项目说明与构建](../README.md)。

###chapter: 约定 | 命名空间、单位、生命周期与默认值约定

- 所有内容位于 `namespace ZUI`。
- 坐标与尺寸单位是 **DIP**（设备无关像素）；渲染前由 `Snap()` 吸附到物理像素。
- 控件用 `std::shared_ptr<T>` 持有，通过布局的 `AddChild()` 挂载。
- 事件用 `Connect(signal, slot)` 绑定，返回 `Connection`，随宿主析构自动断开。
- 各类的 `inline static` 默认值可通过对应的 `static SetDefault*()` 全局修改，影响之后新建的实例。


###chapter: 基础类型 | Color、Rect、Thickness、Size

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

| 成员 | 说明 |
| --- | --- |
| `Color(r,g,b,a)` | 分量范围 `[0,1]`，默认黑色不透明 |
| `FromArgb(a,r,g,b)` | 用 0–255 的分量构造 |
| `ToD2D()` | 转为 Direct2D 颜色结构 |
| `Lerp(c1,c2,t)` | 线性插值 |

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


###chapter: 全局函数与 DPI | clamp、DPI 缩放与像素吸附

```cpp
template<typename T> T clamp(T value, T low, T high);
inline float& GlobalDpiScaleRef();
inline void SetGlobalDpiScale(float scale);
inline float GetGlobalDpiScale();
inline float Snap(float dip);
```

| 函数 | 说明 |
| --- | --- |
| `clamp(v, low, high)` | 截断到区间（注意参数顺序与 `std::clamp` 相反） |
| `SetGlobalDpiScale(s)` | 设置全局 DPI 缩放，`s<=0` 回退 `1.0f` |
| `GetGlobalDpiScale()` | 读取当前缩放，默认 `1.0f` |
| `Snap(dip)` | 把 DIP 吸附到最近物理像素 |


###chapter: 信号与连接 | ZSignal、Connection、ConnectionGroup 与全局信号

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
**推荐用法**：控件内部已提供 `UIElement::Connect(signal, slot)`，会把连接挂到该控件的 `ConnectionGroup`，控件销毁时自动断开。手动用 `signal.connect(...)` 时要注意连接的生存期。
###block_green_end

## 全局信号 `UIZSignals`

| 信号 | 参数 | 触发时机 |
| --- | --- | --- |
| `DrawOverlay` | `ID2D1RenderTarget*` | 所有 UI 绘制完成后叠加绘制 |
| `GlobalMouseDown` | `float, float` | 全局鼠标按下（DIP 坐标） |
| `WindowDeactivated` | — | 窗口失活/最小化 |
| `ElementCaptureRequest` | `UIElement*` | 控件请求鼠标捕获 |
| `ElementCaptureRelease` | `UIElement*` | 控件释放鼠标捕获 |
| `RepaintRequest` | `UIElement*` | 控件请求重绘 |
| `LayoutInvalidated` | — | 全局布局失效 |


###chapter: 字体 | FontSpec 与 FontManager

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

## FontManager（单例）

| 方法 | 说明 |
| --- | --- |
| `static FontManager& Instance()` | 获取单例 |
| `IDWriteFactory* GetFactory()` | 懒加载的共享 DWrite 工厂 |
| `IDWriteTextFormat* GetFormat(const FontSpec& spec)` | 按字体规格取共享格式（带缓存） |
| `void SetGlobalFont(const FontSpec& spec)` | 设置全局字体并触发 `GlobalFontChanged` |
| `const FontSpec& GetGlobalFont() const` | 读取全局字体 |
| `ZSignal<> GlobalFontChanged` | 全局字体变更信号 |


###chapter: 核心元素 UIElement | 尺寸、布局、绘制、事件与字体接口

所有可视元素的基类。自定义控件需继承它并实现纯虚的 `Measure` 与 `Draw`。

## 布局属性

| 方法 | 说明 |
| --- | --- |
| `void SetMinWidth(float)` / `SetMinHeight(float)` | 最小尺寸 |
| `void SetMaxWidth(float)` / `SetMaxHeight(float)` | 最大尺寸 |
| `void SetMinSize(float w, float h)` / `SetMaxSize(float w, float h)` | 同时设置 |
| `float GetMinWidth()/GetMinHeight()/GetMaxWidth()/GetMaxHeight() const` | 读取 |
| `void SetFillWidth(bool)` / `SetFillHeight(bool)` | 是否填充父容器 |
| `bool GetFillWidth()/GetFillHeight() const` | 读取 |
| `void SetWidth(float)` / `SetHeight(float)` | 固定尺寸 |
| `float GetWidth()/GetHeight() const` | 读取 |
| `void SetMargin(const Thickness&)` / `Thickness GetMargin() const` | 外边距 |
| `void SetStretchWeights(float h, float v)` | 同时设置横/纵拉伸权重 |
| `void SetHorizontalStretchWeight(float)` / `SetVerticalStretchWeight(float)` | 单独设置 |
| `float GetHorizontalStretchWeight()/GetVerticalStretchWeight() const` | 读取 |
| `void InvalidateLayout()` / `bool IsLayoutDirty() const` / `ClearLayoutDirty()` | 布局失效 |

## 测量、排布与绘制

| 方法 | 说明 |
| --- | --- |
| `virtual Size Measure(const Size& availableSize) = 0` | 测量（**必须实现**） |
| `virtual void Arrange(const Rect& finalRect)` | 排布 |
| `Rect GetArrangedRect() const` | 排布结果 |
| `virtual void Draw(ID2D1RenderTarget* rt) = 0` | 绘制（**必须实现**） |
| `virtual std::vector<UIElement*> GetChildren() const` | 子元素列表 |
| `virtual bool UseCache() const` / `SetUseCache(bool)` | 是否使用离屏缓存 |
| `virtual std::optional<D2D1_RECT_F> GetClipRect() const` | 子元素裁剪区 |
| `void RequestRepaint()` | 请求重绘 |

## 事件虚函数（可重写）

`HitTest(x,y)`、`OnMouseEnter/Leave/Move/Down/Up`、`OnMouseWheel(dx,dy)`、`OnKeyDown/Up(key,lParam)`、`OnChar(ch)`、`OnFocus`、`OnBlur`、`UpdateAnimation(dt)`、`HasActiveAnimation()`、`IsFocusable()`、`IsTextInput()`、`ReleaseDeviceResources()`、`GetImeCandidateRect()`、`SetCompositionText(text, hasComposition, cursorPos)`、`GetChildRenderTransform(child)`、`OnFontChanged()`。

## 回调成员（`std::function`）

`MouseEnterHandler`、`MouseLeaveHandler`、`MouseMoveHandler`、`MouseDownHandler`、`MouseUpHandler`、`KeyDownHandler`、`KeyUpHandler`、`CharHandler`、`FocusHandler`、`BlurHandler`。

## 父子、可见性与菜单

| 方法 | 说明 |
| --- | --- |
| `void SetParent(UIElement*)` / `UIElement* GetParent() const` | 父元素 |
| `void SetVisible(bool)` / `bool IsVisible() const` | 可见性 |
| `void SetContextMenu(std::shared_ptr<Menu>)` / `GetContextMenu()` | 右键菜单 |
| `void SetBleed(float)` / `float GetBleed() const` | 出血尺寸（默认 `4.0f`） |
| `template<typename Signal, typename Slot> auto Connect(Signal&, Slot&&)` | 连接信号（自动管理生存期） |

## 字体接口

| 方法 | 说明 |
| --- | --- |
| `static void SetGlobalFont(const FontSpec&)` | 全局字体 |
| `static void SetGlobalFontFamily(const std::wstring&)` | 全局字体族 |
| `static void SetGlobalFontSize(float)` | 全局字号 |
| `static FontSpec GetGlobalFont()` | 读取全局字体 |
| `void SetFont(const FontSpec&)` / `SetFontFamily(const std::wstring&)` / `SetFontSize(float)` / `SetFontWeight(DWRITE_FONT_WEIGHT)` | 实例级覆盖 |
| `void ClearFont()` / `bool HasFontOverride() const` | 清除/查询覆盖 |
| `FontSpec GetEffectiveFontSpec() const` | 解析：实例覆盖 → 类型默认 → 全局默认 |
| `IDWriteTextFormat* GetFontFormat() const` | 共享文本格式 |


###chapter: 布局 | Layout、ColumnBox、RowBox、GridLayout、LayoutHost、Card、Page、PageHost

## Layout（基类）

```cpp
class Layout : public UIElement {
    virtual ~Layout() = default;
    bool UseCache() const override;   // 布局容器默认不用缓存
};
```

## ColumnBox（垂直布局）

```cpp
class ColumnBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
    // 默认横向拉伸权重 1.0，纵向 0.0
};
```

## RowBox（水平布局）

```cpp
class RowBox : public Layout {
    void AddChild(std::shared_ptr<UIElement> child);
    void SetSpacing(float spacing);
    float GetSpacing() const;
    // 默认横向拉伸权重 0.0，纵向 1.0
};
```

## GridLayout（网格布局）

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

默认横纵间距 `0`、对齐 `Start`，默认横纵拉伸权重均 `1.0`。

## LayoutHost（可替换布局容器）

```cpp
class LayoutHost : public UIElement {
    std::shared_ptr<UIElement> GetLayout() const;
    void SetLayout(std::shared_ptr<UIElement> layout);
    template<typename T> std::shared_ptr<T> GetLayoutAs() const;
};
```

构造时默认持有一个 `GridLayout`。`GetLayoutAs<GridLayout>()` 是拿布局的常用方式。

## Card（卡片）

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

## Page（页面）

```cpp
class Page : public LayoutHost {
    static float DefaultPadding;     // 10.0f
    void SetPadding(float);
    void SetBackgroundColor(Color);
};
```

## PageHost（页面宿主 / 切换动画）

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


###chapter: 菜单 | MenuItem、Menu 与 MenuWindow

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

## MenuWindow（弹出菜单窗口，一般由框架内部使用）

```cpp
class MenuWindow {
    MenuWindow(std::shared_ptr<Menu> menu, HWND owner, int x, int y);
    void Show(int x, int y);
    void Hide();
    void CloseAll();
};
```


###chapter: 窗口 Window | 创建、背景、标题栏与根布局

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

`WindowBackdrop` 取值：`None`、`Gradient`、`TransparentGradient`、`BlurBehind`、`AcrylicBlurBehind`。

默认根布局是带 `margin 20`、`spacing 10` 的 `ColumnBox`。


###chapter: 基础控件 | Label、Button、TextBox、ComboBox、ToggleSwitch、ScrollViewer、ProgressBar、Slider

## 辅助函数

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

默认：黑色文字、`Ellipsis`、左对齐、垂直居中、字号 `16`。

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

默认尺寸 `120×36`，圆角 `4`，蓝底白字（常态 `0,120,212`）。鼠标在按钮内抬起时触发 `Clicked`。

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

默认尺寸 `160×30`，字号 `14`，光标闪烁 `0.5s`。支持 Ctrl+C/X/V/A/Z/Y、方向键、Home/End，以及 IME 组合输入。

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

默认尺寸 `160×30`，列表项高 `24`，指示条宽 `3`、高占比 `0.6`。

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

默认尺寸 `50×24`，开 `0,120,212`、关 `200,200,200`、滑块白。

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

滚动条宽 `8`、最小长度 `20`、滚轮步长 `30`。内部 `ScrollBar` 类一般无需直接使用。

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

默认尺寸 `200×20`，不确定块宽 `40`、速度 `100`。

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

默认尺寸 `160×24`，默认范围 `[0,100]`，轨道高 `4`，滑块直径 `14`。


###chapter: 数据视图 | ListView、TableView、TreeNode、TreeView

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

默认尺寸 `200×200`，行高 `28`，`buttonMode_=false`、`buttonSpacing_=4.0f`。

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

默认尺寸 `400×300`，表头高 `26`、行高 `24`、最小列宽 `40`，选择模式默认 `Cell`。注意：**没有**公开的 `GetRowCount()` / `GetColumnCount()`。

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

默认尺寸 `400×300`，行高 `24`、缩进 `16`、表头高 `24`；默认一列、列宽 `40`、表头 `名称`。


###chapter: 附录 | 信号一览、默认值速查与完整示例
## 信号一览

| 类 | 信号 | 参数 |
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
| `UIZSignals` | `DrawOverlay` / `GlobalMouseDown` / ... | 见第 3 节 |

## 常用默认值速查

| 项 | 值 |
| --- | --- |
| 全局字体 | `Segoe UI` / `14.0f` |
| 窗口默认背景 | `AcrylicBlurBehind` / `0x80FFFFFF` |
| 根布局 margin / spacing | `20` / `10` |
| Label | 字号 `16`，`Ellipsis`，垂直居中 |
| Button | `120×36`，圆角 `4`，蓝底白字 |
| TextBox | `160×30`，字号 `14` |
| ComboBox | `160×30`，列表项高 `24` |
| ToggleSwitch | `50×24` |
| ScrollViewer | 滚动条宽 `8`，滚轮步长 `30` |
| ProgressBar | `200×20` |
| Slider | `160×24`，范围 `[0,100]` |
| ListView | `200×200`，行高 `28` |
| TableView | `400×300`，表头 `26`，行高 `24` |
| TreeView | `400×300`，行高 `24`，缩进 `16` |

## 完整示例

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
