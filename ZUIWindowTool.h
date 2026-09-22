#pragma once
// ============================================================================
// ZUIWindowTool.h —— 窗口工具控件（自定义标题栏等）
// ----------------------------------------------------------------------------
// 这里放“窗口级”的控件，未来还会放内置的 MessageBox 之类。
// 自定义标题栏 = TitleBar（继承 UIElement），由 Window::SetCustomTitleBar 安装。
// 它“不参与布局”，由 Window 放到 (0,0)，并把根布局整体下移其高度。
// 标题栏及按钮默认 UseCache，仅状态变化时重绘；且不参与 Tab 焦点。
// ============================================================================

#include "ZUIImages.h"
#include <vector>
#include <memory>

namespace ZUI {

    namespace detail_wintool {
        // 系统标题栏图标字体：Win11 = Segoe Fluent Icons；Win10 = Segoe MDL2 Assets
        inline IDWriteTextFormat* IconFormat(float dipSize) {
            IDWriteFactory* f = FontManager::Instance().GetFactory();
            if (!f) return nullptr;
            static IDWriteTextFormat* fmt = nullptr;
            static float cached = 0.0f;
            if (fmt && cached == dipSize) return fmt;
            if (fmt) { fmt->Release(); fmt = nullptr; }
            const wchar_t* families[] = { L"Segoe Fluent Icons", L"Segoe MDL2 Assets" };
            for (auto fam : families) {
                if (SUCCEEDED(f->CreateTextFormat(fam, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, dipSize, L"en-us", &fmt)) && fmt)
                    break;
            }
            if (fmt) {
                fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            }
            cached = dipSize;
            return fmt;
        }
    }

    // ------------------------------------------------------------------
    // CaptionButton：标题栏三件套按钮（最小化 / 最大化还原 / 关闭）
    // ------------------------------------------------------------------
    class CaptionButton : public UIElement {
    public:
        enum class Kind { Minimize, MaximizeRestore, Close };

        inline static Color DefaultHoverColor = Color::FromArgb(255, 229, 229, 229);
        inline static Color DefaultPressedColor = Color::FromArgb(255, 214, 214, 214);
        inline static Color DefaultCloseHoverColor = Color::FromArgb(255, 196, 43, 28);
        inline static Color DefaultClosePressedColor = Color::FromArgb(255, 177, 36, 24);
        inline static Color DefaultGlyphColor = Color::FromArgb(255, 30, 30, 30);
        inline static float DefaultWidth = 46.0f;

        explicit CaptionButton(Kind kind) : kind_(kind) {}

        // 点击信号（由 DefaultTitleBar 连接默认行为；自定义标题栏可自行连接/拦截）
        ZSignal<> Clicked;

        void SetKind(Kind k) { kind_ = k; RequestRepaint(); }
        Kind GetKind() const { return kind_; }
        void SetHoverColor(Color c) { hoverColor_ = c; RequestRepaint(); }
        void SetPressedColor(Color c) { pressedColor_ = c; RequestRepaint(); }
        void SetCloseHoverColor(Color c) { closeHoverColor_ = c; RequestRepaint(); }
        void SetClosePressedColor(Color c) { closePressedColor_ = c; RequestRepaint(); }
        void SetGlyphColor(Color c) { glyphColor_ = c; RequestRepaint(); }
        void SetButtonWidth(float w) { width_ = max(0.0f, w); InvalidateLayout(); RequestRepaint(); }
        float GetButtonWidth() const { return width_ > 0.0f ? width_ : DefaultWidth; }
        bool HasCustomWidth() const { return width_ > 0.0f; }

        bool IsFocusable() const override { return false; }   // 不参与 Tab 焦点

        Size MeasureOverride(const Size& availableSize) override {
            float h = height_ > 0 ? height_ : (availableSize.height != FLT_MAX ? availableSize.height : 32.0f);
            return Size(GetButtonWidth(), h);
        }

