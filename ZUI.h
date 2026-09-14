#pragma once
#include <windows.h>
#include <windowsx.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <optional>
#include <mutex>
#include <thread>
#include <utility>
#include <tuple>
#include <wrl/client.h>
#include <imm.h>
#include <mmsystem.h>
#include <unordered_set>
#include <unordered_map>
#include <array>

// 调试输出宏：默认关闭，定义 ZUI_DEBUG 后启用（不删除调试代码）
#ifdef ZUI_DEBUG
#define ZUI_DEBUG_LOG_W(msg) OutputDebugStringW(msg)
#define ZUI_DEBUG_LOG_A(msg) OutputDebugStringA(msg)
#else
#define ZUI_DEBUG_LOG_W(msg) ((void)0)
#define ZUI_DEBUG_LOG_A(msg) ((void)0)
#endif

#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

// Windows 头把 CreateWindow 定义为宏，这里取消，避免与 Application::CreateWindow 冲突
#ifdef CreateWindow
#undef CreateWindow
#endif

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

namespace ZUI {

    using Microsoft::WRL::ComPtr;

    template<typename T>
    T clamp(T value, T low, T high) {
        return value < low ? low : (value > high ? high : value);
    }

    // ---------- DPI 物理像素吸附 ----------
    // 动画/滚动进度保持 float 平滑；一旦落到 Arrange 或 Draw，就用 Snap() 吸附到物理像素
    inline float& GlobalDpiScaleRef() {
        static thread_local float scale = 1.0f;
        return scale;
    }

    inline void SetGlobalDpiScale(float scale) {
        GlobalDpiScaleRef() = (scale > 0.0f) ? scale : 1.0f;
    }

    inline float GetGlobalDpiScale() {
        return GlobalDpiScaleRef();
    }

    // 把 DIP 坐标吸附到最近的物理像素（返回值仍以 DIP 表示）
    inline float Snap(float dip) {
        float s = GlobalDpiScaleRef();
        return (s > 0.0f) ? std::round(dip * s) / s : dip;
    }

    // ---------- 未文档化亚克力相关结构 ----------
    enum ACCENT_STATE {
        ACCENT_DISABLED = 0,
        ACCENT_ENABLE_GRADIENT = 1,
        ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
        ACCENT_ENABLE_BLURBEHIND = 3,
        ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
        ACCENT_INVALID_STATE = 5
    };

    struct ACCENT_POLICY {
        ACCENT_STATE AccentState;
        int AccentFlags;
        int GradientColor;
        int AnimationId;
    };

    enum WINDOWCOMPOSITIONATTRIB {
        WCA_ACCENT_POLICY = 19
    };

    struct WINDOWCOMPOSITIONATTRIBDATA {
        WINDOWCOMPOSITIONATTRIB Attrib;
        PVOID pvData;
        SIZE_T cbData;
    };

    // ---------- 背景效果枚举 ----------
    enum class WindowBackdrop {
        None = ACCENT_DISABLED,
        Gradient = ACCENT_ENABLE_GRADIENT,
        TransparentGradient = ACCENT_ENABLE_TRANSPARENTGRADIENT,
        BlurBehind = ACCENT_ENABLE_BLURBEHIND,
        AcrylicBlurBehind = ACCENT_ENABLE_ACRYLICBLURBEHIND
    };

    // ---------- 基础类型 ----------
    struct Color {
        float r, g, b, a;
        Color(float r = 0, float g = 0, float b = 0, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
        static Color FromArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        }
        D2D1_COLOR_F ToD2D() const { return D2D1::ColorF(r, g, b, a); }
        static Color Lerp(const Color& c1, const Color& c2, float t) {
            return Color(c1.r + (c2.r - c1.r) * t,
                c1.g + (c2.g - c1.g) * t,
                c1.b + (c2.b - c1.b) * t,
                c1.a + (c2.a - c1.a) * t);
        }
    };

    struct Rect {
        float x, y, width, height;
        Rect(float x = 0, float y = 0, float w = 0, float h = 0) : x(x), y(y), width(w), height(h) {}
        bool Contains(float px, float py) const {
            return px >= x && px < x + width && py >= y && py < y + height;
        }
        D2D1_RECT_F ToD2D() const { return D2D1::RectF(x, y, x + width, y + height); }
    };

    struct Thickness {
        float left, top, right, bottom;
        Thickness(float l = 0, float t = 0, float r = 0, float b = 0) : left(l), top(t), right(r), bottom(b) {}
    };

    struct Size {
        float width, height;
        Size(float w = 0, float h = 0) : width(w), height(h) {}
    };

    // 前向声明
    class Label;
    class Menu;
    class MenuWindow;
    class ComboBox;
    class Window;

    // ========== 信号槽机制 ==========
    enum class ConnectionThread {
        CurrentThread,
        NewThread,
        UIThread
    };

    class ConnectionGroup;

    namespace detail {
        inline std::thread::id g_uiThreadId;
        inline HWND g_uiDispatcherWindow = nullptr;
        inline constexpr UINT WM_UI_TASK = WM_APP + 1;

        inline void InitializeUIThread() {
            g_uiThreadId = std::this_thread::get_id();
            if (!g_uiDispatcherWindow) {
                static bool classRegistered = false;
                if (!classRegistered) {
                    WNDCLASSEXW wc = {};
                    wc.cbSize = sizeof(WNDCLASSEXW);
                    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
                        if (msg == WM_UI_TASK) {
                            auto* task = reinterpret_cast<std::function<void()>*>(lParam);
                            if (task) {
                                (*task)();
                                delete task;
                            }
                            return 0;
                        }
                        return DefWindowProc(hwnd, msg, wParam, lParam);
                        };
                    wc.hInstance = GetModuleHandle(nullptr);
                    wc.lpszClassName = L"ZUI_DispatcherWindow";
                    RegisterClassExW(&wc);
                    classRegistered = true;
                }
                g_uiDispatcherWindow = CreateWindowExW(0, L"ZUI_DispatcherWindow", L"",
                    WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
            }
        }

        inline void PostToUIThread(std::function<void()> fn) {
            if (std::this_thread::get_id() == g_uiThreadId) {
                fn();
            }
            else if (g_uiDispatcherWindow) {
                auto* task = new std::function<void()>(std::move(fn));
                PostMessage(g_uiDispatcherWindow, WM_UI_TASK, 0, (LPARAM)task);
            }
        }

        struct ConnectionState {
            std::function<void()> disconnect;
            std::weak_ptr<ConnectionGroup> group;
            std::shared_ptr<bool> alive;
            ConnectionState() : alive(std::make_shared<bool>(true)) {}
        };
    }

    class ConnectionGroup : public std::enable_shared_from_this<ConnectionGroup> {
    public:
        ConnectionGroup() = default;
        ~ConnectionGroup() { disconnectAll(); }

        ConnectionGroup(const ConnectionGroup&) = delete;
        ConnectionGroup& operator=(const ConnectionGroup&) = delete;

        void disconnectAll() {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& weak : connections_) {
                if (auto state = weak.lock()) {
                    if (state->disconnect) {
                        state->disconnect();
                        state->disconnect = nullptr;
                    }
                }
            }
            connections_.clear();
        }

        size_t size() const {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t count = 0;
            for (auto& weak : connections_) {
                if (!weak.expired()) ++count;
            }
            return count;
        }

        void addState(const std::shared_ptr<detail::ConnectionState>& state) {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.push_back(state);
            state->group = weak_from_this();
        }