        void UpdateAnimation(float deltaTime) override {
            bool anim = false;
            float targetHover = hovered_ ? 1.0f : 0.0f;
            float targetPress = pressed_ ? 1.0f : 0.0f;
            if (hoverProgress_ != targetHover) {
                hoverProgress_ += (targetHover - hoverProgress_) * (1.0f - exp(-deltaTime * animSpeed_));
                if (fabs(hoverProgress_ - targetHover) < 0.002f) hoverProgress_ = targetHover;
                anim = true;
            }
            if (pressProgress_ != targetPress) {
                pressProgress_ += (targetPress - pressProgress_) * (1.0f - exp(-deltaTime * animSpeed_));
                if (fabs(pressProgress_ - targetPress) < 0.002f) pressProgress_ = targetPress;
                anim = true;
            }
            if (anim) RequestRepaint();
        }
        bool HasActiveAnimation() const override {
            return hoverProgress_ != (hovered_ ? 1.0f : 0.0f) || pressProgress_ != (pressed_ ? 1.0f : 0.0f);
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            bool isClose = (kind_ == Kind::Close);
            bool enabled = IsEffectivelyEnabled();
            // 背景：透明 → hover 色 → pressed 色，按进度渐变
            float hp = hoverProgress_, pp = pressProgress_;
            D2D1_COLOR_F cHover = (isClose ? closeHoverColor_ : hoverColor_).ToD2D();
            D2D1_COLOR_F cPress = (isClose ? closePressedColor_ : pressedColor_).ToD2D();
            auto mix = [](D2D1_COLOR_F a, D2D1_COLOR_F b, float t) -> D2D1_COLOR_F {
                return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                    a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
            };
            D2D1_COLOR_F bg = mix(cHover, cPress, pp);   // 颜色只在悬停色/按下色之间过渡
            bg.a *= max(hp, pp);                          // 透明度按进度渐入 → 不会经过暗色
            if (bg.a > 0.004f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(bg, bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(bg);
                if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get());
            }
            // 字形颜色：关闭键在悬停/按下时变白；禁用时置灰
            Color glyphColor = glyphColor_;
            if (isClose && (hp > 0.01f || pp > 0.01f)) glyphColor = Color::FromArgb(255, 255, 255, 255);
            if (!enabled) glyphColor = Color::FromArgb(255, 150, 150, 150);
            if (!glyphBrush_) rt->CreateSolidColorBrush(glyphColor.ToD2D(), glyphBrush_.GetAddressOf());
            else glyphBrush_->SetColor(glyphColor.ToD2D());
            if (!glyphBrush_) return;

            Window* w = GetWindow();
            bool maxed = w && w->IsMaximizedWindow();
            D2D1_RECT_F r = arrangedRect_.ToD2D();
            const wchar_t* glyph = L"\uE921";               // 最小化
            if (kind_ == Kind::MaximizeRestore) glyph = maxed ? L"\uE923" : L"\uE922";  // 还原 / 最大化
            else if (kind_ == Kind::Close) glyph = L"\uE8BB";                            // 关闭

            IDWriteTextFormat* fmt = detail_wintool::IconFormat(10.0f);
            if (fmt) rt->DrawText(glyph, 1, fmt, r, glyphBrush_.Get());
            else DrawVectorGlyph(rt, r, maxed);
        }
        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); glyphBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

        void OnMouseEnter() override { if (!IsEffectivelyEnabled()) return; hovered_ = true; pressed_ = false; RequestRepaint(); }
        void OnMouseLeave() override { hovered_ = false; pressed_ = false; RequestRepaint(); }   // 必须清 pressed_，否则按下后移出会把按下动画卡死
        void OnMouseDown(float, float) override { if (!IsEffectivelyEnabled()) return; pressed_ = true; RequestRepaint(); }
        void OnMouseUp(float, float) override {
            if (!pressed_) return;
            pressed_ = false;
            RequestRepaint();
            if (!IsEffectivelyEnabled()) return;
            Clicked.Fire();
        }
        void SetAnimationSpeed(float s) { animSpeed_ = max(0.1f, s); }
        void SetPressedVisual(bool p) { pressed_ = p; RequestRepaint(); }   // 只改视觉，不触发动作

    private:
        void DrawVectorGlyph(ID2D1RenderTarget* rt, const D2D1_RECT_F& r, bool maxed) {
            float cx = (r.left + r.right) * 0.5f, cy = (r.top + r.bottom) * 0.5f;
            const float s = 5.0f;
            if (kind_ == Kind::Minimize) {
                rt->DrawLine(D2D1::Point2F(cx - s, cy), D2D1::Point2F(cx + s, cy), glyphBrush_.Get(), 1.0f);
            } else if (kind_ == Kind::MaximizeRestore) {
                if (maxed) {
                    rt->DrawRectangle(D2D1::RectF(cx - s, cy - s + 2.0f, cx + s, cy + s), glyphBrush_.Get(), 1.0f);
                    rt->DrawRectangle(D2D1::RectF(cx - s + 2.0f, cy - s, cx + s, cy + s - 2.0f), glyphBrush_.Get(), 1.0f);
                } else {
                    rt->DrawRectangle(D2D1::RectF(cx - s, cy - s, cx + s, cy + s), glyphBrush_.Get(), 1.0f);
                }
            } else {
                rt->DrawLine(D2D1::Point2F(cx - s, cy - s), D2D1::Point2F(cx + s, cy + s), glyphBrush_.Get(), 1.0f);
                rt->DrawLine(D2D1::Point2F(cx + s, cy - s), D2D1::Point2F(cx - s, cy + s), glyphBrush_.Get(), 1.0f);
            }
        }