    private:
        mutable std::mutex mutex_;
        std::vector<std::weak_ptr<detail::ConnectionState>> connections_;
    };

    class Connection {
    public:
        Connection() = default;
        Connection(std::shared_ptr<detail::ConnectionState> state) : state_(std::move(state)) {}
        ~Connection() { disconnect(); }

        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        Connection(Connection&& other) noexcept : state_(std::move(other.state_)) {}
        Connection& operator=(Connection&& other) noexcept {
            if (this != &other) {
                disconnect();
                state_ = std::move(other.state_);
            }
            return *this;
        }

        void disconnect() {
            if (state_) {
                if (state_->disconnect) {
                    state_->disconnect();
                    state_->disconnect = nullptr;
                }
                state_.reset();
            }
        }

        bool isConnected() const {
            return state_ && state_->disconnect != nullptr && *(state_->alive);
        }

    private:
        std::shared_ptr<detail::ConnectionState> state_;
    };

    // 信号类：使用 Fire 避免宏冲突
    template <typename... TArgs>
    class ZSignal {
    public:
        using SlotType = std::function<void(TArgs...)>;

        ZSignal() = default;
        ~ZSignal() {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& data : connections_) {
                if (data && data->state && data->state->alive) {
                    *(data->state->alive) = false;
                }
            }
            connections_.clear();
        }

        ZSignal(const ZSignal&) = delete;
        ZSignal& operator=(const ZSignal&) = delete;
        ZSignal(ZSignal&& other) noexcept {
            std::lock_guard<std::mutex> lock(other.mutex_);
            connections_ = std::move(other.connections_);
        }
        ZSignal& operator=(ZSignal&& other) noexcept {
            if (this != &other) {
                std::lock_guard<std::mutex> lock1(mutex_);
                std::lock_guard<std::mutex> lock2(other.mutex_);
                connections_ = std::move(other.connections_);
            }
            return *this;
        }

        Connection connect(SlotType slot,
            ConnectionThread thread = ConnectionThread::CurrentThread,
            std::shared_ptr<ConnectionGroup> group = nullptr) {
            auto state = std::make_shared<detail::ConnectionState>();
            auto data = std::make_shared<ConnectionData>(std::move(slot), thread, state);

            state->disconnect = [this, weak = std::weak_ptr<ConnectionData>(data)]() {
                if (auto shared = weak.lock()) {
                    this->removeConnection(shared);
                }
                };

            {
                std::lock_guard<std::mutex> lock(mutex_);
                connections_.push_back(data);
            }

            if (group) {
                group->addState(state);
            }

            return Connection(state);
        }

        void Fire(TArgs... targs) const {
            // 复用线程本地缓冲，避免每次触发都堆分配；
            // 重入（slot 内再次触发同一信号）时退回局部拷贝，保证正确性。
            static thread_local std::vector<std::shared_ptr<ConnectionData>> tlSnapshot;
            static thread_local int tlDepth = 0;
            std::vector<std::shared_ptr<ConnectionData>> reentrant;
            std::vector<std::shared_ptr<ConnectionData>>* snapshot = nullptr;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (tlDepth == 0) {
                    tlSnapshot.assign(connections_.begin(), connections_.end());
                    snapshot = &tlSnapshot;
                }
                else {
                    reentrant = connections_;
                    snapshot = &reentrant;
                }
                tlDepth++;
            }

            for (auto& data : *snapshot) {
                if (!data || !data->state || !*(data->state->alive)) continue;
                switch (data->thread) {
                case ConnectionThread::CurrentThread:
                    data->slot(targs...);
                    break;
                case ConnectionThread::NewThread: {
                    auto tupleArgs = std::make_tuple(targs...);
                    std::thread([slot = data->slot, tupleArgs = std::move(tupleArgs)]() mutable {
                        std::apply(slot, std::move(tupleArgs));
                        }).detach();
                    break;
                }
                case ConnectionThread::UIThread: {
                    auto tupleArgs = std::make_tuple(targs...);
                    detail::PostToUIThread([slot = data->slot, tupleArgs = std::move(tupleArgs)]() mutable {
                        std::apply(slot, std::move(tupleArgs));
                        });
                    break;
                }
                }
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                tlDepth--;
            }
        }

        void operator()(TArgs... targs) const {
            Fire(std::forward<TArgs>(targs)...);
        }

    private:
        struct ConnectionData {
            SlotType slot;
            ConnectionThread thread;
            std::shared_ptr<detail::ConnectionState> state;

            ConnectionData(SlotType s, ConnectionThread t,
                std::shared_ptr<detail::ConnectionState> st)
                : slot(std::move(s)), thread(t), state(std::move(st)) {}
        };

        void removeConnection(const std::shared_ptr<ConnectionData>& data) {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.erase(
                std::remove(connections_.begin(), connections_.end(), data),
                connections_.end());
        }

        mutable std::mutex mutex_;
        std::vector<std::shared_ptr<ConnectionData>> connections_;
    };

    // ========== 字体管理 ==========
    struct FontSpec {
        std::wstring familyName = L"Segoe UI";
        float size = 14.0f;
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
        DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
        DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
        std::wstring locale = L"en-us";

        bool operator==(const FontSpec& other) const {
            return familyName == other.familyName &&
                size == other.size &&
                weight == other.weight &&
                style == other.style &&
                stretch == other.stretch &&
                locale == other.locale;
        }
        bool operator!=(const FontSpec& other) const { return !(*this == other); }
    };

    struct FontSpecHash {
        size_t operator()(const FontSpec& spec) const {
            size_t h = std::hash<std::wstring>()(spec.familyName);
            h ^= std::hash<float>()(spec.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()((int)spec.weight) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()((int)spec.style) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()((int)spec.stretch) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<std::wstring>()(spec.locale) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    // 全局字体管理器：单例，持有唯一的 DWriteFactory 和 IDWriteTextFormat 缓存
    class FontManager {
    public:
        static FontManager& Instance() {
            static FontManager instance;
            return instance;
        }

        IDWriteFactory* GetFactory() {
            if (!dwriteFactory_) {
                DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &dwriteFactory_);
            }
            return dwriteFactory_.Get();
        }

        // 按 spec 获取共享的 IDWriteTextFormat（带缓存）
        IDWriteTextFormat* GetFormat(const FontSpec& spec) {
            auto it = formatCache_.find(spec);
            if (it != formatCache_.end()) return it->second.Get();

            IDWriteFactory* factory = GetFactory();
            if (!factory) return nullptr;

            ComPtr<IDWriteTextFormat> fmt;
            HRESULT hr = factory->CreateTextFormat(
                spec.familyName.c_str(), nullptr,
                spec.weight, spec.style, spec.stretch,
                spec.size, spec.locale.c_str(), &fmt);
            if (FAILED(hr) || !fmt) return nullptr;

            fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            IDWriteTextFormat* raw = fmt.Get();
            formatCache_.emplace(spec, std::move(fmt));
            return raw;
        }

        // 设置全局默认字体，触发 GlobalFontChanged 让所有未覆盖的控件重建
        void SetGlobalFont(const FontSpec& spec) {
            globalFont_ = spec;
            GlobalFontChanged();
        }

        const FontSpec& GetGlobalFont() const { return globalFont_; }

        ZSignal<> GlobalFontChanged;

    private:
        FontManager() = default;
        FontManager(const FontManager&) = delete;
        FontManager& operator=(const FontManager&) = delete;

        ComPtr<IDWriteFactory> dwriteFactory_;
        std::unordered_map<FontSpec, ComPtr<IDWriteTextFormat>, FontSpecHash> formatCache_;
        FontSpec globalFont_;
    };

    class UIElement;  // 前向声明

    namespace UIZSignals {
        // 叠加绘制（参数：所属窗口, 渲染目标）。订阅者必须按窗口过滤，避免把 A 窗口的弹层画到 B 窗口。
        inline ZSignal<Window*, ID2D1RenderTarget*> DrawOverlay;

        // 全局鼠标按下（参数：所在窗口, x, y，单位 dip）
        inline ZSignal<Window*, float, float> GlobalMouseDown;

        // 窗口失去激活（失活或最小化），参数为失活的窗口
        inline ZSignal<Window*> WindowDeactivated;

        // 控件请求/释放鼠标捕获（参数：所属窗口, 控件）。已挂载控件走 Window 直接路由，这里是无窗口时的兜底。
        inline ZSignal<Window*, UIElement*> ElementCaptureRequest;
        inline ZSignal<Window*, UIElement*> ElementCaptureRelease;

        // 重绘请求（参数：所属窗口, 控件）。已挂载控件走 Window 直接路由，这里是无窗口时的兜底。
        inline ZSignal<Window*, UIElement*> RepaintRequest;

        // 布局失效（参数：所属窗口）。已挂载控件走 Window 直接路由，这里是无窗口时的兜底。
        inline ZSignal<Window*> LayoutInvalidated;
    }

    // ---------- 基础元素 ----------
    class UIElement {
    public:
        UIElement() : parent_(nullptr), visible_(true), width_(0), height_(0), arrangedRect_(),
            minWidth_(0), minHeight_(0), maxWidth_(FLT_MAX), maxHeight_(FLT_MAX),
            fillWidth_(false), fillHeight_(false), layoutDirty_(true),
            connectionGroup_(std::make_shared<ConnectionGroup>()),
            cacheValid_(false), useCache_(true) {
            // 订阅全局字体变更：未覆盖字体的控件自动重建
            auto fontConn = FontManager::Instance().GlobalFontChanged.connect(
                [this]() {
                    if (!fontOverride_) OnFontChanged();
                },
                ConnectionThread::CurrentThread,
                connectionGroup_
            );
            autoConnections_.push_back(std::move(fontConn));
        }

        virtual ~UIElement() = default;

        // ---------- 布局相关 ----------
        void InvalidateLayout();   // 定义见文件末尾（需要 Window 完整类型才能路由到所属窗口）
        bool IsLayoutDirty() const { return layoutDirty_; }
        void ClearLayoutDirty() { layoutDirty_ = false; }

        void SetMinWidth(float w) { minWidth_ = w; InvalidateLayout(); }
        void SetMinHeight(float h) { minHeight_ = h; InvalidateLayout(); }
        void SetMaxWidth(float w) { maxWidth_ = w; InvalidateLayout(); }
        void SetMaxHeight(float h) { maxHeight_ = h; InvalidateLayout(); }
        void SetMinSize(float w, float h) { minWidth_ = w; minHeight_ = h; InvalidateLayout(); }
        void SetMaxSize(float w, float h) { maxWidth_ = w; maxHeight_ = h; InvalidateLayout(); }
        float GetMinWidth() const { return minWidth_; }
        float GetMinHeight() const { return minHeight_; }
        float GetMaxWidth() const { return maxWidth_; }
        float GetMaxHeight() const { return maxHeight_; }

        void SetFillWidth(bool fill) { fillWidth_ = fill; InvalidateLayout(); }
        void SetFillHeight(bool fill) { fillHeight_ = fill; InvalidateLayout(); }
        bool GetFillWidth() const { return fillWidth_; }
        bool GetFillHeight() const { return fillHeight_; }

        void SetMargin(const Thickness& margin) { margin_ = margin; InvalidateLayout(); }
        Thickness GetMargin() const { return margin_; }
        void SetWidth(float width) { width_ = width; InvalidateLayout(); }
        void SetHeight(float height) { height_ = height; InvalidateLayout(); }
        float GetWidth() const { return width_; }
        float GetHeight() const { return height_; }

        void SetStretchWeights(float horizontal, float vertical) {
            horizontalStretchWeight_ = horizontal;
            verticalStretchWeight_ = vertical;
        }
        void SetHorizontalStretchWeight(float weight) { horizontalStretchWeight_ = weight; }
        void SetVerticalStretchWeight(float weight) { verticalStretchWeight_ = weight; }
        float GetHorizontalStretchWeight() const {
            return horizontalStretchWeight_.has_value() ? horizontalStretchWeight_.value() : GetDefaultHorizontalStretchWeight();
        }
        float GetVerticalStretchWeight() const {
            return verticalStretchWeight_.has_value() ? verticalStretchWeight_.value() : GetDefaultVerticalStretchWeight();
        }

        virtual float GetDefaultHorizontalStretchWeight() const { return 0.0f; }
        virtual float GetDefaultVerticalStretchWeight() const { return 0.0f; }

        virtual Size Measure(const Size& availableSize) = 0;
        virtual void Arrange(const Rect& finalRect) { arrangedRect_ = finalRect; layoutDirty_ = false; }
        Rect GetArrangedRect() const { return arrangedRect_; }

        // 获取 IME 候选框定位矩形（DIP 坐标）
        virtual Rect GetImeCandidateRect() const {
            return arrangedRect_;
        }
        virtual void SetCompositionText(const std::wstring& text, bool hasComposition, int cursorPos = -1) {}

        // ---------- 绘图 ----------
        virtual void Draw(ID2D1RenderTarget* rt) = 0;

        // ---------- 子元素列表（新增） ----------
        virtual const std::vector<UIElement*>& GetChildren() const { return childrenView_; }

        // ---------- 缓存相关（新增） ----------
        // 控件可重写此方法声明不使用离屏缓存（如动画频繁的控件）
        virtual bool UseCache() const { return useCache_; }
        void SetUseCache(bool use) { useCache_ = use; }

        // 获取元素需要应用于其子元素的裁剪矩形（相对于自身坐标系），返回 std::nullopt 表示不裁剪
        virtual std::optional<D2D1_RECT_F> GetClipRect() const { return std::nullopt; }

        // 请求重绘（仅视觉变化）；定义见文件末尾
        void RequestRepaint();

        // 缓存有效性标记（由 Window 管理，但为了方便检查放在这里）
        bool cacheValid_ = false;
        ComPtr<ID2D1BitmapRenderTarget> cacheRT_;
        // 缓存尺寸记录
        Size cacheSize_;

        // ---------- 事件 ----------
        virtual UIElement* HitTest(float x, float y) {
            if (visible_ && arrangedRect_.Contains(x, y)) return this;
            return nullptr;
        }

        virtual void OnMouseEnter() {}
        virtual void OnMouseLeave() {}
        virtual void OnMouseMove(float x, float y) {}
        virtual void OnMouseDown(float x, float y) {}
        virtual void OnMouseUp(float x, float y) {}
        // 右键菜单：返回 true 表示控件已自行处理（框架不再弹默认菜单）
        virtual bool OnContextMenu(float x, float y) { return false; }
        virtual void OnKeyDown(WPARAM key, LPARAM lParam) {}
        virtual void OnKeyUp(WPARAM key, LPARAM lParam) {}
        virtual void OnChar(wchar_t ch) {}
        virtual void OnFocus() {}
        virtual void OnBlur() {}
        virtual void UpdateAnimation(float deltaTime) {}
        virtual bool OnMouseWheel(float deltaX, float deltaY) { return false; }
        virtual bool HasActiveAnimation() const { return false; }
        virtual void CollectExpandedComboBoxes(std::vector<ComboBox*>& list) {}

        virtual void ReleaseDeviceResources() {
            // 释放缓存资源
            cacheRT_.Reset();
            cacheValid_ = false;
        }

        // 类型判断辅助
        virtual bool IsTextInput() const { return false; }

        virtual D2D1::Matrix3x2F GetChildRenderTransform(UIElement* child) const {
            return D2D1::Matrix3x2F::Identity();
        }

        // ---------- 信号连接自动管理（改进版） ----------
        template<typename Signal, typename Slot>
        auto Connect(Signal& signal, Slot&& slot) {
            auto conn = signal.connect(std::forward<Slot>(slot), ConnectionThread::CurrentThread, connectionGroup_);
            autoConnections_.push_back(std::move(conn));
            return conn;
        }

        // 在 UIElement 类内（public 或 protected）
        template<typename T>
        static void ConvergeValue(T& value, const T& target, float epsilon = 0.001f) {
            if (fabs(value - target) < epsilon) {
                value = target;
            }
        }

        // ---------- 事件处理器 ----------
        std::function<void()> MouseEnterHandler;
        std::function<void()> MouseLeaveHandler;
        std::function<void(float, float)> MouseMoveHandler;
        std::function<void(float, float)> MouseDownHandler;
        std::function<void(float, float)> MouseUpHandler;
        std::function<void(WPARAM, LPARAM)> KeyDownHandler;
        std::function<void(WPARAM, LPARAM)> KeyUpHandler;
        std::function<void(wchar_t)> CharHandler;
        std::function<void()> FocusHandler;
        std::function<void()> BlurHandler;

        virtual bool IsFocusable() const { return false; }

        // ---------- 父子关系 ----------
        void SetParent(UIElement* parent) {
            parent_ = parent;
            // 挂到已属于某窗口的父级时，立即把自己的子树也归属到该窗口；
            // 若父级尚未挂载，则等父级挂载时由 AttachWindowRecursive 统一传播。
            AttachWindowRecursive(parent ? parent->window_ : nullptr);
        }
        UIElement* GetParent() const { return parent_; }

        // 所属窗口（挂载到窗口的树后由框架设置；未挂载时为 nullptr）
        Window* GetWindow() const { return window_; }
        // 把“所属窗口”沿子树传播；容器需重写以递归自己的子元素
        virtual void AttachWindowRecursive(Window* w) { window_ = w; }
        void SetVisible(bool visible) {
            if (visible_ != visible) {
                visible_ = visible;
                if (!visible_) {
                    cacheRT_.Reset();
                    cacheValid_ = false;
                }
                InvalidateLayout();
            }
        }
        bool IsVisible() const { return visible_; }
        void SetContextMenu(std::shared_ptr<Menu> menu) { contextMenu_ = menu; }
        std::shared_ptr<Menu> GetContextMenu() const { return contextMenu_; }
        // 设置出血尺寸（单位：DIP），影响缓存大小和贴图偏移
        void SetBleed(float bleed) { bleed_ = max(0.0f, bleed); }
        float GetBleed() const { return bleed_; }

        // ---------- 禁用态 ----------
        void SetEnabled(bool enabled) {
            if (enabled_ != enabled) { enabled_ = enabled; cacheValid_ = false; RequestRepaint(); }
        }
        bool IsEnabled() const { return enabled_; }
        // 考虑父链的实际可用性
        bool IsEffectivelyEnabled() const {
            const UIElement* p = this;
            while (p) { if (!p->enabled_) return false; p = p->parent_; }
            return true;
        }

        // ---------- 悬停提示 ----------
        void SetToolTip(const std::wstring& text) { tooltip_ = text; }
        std::wstring GetToolTip() const { return tooltip_; }

        // ---------- 阴影（默认关闭；元素设置，Window 合成进缓存）----------
        void SetShadow(bool enable) { shadowEnabled_ = enable; cacheValid_ = false; RequestRepaint(); }
        bool HasShadow() const { return shadowEnabled_; }
        void SetShadowColor(Color c) { shadowColor_ = c.ToD2D(); cacheValid_ = false; RequestRepaint(); }
        void SetShadowBlur(float blur) { shadowBlur_ = max(0.0f, blur); cacheValid_ = false; RequestRepaint(); }
        void SetShadowOffset(float x, float y) { shadowOffsetX_ = x; shadowOffsetY_ = y; cacheValid_ = false; RequestRepaint(); }
        void SetShadowCornerRadius(float r) { shadowCornerRadius_ = r; cacheValid_ = false; RequestRepaint(); }
        D2D1_COLOR_F GetShadowColor() const { return shadowColor_; }
        float GetShadowBlur() const { return shadowBlur_; }
        float GetShadowOffsetX() const { return shadowOffsetX_; }
        float GetShadowOffsetY() const { return shadowOffsetY_; }
        float GetShadowCornerRadius() const { return shadowCornerRadius_; }
        float GetShadowExtent() const {
            return shadowEnabled_ ? (shadowBlur_ * 2.0f + max(fabs(shadowOffsetX_), fabs(shadowOffsetY_))) : 0.0f;
        }
        bool enabled_ = true;
        std::wstring tooltip_;
        bool shadowEnabled_ = false;
        D2D1_COLOR_F shadowColor_ = D2D1::ColorF(0.0f, 0.0f, 0.02f, 0.42f);
        float shadowBlur_ = 10.0f;
        float shadowOffsetX_ = 0.0f;
        float shadowOffsetY_ = 3.0f;
        float shadowCornerRadius_ = -1.0f;

        // 缓存内容实际绘制的原点（相对于缓存位图左上角）
        float cacheOriginX_ = 0.0f;
        float cacheOriginY_ = 0.0f;

        // ---------- 字体接口 ----------
        // 全局字体控制（转发到 FontManager）
        static void SetGlobalFont(const FontSpec& spec) { FontManager::Instance().SetGlobalFont(spec); }
        static void SetGlobalFontFamily(const std::wstring& family) {
            FontSpec s = FontManager::Instance().GetGlobalFont();
            s.familyName = family;
            FontManager::Instance().SetGlobalFont(s);
        }
        static void SetGlobalFontSize(float size) {
            FontSpec s = FontManager::Instance().GetGlobalFont();
            s.size = size;
            FontManager::Instance().SetGlobalFont(s);
        }
        static FontSpec GetGlobalFont() { return FontManager::Instance().GetGlobalFont(); }

        // 实例级字体覆盖
        void SetFont(const FontSpec& spec) {
            fontOverride_ = spec;
            cachedFormatRaw_ = nullptr;
            cachedFormatValid_ = false;
            InvalidateLayout();
            RequestRepaint();
        }
        void SetFontFamily(const std::wstring& family) {
            FontSpec spec = GetEffectiveFontSpec();
            spec.familyName = family;
            SetFont(spec);
        }
        void SetFontSize(float size) {
            FontSpec spec = GetEffectiveFontSpec();
            spec.size = size;
            SetFont(spec);
        }
        void SetFontWeight(DWRITE_FONT_WEIGHT weight) {
            FontSpec spec = GetEffectiveFontSpec();
            spec.weight = weight;
            SetFont(spec);
        }
        void ClearFont() {
            fontOverride_.reset();
            cachedFormatRaw_ = nullptr;
            cachedFormatValid_ = false;
            InvalidateLayout();
            RequestRepaint();
        }
        bool HasFontOverride() const { return fontOverride_.has_value(); }

        // 子类可重写：返回该类型的默认字体（例如 Label 用 16，TextBox 用 14）
        virtual std::optional<FontSpec> GetTypeDefaultFont() const { return std::nullopt; }

        // 字体解析链：实例覆盖 → 类型默认 → 全局默认
        FontSpec GetEffectiveFontSpec() const {
            if (fontOverride_) return *fontOverride_;
            if (auto t = GetTypeDefaultFont()) return *t;
            return FontManager::Instance().GetGlobalFont();
        }

        // 获取共享的 IDWriteTextFormat（带每实例缓存，快速命中时零开销）
        IDWriteTextFormat* GetFontFormat() const {
            FontSpec spec = GetEffectiveFontSpec();
            if (cachedFormatValid_ && cachedSpec_ == spec && cachedFormatRaw_) {
                return cachedFormatRaw_;
            }
            cachedFormatRaw_ = FontManager::Instance().GetFormat(spec);
            cachedSpec_ = spec;
            cachedFormatValid_ = (cachedFormatRaw_ != nullptr);
            return cachedFormatRaw_;
        }

        // 字体变化时的回调（默认：失效缓存 + 重新布局）
        virtual void OnFontChanged() {
            cachedFormatRaw_ = nullptr;
            cachedFormatValid_ = false;
            InvalidateLayout();
            RequestRepaint();
        }

    protected:
        float bleed_ = 4.0f;  // 离屏缓存出血尺寸（四周额外空间）
        UIElement* parent_;
        bool visible_;
        Thickness margin_;
        float width_;
        float height_;
        Rect arrangedRect_;
        float minWidth_, minHeight_, maxWidth_, maxHeight_;
        bool fillWidth_, fillHeight_;
        bool layoutDirty_;
        std::shared_ptr<Menu> contextMenu_;
        std::optional<float> horizontalStretchWeight_;
        std::optional<float> verticalStretchWeight_;

        std::shared_ptr<ConnectionGroup> connectionGroup_;
        std::vector<Connection> autoConnections_;

        bool useCache_; // 默认 true，可被重写
        Window* window_ = nullptr;  // 所属窗口（非拥有，由框架在挂载时设置）
        mutable std::vector<UIElement*> childrenView_; // GetChildren 复用的视图缓冲，避免每帧分配

        // ---------- 字体相关成员 ----------
        std::optional<FontSpec> fontOverride_;
        mutable IDWriteTextFormat* cachedFormatRaw_ = nullptr;
        mutable FontSpec cachedSpec_;
        mutable bool cachedFormatValid_ = false;
    };

    // ---------- 布局基类 ----------
    class Layout : public UIElement {
    public:
        virtual ~Layout() = default;
        // 布局容器默认不使用缓存
        bool UseCache() const override { return false; }
    };

    // ---------- 垂直布局容器 ----------
    class ColumnBox : public Layout {
    public:
        ColumnBox() : spacing_(0) {}
        void AddChild(std::shared_ptr<UIElement> child) {
            children_.push_back(child);
            child->SetParent(this);
            InvalidateLayout();
        }
        void SetSpacing(float spacing) { spacing_ = spacing; InvalidateLayout(); }
        float GetSpacing() const { return spacing_; }

        float GetDefaultHorizontalStretchWeight() const override { return 1.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 0.0f; }

        Size Measure(const Size& availableSize) override {
            float totalHeight = 0;
            float maxWidth = 0;
            for (auto& child : children_) {
                if (!child->IsVisible()) continue;
                Size childSize = child->Measure(availableSize);
                totalHeight += childSize.height;
                maxWidth = max(maxWidth, childSize.width + child->GetMargin().left + child->GetMargin().right);
                totalHeight += child->GetMargin().top + child->GetMargin().bottom;
            }
            if (!children_.empty()) totalHeight += spacing_ * (children_.size() - 1);
            return Size(width_ > 0 ? width_ : maxWidth, height_ > 0 ? height_ : totalHeight);
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            float y = finalRect.y;
            for (auto& child : children_) {
                if (!child->IsVisible()) continue;
                Thickness margin = child->GetMargin();
                y += margin.top;
                float availW = finalRect.width - margin.left - margin.right;
                float childW = child->GetWidth() > 0 ? child->GetWidth() : availW;
                if (child->GetFillWidth()) childW = availW;
                childW = clamp(childW, child->GetMinWidth(), child->GetMaxWidth());
                Size childSize = child->Measure(Size(childW, FLT_MAX));
                float childH = child->GetHeight() > 0 ? child->GetHeight() : childSize.height;
                if (child->GetFillHeight()) childH = finalRect.height - y;
                childH = clamp(childH, child->GetMinHeight(), child->GetMaxHeight());
                child->Arrange(Rect(finalRect.x + margin.left, y, childW, childH));
                y += childH + margin.bottom + spacing_;
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // 布局容器自身无视觉内容，不绘制子元素（由 Window 合成）
        }

        const std::vector<UIElement*>& GetChildren() const override {
            childrenView_.clear();
            for (auto& child : children_) childrenView_.push_back(child.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            window_ = w;
            for (auto& child : children_) child->AttachWindowRecursive(w);
        }

        UIElement* HitTest(float x, float y) override {
            for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
                if (UIElement* hit = (*it)->HitTest(x, y)) return hit;
            }
            return UIElement::HitTest(x, y);
        }

        void UpdateAnimation(float deltaTime) override {
            for (auto& child : children_) child->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            for (const auto& child : children_) {
                if (child->HasActiveAnimation()) return true;
            }
            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& child : children_) child->ReleaseDeviceResources();
        }

    private:
        std::vector<std::shared_ptr<UIElement>> children_;
        float spacing_;
    };

    // ---------- 水平布局容器 ----------
    class RowBox : public Layout {
    public:
        RowBox() : spacing_(0) {}
        void AddChild(std::shared_ptr<UIElement> child) {
            children_.push_back(child);
            child->SetParent(this);
            InvalidateLayout();
        }
        void SetSpacing(float spacing) { spacing_ = spacing; InvalidateLayout(); }
        float GetSpacing() const { return spacing_; }

        float GetDefaultHorizontalStretchWeight() const override { return 0.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 1.0f; }

        Size Measure(const Size& availableSize) override {
            float totalWidth = 0;
            float maxHeight = 0;
            for (auto& child : children_) {
                if (!child->IsVisible()) continue;
                Size childSize = child->Measure(availableSize);
                totalWidth += childSize.width;
                maxHeight = max(maxHeight, childSize.height + child->GetMargin().top + child->GetMargin().bottom);
                totalWidth += child->GetMargin().left + child->GetMargin().right;
            }
            if (!children_.empty()) totalWidth += spacing_ * (children_.size() - 1);
            return Size(width_ > 0 ? width_ : totalWidth, height_ > 0 ? height_ : maxHeight);
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            float x = finalRect.x;
            for (auto& child : children_) {
                if (!child->IsVisible()) continue;
                Thickness margin = child->GetMargin();
                x += margin.left;
                float availH = finalRect.height - margin.top - margin.bottom;
                float childH = child->GetHeight() > 0 ? child->GetHeight() : availH;
                if (child->GetFillHeight()) childH = availH;
                childH = clamp(childH, child->GetMinHeight(), child->GetMaxHeight());
                Size childSize = child->Measure(Size(FLT_MAX, childH));
                float childW = child->GetWidth() > 0 ? child->GetWidth() : childSize.width;
                if (child->GetFillWidth()) childW = finalRect.width - x;
                childW = clamp(childW, child->GetMinWidth(), child->GetMaxWidth());
                child->Arrange(Rect(x, finalRect.y + margin.top, childW, childH));
                x += childW + margin.right + spacing_;
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // 无自身视觉内容
        }

        const std::vector<UIElement*>& GetChildren() const override {
            childrenView_.clear();
            for (auto& child : children_) childrenView_.push_back(child.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            window_ = w;
            for (auto& child : children_) child->AttachWindowRecursive(w);
        }

        UIElement* HitTest(float x, float y) override {
            for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
                if (UIElement* hit = (*it)->HitTest(x, y)) return hit;
            }
            return UIElement::HitTest(x, y);
        }

        void UpdateAnimation(float deltaTime) override {
            for (auto& child : children_) child->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            for (const auto& child : children_) {
                if (child->HasActiveAnimation()) return true;
            }
            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& child : children_) child->ReleaseDeviceResources();
        }

    private:
        std::vector<std::shared_ptr<UIElement>> children_;
        float spacing_;
    };

    // ---------- 网格布局 ----------
    class GridLayout : public Layout {
    public:
        enum class Alignment { Start, Center, End };

        struct GridItem {
            std::shared_ptr<UIElement> element;
            int row, col;
            int rowSpan, colSpan;
        };

        GridLayout() : horizontalSpacing_(0), verticalSpacing_(0),
            horizontalAlignment_(Alignment::Start), verticalAlignment_(Alignment::Start) {}

        void AddChild(std::shared_ptr<UIElement> child, int row, int col, int rowSpan = 1, int colSpan = 1) {
            GridItem item;
            item.element = child;
            item.row = row;
            item.col = col;
            item.rowSpan = max(1, rowSpan);
            item.colSpan = max(1, colSpan);
            items_.push_back(item);
            child->SetParent(this);
            InvalidateLayout();
        }

        void SetSpacing(float horizontal, float vertical) {
            horizontalSpacing_ = horizontal;
            verticalSpacing_ = vertical;
            InvalidateLayout();
        }

        void SetColumnStretch(int col, float weight) {
            if (col >= 0) {
                if (colStretch_.size() <= col) colStretch_.resize(col + 1, 0.0f);
                colStretch_[col] = weight;
                InvalidateLayout();
            }
        }
        void SetRowStretch(int row, float weight) {
            if (row >= 0) {
                if (rowStretch_.size() <= row) rowStretch_.resize(row + 1, 0.0f);
                rowStretch_[row] = weight;
                InvalidateLayout();
            }
        }

        void SetHorizontalAlignment(Alignment align) { horizontalAlignment_ = align; InvalidateLayout(); }
        void SetVerticalAlignment(Alignment align) { verticalAlignment_ = align; InvalidateLayout(); }

        float GetDefaultHorizontalStretchWeight() const override { return 1.0f; }
        float GetDefaultVerticalStretchWeight() const override { return 1.0f; }

        Size Measure(const Size& availableSize) override {
            int maxRow = 0, maxCol = 0;
            for (auto& item : items_) {
                maxRow = max(maxRow, item.row + item.rowSpan);
                maxCol = max(maxCol, item.col + item.colSpan);
            }
            if (maxRow == 0 || maxCol == 0) return Size(0, 0);

            std::vector<float> rowHeights(maxRow, 0.0f);
            std::vector<float> colWidths(maxCol, 0.0f);

            for (auto& item : items_) {
                if (!item.element->IsVisible()) continue;
                Size childSize = item.element->Measure(availableSize);
                float heightPerRow = childSize.height / item.rowSpan;
                float widthPerCol = childSize.width / item.colSpan;
                for (int r = item.row; r < item.row + item.rowSpan; ++r) rowHeights[r] = max(rowHeights[r], heightPerRow);
                for (int c = item.col; c < item.col + item.colSpan; ++c) colWidths[c] = max(colWidths[c], widthPerCol);
            }

            float totalWidth = (maxCol > 1) ? (maxCol - 1) * horizontalSpacing_ : 0;
            for (float w : colWidths) totalWidth += w;
            float totalHeight = (maxRow > 1) ? (maxRow - 1) * verticalSpacing_ : 0;
            for (float h : rowHeights) totalHeight += h;
            return Size(totalWidth, totalHeight);
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            int maxRow = 0, maxCol = 0;
            for (auto& item : items_) {
                maxRow = max(maxRow, item.row + item.rowSpan);
                maxCol = max(maxCol, item.col + item.colSpan);
            }
            if (maxRow == 0 || maxCol == 0) return;

            std::vector<float> rowMinHeights(maxRow, 0.0f);
            std::vector<float> colMinWidths(maxCol, 0.0f);
            for (auto& item : items_) {
                if (!item.element->IsVisible()) continue;
                Size childSize = item.element->Measure(Size(FLT_MAX, FLT_MAX));
                float hPerRow = childSize.height / item.rowSpan;
                float wPerCol = childSize.width / item.colSpan;
                for (int r = item.row; r < item.row + item.rowSpan; ++r)
                    rowMinHeights[r] = max(rowMinHeights[r], hPerRow);
                for (int c = item.col; c < item.col + item.colSpan; ++c)
                    colMinWidths[c] = max(colMinWidths[c], wPerCol);
            }

            float minTotalHeight = (maxRow > 1) ? (maxRow - 1) * verticalSpacing_ : 0;
            for (float h : rowMinHeights) minTotalHeight += h;
            float minTotalWidth = (maxCol > 1) ? (maxCol - 1) * horizontalSpacing_ : 0;
            for (float w : colMinWidths) minTotalWidth += w;

            float extraHeight = max(0.0f, finalRect.height - minTotalHeight);
            float extraWidth = max(0.0f, finalRect.width - minTotalWidth);

            std::vector<float> rowHeights = rowMinHeights;
            std::vector<float> colWidths = colMinWidths;

            std::vector<float> rowStretchWeights(maxRow, 0.0f);
            std::vector<float> colStretchWeights(maxCol, 0.0f);

            if (!rowStretch_.empty()) {
                for (int r = 0; r < maxRow; ++r)
                    if (r < rowStretch_.size()) rowStretchWeights[r] = rowStretch_[r];
            }
            else {
                for (auto& item : items_) {
                    if (!item.element->IsVisible()) continue;
                    float weight = item.element->GetVerticalStretchWeight();
                    for (int r = item.row; r < item.row + item.rowSpan; ++r)
                        rowStretchWeights[r] = max(rowStretchWeights[r], weight);
                }
            }

            if (!colStretch_.empty()) {
                for (int c = 0; c < maxCol; ++c)
                    if (c < colStretch_.size()) colStretchWeights[c] = colStretch_[c];
            }
            else {
                for (auto& item : items_) {
                    if (!item.element->IsVisible()) continue;
                    float weight = item.element->GetHorizontalStretchWeight();
                    for (int c = item.col; c < item.col + item.colSpan; ++c)
                        colStretchWeights[c] = max(colStretchWeights[c], weight);
                }
            }

            if (extraHeight > 0) {
                float totalWeight = 0.0f;
                for (float w : rowStretchWeights) totalWeight += w;
                if (totalWeight > 0) {
                    for (int r = 0; r < maxRow; ++r) {
                        if (rowStretchWeights[r] > 0)
                            rowHeights[r] += extraHeight * (rowStretchWeights[r] / totalWeight);
                    }
                }
            }
            else if (minTotalHeight > 0) {
                float scale = finalRect.height / minTotalHeight;
                for (auto& h : rowHeights) h *= scale;
            }

            if (extraWidth > 0) {
                float totalWeight = 0.0f;
                for (float w : colStretchWeights) totalWeight += w;
                if (totalWeight > 0) {
                    for (int c = 0; c < maxCol; ++c) {
                        if (colStretchWeights[c] > 0)
                            colWidths[c] += extraWidth * (colStretchWeights[c] / totalWeight);
                    }
                }
            }
            else if (minTotalWidth > 0) {
                float scale = finalRect.width / minTotalWidth;
                for (auto& w : colWidths) w *= scale;
            }

            float offsetX = 0.0f, offsetY = 0.0f;
            if (extraWidth > 0 && colStretchWeights.empty() && maxCol > 1) {
                switch (horizontalAlignment_) {
                case Alignment::Start: offsetX = 0.0f; break;
                case Alignment::Center: offsetX = extraWidth / 2.0f; break;
                case Alignment::End: offsetX = extraWidth; break;
                }
            }
            if (extraHeight > 0 && rowStretchWeights.empty() && maxRow > 1) {
                switch (verticalAlignment_) {
                case Alignment::Start: offsetY = 0.0f; break;
                case Alignment::Center: offsetY = extraHeight / 2.0f; break;
                case Alignment::End: offsetY = extraHeight; break;
                }
            }

            std::vector<float> rowY(maxRow);
            std::vector<float> colX(maxCol);
            float y = finalRect.y + offsetY;
            for (int r = 0; r < maxRow; ++r) {
                rowY[r] = y;
                y += rowHeights[r];
                if (r < maxRow - 1) y += verticalSpacing_;
            }
            float x = finalRect.x + offsetX;
            for (int c = 0; c < maxCol; ++c) {
                colX[c] = x;
                x += colWidths[c];
                if (c < maxCol - 1) x += horizontalSpacing_;
            }

            for (auto& item : items_) {
                if (!item.element->IsVisible()) continue;
                float itemX = colX[item.col];
                float itemY = rowY[item.row];
                float itemW = 0;
                for (int c = item.col; c < item.col + item.colSpan; ++c) {
                    itemW += colWidths[c];
                    if (c < item.col + item.colSpan - 1) itemW += horizontalSpacing_;
                }
                float itemH = 0;
                for (int r = item.row; r < item.row + item.rowSpan; ++r) {
                    itemH += rowHeights[r];
                    if (r < item.row + item.rowSpan - 1) itemH += verticalSpacing_;
                }
                item.element->Arrange(Rect(itemX, itemY, itemW, itemH));
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            // 无自身视觉内容
        }

        const std::vector<UIElement*>& GetChildren() const override {
            childrenView_.clear();
            for (auto& item : items_) childrenView_.push_back(item.element.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            window_ = w;
            for (auto& item : items_) if (item.element) item.element->AttachWindowRecursive(w);
        }

        UIElement* HitTest(float x, float y) override {
            for (auto it = items_.rbegin(); it != items_.rend(); ++it) {
                if (UIElement* hit = it->element->HitTest(x, y)) return hit;
            }
            return UIElement::HitTest(x, y);
        }

        void UpdateAnimation(float deltaTime) override {
            for (auto& item : items_) item.element->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            for (const auto& item : items_) {
                if (item.element->HasActiveAnimation()) return true;
            }
            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& item : items_) item.element->ReleaseDeviceResources();
        }

    private:
        std::vector<GridItem> items_;
        float horizontalSpacing_;
        float verticalSpacing_;
        std::vector<float> colStretch_;
        std::vector<float> rowStretch_;
        Alignment horizontalAlignment_;
        Alignment verticalAlignment_;
    };

    // ---------- 可替换布局容器基类 ----------
    class LayoutHost : public UIElement {
    public:
        LayoutHost() {
            auto grid = std::make_shared<GridLayout>();
            grid->SetParent(this);
            layout_ = grid;
        }
        virtual ~LayoutHost() = default;

        std::shared_ptr<UIElement> GetLayout() const { return layout_; }

        void SetLayout(std::shared_ptr<UIElement> layout) {
            if (!layout || layout.get() == this) return;
            layout_ = layout;
            layout_->SetParent(this);
            InvalidateLayout();
        }

        template<typename T>
        std::shared_ptr<T> GetLayoutAs() const {
            return std::dynamic_pointer_cast<T>(layout_);
        }

        const std::vector<UIElement*>& GetChildren() const override {
            childrenView_.clear();
            if (layout_) childrenView_.push_back(layout_.get());
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            window_ = w;
            if (layout_) layout_->AttachWindowRecursive(w);
        }

        bool UseCache() const override { return false; }

    protected:
        std::shared_ptr<UIElement> layout_;
    };

    // ---------- 卡片控件 ----------
    class Card : public LayoutHost {
    public:
        inline static float DefaultPadding = 12.0f;
        inline static float DefaultCornerRadius = 8.0f;
        inline static Color DefaultBgColor = Color::FromArgb(255, 255, 255, 255);
        inline static Color DefaultBorderColor = Color::FromArgb(255, 200, 200, 200);
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        Card() : padding_(DefaultPadding), cornerRadius_(DefaultCornerRadius),
            bgColor_(DefaultBgColor), borderColor_(DefaultBorderColor),
            hoverBgColor_(DefaultBgColor), hoverBorderColor_(DefaultBorderColor) {}

        void SetPadding(float padding) { padding_ = padding; InvalidateLayout(); }
        void SetCornerRadius(float radius) { cornerRadius_ = radius; RequestRepaint(); }
        void SetBackgroundColor(Color color) { bgColor_ = color; bgBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color; borderBrush_.Reset(); RequestRepaint(); }
        void SetHoverBackgroundColor(Color color) { hoverBgColor_ = color; RequestRepaint(); }
        void SetHoverBorderColor(Color color) { hoverBorderColor_ = color; RequestRepaint(); }
        void SetHoverAnimationSpeed(float speed) { hoverSpeed_ = speed; }

        static void SetDefaultPadding(float padding) { DefaultPadding = padding; }
        static void SetDefaultCornerRadius(float radius) { DefaultCornerRadius = radius; }
        static void SetDefaultBgColor(Color color) { DefaultBgColor = color; }
        static void SetDefaultBorderColor(Color color) { DefaultBorderColor = color; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        Size Measure(const Size& availableSize) override {
            if (!layout_) return Size(0, 0);
            Size childSize = layout_->Measure(Size(availableSize.width - padding_ * 2, availableSize.height - padding_ * 2));
            return Size(childSize.width + padding_ * 2, childSize.height + padding_ * 2);
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            if (layout_) {
                layout_->Arrange(Rect(finalRect.x + padding_, finalRect.y + padding_, finalRect.width - padding_ * 2, finalRect.height - padding_ * 2));
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            bool en = IsEffectivelyEnabled();

            D2D1_COLOR_F bg = en ? D2D1::ColorF(
                bgColor_.r + (hoverBgColor_.r - bgColor_.r) * hoverProgress_,
                bgColor_.g + (hoverBgColor_.g - bgColor_.g) * hoverProgress_,
                bgColor_.b + (hoverBgColor_.b - bgColor_.b) * hoverProgress_,
                bgColor_.a + (hoverBgColor_.a - bgColor_.a) * hoverProgress_)
                : D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
            if (!bgBrush_) rt->CreateSolidColorBrush(bg, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(bg);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), cornerRadius_, cornerRadius_), bgBrush_.Get());

            D2D1_COLOR_F bd = en ? D2D1::ColorF(
                borderColor_.r + (hoverBorderColor_.r - borderColor_.r) * hoverProgress_,
                borderColor_.g + (hoverBorderColor_.g - borderColor_.g) * hoverProgress_,
                borderColor_.b + (hoverBorderColor_.b - borderColor_.b) * hoverProgress_,
                borderColor_.a + (hoverBorderColor_.a - borderColor_.a) * hoverProgress_)
                : D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);
            if (!borderBrush_) rt->CreateSolidColorBrush(bd, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(bd);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), cornerRadius_, cornerRadius_), borderBrush_.Get(), 1.0f);

            // 不再绘制 layout_，子元素由 Window 合成
        }

        void OnMouseEnter() override { hovered_ = true; RequestRepaint(); if (MouseEnterHandler) MouseEnterHandler(); }
        void OnMouseLeave() override { hovered_ = false; RequestRepaint(); if (MouseLeaveHandler) MouseLeaveHandler(); }

        UIElement* HitTest(float x, float y) override {
            if (visible_ && arrangedRect_.Contains(x, y)) {
                if (layout_) if (UIElement* hit = layout_->HitTest(x, y)) return hit;
                return this;
            }
            return nullptr;
        }

        void UpdateAnimation(float deltaTime) override {
            float target = hovered_ ? 1.0f : 0.0f;
            if (hoverProgress_ < target) { hoverProgress_ += hoverSpeed_ * deltaTime; if (hoverProgress_ > target) hoverProgress_ = target; RequestRepaint(); }
            else if (hoverProgress_ > target) { hoverProgress_ -= hoverSpeed_ * deltaTime; if (hoverProgress_ < target) hoverProgress_ = target; RequestRepaint(); }
            if (layout_) layout_->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            bool hoverAnim = (hovered_ ? (hoverProgress_ < 0.999f) : (hoverProgress_ > 0.001f));
            return hoverAnim || (layout_ ? layout_->HasActiveAnimation() : false);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            borderBrush_.Reset();
            UIElement::ReleaseDeviceResources();
            if (layout_) layout_->ReleaseDeviceResources();
        }

        bool UseCache() const override { return HasShadow(); }

    private:
        float padding_;
        float cornerRadius_;
        Color bgColor_;
        Color borderColor_;
        Color hoverBgColor_;
        Color hoverBorderColor_;
        float hoverProgress_ = 0.0f;
        bool hovered_ = false;
        float hoverSpeed_ = 8.0f;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
    };

    // ---------- 页面（Page） ----------
    class Page : public LayoutHost {
    public:
        inline static float DefaultPadding = 10.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        Page() : padding_(DefaultPadding), backgroundColor_(Color(0, 0, 0, 0)) {}

        void SetPadding(float padding) { padding_ = padding; InvalidateLayout(); }
        void SetBackgroundColor(Color color) { backgroundColor_ = color; bgBrush_.Reset(); RequestRepaint(); }

        static void SetDefaultPadding(float padding) { DefaultPadding = padding; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        Size Measure(const Size& availableSize) override {
            if (!layout_) return Size(0, 0);
            Size childSize = layout_->Measure(Size(availableSize.width - padding_ * 2, availableSize.height - padding_ * 2));
            return Size(childSize.width + padding_ * 2, childSize.height + padding_ * 2);
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            if (layout_) {
                layout_->Arrange(Rect(finalRect.x + padding_, finalRect.y + padding_, finalRect.width - padding_ * 2, finalRect.height - padding_ * 2));
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            if (backgroundColor_.a > 0.0f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_.ToD2D(), bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(backgroundColor_.ToD2D());
                if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get());
            }
            // 不绘制 layout_
        }

        UIElement* HitTest(float x, float y) override {
            if (visible_ && arrangedRect_.Contains(x, y)) {
                if (layout_) if (UIElement* hit = layout_->HitTest(x, y)) return hit;
                return this;
            }
            return nullptr;
        }

        void UpdateAnimation(float deltaTime) override {
            if (layout_) layout_->UpdateAnimation(deltaTime);
        }

        bool HasActiveAnimation() const override {
            return layout_ ? layout_->HasActiveAnimation() : false;
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            UIElement::ReleaseDeviceResources();
            if (layout_) layout_->ReleaseDeviceResources();
        }

        bool UseCache() const override { return false; }

    private:
        float padding_;
        Color backgroundColor_;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
    };

    // ---------- 页面宿主（PageHost） ----------
    class PageHost : public UIElement {
    public:
        enum class TransitionDirection { Left, Right, Up, Down };
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        PageHost() : currentIndex_(-1), animating_(false), animProgress_(0.0f),
            fromIndex_(-1), toIndex_(-1), direction_(TransitionDirection::Left),
            animDuration_(0.3f) {
            width_ = 0; height_ = 0;
            minWidth_ = 100.0f;
            minHeight_ = 100.0f;
            fillWidth_ = true;
            fillHeight_ = true;
        }

        void AddPage(std::shared_ptr<Page> page) {
            if (!page) return;
            pages_.push_back(page);
            page->SetParent(this);
            if (currentIndex_ == -1) currentIndex_ = 0;
            InvalidateLayout();
        }

        void NavigateTo(int index) {
            if (index < 0 || index >= (int)pages_.size() || index == currentIndex_) return;
            fromIndex_ = currentIndex_;
            toIndex_ = index;
            animating_ = true;
            animProgress_ = 0.0f;
            RequestRepaint(); // 动画开始需要重绘
            InvalidateLayout(); // 页面切换可能影响布局？通常不影响，但为安全
        }

        void SetTransitionDirection(TransitionDirection dir) { direction_ = dir; }
        void SetAnimationDuration(float seconds) { animDuration_ = max(0.01f, seconds); }

        int GetCurrentIndex() const { return currentIndex_; }
        std::shared_ptr<Page> GetCurrentPage() const {
            if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size())
                return pages_[currentIndex_];
            return nullptr;
        }

        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }

        // PageHost 不使用缓存，因为动画期间内容变化频繁
        bool UseCache() const override { return false; }

        Size Measure(const Size& availableSize) override {
            Size result;
            if (width_ > 0) result.width = width_;
            else if (availableSize.width != FLT_MAX) result.width = availableSize.width;
            else result.width = max(minWidth_, 0.0f);

            if (height_ > 0) result.height = height_;
            else if (availableSize.height != FLT_MAX) result.height = availableSize.height;
            else result.height = max(minHeight_, 0.0f);
            return result;
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            for (auto& page : pages_) page->Arrange(finalRect);
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_ || pages_.empty()) return;
            rt->PushAxisAlignedClip(arrangedRect_.ToD2D(), D2D1_ANTIALIAS_MODE_ALIASED);

            if (!animating_) {
                if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size())
                    pages_[currentIndex_]->Draw(rt);
            }
            else {
                float t = clamp(animProgress_, 0.0f, 1.0f);
                float w = arrangedRect_.width, h = arrangedRect_.height;
                float oldOffsetX = 0, oldOffsetY = 0, newOffsetX = 0, newOffsetY = 0;
                switch (direction_) {
                case TransitionDirection::Left: oldOffsetX = -w * t; newOffsetX = w * (1 - t); break;
                case TransitionDirection::Right: oldOffsetX = w * t; newOffsetX = -w * (1 - t); break;
                case TransitionDirection::Up: oldOffsetY = -h * t; newOffsetY = h * (1 - t); break;
                case TransitionDirection::Down: oldOffsetY = h * t; newOffsetY = -h * (1 - t); break;
                }

                D2D1::Matrix3x2F oldTransform;
                rt->GetTransform(&oldTransform);

                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size()) {
                    rt->SetTransform(D2D1::Matrix3x2F::Translation(oldOffsetX, oldOffsetY));
                    pages_[fromIndex_]->Draw(rt);
                }
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size()) {
                    rt->SetTransform(D2D1::Matrix3x2F::Translation(newOffsetX, newOffsetY));
                    pages_[toIndex_]->Draw(rt);
                }
                rt->SetTransform(oldTransform);
            }

            rt->PopAxisAlignedClip();
        }

        const std::vector<UIElement*>& GetChildren() const override {
            childrenView_.clear();
            if (animating_) {
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size())
                    childrenView_.push_back(pages_[fromIndex_].get());
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size())
                    childrenView_.push_back(pages_[toIndex_].get());
            }
            else if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                childrenView_.push_back(pages_[currentIndex_].get());
            }
            return childrenView_;
        }

        void AttachWindowRecursive(Window* w) override {
            window_ = w;
            for (auto& p : pages_) if (p) p->AttachWindowRecursive(w);
        }

        std::optional<D2D1_RECT_F> GetClipRect() const override {
            // 裁剪到自身区域
            return arrangedRect_.ToD2D();
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_ || !arrangedRect_.Contains(x, y)) return nullptr;

            if (animating_) {
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size()) {
                    UIElement* hit = pages_[toIndex_]->HitTest(x, y);
                    if (hit) return hit;
                }
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size()) {
                    UIElement* hit = pages_[fromIndex_]->HitTest(x, y);
                    if (hit) return hit;
                }
                return this;
            }

            if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                UIElement* hit = pages_[currentIndex_]->HitTest(x, y);
                if (hit) return hit;
            }
            return this;
        }

        void UpdateAnimation(float deltaTime) override {
#ifdef ZUI_DEBUG
            // 调试：当自身动画结束但整体仍为 true 时，打印子页面状态
            if (!animating_ && animProgress_ == 0.0f) {
                for (size_t i = 0; i < pages_.size(); ++i) {
                    if (pages_[i] && pages_[i]->HasActiveAnimation()) {
                        wchar_t buf[256];
                        swprintf(buf, 256, L"  Page[%zu] has animation\n", i);
                        ZUI_DEBUG_LOG_W(buf);
                    }
                }
            }

            // 如果动画已结束但仍有子元素在动画，打印子元素状态
            if (!animating_ && animProgress_ >= 1.0f) {
                for (auto& page : pages_) {
                    if (page && page->HasActiveAnimation()) {
                        // 递归打印子元素动画状态（简化版，只打印一层）
                        wchar_t msg[256];
                        swprintf(msg, 256, L"  Sub-page has animation: %s\n", typeid(*page).name());
                        ZUI_DEBUG_LOG_W(msg);
                        // 如果有 layout，继续检查
                        if (auto* layout = dynamic_cast<LayoutHost*>(page.get())) {
                            if (auto* inner = layout->GetLayout().get()) {
                                if (inner->HasActiveAnimation()) {
                                    swprintf(msg, 256, L"    Layout has animation: %s\n", typeid(*inner).name());
                                    ZUI_DEBUG_LOG_W(msg);
                                }
                            }
                        }
                    }
                }
            }
#endif

            if (animating_) {
                animProgress_ += deltaTime / animDuration_;
                // 强制收敛：若进度接近 1.0（浮点误差），立即完成动画
                const float epsilon = 0.0001f;
                if (animProgress_ >= 1.0f - epsilon) {
                    animProgress_ = 1.0f;
                    animating_ = false;
                    currentIndex_ = toIndex_;
                    fromIndex_ = -1;
                    toIndex_ = -1;
                }
                // 动画过程中强制重绘子页面，确保缓存更新
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size())
                    pages_[fromIndex_]->RequestRepaint();
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size())
                    pages_[toIndex_]->RequestRepaint();
                RequestRepaint(); // 页面容器也需要重绘
            }
            // 更新页面动画：动画中只更新源/目标页，否则只更新当前页。
            // 注意：动画期间 currentIndex_ == fromIndex_，若三条路径都走会导致同一页每帧被更新两次、速度翻倍。
            if (animating_) {
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size())
                    pages_[fromIndex_]->UpdateAnimation(deltaTime);
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size())
                    pages_[toIndex_]->UpdateAnimation(deltaTime);
            }
            else if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                pages_[currentIndex_]->UpdateAnimation(deltaTime);
            }

            if (!animating_ && animProgress_ >= 1.0f) {
                // 动画结束，释放非当前页面的缓存
                for (size_t i = 0; i < pages_.size(); ++i) {
                    if (i != currentIndex_ && pages_[i]) {
                        pages_[i]->ReleaseDeviceResources();
                    }
                }
            }
        }

        bool HasActiveAnimation() const override {
            if (animating_) return true;  // 自身过渡动画

            // 检查当前页面
            if (currentIndex_ >= 0 && currentIndex_ < (int)pages_.size()) {
                if (pages_[currentIndex_]->HasActiveAnimation()) return true;
            }

            // 如果正在过渡，额外检查源页面和目标页面
            if (animating_) {
                if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size() &&
                    pages_[fromIndex_]->HasActiveAnimation()) return true;
                if (toIndex_ >= 0 && toIndex_ < (int)pages_.size() &&
                    pages_[toIndex_]->HasActiveAnimation()) return true;
            }

            return false;
        }

        void ReleaseDeviceResources() override {
            UIElement::ReleaseDeviceResources();
            for (auto& page : pages_) page->ReleaseDeviceResources();
        }

        D2D1::Matrix3x2F GetChildRenderTransform(UIElement* child) const override {
            // 非动画状态，所有子元素无额外变换
            if (!animating_) return D2D1::Matrix3x2F::Identity();

            float t = clamp(animProgress_, 0.0f, 1.0f);
            float w = arrangedRect_.width;
            float h = arrangedRect_.height;
            float offsetX = 0.0f, offsetY = 0.0f;

            // 判断子元素是源页面还是目标页面，并计算对应偏移
            if (fromIndex_ >= 0 && fromIndex_ < (int)pages_.size() && child == pages_[fromIndex_].get()) {
                // 源页面逐渐移出
                switch (direction_) {
                case TransitionDirection::Left:  offsetX = -w * t; break;
                case TransitionDirection::Right: offsetX = w * t; break;
                case TransitionDirection::Up:    offsetY = -h * t; break;
                case TransitionDirection::Down:  offsetY = h * t; break;
                }
            }
            else if (toIndex_ >= 0 && toIndex_ < (int)pages_.size() && child == pages_[toIndex_].get()) {
                // 目标页面逐渐移入
                switch (direction_) {
                case TransitionDirection::Left:  offsetX = w * (1.0f - t); break;
                case TransitionDirection::Right: offsetX = -w * (1.0f - t); break;
                case TransitionDirection::Up:    offsetY = h * (1.0f - t); break;
                case TransitionDirection::Down:  offsetY = -h * (1.0f - t); break;
                }
            }
            // 其他子元素（理论上动画期间不会出现）返回单位矩阵
            return D2D1::Matrix3x2F::Translation(offsetX, offsetY);
        }

    private:
        std::vector<std::shared_ptr<Page>> pages_;
        int currentIndex_;
        bool animating_;
        float animProgress_;
        int fromIndex_;
        int toIndex_;
        TransitionDirection direction_;
        float animDuration_;
    };

    // ========== 右键菜单 ==========
    class MenuItem {
    public:
        enum class Type { Normal, Separator, Submenu };
        std::wstring text;
        std::function<void()> callback;
        std::shared_ptr<Menu> submenu;
        Type type = Type::Normal;
        bool enabled = true;
        std::shared_ptr<Label> icon; // 前向声明即可
    };

    class Menu : public std::enable_shared_from_this<Menu> {
    public:
        void AddItem(const std::wstring& text, std::function<void()> callback = nullptr) {
            auto item = std::make_shared<MenuItem>();
            item->type = MenuItem::Type::Normal;
            item->text = text;
            item->callback = callback;
            items.push_back(item);
        }
        void AddSeparator() {
            auto item = std::make_shared<MenuItem>();
            item->type = MenuItem::Type::Separator;
            items.push_back(item);
        }
        void AddSubmenu(const std::wstring& text, std::shared_ptr<Menu> submenu) {
            auto item = std::make_shared<MenuItem>();
            item->type = MenuItem::Type::Submenu;
            item->text = text;
            item->submenu = submenu;
            items.push_back(item);
        }
        std::vector<std::shared_ptr<MenuItem>> items;
    };

    class MenuWindow {
    public:
        MenuWindow(std::shared_ptr<Menu> menu, HWND owner, int x, int y)
            : menu_(menu), owner_(owner), screenX_(x), screenY_(y) {
            dpi_ = GetDpiForWindow(owner_);
            if (dpi_ == 0) dpi_ = GetDpiForSystem();
            if (dpi_ == 0) dpi_ = 96;
            CreateWindowResources();
        }

        ~MenuWindow() {
            if (hwnd_ && IsWindow(hwnd_))
                DestroyWindow(hwnd_);
            DiscardDeviceResources();
        }

        void Show(int x, int y) {
            if (!hwnd_) return;
            if (visible_) return;

            screenX_ = x;
            screenY_ = y;

            if (animating_) {
                KillTimer(hwnd_, animTimerId_);
                animating_ = false;
            }

            SetWindowPos(hwnd_, nullptr, screenX_, screenY_, windowWidthPx_, 0,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

            animating_ = true;
            currentHeightPx_ = 0;
            targetHeightPx_ = windowHeightPx_;
            visible_ = true;

            SetTimer(hwnd_, animTimerId_, 10, nullptr);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void Hide() {
            if (!hwnd_ || !IsWindow(hwnd_)) return;
            if (animating_) {
                KillTimer(hwnd_, animTimerId_);
                animating_ = false;
            }
            ShowWindow(hwnd_, SW_HIDE);
            visible_ = false;
            KillTimer(hwnd_, kSubmenuTimerId);
            KillTimer(hwnd_, kSubmenuHideTimerId);
            if (childMenu_) childMenu_->Hide();
        }

        void CloseAll() {
            if (animating_) {
                KillTimer(hwnd_, animTimerId_);
                animating_ = false;
            }
            visible_ = false;
            if (childMenu_) {
                childMenu_->CloseAll();
                childMenu_.reset();
            }
            if (hwnd_ && IsWindow(hwnd_)) {
                DestroyWindow(hwnd_);
                hwnd_ = nullptr;
            }
        }

    private:
        static constexpr int kSubmenuDelayMs = 300;
        static constexpr int kSubmenuHideDelayMs = 300;
        static constexpr UINT_PTR kSubmenuTimerId = 1;
        static constexpr UINT_PTR kSubmenuHideTimerId = 3;
        static constexpr UINT_PTR animTimerId_ = 4;

        int itemHeightDip_ = 30;
        int separatorHeightDip_ = 9;
        int paddingDip_ = 6;
        int arrowWidthDip_ = 20;
        float cornerRadiusDip_ = 12.0f;

        bool animating_ = false;
        int currentHeightPx_ = 0;
        int targetHeightPx_ = 0;
        bool visible_ = false;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            MenuWindow* self = nullptr;
            if (msg == WM_NCCREATE) {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                self = reinterpret_cast<MenuWindow*>(cs->lpCreateParams);
                self->hwnd_ = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else {
                self = reinterpret_cast<MenuWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            }
            if (self) return self->HandleMessage(msg, wParam, lParam);
            return DefWindowProc(hwnd, msg, wParam, lParam);
        }

        LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
            switch (msg) {
            case WM_PAINT: OnPaint(); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_MOUSEMOVE: OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
            case WM_LBUTTONDOWN: {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                RECT client;
                GetClientRect(hwnd_, &client);
                if (!PtInRect(&client, pt)) {
                    POINT screenPt = pt;
                    ClientToScreen(hwnd_, &screenPt);
                    CloseAll();
                    ScreenToClient(owner_, &screenPt);
                    SendMessage(owner_, WM_LBUTTONDOWN, wParam, MAKELPARAM(screenPt.x, screenPt.y));
                    SendMessage(owner_, WM_LBUTTONUP, wParam, MAKELPARAM(screenPt.x, screenPt.y));
                    return 0;
                }
                OnMouseDown(pt.x, pt.y);
                return 0;
            }
            case WM_LBUTTONUP: OnMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); return 0;
            case WM_MOUSELEAVE: OnMouseLeave(); return 0;
            case WM_CAPTURECHANGED: pressedIndex_ = -1; return 0;
            case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
            case WM_NCACTIVATE: return FALSE;
            case WM_TIMER:
                if (wParam == kSubmenuTimerId) {
                    KillTimer(hwnd_, kSubmenuTimerId);
                    if (submenuPendingIndex_ >= 0) {
                        OpenSubmenu(submenuPendingIndex_);
                        submenuPendingIndex_ = -1;
                    }
                }
                else if (wParam == kSubmenuHideTimerId) {
                    KillTimer(hwnd_, kSubmenuHideTimerId);
                    POINT pt;
                    GetCursorPos(&pt);
                    if (!IsPointInMenuTree(pt)) {
                        if (childMenu_) childMenu_->Hide();
                    }
                    else {
                        SetTimer(hwnd_, kSubmenuHideTimerId, kSubmenuHideDelayMs, nullptr);
                    }
                }
                else if (wParam == animTimerId_) {
                    HandleAnimationTimer();
                }
                return 0;
            case WM_DPICHANGED:
                dpi_ = HIWORD(wParam);
                if (dpi_ == 0) dpi_ = 96;
                DiscardDeviceResources();
                CreateWindowResources();
                SetWindowPos(hwnd_, nullptr, screenX_, screenY_, windowWidthPx_, windowHeightPx_, SWP_NOZORDER);
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case WM_DESTROY:
                if (animating_) {
                    KillTimer(hwnd_, animTimerId_);
                    animating_ = false;
                }
                KillTimer(hwnd_, kSubmenuTimerId);
                KillTimer(hwnd_, kSubmenuHideTimerId);
                DiscardDeviceResources();
                return 0;
            }
            return DefWindowProc(hwnd_, msg, wParam, lParam);
        }

        void HandleAnimationTimer() {
            if (!animating_) { KillTimer(hwnd_, animTimerId_); return; }
            int step = max(1, targetHeightPx_ / 10);
            currentHeightPx_ += step;
            if (currentHeightPx_ >= targetHeightPx_) {
                currentHeightPx_ = targetHeightPx_;
                animating_ = false;
                KillTimer(hwnd_, animTimerId_);
            }
            SetWindowPos(hwnd_, nullptr, screenX_, screenY_, windowWidthPx_, currentHeightPx_,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        bool IsPointInMenuTree(POINT ptScreen) {
            RECT rc;
            GetWindowRect(hwnd_, &rc);
            if (PtInRect(&rc, ptScreen)) return true;
            if (childMenu_ && childMenu_->IsPointInMenuTree(ptScreen)) return true;
            return false;
        }

        void CreateWindowResources() {
            static bool classRegistered = false;
            if (!classRegistered) {
                WNDCLASSEXW wc = {};
                wc.cbSize = sizeof(WNDCLASSEXW);
                wc.lpfnWndProc = MenuWindow::WndProc;
                wc.hInstance = GetModuleHandle(nullptr);
                wc.lpszClassName = L"ZUI_MenuWindow";
                wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
                wc.hbrBackground = nullptr;
                wc.style = CS_DROPSHADOW;
                RegisterClassExW(&wc);
                classRegistered = true;
            }

            if (!sharedDWriteFactory_) {
                DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &sharedDWriteFactory_);
            }
            if (sharedDWriteFactory_) {
                sharedDWriteFactory_->CreateTextFormat(
                    L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                    14.0f, L"en-us", &textFormat_);
                if (textFormat_) {
                    textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                    textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                    textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                }
            }

            CalculateWindowSizeDip();
            windowWidthPx_ = MulDiv(windowWidthDip_, dpi_, 96);
            windowHeightPx_ = MulDiv(windowHeightDip_, dpi_, 96);
            AdjustPositionToScreen(screenX_, screenY_, windowWidthPx_, windowHeightPx_);

            hwnd_ = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                L"ZUI_MenuWindow", L"",
                WS_POPUP,
                screenX_, screenY_, windowWidthPx_, windowHeightPx_,
                owner_, nullptr, GetModuleHandle(nullptr), this);
            if (!hwnd_) return;

            SetRoundCornerRegion();

            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory_);
            if (d2dFactory_) {
                RECT rc;
                GetClientRect(hwnd_, &rc);
                D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
                D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                    D2D1_RENDER_TARGET_TYPE_DEFAULT,
                    D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
                    (FLOAT)dpi_, (FLOAT)dpi_);
                d2dFactory_->CreateHwndRenderTarget(
                    props,
                    D2D1::HwndRenderTargetProperties(hwnd_, size),
                    &renderTarget_);
                if (renderTarget_) {
                    renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                }
            }

            if (renderTarget_) {
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &bgBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.85f, 0.85f, 0.85f), &hoverBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &textBrush_);
                renderTarget_->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.8f, 0.8f), &separatorBrush_);
            }
        }

        void SetRoundCornerRegion() {
            int radiusPx = MulDiv((int)cornerRadiusDip_, dpi_, 96);
            HRGN hRgn = CreateRoundRectRgn(0, 0, windowWidthPx_ + 1, windowHeightPx_ + 1,
                radiusPx, radiusPx);
            SetWindowRgn(hwnd_, hRgn, TRUE);
        }

        void DiscardDeviceResources() {
            if (renderTarget_) { renderTarget_->Release(); renderTarget_ = nullptr; }
            if (bgBrush_) { bgBrush_->Release(); bgBrush_ = nullptr; }
            if (hoverBrush_) { hoverBrush_->Release(); hoverBrush_ = nullptr; }
            if (textBrush_) { textBrush_->Release(); textBrush_ = nullptr; }
            if (separatorBrush_) { separatorBrush_->Release(); separatorBrush_ = nullptr; }
            if (textFormat_) { textFormat_->Release(); textFormat_ = nullptr; }
            if (d2dFactory_) { d2dFactory_->Release(); d2dFactory_ = nullptr; }
        }

        void CalculateWindowSizeDip() {
            int maxTextWidth = 0;
            windowHeightDip_ = paddingDip_ * 2;
            for (auto& item : menu_->items) {
                if (item->type == MenuItem::Type::Separator) {
                    windowHeightDip_ += separatorHeightDip_;
                    continue;
                }
                ComPtr<IDWriteTextLayout> layout;
                if (sharedDWriteFactory_ && textFormat_) {
                    sharedDWriteFactory_->CreateTextLayout(
                        item->text.c_str(), (UINT32)item->text.length(),
                        textFormat_, 10000.0f, 10000.0f, &layout);
                    if (layout) {
                        DWRITE_TEXT_METRICS metrics;
                        layout->GetMetrics(&metrics);
                        int width = static_cast<int>(metrics.width + 0.5f);
                        if (item->type == MenuItem::Type::Submenu) width += arrowWidthDip_;
                        maxTextWidth = max(maxTextWidth, width);
                    }
                }
                windowHeightDip_ += itemHeightDip_;
            }
            windowWidthDip_ = paddingDip_ * 2 + maxTextWidth + 24;
            windowWidthDip_ = max(windowWidthDip_, 60);
            windowHeightDip_ = max(windowHeightDip_, 34);
        }

        void AdjustPositionToScreen(int& x, int& y, int width, int height) {
            RECT workArea;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
            if (x + width > workArea.right) x = workArea.right - width;
            if (y + height > workArea.bottom) y = workArea.bottom - height;
            if (x < workArea.left) x = workArea.left;
            if (y < workArea.top) y = workArea.top;
        }

        void OnPaint() {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            if (renderTarget_) {
                SetGlobalDpiScale(dpi_ / 96.0f);   // 线程本地缩放：菜单窗口用自己的 DPI
                renderTarget_->BeginDraw();
                renderTarget_->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
                DrawMenu();
                renderTarget_->EndDraw();
            }
            EndPaint(hwnd_, &ps);
        }

        void DrawMenu() {
            if (!renderTarget_ || !bgBrush_ || !textFormat_) return;

            D2D1_ROUNDED_RECT bgRect = D2D1::RoundedRect(
                D2D1::RectF(0, 0, (FLOAT)windowWidthDip_, (FLOAT)windowHeightDip_),
                cornerRadiusDip_, cornerRadiusDip_);
            renderTarget_->FillRoundedRectangle(bgRect, bgBrush_);

            float y = (float)paddingDip_;
            for (int i = 0; i < (int)menu_->items.size(); ++i) {
                auto& item = menu_->items[i];
                if (item->type == MenuItem::Type::Separator) {
                    D2D1_POINT_2F p1 = D2D1::Point2F(paddingDip_ + 10, y + separatorHeightDip_ / 2);
                    D2D1_POINT_2F p2 = D2D1::Point2F(windowWidthDip_ - paddingDip_ - 10, y + separatorHeightDip_ / 2);
                    renderTarget_->DrawLine(p1, p2, separatorBrush_, 1.0f);
                    y += separatorHeightDip_;
                    continue;
                }

                D2D1_RECT_F itemRect = D2D1::RectF(
                    paddingDip_ + 2, y,
                    windowWidthDip_ - paddingDip_ - 2, y + itemHeightDip_);
                if (i == hoveredIndex_ || i == pressedIndex_) {
                    renderTarget_->FillRoundedRectangle(
                        D2D1::RoundedRect(itemRect, cornerRadiusDip_ * 0.6f, cornerRadiusDip_ * 0.6f),
                        hoverBrush_);
                }

                float textX = paddingDip_ + 14;
                if (item->icon) textX += 20;
                float textRight = itemRect.right - 4;
                if (item->type == MenuItem::Type::Submenu) {
                    textRight = itemRect.right - arrowWidthDip_ - 2;
                }
                D2D1_RECT_F textRect = D2D1::RectF(textX, y, textRight, y + itemHeightDip_);
                if (!item->text.empty()) {
                    renderTarget_->DrawText(item->text.c_str(), (UINT32)item->text.length(),
                        textFormat_, textRect, textBrush_);
                }

                if (item->type == MenuItem::Type::Submenu) {
                    float arrowRight = itemRect.right - 8;
                    D2D1_POINT_2F arrowCenter = D2D1::Point2F(
                        arrowRight - arrowWidthDip_ / 2 + 2, y + itemHeightDip_ / 2);
                    D2D1_POINT_2F p1 = D2D1::Point2F(arrowCenter.x - 3, arrowCenter.y - 5);
                    D2D1_POINT_2F p2 = D2D1::Point2F(arrowCenter.x - 3, arrowCenter.y + 5);
                    D2D1_POINT_2F p3 = D2D1::Point2F(arrowCenter.x + 2, arrowCenter.y);
                    renderTarget_->DrawLine(p1, p2, textBrush_, 1.0f);
                    renderTarget_->DrawLine(p2, p3, textBrush_, 1.0f);
                    renderTarget_->DrawLine(p3, p1, textBrush_, 1.0f);
                }
                y += itemHeightDip_;
            }
        }

        void OnMouseMove(int x, int y) {
            if (!hwnd_) return;
            int dipX = MulDiv(x, 96, dpi_);
            int dipY = MulDiv(y, 96, dpi_);
            int oldHover = hoveredIndex_;
            hoveredIndex_ = HitTestDip(dipX, dipY);

            if (hoveredIndex_ != oldHover) {
                if (submenuPendingIndex_ >= 0) {
                    KillTimer(hwnd_, kSubmenuTimerId);
                    submenuPendingIndex_ = -1;
                }

                if (childMenu_) {
                    bool hoverOnSubmenuItem = (hoveredIndex_ >= 0 && hoveredIndex_ < (int)menu_->items.size()
                        && menu_->items[hoveredIndex_]->type == MenuItem::Type::Submenu);
                    if (!hoverOnSubmenuItem) {
                        KillTimer(hwnd_, kSubmenuHideTimerId);
                        SetTimer(hwnd_, kSubmenuHideTimerId, kSubmenuHideDelayMs, nullptr);
                    }
                    else {
                        KillTimer(hwnd_, kSubmenuHideTimerId);
                    }
                }

                if (hoveredIndex_ >= 0 && hoveredIndex_ < (int)menu_->items.size()) {
                    auto& item = menu_->items[hoveredIndex_];
                    if (item->type == MenuItem::Type::Submenu && item->submenu) {
                        submenuPendingIndex_ = hoveredIndex_;
                        SetTimer(hwnd_, kSubmenuTimerId, kSubmenuDelayMs, nullptr);
                    }
                }

                InvalidateRect(hwnd_, nullptr, FALSE);
            }

            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd_, 0 };
            TrackMouseEvent(&tme);
        }

        void OnMouseLeave() {
            if (!hwnd_) return;
            hoveredIndex_ = -1;
            pressedIndex_ = -1;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void OnMouseDown(int x, int y) {
            if (!hwnd_) return;
            int dipX = MulDiv(x, 96, dpi_);
            int dipY = MulDiv(y, 96, dpi_);
            pressedIndex_ = HitTestDip(dipX, dipY);
            if (pressedIndex_ >= 0) {
                SetCapture(hwnd_);
            }
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void OnMouseUp(int x, int y) {
            if (!hwnd_) return;
            int dipX = MulDiv(x, 96, dpi_);
            int dipY = MulDiv(y, 96, dpi_);
            int idx = HitTestDip(dipX, dipY);
            if (idx >= 0 && idx == pressedIndex_) {
                auto& item = menu_->items[idx];
                if (item->type == MenuItem::Type::Normal) {
                    auto callback = item->callback;
                    pressedIndex_ = -1;
                    ReleaseCapture();
                    CloseAll();
                    if (callback) callback();
                    return;
                }
                else if (item->type == MenuItem::Type::Submenu) {
                    pressedIndex_ = -1;
                    ReleaseCapture();
                    OpenSubmenu(idx);
                    return;
                }
            }
            pressedIndex_ = -1;
            ReleaseCapture();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        int HitTestDip(int x, int y) {
            if (x < 0 || x >= windowWidthDip_ || y < 0 || y >= windowHeightDip_)
                return -1;
            float fy = (float)y;
            float curY = (float)paddingDip_;
            for (int i = 0; i < (int)menu_->items.size(); ++i) {
                auto& item = menu_->items[i];
                if (item->type == MenuItem::Type::Separator) {
                    curY += separatorHeightDip_;
                    continue;
                }
                if (fy >= curY && fy < curY + itemHeightDip_) {
                    return i;
                }
                curY += itemHeightDip_;
            }
            return -1;
        }

        void OpenSubmenu(int index) {
            if (index < 0 || index >= (int)menu_->items.size()) return;
            auto& item = menu_->items[index];
            if (item->type != MenuItem::Type::Submenu || !item->submenu) return;

            POINT pt;
            RECT rc;
            GetWindowRect(hwnd_, &rc);
            float curY = (float)paddingDip_;
            for (int i = 0; i < index; ++i) {
                if (menu_->items[i]->type == MenuItem::Type::Separator)
                    curY += separatorHeightDip_;
                else
                    curY += itemHeightDip_;
            }
            pt.x = rc.right;
            pt.y = rc.top + MulDiv((int)curY, dpi_, 96);

            if (childMenu_ && childMenu_->menu_ == item->submenu) {
                if (childMenu_->visible_) return;
                else {
                    childMenu_->Show(pt.x, pt.y);
                    return;
                }
            }

            if (childMenu_) {
                childMenu_->CloseAll();
                childMenu_.reset();
            }

            childMenu_ = std::make_unique<MenuWindow>(item->submenu, owner_, pt.x, pt.y);
            childMenu_->parent_ = this;
            childMenu_->Show(pt.x, pt.y);
        }

        std::shared_ptr<Menu> menu_;
        HWND hwnd_ = nullptr;
        HWND owner_ = nullptr;
        int screenX_, screenY_;
        UINT dpi_ = 96;
        int windowWidthDip_ = 0, windowHeightDip_ = 0;
        int windowWidthPx_ = 0, windowHeightPx_ = 0;

        int hoveredIndex_ = -1;
        int pressedIndex_ = -1;
        bool isClosing_ = false;

        std::unique_ptr<MenuWindow> childMenu_;
        MenuWindow* parent_ = nullptr;
        int submenuPendingIndex_ = -1;

        ID2D1Factory* d2dFactory_ = nullptr;
        ID2D1HwndRenderTarget* renderTarget_ = nullptr;
        ID2D1SolidColorBrush* bgBrush_ = nullptr;
        ID2D1SolidColorBrush* hoverBrush_ = nullptr;
        ID2D1SolidColorBrush* textBrush_ = nullptr;
        ID2D1SolidColorBrush* separatorBrush_ = nullptr;
        IDWriteTextFormat* textFormat_ = nullptr;

        static ComPtr<IDWriteFactory> sharedDWriteFactory_;
    };

    inline ComPtr<IDWriteFactory> MenuWindow::sharedDWriteFactory_ = nullptr;

    // ---------- 应用核心（进程 / UI 线程级单例） ----------
    namespace detail {
        class AppCore {
        public:
            static AppCore& Instance() { static AppCore s; return s; }

            ID2D1Factory* GetFactory() {
                if (!d2dFactory_) D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
                return d2dFactory_.Get();
            }

            void AddWindow(Window* w) {
                if (w && std::find(windows_.begin(), windows_.end(), w) == windows_.end())
                    windows_.push_back(w);
            }
            void RemoveWindow(Window* w) {
                windows_.erase(std::remove(windows_.begin(), windows_.end(), w), windows_.end());
                if (windows_.empty()) Quit(0);   // 最后一个窗口关闭才退出
            }
            int WindowCount() const { return (int)windows_.size(); }
            const std::vector<Window*>& Windows() const { return windows_; }

            int Run() {
                InitializeUIThread();
                if (running_) return exitCode_;   // 防止嵌套消息循环
                running_ = true;
                MSG msg;
                while (GetMessage(&msg, nullptr, 0, 0)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                running_ = false;
                return exitCode_;
            }
            bool IsRunning() const { return running_; }
            void Quit(int code = 0) {
                exitCode_ = code;
                if (running_) PostQuitMessage(code);
            }

        private:
            AppCore() { timeBeginPeriod(1); }
            ~AppCore() { timeEndPeriod(1); }
            ComPtr<ID2D1Factory> d2dFactory_;
            std::vector<Window*> windows_;
            bool running_ = false;
            int exitCode_ = 0;
        };
    }

    // ---------- 窗口 ----------
    class Window {
    public:
        inline static WindowBackdrop DefaultBackdrop = WindowBackdrop::AcrylicBlurBehind;
        inline static DWORD DefaultBackdropColor = 0x80FFFFFF;
        inline static Color DefaultBackgroundColor = Color(0, 0, 0, 0);

        Window() : core_(&detail::AppCore::Instance()), hwnd_(nullptr), d2dFactory_(nullptr), renderTarget_(nullptr),
            rootElement_(nullptr), currentHovered_(nullptr), pressedElement_(nullptr),
            focusedElement_(nullptr), dpi_(96),
            captionColor_(RGB(240, 240, 240)), textColor_(RGB(0, 0, 0)), borderColor_(RGB(180, 180, 180)),
            hasCustomMinSize_(false), customMinWidth_(0), customMinHeight_(0),
            lastTime_(std::chrono::steady_clock::now()), layoutNeeded_(true),
            backdrop_(DefaultBackdrop), backdropColor_(DefaultBackdropColor), backgroundColor_(DefaultBackgroundColor),
            animationTimerActive_(false), animationIdleFrames_(0),
            layoutInvalidated_(false) {}

        ~Window() {
            if (rootElement_) rootElement_->AttachWindowRecursive(nullptr);
            if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
            DiscardDeviceResources();
            // d2dFactory_ 由 AppCore 共享，不在此释放
        }

        void SetMouseCapture(UIElement* elem) { mouseCaptureElement_ = elem; }
        void ReleaseMouseCapture(UIElement* elem) {
            if (mouseCaptureElement_ == elem) mouseCaptureElement_ = nullptr;
        }

        void SetBackdrop(WindowBackdrop backdrop, DWORD color = 0x80FFFFFF) {
            backdrop_ = backdrop;
            backdropColor_ = color;
            ApplyBackdrop();
        }
        void SetBackgroundColor(Color color) {
            backgroundColor_ = color;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        static void SetDefaultBackdrop(WindowBackdrop backdrop, DWORD color = 0x80FFFFFF) {
            DefaultBackdrop = backdrop;
            DefaultBackdropColor = color;
        }

        bool Create(int width, int height, const std::wstring& title) {
            HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
            if (hUser32) {
                typedef BOOL(WINAPI* pSetProcessDpiAwarenessContext)(HANDLE);
                pSetProcessDpiAwarenessContext SetProcessDpiAwarenessContext = (pSetProcessDpiAwarenessContext)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
                if (SetProcessDpiAwarenessContext) {
                    SetProcessDpiAwarenessContext((HANDLE)-4);
                }
                else {
                    typedef BOOL(WINAPI* pSetProcessDPIAware)(void);
                    pSetProcessDPIAware SetProcessDPIAware = (pSetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
                    if (SetProcessDPIAware) SetProcessDPIAware();
                }
            }

            UINT sysDpi = GetDpiForSystem();
            if (sysDpi == 0) sysDpi = 96;
            int physicalWidth = MulDiv(width, sysDpi, 96);
            int physicalHeight = MulDiv(height, sysDpi, 96);

            WNDCLASSEXW wc = {};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.lpfnWndProc = Window::WndProc;
            wc.hInstance = GetModuleHandle(nullptr);
            wc.lpszClassName = L"ZUIWindowClass";
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = nullptr;
            RegisterClassExW(&wc);

            hwnd_ = CreateWindowExW(0, L"ZUIWindowClass", title.c_str(), WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, physicalWidth, physicalHeight,
                nullptr, nullptr, GetModuleHandle(nullptr), this);
            if (!hwnd_) return false;

            dpi_ = GetDpiForWindow(hwnd_);
            if (dpi_ == 0) dpi_ = 96;

            d2dFactory_ = core_->GetFactory();   // 共享工厂（进程级）
            if (!d2dFactory_) return false;
            if (FAILED(CreateDeviceResources())) return false;

            core_->AddWindow(this);

            ApplyBackdrop();
            ApplyTitleBarColors();

            auto defaultRoot = std::make_shared<ColumnBox>();
            defaultRoot->SetMargin(Thickness(20, 20, 20, 20));
            defaultRoot->SetSpacing(10);
            rootElement_ = defaultRoot;
            rootElement_->AttachWindowRecursive(this);
            layoutNeeded_ = true;

            defaultIMC_ = ImmGetContext(hwnd_);
            if (defaultIMC_) ImmReleaseContext(hwnd_, defaultIMC_);

            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);

            // 启动常驻定时器
            UpdateTimerState();
            animationTimerActive_ = true; // 常驻定时器标志

            return true;
        }

        void SetRootLayout(std::shared_ptr<Layout> layout) {
            if (!layout) return;
            if (rootElement_ && rootElement_ != layout) rootElement_->AttachWindowRecursive(nullptr);
            rootElement_ = layout;
            rootElement_->AttachWindowRecursive(this);
            layoutNeeded_ = true;
            layoutInvalidated_ = true;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        std::shared_ptr<Layout> GetRootLayout() const { return rootElement_; }

        void SetContextMenu(std::shared_ptr<Menu> menu) { windowContextMenu_ = menu; }

        std::shared_ptr<ColumnBox> GetRootColumnBox() const {
            return std::dynamic_pointer_cast<ColumnBox>(rootElement_);
        }

        void SetTitleBarColors(COLORREF caption, COLORREF text, COLORREF border) {
            captionColor_ = caption; textColor_ = text; borderColor_ = border;
            if (hwnd_) ApplyTitleBarColors();
        }
        void SetCaptionColor(COLORREF color) { captionColor_ = color; if (hwnd_) ApplyTitleBarColors(); }
        void SetTitleTextColor(COLORREF color) { textColor_ = color; if (hwnd_) ApplyTitleBarColors(); }
        void SetBorderColor(COLORREF color) { borderColor_ = color; if (hwnd_) ApplyTitleBarColors(); }

        void SetMinSize(int width, int height) {
            hasCustomMinSize_ = true;
            customMinWidth_ = width;
            customMinHeight_ = height;
        }

        // 多窗口常用：把窗口移动到指定位置 / 设置尺寸（单位 DIP）
        void SetPosition(int x, int y) {
            if (!hwnd_) return;
            SetWindowPos(hwnd_, nullptr, MulDiv(x, dpi_, 96), MulDiv(y, dpi_, 96), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        void SetSize(int width, int height) {
            if (!hwnd_) return;
            SetWindowPos(hwnd_, nullptr, 0, 0, MulDiv(width, dpi_, 96), MulDiv(height, dpi_, 96), SWP_NOMOVE | SWP_NOZORDER);
        }
        bool IsValid() const { return hwnd_ != nullptr; }

        // 父子（owned）窗口：设置所有者后，本窗口会始终位于所有者之上，并随所有者最小化。
        void SetOwner(Window* owner) {
            owner_ = owner;
            if (hwnd_) SetWindowLongPtr(hwnd_, GWLP_HWNDPARENT, (LONG_PTR)(owner ? owner->hwnd_ : nullptr));
        }
        Window* GetOwner() const { return owner_; }

        void Run() { core_->Run(); }

        // 以“模态”方式运行本窗口：禁用 owner（未指定则用 SetOwner 设置的所有者，再否则禁用当前活动窗口），
        // 运行一个嵌套消息循环，直到本窗口关闭；返回后恢复 owner。
        int RunModal(Window* owner = nullptr) {
            if (!hwnd_) return 0;
            Window* ow = owner ? owner : owner_;
            HWND ownerHwnd = ow ? ow->hwnd_ : nullptr;
            if (!ownerHwnd) ownerHwnd = GetActiveWindow();
            if (ownerHwnd == hwnd_) ownerHwnd = nullptr;   // 不要禁用自己
            if (ownerHwnd) EnableWindow(ownerHwnd, FALSE);

            int modalExit = 0;
            MSG msg;
            while (IsWindow(hwnd_) && GetMessage(&msg, nullptr, 0, 0)) {
                if (msg.message == WM_QUIT) { PostQuitMessage((int)msg.wParam); break; }
                if (hwnd_ && IsDialogMessage(hwnd_, &msg)) continue;   // Tab/方向键等对话框导航
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (ownerHwnd && IsWindow(ownerHwnd)) {
                EnableWindow(ownerHwnd, TRUE);
                SetForegroundWindow(ownerHwnd);
            }
            return modalExit;
        }

        // 关闭本窗口（其余窗口不受影响；全部关闭后消息循环才会退出）
        void Close() { if (hwnd_) DestroyWindow(hwnd_); }

        // 由 UIElement 直接路由，多窗口互不干扰
        void MarkRepaint(UIElement* elem) { if (elem) pendingRepaint_.insert(elem); }
        void MarkLayoutInvalidated() { layoutInvalidated_ = true; }

        // 控件鼠标捕获（按窗口路由）
        void RequestElementCapture(UIElement* elem) { if (elem) mouseCaptureElement_ = elem; }
        void ReleaseElementCapture(UIElement* elem) { if (mouseCaptureElement_ == elem) mouseCaptureElement_ = nullptr; }

    public:
        // 窗口实例信号：按窗口区分激活/失活/关闭
        ZSignal<> Activated;
        ZSignal<> Deactivated;
        ZSignal<> Closed;

    private:
        // 移除 kAnimationIdleFramesToStop 相关逻辑，定时器常驻

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
            Window* self = nullptr;
            if (message == WM_NCCREATE) {
                CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
                self = reinterpret_cast<Window*>(cs->lpCreateParams);
                self->hwnd_ = hwnd;
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            }
            else {
                self = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            }
            if (self) return self->HandleMessage(message, wParam, lParam);
            return DefWindowProc(hwnd, message, wParam, lParam);
        }

        void UpdateIMEAssociation() {
            if (!hwnd_) return;
            bool needIME = (focusedElement_ && focusedElement_->IsTextInput());
            if (needIME) {
                ImmAssociateContext(hwnd_, defaultIMC_);
            }
            else {
                ImmAssociateContext(hwnd_, NULL);
            }
        }

#ifdef ZUI_DEBUG
        void PrintActiveAnimations(UIElement* elem, int depth) {
            if (!elem) return;
            if (elem->HasActiveAnimation()) {
                std::wstring indent(depth * 2, L' ');
                const char* typeName = typeid(*elem).name();
                wchar_t wtype[128];
                MultiByteToWideChar(CP_ACP, 0, typeName, -1, wtype, 128);

                // 特殊处理 PageHost：暴力读取成员变量（调试专用）
                if (strcmp(typeName, "class ZUI::PageHost") == 0) {
                    wchar_t buf[512];
                    swprintf(buf, 512, L"%s%s (0x%p) [PageHost - check manually]\n",
                        indent.c_str(), wtype, (void*)elem);
                    ZUI_DEBUG_LOG_W(buf);
                }
                else {
                    wchar_t buf[512];
                    swprintf(buf, 512, L"%s%s (0x%p)\n", indent.c_str(), wtype, (void*)elem);
                    ZUI_DEBUG_LOG_W(buf);
                }
            }
            for (auto* child : elem->GetChildren()) {
                PrintActiveAnimations(child, depth + 1);
            }
        }
#endif

        LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
            switch (message) {
            case WM_IME_SETCONTEXT:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
                }
                else {
                    lParam &= ~(ISC_SHOWUICOMPOSITIONWINDOW | ISC_SHOWUICANDIDATEWINDOW);
                }
                LRESULT lResult = DefWindowProc(hwnd_, message, wParam, lParam);
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    SetImePosition();
                }
                return lResult;
            }
            case WM_IME_STARTCOMPOSITION:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    UpdateCompositionText();
                    focusedElement_->SetCompositionText(compositionText_, hasComposition_, compositionCursorPos_);
                    SetImePosition();
                    return 0;
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_COMPOSITION:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    if (lParam & GCS_RESULTSTR) {
                        hasComposition_ = false;
                        compositionText_.clear();
                        focusedElement_->SetCompositionText(L"", false);
                    }
                    else if (lParam == 0) {
                        hasComposition_ = false;
                        compositionText_.clear();
                        focusedElement_->SetCompositionText(L"", false);
                    }
                    else {
                        UpdateCompositionText();
                        focusedElement_->SetCompositionText(compositionText_, hasComposition_, compositionCursorPos_);
                    }
                    SetImePosition();
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_ENDCOMPOSITION:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    hasComposition_ = false;
                    compositionText_.clear();
                    focusedElement_->SetCompositionText(L"", false);
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_CHAR:
            {
                if (focusedElement_ && focusedElement_->IsTextInput()) {
                    focusedElement_->OnChar(static_cast<wchar_t>(wParam));
                }
                return 0;
            }
            case WM_IME_NOTIFY:
            {
                if (wParam == IMN_OPENCANDIDATE || wParam == IMN_SETCANDIDATEPOS ||
                    wParam == IMN_CHANGECANDIDATE || wParam == IMN_CLOSECANDIDATE) {
                    if (focusedElement_ && focusedElement_->IsTextInput()) {
                        SetImePosition();
                    }
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_IME_REQUEST:
            {
                if (wParam == IMR_QUERYCHARPOSITION && focusedElement_ && focusedElement_->IsTextInput()) {
                    Rect caretRect = focusedElement_->GetImeCandidateRect();
                    float scale = dpi_ / 96.0f;
                    POINT pt;
                    pt.x = static_cast<LONG>(caretRect.x * scale);
                    pt.y = static_cast<LONG>(caretRect.y * scale);
                    ClientToScreen(hwnd_, &pt);

                    IMECHARPOSITION* pCharPos = reinterpret_cast<IMECHARPOSITION*>(lParam);
                    pCharPos->pt = pt;
                    pCharPos->cLineHeight = static_cast<DWORD>(focusedElement_->GetArrangedRect().height * scale);
                    pCharPos->rcDocument = { 0, 0, 0, 0 };
                    return 1;
                }
                return DefWindowProc(hwnd_, message, wParam, lParam);
            }
            case WM_PAINT: OnPaint(); return 0;
            case WM_ERASEBKGND: return 1;
            case WM_SIZE:
                UpdateTimerState();
                if (wParam == SIZE_MINIMIZED) {
                    Deactivated(); UIZSignals::WindowDeactivated(this);
                }
                if (!IsIconic(hwnd_)) {
                    layoutNeeded_ = true;
                    layoutInvalidated_ = true;
                    InvalidateRect(hwnd_, nullptr, FALSE);
                }
                else {
                    if (currentHovered_) { currentHovered_->OnMouseLeave(); currentHovered_ = nullptr; }
                    return 0;
                }
                if (renderTarget_) {
                    RECT rc; GetClientRect(hwnd_, &rc);
                    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
                    renderTarget_->Resize(size);
                }
                return 0;
            case WM_ACTIVATE:
                UpdateTimerState();
                if (LOWORD(wParam) == WA_INACTIVE) {
                    Deactivated(); UIZSignals::WindowDeactivated(this);
                }
                else {
                    Activated();
                }
                return 0;
            case WM_DPICHANGED:
                UpdateTimerState();
                dpi_ = HIWORD(wParam); if (dpi_ == 0) dpi_ = 96;
                DiscardDeviceResources(); CreateDeviceResources();
                layoutNeeded_ = true;
                layoutInvalidated_ = true;
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case WM_DISPLAYCHANGE: InvalidateRect(hwnd_, nullptr, FALSE); UpdateTimerState(); return 0;
            case WM_TIMER:
                if (wParam == 1) {
#ifdef ZUI_DEBUG
                    // ---- 调试输出开始 ----
                    bool hasLayout = layoutInvalidated_;
                    size_t pendingCount = pendingRepaint_.size();
                    bool hasAnim = rootElement_ ? rootElement_->HasActiveAnimation() : false;

                    wchar_t buf[256];
                    swprintf(buf, 256, L"[Timer] layout=%d, pending=%zu, anim=%d\n",
                        hasLayout, pendingCount, hasAnim ? 1 : 0);
                    ZUI_DEBUG_LOG_W(buf);

                    // 如果动画存在，打印详细元素树
                    if (hasAnim && rootElement_) {
                        ZUI_DEBUG_LOG_W(L"--- Active Animation Elements ---\n");
                        PrintActiveAnimations(rootElement_.get(), 0);
                        ZUI_DEBUG_LOG_W(L"--- End ---\n");
                    }
                    // ---- 调试输出结束 ----
#endif
                    UpdateTooltip();
                    if (HasRenderWork()) {          // 关键：先判断是否有工作
                        InvalidateRect(hwnd_, nullptr, FALSE);
                    }
                    // 若无工作，则什么都不做，定时器继续运行
                }
                return 0;
            case WM_MOUSEMOVE:
                OnMouseMove(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_MOUSELEAVE: OnMouseLeave(); return 0;
            case WM_LBUTTONDOWN:
                if (activeMenuRoot_) { CloseActiveMenuWindow(); }
                OnMouseDown(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_LBUTTONUP:
                OnMouseUp(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_RBUTTONUP:
                OnContextMenu(PixelToDipX(GET_X_LPARAM(lParam)), PixelToDipY(GET_Y_LPARAM(lParam)));
                return 0;
            case WM_SETFOCUS:
                if (focusedElement_) focusedElement_->OnFocus();
                UpdateIMEAssociation();
                return 0;
            case WM_KILLFOCUS:
                UpdateTimerState();
                CloseActiveMenuWindow();
                if (focusedElement_) focusedElement_->OnBlur();
                ImmAssociateContext(hwnd_, NULL);
                return 0;
            case WM_KEYDOWN:
                if (wParam == VK_TAB) { MoveFocusByTab((GetKeyState(VK_SHIFT) & 0x8000) != 0); return 0; }
                if (focusedElement_ && focusedElement_->IsEffectivelyEnabled()) focusedElement_->OnKeyDown(wParam, lParam);
                return 0;
            case WM_KEYUP: if (focusedElement_) focusedElement_->OnKeyUp(wParam, lParam); return 0;
            case WM_CHAR: if (focusedElement_) focusedElement_->OnChar(static_cast<wchar_t>(wParam)); return 0;
            case WM_MOUSEWHEEL: {
                POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hwnd_, &pt);
                float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                OnMouseWheel(PixelToDipX(pt.x), PixelToDipY(pt.y), 0.0f, delta);
                return 0;
            }
            case WM_MOUSEHWHEEL: {
                POINT pt; pt.x = GET_X_LPARAM(lParam); pt.y = GET_Y_LPARAM(lParam);
                ScreenToClient(hwnd_, &pt);
                float delta = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                OnMouseWheel(PixelToDipX(pt.x), PixelToDipY(pt.y), delta, 0.0f);
                return 0;
            }
            case WM_GETMINMAXINFO: {
                MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
                if (hasCustomMinSize_) {
                    mmi->ptMinTrackSize.x = MulDiv(customMinWidth_, dpi_, 96);
                    mmi->ptMinTrackSize.y = MulDiv(customMinHeight_, dpi_, 96);
                }
                else {
                    if (rootElement_) {
                        Size minSize = rootElement_->Measure(Size(0, 0));
                        int minWidthPx = MulDiv((int)ceil(minSize.width), dpi_, 96);
                        int minHeightPx = MulDiv((int)ceil(minSize.height), dpi_, 96);
                        if (minWidthPx < 200) minWidthPx = 200;
                        if (minHeightPx < 150) minHeightPx = 150;
                        RECT rect = { 0, 0, minWidthPx, minHeightPx };
                        AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
                        mmi->ptMinTrackSize.x = rect.right - rect.left;
                        mmi->ptMinTrackSize.y = rect.bottom - rect.top;
                    }
                }
                return 0;
            }
            case WM_SETCURSOR:
                if (currentHovered_ && currentHovered_->IsTextInput()) {
                    SetCursor(LoadCursor(nullptr, IDC_IBEAM));
                    return TRUE;
                }
                break;
            case WM_SHOWWINDOW:
                UpdateTimerState();
                return 0;
            case WM_CAPTURECHANGED:
                // 捕获被系统/其他窗口夺走时，清理拖拽按下状态，避免松手后残留
                pressedElement_ = nullptr;
                return 0;
            case WM_DESTROY:
                if (timerRunning_) {
                    KillTimer(hwnd_, 1);
                    timerRunning_ = false;
                }
                if (rootElement_) rootElement_->AttachWindowRecursive(nullptr);  // 清除整棵树的窗口指针，避免外部持有元素时悬垂
                hwnd_ = nullptr;
                Closed();
                if (core_) core_->RemoveWindow(this);
                return 0;
            }
            return DefWindowProc(hwnd_, message, wParam, lParam);
        }

        void UpdateTooltip() {
            bool candidate = currentHovered_ && currentHovered_->IsEffectivelyEnabled()
                && currentHovered_->IsVisible() && !currentHovered_->GetToolTip().empty();
            if (!candidate) {
                if (tooltipTarget_) { tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f; }
                return;
            }
            if (!tooltipTarget_) {
                if (GetTickCount() - hoverStartTick_ >= 500) {
                    tooltipTarget_ = currentHovered_;
                    tooltipAnchorPt_ = D2D1::Point2F(mouseX_, mouseY_);
                    tooltipProgress_ = 0.0f;
                }
                return;
            }
            if (tooltipTarget_ != currentHovered_) {
                tooltipTarget_ = nullptr;
                tooltipProgress_ = 0.0f;
                return;
            }
            if (tooltipProgress_ < 1.0f) {
                tooltipProgress_ += 0.12f;
                if (tooltipProgress_ > 1.0f) tooltipProgress_ = 1.0f;
            }
        }
        bool HasRenderWork() const {
            if (layoutInvalidated_ || focusDirty_ || !pendingRepaint_.empty() || (rootElement_ && rootElement_->HasActiveAnimation())) return true;
            if (tooltipTarget_ || tooltipProgress_ > 0.0f) return true;
            if (currentHovered_ && currentHovered_->IsEffectivelyEnabled() && !currentHovered_->GetToolTip().empty()) return true;
            return false;
        }

        void Compose(UIElement* elem, ID2D1RenderTarget* rt) {
            ComposeImpl(elem, rt);
        }
        // 带裁剪剔除的合成：clip 为累计裁剪矩形（DIP 绝对坐标），完全在裁剪外的子树直接跳过
        void ComposeImpl(UIElement* elem, ID2D1RenderTarget* rt,
                         const D2D1_RECT_F& clip = D2D1::RectF(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX),
                         bool hasClip = false) {
            if (!elem || !elem->IsVisible()) return;

            if (hasClip) {
                Rect er = elem->GetArrangedRect();
                if (er.x >= clip.right || er.x + er.width <= clip.left ||
                    er.y >= clip.bottom || er.y + er.height <= clip.top)
                    return;   // 完全在裁剪区外，跳过整棵子树
            }

            // 应用裁剪（如果有）
            bool clipPushed = false;
            D2D1_RECT_F effClip = clip;
            bool effHasClip = hasClip;
            if (auto c = elem->GetClipRect()) {
                rt->PushAxisAlignedClip(*c, D2D1_ANTIALIAS_MODE_ALIASED);
                clipPushed = true;
                if (effHasClip) {
                    effClip = D2D1::RectF(max(effClip.left, c->left), max(effClip.top, c->top),
                        min(effClip.right, c->right), min(effClip.bottom, c->bottom));
                }
                else {
                    effClip = *c;
                    effHasClip = true;
                }
            }

            // 判断是否使用缓存
            if (elem->UseCache()) {
                // 确保缓存存在且尺寸匹配
                EnsureCache(elem);

                // 如果待重绘或缓存无效，则绘制到缓存
                if (pendingRepaint_.count(elem) || !elem->cacheValid_) {
                    if (elem->cacheRT_) {
                        auto cacheRT = elem->cacheRT_.Get();
                        cacheRT->BeginDraw();
                        cacheRT->Clear(D2D1::ColorF(0, 0, 0, 0)); // 透明背景

                        // 设置平移变换，使元素内部使用的绝对坐标映射到缓存原点
                        D2D1::Matrix3x2F oldTransform;
                        cacheRT->GetTransform(&oldTransform);
                        // 内容原点吸附到物理像素，避免缓存内容整体落在半像素上模糊
                        D2D1::Matrix3x2F newTransform = D2D1::Matrix3x2F::Translation(
                            Snap(elem->cacheOriginX_ - elem->GetArrangedRect().x),
                            Snap(elem->cacheOriginY_ - elem->GetArrangedRect().y)
                        );
                        cacheRT->SetTransform(newTransform);

                        if (elem->HasShadow()) DrawShadow(cacheRT, elem);

                        elem->Draw(cacheRT);

                        cacheRT->SetTransform(oldTransform);
                        cacheRT->EndDraw();

                        elem->cacheValid_ = true;
                        pendingRepaint_.erase(elem);
                    }
                }

                // 将缓存位图绘制到主 RT
                if (elem->cacheRT_) {
                    ComPtr<ID2D1Bitmap> bitmap;
                    elem->cacheRT_->GetBitmap(&bitmap);
                    if (bitmap) {
                        Rect r = elem->GetArrangedRect();
                        FLOAT dpiX, dpiY;
                        renderTarget_->GetDpi(&dpiX, &dpiY);
                        float scaleX = dpiX / 96.0f;
                        float scaleY = dpiY / 96.0f;

                        // 坐标取整到物理像素，避免亚像素模糊
                        auto roundToPixelX = [&](float dip) -> float {
                            return std::round(dip * scaleX) / scaleX;
                            };
                        auto roundToPixelY = [&](float dip) -> float {
                            return std::round(dip * scaleY) / scaleY;
                            };

                        float dstX = roundToPixelX(r.x - elem->cacheOriginX_);
                        float dstY = roundToPixelY(r.y - elem->cacheOriginY_);
                        float dstW = roundToPixelX(r.x - elem->cacheOriginX_ + elem->cacheSize_.width) - dstX;
                        float dstH = roundToPixelY(r.y - elem->cacheOriginY_ + elem->cacheSize_.height) - dstY;

                        rt->DrawBitmap(
                            bitmap.Get(),
                            D2D1::RectF(dstX, dstY, dstX + dstW, dstY + dstH),
                            1.0f,
                            // 缓存尺寸与目标尺寸一致，用最近邻避免线性插值带来的二次模糊
                            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                        );
                    }
                }
            }
            else {
                // 无缓存元素，直接绘制
                elem->Draw(rt);
                // 从待重绘集合中移除
                pendingRepaint_.erase(elem);
            }

            // 递归处理子元素
            for (auto* child : elem->GetChildren()) {
                D2D1::Matrix3x2F childTransform = elem->GetChildRenderTransform(child);
                bool hasTransform = !childTransform.IsIdentity();

                if (hasTransform) {
                    D2D1::Matrix3x2F oldTransform;
                    rt->GetTransform(&oldTransform);
                    rt->SetTransform(oldTransform * childTransform);
                    ComposeImpl(child, rt, effClip, effHasClip);
                    rt->SetTransform(oldTransform);
                }
                else {
                    ComposeImpl(child, rt, effClip, effHasClip);
                }
            }

            if (clipPushed) {
                rt->PopAxisAlignedClip();
            }
        }

        // 高斯 CDF 分层的软阴影：让累积 alpha 逼近 targetA*(1-Φ(d/σ))，接近 DWM 质感
        void DrawSoftShadow(ID2D1RenderTarget* rt, const D2D1_RECT_F& rect, float radius,
                            float ox, float oy, float blur, D2D1_COLOR_F col, int steps, float alphaScale = 1.0f) {
            if (steps < 2) steps = 2;
            if (steps > 63) steps = 63;
            if (blur <= 0.0f) blur = 0.001f;
            col.a *= alphaScale;
            ComPtr<ID2D1SolidColorBrush> brush;
            rt->CreateSolidColorBrush(col, brush.GetAddressOf());
            if (!brush) return;

            float targetA = min(col.a * 2.0f, 0.98f);   // col.a 视为“边缘可见 alpha”
            float sigma = blur * 0.5f;                  // blur 约等于 2σ
            float extent = sigma * 3.0f;                // 3σ 覆盖约 99.7%

            auto Phi = [](float x) { return 0.5f * (1.0f + erff(x * 0.70710678f)); };
            std::array<float, 64> S{};
            std::array<float, 64> alphas{};
            for (int k = steps - 1; k >= 0; --k) {
                float t = (float)k / (float)(steps - 1);
                float phi = Phi(3.0f * t);
                S[k] = -logf(max(1e-6f, 1.0f - targetA * (1.0f - phi)));
            }
            for (int k = 0; k < steps; ++k) alphas[k] = 1.0f - expf(-(S[k] - S[k + 1]));

            for (int k = steps - 1; k >= 0; --k) {
                float t = (float)k / (float)(steps - 1);
                float grow = extent * t;
                D2D1_COLOR_F c = col; c.a = alphas[k];
                brush->SetColor(c);
                D2D1_RECT_F rr = D2D1::RectF(rect.left + ox - grow, rect.top + oy - grow,
                    rect.right + ox + grow, rect.bottom + oy + grow);
                rt->FillRoundedRectangle(D2D1::RoundedRect(rr, radius + grow, radius + grow), brush.Get());
            }
        }

        void DrawShadow(ID2D1RenderTarget* rt, UIElement* elem) {
            float radius = elem->GetShadowCornerRadius();
            if (radius < 0.0f) radius = 8.0f;
            DrawSoftShadow(rt, elem->GetArrangedRect().ToD2D(), radius,
                elem->GetShadowOffsetX(), elem->GetShadowOffsetY(),
                elem->GetShadowBlur(), elem->GetShadowColor(), 40);
        }

        void DrawFocusAndTooltip(ID2D1RenderTarget* rt) {
            if (showFocusRing_ && focusedElement_ && focusedElement_->IsVisible() && focusedElement_->IsEffectivelyEnabled()) {
                Rect fr = focusedElement_->GetArrangedRect();
                ComPtr<ID2D1SolidColorBrush> fb;
                rt->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.47f, 0.84f, 0.9f), fb.GetAddressOf());
                if (fb) rt->DrawRoundedRectangle(D2D1::RoundedRect(fr.ToD2D(), 4, 4), fb.Get(), 2.0f);
            }
            if (tooltipTarget_ && tooltipProgress_ > 0.001f) {
                std::wstring tip = tooltipTarget_->GetToolTip();
                if (!tip.empty()) DrawToolTip(rt, tip, tooltipAnchorPt_, tooltipProgress_);
            }
        }
        void DrawToolTip(ID2D1RenderTarget* rt, const std::wstring& text, const D2D1_POINT_2F& anchorPt, float alpha) {
            auto fmt = FontManager::Instance().GetFormat(FontManager::Instance().GetGlobalFont());
            IDWriteFactory* factory = FontManager::Instance().GetFactory();
            if (!fmt || !factory) return;
            alpha = clamp(alpha, 0.0f, 1.0f);
            const float maxTextWidth = 320.0f;
            ComPtr<IDWriteTextLayout> layout;
            // maxHeight=0 表示不约束高度，避免段落对齐导致文字被画到布局中部
            factory->CreateTextLayout(text.c_str(), (UINT32)text.length(), fmt, maxTextWidth, 0.0f, &layout);
            if (layout) {
                layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
                layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                layout->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
            }
            DWRITE_TEXT_METRICS tm{};
            if (layout) layout->GetMetrics(&tm);
            float pad = 8.0f;
            float tw = (tm.width > 0.0f ? tm.width : 20.0f);
            float th = (tm.height > 0.0f ? tm.height : 16.0f);
            if (th > 400.0f) th = 400.0f;
            float w = tw + pad * 2.0f, h = th + pad * 2.0f;
            D2D1_SIZE_F sz = rt->GetSize();
            // 固定在“显示时的鼠标位置”上方，带固定偏移（不跟随鼠标移动）
            float x = anchorPt.x - w / 2.0f;
            float y = anchorPt.y - h - 14.0f;
            if (x < 4.0f) x = 4.0f;
            if (x + w > sz.width - 4.0f) x = sz.width - w - 4.0f;
            if (y < 4.0f) y = anchorPt.y + 18.0f;      // 上方空间不足则放到鼠标下方
            if (y + h > sz.height - 4.0f) y = sz.height - h - 4.0f;
            if (x < 4.0f) x = 4.0f;
            D2D1_RECT_F rr = D2D1::RectF(x, y, x + w, y + h);
            // 柔和阴影（与元素阴影同一套高斯 CDF 分层，ToolTip 每帧绘制用较少层数）
            DrawSoftShadow(rt, rr, 6.0f, 0.5f, 2.0f, 5.0f, D2D1::ColorF(0, 0, 0, 0.30f), 20, alpha);
            ComPtr<ID2D1SolidColorBrush> bg, fg;
            rt->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.98f * alpha), bg.GetAddressOf());
            rt->CreateSolidColorBrush(D2D1::ColorF(0.10f, 0.10f, 0.10f, alpha), fg.GetAddressOf());
            if (bg) rt->FillRoundedRectangle(D2D1::RoundedRect(rr, 6, 6), bg.Get());
            if (layout && fg) rt->DrawTextLayout(D2D1::Point2F(x + pad, y + pad), layout.Get(), fg.Get());
        }
        void CollectFocusable(UIElement* elem, std::vector<UIElement*>& out) {
            if (!elem || !elem->IsVisible() || !elem->IsEffectivelyEnabled()) return;
            if (elem->IsFocusable()) out.push_back(elem);
            for (auto* c : elem->GetChildren()) CollectFocusable(c, out);
        }
        void SetFocusElement(UIElement* e, bool showRing = false) {
            showFocusRing_ = showRing;
            if (focusedElement_ == e) { focusDirty_ = true; return; }
            if (focusedElement_) focusedElement_->OnBlur();
            focusedElement_ = e;
            if (focusedElement_) focusedElement_->OnFocus();
            UpdateIMEAssociation();
            focusDirty_ = true;
        }
        void MoveFocusByTab(bool backward) {
            std::vector<UIElement*> list;
            CollectFocusable(rootElement_.get(), list);
            if (list.empty()) return;
            int cur = -1;
            for (int i = 0; i < (int)list.size(); ++i) if (list[i] == focusedElement_) { cur = i; break; }
            int n = (int)list.size();
            int next;
            if (cur < 0) next = backward ? n - 1 : 0;
            else next = ((cur + (backward ? -1 : 1)) % n + n) % n;
            SetFocusElement(list[next], true);
        }

        void EnsureCache(UIElement* elem) {
            Rect r = elem->GetArrangedRect();
            if (r.width <= 0 || r.height <= 0) {
                elem->cacheRT_.Reset();
                elem->cacheValid_ = false;
                return;
            }

            float bleed = elem->GetBleed() + elem->GetShadowExtent();
            float w = r.width + bleed * 2;
            float h = r.height + bleed * 2;

            bool needCreate = !elem->cacheRT_;
            if (elem->cacheRT_) {
                if (elem->cacheSize_.width != w || elem->cacheSize_.height != h) {
                    needCreate = true;
                }
            }

            if (needCreate) {
                elem->cacheRT_.Reset();
                if (renderTarget_) {
                    HRESULT hr = renderTarget_->CreateCompatibleRenderTarget(
                        D2D1::SizeF(w, h),
                        elem->cacheRT_.GetAddressOf()
                    );
                    if (SUCCEEDED(hr)) {
                        elem->cacheSize_ = Size(w, h);
                        elem->cacheValid_ = false;
                        elem->cacheRT_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                        elem->cacheRT_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);   // 必须改回 GRAYSCALE
                        FLOAT dpiX, dpiY;
                        renderTarget_->GetDpi(&dpiX, &dpiY);
                        elem->cacheRT_->SetDpi(dpiX, dpiY);
                    }
                }
            }

            elem->cacheOriginX_ = bleed;
            elem->cacheOriginY_ = bleed;
        }

        void ClearAllCaches() {
            std::function<void(UIElement*)> clearRecursive = [&](UIElement* elem) {
                if (!elem) return;
                if (elem->UseCache()) {
                    elem->cacheRT_.Reset();
                    elem->cacheValid_ = false;
                }
                for (auto* child : elem->GetChildren()) {
                    clearRecursive(child);
                }
                };
            clearRecursive(rootElement_.get());
        }

        void CollectVisibleCachedElements(UIElement* elem, std::unordered_set<UIElement*>& set) {
            if (!elem || !elem->IsVisible()) return;
            if (elem->UseCache()) {
                set.insert(elem);
            }
            for (auto* child : elem->GetChildren()) {
                CollectVisibleCachedElements(child, set);
            }
        }

        void OnPaint() {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            if (IsIconic(hwnd_)) { EndPaint(hwnd_, &ps); return; }

            if (!renderTarget_) {
                if (FAILED(CreateDeviceResources())) {
                    EndPaint(hwnd_, &ps);
                    return;
                }
            }

            RECT rc; GetClientRect(hwnd_, &rc);
            float clientWidthDip = (rc.right - rc.left) * 96.0f / dpi_;
            float clientHeightDip = (rc.bottom - rc.top) * 96.0f / dpi_;

            Thickness rootMargin = rootElement_ ? rootElement_->GetMargin() : Thickness();
            float left = rootMargin.left, top = rootMargin.top;
            float availWidth = clientWidthDip - left - rootMargin.right;
            float availHeight = clientHeightDip - top - rootMargin.bottom;

            // 1. 帧开始，计算时间差
            auto now = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
            lastTime_ = now;
            // 钳制 deltaTime：空闲/最小化/断点恢复后首帧可能得到很大的 dt，
            // 会让线性累加型动画（Button hover、PageHost 切页、CheckBox 等）一帧跳到终点。
            // 上限取 ~2 帧（0.033s），最坏也只表现为一次轻微卡顿。
            if (deltaTime > 0.033f) deltaTime = 0.033f;
            if (deltaTime < 0.0f) deltaTime = 0.0f;

            // 2. 布局检查
            if (layoutInvalidated_ || layoutNeeded_ || (rootElement_ && rootElement_->IsLayoutDirty())) {
                if (rootElement_) {
                    rootElement_->Measure(Size(availWidth, availHeight));
                    rootElement_->Arrange(Rect(left, top, availWidth, availHeight));
                    rootElement_->ClearLayoutDirty();
                }
                layoutNeeded_ = false;
                layoutInvalidated_ = false;

                // 清空所有缓存，将可见有缓存元素加入待重绘集合
                ClearAllCaches();
                pendingRepaint_.clear();
                if (rootElement_) {
                    CollectVisibleCachedElements(rootElement_.get(), pendingRepaint_);
                }
            }

            // 3. 动画更新
            if (rootElement_) {
                rootElement_->UpdateAnimation(deltaTime);
            }

            // 自动收集活跃动画元素（确保动画期间每帧重绘这些元素）
            activeAnimScratch_.clear();
            CollectActiveAnimations(rootElement_.get(), activeAnimScratch_);
            for (auto* elem : activeAnimScratch_) {
                pendingRepaint_.insert(elem);
            }
            // 上一帧活跃但当前不活跃的元素也加入，确保动画结束状态正确
            for (auto* elem : lastActiveAnimElements_) {
                if (activeAnimScratch_.find(elem) == activeAnimScratch_.end()) {
                    pendingRepaint_.insert(elem);
                }
            }
            lastActiveAnimElements_ = activeAnimScratch_;   // 复用成员缓冲与容量

            // 4. 合成绘制
            SetGlobalDpiScale(dpi_ / 96.0f);
            renderTarget_->BeginDraw();
            renderTarget_->Clear(backgroundColor_.ToD2D());

            if (rootElement_) {
                D2D1_SIZE_F rsz = renderTarget_->GetSize();
                ComposeImpl(rootElement_.get(), renderTarget_, D2D1::RectF(0, 0, rsz.width, rsz.height), true);
            }

            UIZSignals::DrawOverlay(this, renderTarget_);

            DrawFocusAndTooltip(renderTarget_);
            focusDirty_ = false;

            HRESULT hr = renderTarget_->EndDraw();
            if (hr == D2DERR_RECREATE_TARGET) {
                DiscardDeviceResources();
                if (FAILED(CreateDeviceResources())) {
                    MessageBoxW(hwnd_, L"设备资源重建失败", L"错误", MB_ICONERROR);
                    DestroyWindow(hwnd_);
                    EndPaint(hwnd_, &ps);
                    return;
                }
                // 设备重建后，清空缓存，但布局不需要重做
                ClearAllCaches();
                pendingRepaint_.clear();
                if (rootElement_) {
                    CollectVisibleCachedElements(rootElement_.get(), pendingRepaint_);
                }
            }
            else {
                // 本帧所有可见元素都已重新绘制，清空待重绘集合，避免空闲时残留 pending
                pendingRepaint_.clear();
            }

            EndPaint(hwnd_, &ps);
        }

        // 以下为原有事件处理函数，保留不变
        void OnMouseMove(float x, float y) {
            // 只有真实位移才重置悬停计时并关闭提示；重复的 WM_MOUSEMOVE（坐标未变）忽略
            bool realMove = fabs(x - mouseX_) > 0.5f || fabs(y - mouseY_) > 0.5f;
            mouseX_ = x; mouseY_ = y;
            if (realMove) {
                hoverStartTick_ = GetTickCount();
                if (tooltipTarget_ || tooltipProgress_ > 0.0f) { tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f; }
            }
            if (!rootElement_) return;
            if (mouseCaptureElement_) {
                mouseCaptureElement_->OnMouseMove(x, y);
                return;
            }
            if (pressedElement_) {
                pressedElement_->OnMouseMove(x, y);
                return;
            }
            UpdateHover(x, y);
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd_, 0 };
            TrackMouseEvent(&tme);
        }

        void OnMouseLeave() {
            if (currentHovered_) { currentHovered_->OnMouseLeave(); currentHovered_ = nullptr; }
            tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f;
        }

        void OnMouseDown(float x, float y) {
            tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f;
            if (mouseCaptureElement_) {
                if (pressedElement_) {
                    pressedElement_ = nullptr;
                    ReleaseCapture();
                }
                mouseCaptureElement_->OnMouseDown(x, y);
                return;
            }

            UIZSignals::GlobalMouseDown(this, x, y);

            UIElement* hit = rootElement_ ? rootElement_->HitTest(x, y) : nullptr;
            if (hit) {
                hit->OnMouseDown(x, y);
                SetCapture(hwnd_);
                pressedElement_ = hit;

                if (hit->IsFocusable()) {
                    SetFocusElement(hit, false);
                }
                else {
                    if (focusedElement_) { focusedElement_->OnBlur(); focusedElement_ = nullptr; }
                }
            }
            else {
                if (focusedElement_) { focusedElement_->OnBlur(); focusedElement_ = nullptr; }
            }
            UpdateIMEAssociation();
        }

        void OnMouseUp(float x, float y) {
            // 注意：Win32 捕获(SetCapture)只在“按住鼠标”期间需要；
            // 元素级捕获(mouseCaptureElement_，如 ComboBox 展开、拖拽)与它无关，松开鼠标必须释放 Win32 捕获，
            // 否则整个线程的鼠标都会被本窗口截走，其他窗口无法交互。
            UIElement* a = pressedElement_;
            UIElement* b = mouseCaptureElement_;
            if (a) a->OnMouseUp(x, y);
            if (b && b != a) b->OnMouseUp(x, y);
            pressedElement_ = nullptr;
            if (GetCapture() == hwnd_) ReleaseCapture();
            UpdateHover(x, y);
        }

        void OnContextMenu(float x, float y) {
            if (!rootElement_) return;
            UIElement* hit = rootElement_->HitTest(x, y);
            if (hit && hit->OnContextMenu(x, y)) return;   // 控件已处理右键
            std::shared_ptr<Menu> menu;
            if (hit && hit->GetContextMenu()) {
                menu = hit->GetContextMenu();
            }
            else if (windowContextMenu_) {
                menu = windowContextMenu_;
            }
            if (menu) {
                POINT pt;
                pt.x = static_cast<LONG>(MulDiv(static_cast<int>(x), static_cast<int>(dpi_), 96));
                pt.y = static_cast<LONG>(MulDiv(static_cast<int>(y), static_cast<int>(dpi_), 96));
                ClientToScreen(hwnd_, &pt);
                CloseActiveMenuWindow();
                activeMenuRoot_ = std::make_unique<MenuWindow>(menu, hwnd_, pt.x, pt.y);
                activeMenuRoot_->Show(pt.x, pt.y);
            }
        }

        void UpdateHover(float x, float y) {
            if (!rootElement_) return;
            UIElement* hit = rootElement_->HitTest(x, y);
            if (hit && !hit->IsEffectivelyEnabled()) hit = nullptr;
            if (hit != currentHovered_) {
                if (currentHovered_) currentHovered_->OnMouseLeave();
                if (hit) hit->OnMouseEnter();
                currentHovered_ = hit;
                hoverStartTick_ = GetTickCount();
            }
            if (hit) hit->OnMouseMove(x, y);
        }

        void CloseActiveMenuWindow() {
            if (activeMenuRoot_) {
                activeMenuRoot_->CloseAll();
                activeMenuRoot_.reset();
            }
        }

        void OnMouseWheel(float x, float y, float deltaX, float deltaY) {
            tooltipTarget_ = nullptr; tooltipProgress_ = 0.0f;
            if (!rootElement_) return;
            UIElement* elem = currentHovered_;
            if (!elem) elem = rootElement_->HitTest(x, y);
            while (elem) {
                if (elem->OnMouseWheel(deltaX, deltaY)) break;
                elem = elem->GetParent();
            }
        }

        HRESULT CreateDeviceResources() {
            if (renderTarget_) return S_OK;
            RECT rc; GetClientRect(hwnd_, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
            D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
                (FLOAT)dpi_, (FLOAT)dpi_);
            HRESULT hr = d2dFactory_->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(hwnd_, size), &renderTarget_);
            if (SUCCEEDED(hr)) {
                renderTarget_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                renderTarget_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);   // 必须改回 GRAYSCALE
            }
            return hr;
        }

        void DiscardDeviceResources() {
            if (renderTarget_) { renderTarget_->Release(); renderTarget_ = nullptr; }
            if (rootElement_) {
                // 释放所有元素的缓存和设备资源
                std::function<void(UIElement*)> releaseRecursive = [&](UIElement* elem) {
                    if (!elem) return;
                    elem->ReleaseDeviceResources();
                    for (auto* child : elem->GetChildren()) {
                        releaseRecursive(child);
                    }
                    };
                releaseRecursive(rootElement_.get());
            }
        }

        void ApplyBackdrop() {
            if (!hwnd_) return;
            HMODULE hUser = GetModuleHandleW(L"user32.dll");
            if (!hUser) return;
            auto pSetWindowCompositionAttribute = (BOOL(WINAPI*)(HWND, void*))GetProcAddress(hUser, "SetWindowCompositionAttribute");
            if (!pSetWindowCompositionAttribute) return;

            ACCENT_POLICY accent = {};
            accent.AccentState = (ACCENT_STATE)backdrop_;
            accent.GradientColor = backdropColor_;
            accent.AccentFlags = 0;
            accent.AnimationId = 0;

            WINDOWCOMPOSITIONATTRIBDATA data = {};
            data.Attrib = WCA_ACCENT_POLICY;
            data.pvData = &accent;
            data.cbData = sizeof(accent);
            pSetWindowCompositionAttribute(hwnd_, &data);
        }

        void ApplyTitleBarColors() {
            if (!hwnd_) return;
            DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionColor_, sizeof(captionColor_));
            DwmSetWindowAttribute(hwnd_, DWMWA_TEXT_COLOR, &textColor_, sizeof(textColor_));
            DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &borderColor_, sizeof(borderColor_));
        }

        void UpdateCompositionText() {
            HIMC hIMC = ImmGetContext(hwnd_);
            if (!hIMC) return;

            LONG strLen = ImmGetCompositionString(hIMC, GCS_COMPSTR, nullptr, 0);
            if (strLen > 0) {
                std::vector<wchar_t> buffer(strLen / sizeof(wchar_t) + 1);
                ImmGetCompositionString(hIMC, GCS_COMPSTR, buffer.data(), strLen);
                buffer[strLen / sizeof(wchar_t)] = L'\0';
                compositionText_ = buffer.data();
                hasComposition_ = true;

                LONG cursorPos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, nullptr, 0);
                if (cursorPos >= 0) compositionCursorPos_ = cursorPos;
                else compositionCursorPos_ = static_cast<int>(compositionText_.size());
            }
            else {
                hasComposition_ = false;
                compositionText_.clear();
                compositionCursorPos_ = 0;
            }
            ImmReleaseContext(hwnd_, hIMC);
            InvalidateRect(hwnd_, nullptr, FALSE);
        }

        void SetImePosition() {
            if (imePosUpdating_) return;
            imePosUpdating_ = true;

            if (!focusedElement_ || !focusedElement_->IsTextInput()) {
                imePosUpdating_ = false;
                return;
            }

            if (layoutNeeded_ || rootElement_->IsLayoutDirty()) {
                RECT rc;
                GetClientRect(hwnd_, &rc);
                float clientWidthDip = (rc.right - rc.left) * 96.0f / dpi_;
                float clientHeightDip = (rc.bottom - rc.top) * 96.0f / dpi_;
                Thickness rootMargin = rootElement_->GetMargin();
                float left = rootMargin.left, top = rootMargin.top;
                float availWidth = clientWidthDip - left - rootMargin.right;
                float availHeight = clientHeightDip - top - rootMargin.bottom;
                rootElement_->Measure(Size(availWidth, availHeight));
                rootElement_->Arrange(Rect(left, top, availWidth, availHeight));
                rootElement_->ClearLayoutDirty();
                layoutNeeded_ = false;
            }

            HIMC hIMC = ImmGetContext(hwnd_);
            if (!hIMC) { imePosUpdating_ = false; return; }

            float scale = dpi_ / 96.0f;
            Rect caretRect = focusedElement_->GetImeCandidateRect();
            float elemHeight = focusedElement_->GetArrangedRect().height * scale;

            COMPOSITIONFORM compForm = {};
            compForm.dwStyle = CFS_FORCE_POSITION;
            compForm.ptCurrentPos.x = static_cast<LONG>(caretRect.x * scale);
            compForm.ptCurrentPos.y = static_cast<LONG>(caretRect.y * scale + 2 * scale);
            compForm.rcArea = { 0, 0, 0, 0 };
            ImmSetCompositionWindow(hIMC, &compForm);

            CANDIDATEFORM candForm = {};
            candForm.dwIndex = 0;
            candForm.dwStyle = CFS_CANDIDATEPOS;
            candForm.ptCurrentPos.x = static_cast<LONG>(caretRect.x * scale);
            candForm.ptCurrentPos.y = static_cast<LONG>((caretRect.y + caretRect.height - 2) * scale);
            candForm.rcArea = { 0, 0, 0, 0 };
            ImmSetCandidateWindow(hIMC, &candForm);

            ImmReleaseContext(hwnd_, hIMC);
            imePosUpdating_ = false;
        }
        void CollectActiveAnimations(UIElement* elem, std::unordered_set<UIElement*>& activeSet) {
            if (!elem || !elem->IsVisible()) return;

            if (elem->HasActiveAnimation()) {
                activeSet.insert(elem);
            }
            for (auto* child : elem->GetChildren()) {
                CollectActiveAnimations(child, activeSet);
            }
        }

        void UpdateTimerState() {
            bool shouldRun = false;
            if (hwnd_ && IsWindow(hwnd_)) {
                shouldRun = IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
            }
            if (shouldRun && !timerRunning_) {
                SetTimer(hwnd_, 1, 16, nullptr);
                timerRunning_ = true;
            }
            else if (!shouldRun && timerRunning_) {
                KillTimer(hwnd_, 1);
                timerRunning_ = false;
            }
        }

        float PixelToDipX(int pixelX) const { return pixelX * 96.0f / dpi_; }
        float PixelToDipY(int pixelY) const { return pixelY * 96.0f / dpi_; }

        HWND hwnd_;
        detail::AppCore* core_ = nullptr;
        ID2D1Factory* d2dFactory_;
        ID2D1HwndRenderTarget* renderTarget_;
        std::shared_ptr<Layout> rootElement_;
        UIElement* currentHovered_;
        UIElement* pressedElement_;
        UIElement* focusedElement_;
        float mouseX_ = 0.0f, mouseY_ = 0.0f;
        DWORD hoverStartTick_ = 0;
        UIElement* tooltipTarget_ = nullptr;
        float tooltipProgress_ = 0.0f;
        D2D1_POINT_2F tooltipAnchorPt_ = {};
        bool focusDirty_ = false;
        bool showFocusRing_ = false;
        UINT dpi_;
        COLORREF captionColor_, textColor_, borderColor_;
        bool hasCustomMinSize_;
        int customMinWidth_, customMinHeight_;
        std::chrono::steady_clock::time_point lastTime_;
        bool layoutNeeded_;
        WindowBackdrop backdrop_;
        DWORD backdropColor_;
        Color backgroundColor_;
        bool animationTimerActive_;   // 常驻定时器，始终 true
        int animationIdleFrames_;     // 未使用，保留兼容
        std::shared_ptr<Menu> windowContextMenu_;
        std::unique_ptr<MenuWindow> activeMenuRoot_;
        UIElement* mouseCaptureElement_ = nullptr;
        Window* owner_ = nullptr;
        bool imePosUpdating_ = false;
        HIMC defaultIMC_ = nullptr;

        std::wstring compositionText_;
        bool hasComposition_ = false;
        int compositionCursorPos_ = 0;

        // 新增成员
        std::unordered_set<UIElement*> pendingRepaint_;
        bool layoutInvalidated_;
        // 用于记录上一帧活跃动画元素
        std::unordered_set<UIElement*> lastActiveAnimElements_;
        std::unordered_set<UIElement*> activeAnimScratch_;

        bool timerRunning_ = false;
    };

    // ---------- UIElement 路由实现（需 Window 完整类型） ----------
    inline void UIElement::RequestRepaint() {
        if (window_) window_->MarkRepaint(this);
        else UIZSignals::RepaintRequest(window_, this);
    }
    inline void UIElement::InvalidateLayout() {
        layoutDirty_ = true;
        ZUI_DEBUG_LOG_A((std::string("InvalidateLayout called by: ") + typeid(*this).name() + "\n").c_str());
        if (window_) window_->MarkLayoutInvalidated();
        else UIZSignals::LayoutInvalidated(window_);
    }

    // ---------- 应用（Qt 风格：app.CreateWindow(...) -> app.Run()） ----------
    class Application {
    public:
        Application() = default;

        // 创建并注册一个窗口；返回的 shared_ptr 需要被持有以维持窗口存活
        std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title) {
            auto w = std::make_shared<Window>();
            if (!w->Create(width, height, title)) return nullptr;
            return w;
        }
        // 带所有者的窗口（父子/owned）
        std::shared_ptr<Window> CreateWindow(int width, int height, const std::wstring& title, Window* owner) {
            auto w = CreateWindow(width, height, title);
            if (w) w->SetOwner(owner);
            return w;
        }
        // 把已有窗口登记进应用（一般由 Create 自动完成）
        void AddWindow(const std::shared_ptr<Window>& w) {
            if (w) detail::AppCore::Instance().AddWindow(w.get());
        }
        int Run() { return detail::AppCore::Instance().Run(); }
        void Quit(int code = 0) { detail::AppCore::Instance().Quit(code); }
        void CloseAllWindows() {
            auto wins = detail::AppCore::Instance().Windows();
            for (auto* w : wins) if (w) w->Close();
        }
        size_t WindowCount() const { return (size_t)detail::AppCore::Instance().WindowCount(); }

        static Application& Instance() { static Application a; return a; }
    };

} // namespace ZUI