        Kind kind_;
        float width_ = 0.0f;
        bool hovered_ = false;
        bool pressed_ = false;
        float hoverProgress_ = 0.0f;    // hover 渐变进度
        float pressProgress_ = 0.0f;    // 按下渐变进度
        float animSpeed_ = 24.0f;    // 很快，但能看到渐变
        Color hoverColor_ = DefaultHoverColor;
        Color pressedColor_ = DefaultPressedColor;
        Color closeHoverColor_ = DefaultCloseHoverColor;
        Color closePressedColor_ = DefaultClosePressedColor;
        Color glyphColor_ = DefaultGlyphColor;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> glyphBrush_;
    };

    // ------------------------------------------------------------------
    // TitleBar：标题栏基类（图标 + 标题 + 右侧按钮区）
    // ------------------------------------------------------------------
    class TitleBar : public UIElement {
    public:
        TitleBar() { height_ = 32.0f; }

        void SetTitle(const std::wstring& t) { title_ = t; RequestRepaint(); }
        const std::wstring& GetTitle() const { return title_; }
        void SetIcon(std::shared_ptr<Image> img) { icon_ = std::move(img); RequestRepaint(); }
        std::shared_ptr<Image> GetIcon() const { return icon_; }
        void SetIconSize(float w, float h) { iconW_ = w; iconH_ = h; InvalidateLayout(); RequestRepaint(); }
        void SetShowIcon(bool on) { showIcon_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetShowTitle(bool on) { showTitle_ = on; InvalidateLayout(); RequestRepaint(); }
        void SetContentPadding(float left, float right = 8.0f) { padLeft_ = left; padRight_ = right; InvalidateLayout(); RequestRepaint(); }
        void SetBackgroundColor(Color c) { bgColor_ = c; RequestRepaint(); }
        void SetActiveBackgroundColor(Color c) { activeBgColor_ = c; RequestRepaint(); }
        void SetTitleColor(Color c) { titleColor_ = c; RequestRepaint(); }
        void SetActiveTitleColor(Color c) { activeTitleColor_ = c; RequestRepaint(); }
        void SetButtonWidth(float w) { userButtonWidth_ = true; buttonWidth_ = max(0.0f, w); for (auto& b : buttons_) b->SetButtonWidth(w); InvalidateLayout(); RequestRepaint(); }
        void SetButtonHeight(float h) { userButtonHeight_ = true; buttonHeight_ = max(0.0f, h); InvalidateLayout(); RequestRepaint(); }
        // 覆盖右边距（默认跟随 DWM 系统值）
        void SetRightMargin(float m) { rightMarginOverride_ = true; rightMargin_ = max(0.0f, m); InvalidateLayout(); RequestRepaint(); }
        void ClearRightMargin() { rightMarginOverride_ = false; InvalidateLayout(); RequestRepaint(); }

        bool IsFocusable() const override { return false; }   // 不参与 Tab 焦点
        bool UseCache() const override { return true; }

        void AttachWindowRecursive(Window* w) override {
            windowId_ = WindowIdOf(w);
            for (auto& b : buttons_) b->AttachWindowRecursive(w);
            if (w && !connected_) {
                connected_ = true;
                Connect(w->Activated, [this]() { active_ = true; RequestRepaint(); });
                Connect(w->Deactivated, [this]() { active_ = false; RequestRepaint(); });
            }
        }

        Size MeasureOverride(const Size& availableSize) override {
            float w = width_ > 0 ? width_ : (availableSize.width != FLT_MAX ? availableSize.width : 0.0f);
            return Size(w, height_);
        }

        void ArrangeOverride(const Rect& finalRect) override {
            UIElement::ArrangeOverride(finalRect);

            // 右上角锚定、按钮右边缘与标题栏右边缘齐平（不做 DWM 推算，避免被阴影/整体窗口干扰）。
            // 需要留边距可 SetRightMargin 覆盖；需要不同尺寸可 SetButtonWidth/Height。
            float rightMargin = rightMarginOverride_ ? rightMargin_ : 1.0f;
            float btnW = userButtonWidth_ ? buttonWidth_ : CaptionButton::DefaultWidth;
            float btnH = (userButtonHeight_ && buttonHeight_ > 0.0f) ? buttonHeight_ : finalRect.height;
            btnH = clamp(btnH, 1.0f, finalRect.height);
            float btnTop = (finalRect.height - btnH) * 0.5f;

            float right = finalRect.x + finalRect.width - rightMargin;
            for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it) {
                auto& b = *it;
                if (!b->IsVisible()) continue;   // 隐藏的按钮不占位
                if (!b->HasCustomWidth()) b->SetButtonWidth(btnW);
                float bw = b->GetButtonWidth();
                b->SetHeight(btnH);
                b->Arrange(Rect(right - bw, finalRect.y + btnTop, bw, btnH));
                right -= bw;
            }
            buttonsTotalWidth_ = (finalRect.x + finalRect.width - rightMargin) - right;
            SetDragRegion(Rect(0.0f, 0.0f, max(0.0f, right - finalRect.x), finalRect.height));
        }

        const std::vector<UIElement*>& GetChildren() const override {
            if (!childrenDirty_) return childrenView_;
            childrenDirty_ = false;
            childrenView_.clear();
            for (auto& b : buttons_) childrenView_.push_back(b.get());
            return childrenView_;
        }
        UIElement* HitTest(float x, float y) override {
            for (auto it = buttons_.rbegin(); it != buttons_.rend(); ++it)
                if (UIElement* h = (*it)->HitTest(x, y)) return h;
            return UIElement::HitTest(x, y);
        }

        // 把按钮报告为系统按钮码 → 启用系统原生行为（最大化按钮悬浮的 Snap Layouts 等）
        int NonClientHitTest(float x, float y) const override {
            for (auto& b : buttons_) {
                if (!b || !b->IsVisible() || !b->IsEffectivelyEnabled()) continue;
                if (b->GetArrangedRect().Contains(x, y)) {
                    switch (b->GetKind()) {
                    case CaptionButton::Kind::Minimize:        return HTMINBUTTON;
                    case CaptionButton::Kind::MaximizeRestore: return HTMAXBUTTON;
                    case CaptionButton::Kind::Close:           return HTCLOSE;
                    }
                }
            }
            return 0;
        }

        // 标题栏按钮是否可点击（禁用后按钮置灰且不再响应）
        void SetButtonsEnabled(bool on) {
            buttonsEnabled_ = on;
            for (auto& b : buttons_) b->SetEnabled(on);
            RequestRepaint();
        }
        bool AreButtonsEnabled() const { return buttonsEnabled_; }

        // ---- 按按钮类型单独控制（详细禁用 API） ----
        std::shared_ptr<CaptionButton> GetButton(CaptionButton::Kind k) const {
            for (auto& b : buttons_) if (b && b->GetKind() == k) return b;
            return nullptr;
        }
        // 单独禁用/启用某个按钮：置灰、不响应鼠标、不报告系统按钮码
        void SetButtonEnabled(CaptionButton::Kind k, bool on) {
            if (auto b = GetButton(k)) b->SetEnabled(on);
            RequestRepaint();
        }
        bool IsButtonEnabled(CaptionButton::Kind k) const {
            auto b = GetButton(k);
            return b && b->IsEffectivelyEnabled();
        }
        // 单独显示/隐藏某个按钮：隐藏后不再占位（布局自动重排）
        void SetButtonVisible(CaptionButton::Kind k, bool on) {
            if (auto b = GetButton(k)) { b->SetVisible(on); InvalidateLayout(); RequestRepaint(); }
        }
        bool IsButtonVisible(CaptionButton::Kind k) const {
            auto b = GetButton(k);
            return b && b->IsVisible();
        }

        bool HasActiveAnimation() const override {
            for (auto& b : buttons_) if (b && b->HasActiveAnimation()) return true;
            return false;
        }

        void SetNonClientButtonPressed(int hit, bool pressed) override {
            for (auto& b : buttons_) {
                if (!b) continue;
                int code = 0;
                switch (b->GetKind()) {
                case CaptionButton::Kind::Minimize:        code = HTMINBUTTON; break;
                case CaptionButton::Kind::MaximizeRestore: code = HTMAXBUTTON; break;
                case CaptionButton::Kind::Close:           code = HTCLOSE; break;
                }
                if (code == hit) b->SetPressedVisual(pressed);
            }
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;
            Color bg = active_ ? activeBgColor_ : bgColor_;
            if (bg.a > 0.0f) {
                if (!bgBrush_) rt->CreateSolidColorBrush(bg.ToD2D(), bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(bg.ToD2D());
                if (bgBrush_) rt->FillRectangle(arrangedRect_.ToD2D(), bgBrush_.Get());
            }
            float x = arrangedRect_.x + padLeft_;
            float cy = arrangedRect_.y + arrangedRect_.height * 0.5f;
            if (showIcon_ && icon_ && !icon_->IsNull()) {
                float iw = iconW_ > 0 ? iconW_ : (float)icon_->Width();
                float ih = iconH_ > 0 ? iconH_ : (float)icon_->Height();
                icon_->Draw(rt, D2D1::RectF(x, cy - ih * 0.5f, x + iw, cy + ih * 0.5f));
                x += iw + 8.0f;
            }
            if (showTitle_ && !title_.empty()) {
                IDWriteTextFormat* fmt = GetFontFormat();
                IDWriteFactory* dw = FontManager::Instance().GetFactory();
                if (fmt && dw) {
                    Color tc = active_ ? activeTitleColor_ : titleColor_;
                    if (!textBrush_) rt->CreateSolidColorBrush(tc.ToD2D(), textBrush_.GetAddressOf());
                    else textBrush_->SetColor(tc.ToD2D());
                    float textRight = arrangedRect_.x + arrangedRect_.width - buttonsTotalWidth_ - padRight_;
                    D2D1_RECT_F tr = D2D1::RectF(x, arrangedRect_.y, textRight, arrangedRect_.y + arrangedRect_.height);
                    if (tr.right > tr.left && textBrush_) {
                        ComPtr<IDWriteTextLayout> layout;
                        dw->CreateTextLayout(title_.c_str(), (UINT32)title_.length(), fmt,
                            tr.right - tr.left, tr.bottom - tr.top, &layout);
                        if (layout) {
                            layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                            layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
                            layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                            ComPtr<IDWriteInlineObject> ellipsis;
                            if (SUCCEEDED(dw->CreateEllipsisTrimmingSign(fmt, &ellipsis))) {
                                DWRITE_TRIMMING tm = { DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
                                layout->SetTrimming(&tm, ellipsis.Get());
                            }
                            rt->DrawTextLayout(D2D1::Point2F(tr.left, tr.top), layout.Get(), textBrush_.Get());
                        }
                    }
                }
            }
        }
        void UpdateAnimation(float deltaTime) override { for (auto& b : buttons_) b->UpdateAnimation(deltaTime); }
        void ReleaseDeviceResources() override {
            bgBrush_.Reset(); textBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    protected:
        void AddButton(std::shared_ptr<CaptionButton> b) {
            if (!b) return;
            b->SetButtonWidth(buttonWidth_);
            b->SetParent(this);
            buttons_.push_back(std::move(b));
            InvalidateLayout();
            RequestRepaint();
        }

        std::wstring title_;
        std::shared_ptr<Image> icon_;
        bool showIcon_ = true;
        bool showTitle_ = true;
        float iconW_ = 16.0f, iconH_ = 16.0f;
        float padLeft_ = 12.0f, padRight_ = 8.0f;
        float buttonWidth_ = CaptionButton::DefaultWidth;
        bool userButtonWidth_ = false;
        bool userButtonHeight_ = false;
        float buttonHeight_ = 0.0f;
        bool rightMarginOverride_ = false;
        float rightMargin_ = 0.0f;
        float buttonsTotalWidth_ = 0.0f;
        bool active_ = true;
        bool connected_ = false;
        bool buttonsEnabled_ = true;
        Color bgColor_ = Color::FromArgb(0, 0, 0, 0);
        Color activeBgColor_ = Color::FromArgb(0, 0, 0, 0);
        Color titleColor_ = Color::FromArgb(255, 70, 70, 70);
        Color activeTitleColor_ = Color::FromArgb(255, 26, 26, 26);
        std::vector<std::shared_ptr<CaptionButton>> buttons_;
        mutable std::vector<UIElement*> childrenView_;
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
    };

    // ------------------------------------------------------------------
    // DefaultTitleBar：内置三件套 + 图标 + 标题
    // ------------------------------------------------------------------
    class DefaultTitleBar : public TitleBar {
    public:
        DefaultTitleBar() {
            minBtn_ = std::make_shared<CaptionButton>(CaptionButton::Kind::Minimize);
            maxBtn_ = std::make_shared<CaptionButton>(CaptionButton::Kind::MaximizeRestore);
            closeBtn_ = std::make_shared<CaptionButton>(CaptionButton::Kind::Close);
            // 右起顺序：关闭在最右，然后最大化还原，然后最小化
            AddButton(minBtn_);
            AddButton(maxBtn_);
            AddButton(closeBtn_);
            // 默认行为在这里接线（按钮本身只发 Clicked；自定义标题栏可自行改写/拦截）
            Connect(minBtn_->Clicked, [this]() { if (Window* w = GetWindow()) w->Minimize(); });
            Connect(maxBtn_->Clicked, [this]() { if (Window* w = GetWindow()) w->MaximizeRestore(); });
            Connect(closeBtn_->Clicked, [this]() { if (Window* w = GetWindow()) w->Close(); });
        }
        std::shared_ptr<CaptionButton> GetMinButton() const { return minBtn_; }
        std::shared_ptr<CaptionButton> GetMaxButton() const { return maxBtn_; }
        std::shared_ptr<CaptionButton> GetCloseButton() const { return closeBtn_; }

        // 便捷：单独启用/禁用、显示/隐藏三件套
        void SetMinimizeEnabled(bool on) { SetButtonEnabled(CaptionButton::Kind::Minimize, on); }
        void SetMaximizeEnabled(bool on) { SetButtonEnabled(CaptionButton::Kind::MaximizeRestore, on); }
        void SetCloseEnabled(bool on) { SetButtonEnabled(CaptionButton::Kind::Close, on); }
        void SetMinimizeVisible(bool on) { SetButtonVisible(CaptionButton::Kind::Minimize, on); }
        void SetMaximizeVisible(bool on) { SetButtonVisible(CaptionButton::Kind::MaximizeRestore, on); }
        void SetCloseVisible(bool on) { SetButtonVisible(CaptionButton::Kind::Close, on); }

    private:
        std::shared_ptr<CaptionButton> minBtn_, maxBtn_, closeBtn_;
    };

} // namespace ZUI
