#pragma once
#include "ZUI.h"
#include "ZUIWidgets.h"

namespace ZUI {
    // ==================== 列表视图 ListView ====================
    class ListView : public UIElement {
    public:
        // 默认样式
        inline static float DefaultItemHeight = 28.0f;
        inline static D2D1_COLOR_F DefaultBackgroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultSelectedColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultScrollTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultScrollThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultScrollHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultIndicatorAnimSpeed = 12.0f;
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultScrollAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultWidth = 200.0f;
        inline static float DefaultHeight = 200.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;

        // 信号
        ZSignal<int> SelectionChanged;   // 选中项变化
        ZSignal<int> ItemClicked;        // 项目被点击

        ListView()
            : selectedIndex_(-1), hoveredIndex_(-1),
            scrollOffsetY_(0.0f), targetScrollOffsetY_(0.0f), maxScrollY_(0.0f),
            showScrollBar_(false), isDraggingScroll_(false),
            dragStartMouseY_(0.0f), dragStartScrollY_(0.0f),
            itemHeight_(DefaultItemHeight),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            indicatorAnimSpeed_(DefaultIndicatorAnimSpeed),
            indicatorY_(0.0f), targetIndicatorY_(0.0f),
            scrollBarWidth_(DefaultScrollBarWidth),
            scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep),
            scrollAnimationSpeed_(DefaultScrollAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            backgroundColor_(DefaultBackgroundColor),
            textColor_(DefaultTextColor),
            selectedColor_(DefaultSelectedColor),
            hoverColor_(DefaultHoverColor),
            borderColor_(DefaultBorderColor),
            scrollTrackColor_(DefaultScrollTrackColor),
            scrollThumbColor_(DefaultScrollThumbColor),
            scrollHoverThumbColor_(DefaultScrollHoverThumbColor),
            scrollHoverProgress_(0.0f),
            isHovered_(false),
            isScrollBarHovered_(false),
            buttonMode_(false),
            buttonSpacing_(4.0f) {
            width_ = DefaultWidth;
            height_ = DefaultHeight;
            // 使用字体管理器，不再创建本地 textFormat_
        }

        // 数据操作
        void AddItem(std::shared_ptr<Label> label) {
            if (!label) return;
            items_.push_back(label);
            label->SetParent(this);
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        void AddItem(const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            AddItem(label);
        }
        void InsertItem(int index, std::shared_ptr<Label> label) {
            if (index < 0 || index >(int)items_.size() || !label) return;
            items_.insert(items_.begin() + index, label);
            label->SetParent(this);
            if (selectedIndex_ >= index) selectedIndex_++;
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        void InsertItem(int index, const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            InsertItem(index, label);
        }
        void RemoveItem(int index) {
            if (index < 0 || index >= (int)items_.size()) return;
            items_.erase(items_.begin() + index);
            if (selectedIndex_ == index) selectedIndex_ = -1;
            else if (selectedIndex_ > index) selectedIndex_--;
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        void Clear() {
            items_.clear();
            selectedIndex_ = -1;
            hoveredIndex_ = -1;
            scrollOffsetY_ = 0.0f;
            targetScrollOffsetY_ = 0.0f;
            maxScrollY_ = 0.0f;
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int index, std::shared_ptr<Label> label) {
            if (index < 0 || index >= (int)items_.size() || !label) return;
            items_[index] = label;
            label->SetParent(this);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int index, const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            SetItem(index, label);
        }
        std::shared_ptr<Label> GetItemLabel(int index) const {
            if (index < 0 || index >= (int)items_.size()) return nullptr;
            return items_[index];
        }
        std::wstring GetItemText(int index) const {
            auto label = GetItemLabel(index);
            return label ? label->GetText() : L"";
        }
        int GetItemCount() const { return (int)items_.size(); }

        // 选择
        void SetSelectedIndex(int index) {
            if (index < -1 || index >= (int)items_.size()) return;
            if (selectedIndex_ != index) {
                selectedIndex_ = index;
                SelectionChanged(selectedIndex_);
                EnsureVisible(selectedIndex_);
                UpdateIndicatorTarget();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        int GetSelectedIndex() const { return selectedIndex_; }
        std::wstring GetSelectedText() const { return GetItemText(selectedIndex_); }

        // 按钮模式设置
        void SetButtonMode(bool enable) { buttonMode_ = enable; UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }
        bool IsButtonMode() const { return buttonMode_; }
        void SetButtonSpacing(float spacing) { buttonSpacing_ = max(0.0f, spacing); UpdateScrollInfo(); InvalidateLayout(); RequestRepaint(); }

        // 样式设置
        void SetItemHeight(float height) { itemHeight_ = height; UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint(); }
        float GetItemHeight() const { return itemHeight_; }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetIndicatorColor(Color color) { indicatorColor_ = color.ToD2D(); indicatorBrush_.Reset(); RequestRepaint(); }
        void SetIndicatorAnimationSpeed(float speed) { indicatorAnimSpeed_ = speed; }

        void SetBackgroundColor(Color color) { backgroundColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetTextColor(Color color) {
            textColor_ = color.ToD2D();
            for (auto& label : items_) {
                if (label) label->SetTextColor(color);
            }
            textBrush_.Reset();
            RequestRepaint();
        }
        void SetSelectedColor(Color color) { selectedColor_ = color.ToD2D(); selectedBrush_.Reset(); RequestRepaint(); }
        void SetHoverColor(Color color) { hoverColor_ = color.ToD2D(); hoverBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            scrollTrackColor_ = track.ToD2D();
            scrollThumbColor_ = thumb.ToD2D();
            scrollHoverThumbColor_ = hoverThumb.ToD2D();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            RequestRepaint();
        }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetScrollAnimationSpeed(float speed) { scrollAnimationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = speed; }

        // 全局默认样式
        static void SetDefaultItemHeight(float height) { DefaultItemHeight = height; }
        static void SetDefaultColors(Color bg, Color text, Color selected, Color hover, Color border) {
            DefaultBackgroundColor = bg.ToD2D();
            DefaultTextColor = text.ToD2D();
            DefaultSelectedColor = selected.ToD2D();
            DefaultHoverColor = hover.ToD2D();
            DefaultBorderColor = border.ToD2D();
        }
        static void SetDefaultIndicatorColor(Color color) { DefaultIndicatorColor = color.ToD2D(); }
        static void SetDefaultIndicatorWidth(float width) { DefaultIndicatorWidth = width; }
        static void SetDefaultIndicatorHeightRatio(float ratio) { DefaultIndicatorHeightRatio = clamp(ratio, 0.1f, 1.0f); }
        static void SetDefaultIndicatorAnimationSpeed(float speed) { DefaultIndicatorAnimSpeed = speed; }
        static void SetDefaultScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            DefaultScrollTrackColor = track.ToD2D();
            DefaultScrollThumbColor = thumb.ToD2D();
            DefaultScrollHoverThumbColor = hoverThumb.ToD2D();
        }
        static void SetDefaultScrollBarWidth(float width) { DefaultScrollBarWidth = width; }
        static void SetDefaultScrollBarMinLength(float length) { DefaultScrollBarMinLength = length; }
        static void SetDefaultScrollWheelStep(float step) { DefaultScrollWheelStep = step; }
        static void SetDefaultScrollAnimationSpeed(float speed) { DefaultScrollAnimationSpeed = speed; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }

        // UIElement 接口
        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }
        bool IsFocusable() const override { return true; }

        Size Measure(const Size& availableSize) override { return Size(width_, height_); }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            float viewportWidth = arrangedRect_.width - (showScrollBar_ ? scrollBarWidth_ : 0);
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;

            if (!buttonMode_) {
                if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_, bgBrush_.GetAddressOf());
                else bgBrush_->SetColor(backgroundColor_);
                if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());
            }

            D2D1_RECT_F clipRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + arrangedRect_.height);
            rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            int firstVisible = (int)(scrollOffsetY_ / effectiveRowHeight);
            int lastVisible = (int)((scrollOffsetY_ + arrangedRect_.height) / effectiveRowHeight);
            lastVisible = min(lastVisible, (int)items_.size() - 1);
            if (firstVisible < 0) firstVisible = 0;

            IDWriteTextFormat* fmt = GetFontFormat();

            for (int i = firstVisible; i <= lastVisible && i < (int)items_.size(); ++i) {
                float itemY = arrangedRect_.y + i * effectiveRowHeight - Snap(scrollOffsetY_);
                D2D1_RECT_F itemRect = D2D1::RectF(arrangedRect_.x, itemY,
                    arrangedRect_.x + viewportWidth, itemY + itemHeight_);

                if (buttonMode_) {
                    itemRect.left += 4.0f;
                    itemRect.right -= 4.0f;
                    itemRect.top += buttonSpacing_ / 2.0f;
                    itemRect.bottom -= buttonSpacing_ / 2.0f;
                }

                if (i == selectedIndex_) {
                    if (!selectedBrush_) rt->CreateSolidColorBrush(selectedColor_, selectedBrush_.GetAddressOf());
                    if (buttonMode_) rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, 4, 4), selectedBrush_.Get());
                    else rt->FillRectangle(itemRect, selectedBrush_.Get());
                }
                else if (i == hoveredIndex_) {
                    if (!hoverBrush_) rt->CreateSolidColorBrush(hoverColor_, hoverBrush_.GetAddressOf());
                    if (buttonMode_) rt->FillRoundedRectangle(D2D1::RoundedRect(itemRect, 4, 4), hoverBrush_.Get());
                    else rt->FillRectangle(itemRect, hoverBrush_.Get());
                }

                auto label = items_[i];
                if (label && !label->GetText().empty()) {
                    D2D1_RECT_F textRect = itemRect;
                    textRect.left += 8.0f;
                    textRect.right -= 8.0f;
                    DrawTextWithEllipsis(rt, label->GetText(), textRect, textColor_,
                        GetEffectiveFontSpec(), textBrush_, fmt);
                }
            }

            // 指示条
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
                float indicatorTop = arrangedRect_.y + indicatorY_ - Snap(scrollOffsetY_);
                float indicatorHeight = itemHeight_ * indicatorHeightRatio_;
                float indicatorOffset = (itemHeight_ - indicatorHeight) / 2.0f;
                float drawTop = indicatorTop + indicatorOffset;
                float drawBottom = drawTop + indicatorHeight;

                if (drawBottom > arrangedRect_.y && drawTop < arrangedRect_.y + arrangedRect_.height) {
                    if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, indicatorBrush_.GetAddressOf());
                    else indicatorBrush_->SetColor(indicatorColor_);
                    float indicatorLeft = buttonMode_ ? arrangedRect_.x + 4.0f : arrangedRect_.x;
                    D2D1_RECT_F indicatorRect = D2D1::RectF(indicatorLeft, drawTop,
                        indicatorLeft + indicatorWidth_, drawBottom);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, indicatorWidth_ / 2, indicatorWidth_ / 2), indicatorBrush_.Get());
                }
            }

            rt->PopAxisAlignedClip();

            if (showScrollBar_) DrawScrollBar(rt, viewportWidth);

            if (!buttonMode_) {
                if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
                else borderBrush_->SetColor(borderColor_);
                if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);
            }
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) {
                if (showScrollBar_) {
                    float trackX = arrangedRect_.x + arrangedRect_.width - scrollBarWidth_;
                    if (x >= trackX) return this;
                }
                return this;
            }
            return nullptr;
        }

        void OnMouseEnter() override { isHovered_ = true; RequestRepaint(); if (MouseEnterHandler) MouseEnterHandler(); }
        void OnMouseLeave() override {
            isHovered_ = false;
            hoveredIndex_ = -1;
            isScrollBarHovered_ = false;
            RequestRepaint();
            if (MouseLeaveHandler) MouseLeaveHandler();
        }
        void OnMouseMove(float x, float y) override {
            if (isDraggingScroll_) {
                float trackY = arrangedRect_.y;
                float trackHeight = arrangedRect_.height;
                float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                if (trackHeight > thumbLength) {
                    float ratio = (y - dragStartMouseY_) / (trackHeight - thumbLength);
                    targetScrollOffsetY_ = clamp(dragStartScrollY_ + ratio * maxScrollY_, 0.0f, maxScrollY_);
                    RequestRepaint();
                }
                return;
            }
            if (!arrangedRect_.Contains(x, y)) {
                hoveredIndex_ = -1;
                isScrollBarHovered_ = false;
                return;
            }
            isScrollBarHovered_ = (showScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_);
            if (showScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) {
                hoveredIndex_ = -1;
                RequestRepaint();
                return;
            }
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float relY = y - arrangedRect_.y + Snap(scrollOffsetY_);
            int idx = (int)(relY / effectiveRowHeight);
            if (idx >= 0 && idx < (int)items_.size())
                hoveredIndex_ = idx;
            else
                hoveredIndex_ = -1;
            RequestRepaint();
            if (MouseMoveHandler) MouseMoveHandler(x, y);
        }
        void OnMouseDown(float x, float y) override {
            if (!arrangedRect_.Contains(x, y)) return;
            if (showScrollBar_) {
                float trackX = arrangedRect_.x + arrangedRect_.width - scrollBarWidth_;
                if (x >= trackX) {
                    float trackY = arrangedRect_.y;
                    float trackHeight = arrangedRect_.height;
                    float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                    float thumbY = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
                    D2D1_RECT_F thumbRect = D2D1::RectF(trackX, thumbY, trackX + scrollBarWidth_, thumbY + thumbLength);
                    if (y >= thumbRect.top && y <= thumbRect.bottom) {
                        isDraggingScroll_ = true;
                        dragStartMouseY_ = y;
                        dragStartScrollY_ = scrollOffsetY_;
                        return;
                    }
                    else {
                        float ratio = (y - trackY - thumbLength / 2) / (trackHeight - thumbLength);
                        targetScrollOffsetY_ = clamp(ratio * maxScrollY_, 0.0f, maxScrollY_);
                        RequestRepaint();
                        return;
                    }
                }
            }
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float relY = y - arrangedRect_.y + Snap(scrollOffsetY_);
            int idx = (int)(relY / effectiveRowHeight);
            if (idx >= 0 && idx < (int)items_.size()) {
                SetSelectedIndex(idx);
                ItemClicked(idx);
            }
            if (MouseDownHandler) MouseDownHandler(x, y);
        }
        void OnMouseUp(float x, float y) override {
            if (isDraggingScroll_) {
                isDraggingScroll_ = false;
                RequestRepaint();
                return;
            }
            if (MouseUpHandler) MouseUpHandler(x, y);
        }
        bool OnMouseWheel(float deltaX, float deltaY) override {
            if (maxScrollY_ > 0) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                RequestRepaint();
                return true;
            }
            return false;
        }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsFocusable() || items_.empty()) return;
            switch (key) {
            case VK_UP:
                if (selectedIndex_ > 0) SetSelectedIndex(selectedIndex_ - 1);
                break;
            case VK_DOWN:
                if (selectedIndex_ < (int)items_.size() - 1) SetSelectedIndex(selectedIndex_ + 1);
                break;
            case VK_HOME:
                if (selectedIndex_ != 0) SetSelectedIndex(0);
                break;
            case VK_END:
                if (selectedIndex_ != (int)items_.size() - 1) SetSelectedIndex((int)items_.size() - 1);
                break;
            case VK_PRIOR:
                if (selectedIndex_ > 0) {
                    int newIdx = max(0, selectedIndex_ - (int)(arrangedRect_.height / itemHeight_));
                    SetSelectedIndex(newIdx);
                }
                break;
            case VK_NEXT:
                if (selectedIndex_ < (int)items_.size() - 1) {
                    int newIdx = min((int)items_.size() - 1, selectedIndex_ + (int)(arrangedRect_.height / itemHeight_));
                    SetSelectedIndex(newIdx);
                }
                break;
            default:
                break;
            }
            if (KeyDownHandler) KeyDownHandler(key, lParam);
        }
        void OnFocus() override { RequestRepaint(); if (FocusHandler) FocusHandler(); }
        void OnBlur() override { RequestRepaint(); if (BlurHandler) BlurHandler(); }

        void UpdateAnimation(float deltaTime) override {
            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                RequestRepaint();
            }
            else {
                scrollOffsetY_ = targetScrollOffsetY_;
            }
            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorAnimSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorY_ - targetIndicatorY_) > 0.01f) RequestRepaint();

            if (isScrollBarHovered_ || isDraggingScroll_) {
                if (scrollHoverProgress_ < 1.0f) {
                    scrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (scrollHoverProgress_ > 1.0f) scrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (scrollHoverProgress_ > 0.0f) {
                    scrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (scrollHoverProgress_ < 0.0f) scrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            // 在末尾强制收敛
            ConvergeValue(scrollHoverProgress_, isScrollBarHovered_ ? 1.0f : 0.0f, 0.001f);
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.001f);
            ConvergeValue(scrollOffsetY_, targetScrollOffsetY_, 0.001f);
        }

        bool HasActiveAnimation() const override {
            const float eps = 0.001f;
            return fabs(targetScrollOffsetY_ - scrollOffsetY_) > eps ||
                fabs(indicatorY_ - targetIndicatorY_) > eps ||
                (isScrollBarHovered_ && scrollHoverProgress_ < 1.0f - eps) ||
                (!isScrollBarHovered_ && scrollHoverProgress_ > eps);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            textBrush_.Reset();
            selectedBrush_.Reset();
            hoverBrush_.Reset();
            borderBrush_.Reset();
            indicatorBrush_.Reset();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            for (auto& label : items_) if (label) label->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    private:
        void UpdateScrollInfo() {
            float contentHeight = items_.size() * (buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_);
            showScrollBar_ = contentHeight > arrangedRect_.height;
            maxScrollY_ = max(0.0f, contentHeight - arrangedRect_.height);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void EnsureVisible(int index) {
            if (index < 0) return;
            float effectiveRowHeight = buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_;
            float itemTop = index * effectiveRowHeight;
            float itemBottom = itemTop + itemHeight_;
            if (itemTop < scrollOffsetY_) {
                targetScrollOffsetY_ = itemTop;
            }
            else if (itemBottom > scrollOffsetY_ + arrangedRect_.height) {
                targetScrollOffsetY_ = itemBottom - arrangedRect_.height;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void UpdateIndicatorTarget() {
            if (selectedIndex_ >= 0 && selectedIndex_ < (int)items_.size()) {
                targetIndicatorY_ = selectedIndex_ * (buttonMode_ ? (itemHeight_ + buttonSpacing_) : itemHeight_);
            }
            else {
                targetIndicatorY_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
        }

        void DrawScrollBar(ID2D1RenderTarget* rt, float viewportWidth) {
            float baseTrackWidth = scrollBarWidth_;
            float trackWidth = baseTrackWidth * (1.0f + 0.25f * scrollHoverProgress_);
            float trackX = arrangedRect_.x + arrangedRect_.width - trackWidth;
            float trackY = arrangedRect_.y;
            float trackHeight = arrangedRect_.height;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            if (scrollTrackBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight),
                    trackWidth / 2, trackWidth / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            float thumbPos = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
            float thumbWidth = trackWidth - 2.0f;
            if (thumbWidth < 2.0f) thumbWidth = 2.0f;
            float thumbX = trackX + (trackWidth - thumbWidth) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (scrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * scrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * scrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * scrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * scrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            if (scrollThumbBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbX, thumbPos, thumbX + thumbWidth, thumbPos + thumbLength),
                    thumbWidth / 2, thumbWidth / 2), scrollThumbBrush_.Get());
        }

        std::vector<std::shared_ptr<Label>> items_;
        int selectedIndex_;
        int hoveredIndex_;
        float scrollOffsetY_;
        float targetScrollOffsetY_;
        float maxScrollY_;
        bool showScrollBar_;
        bool isDraggingScroll_;
        float dragStartMouseY_;
        float dragStartScrollY_;
        float itemHeight_;
        float indicatorWidth_;
        float indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;
        float indicatorAnimSpeed_;
        float indicatorY_;
        float targetIndicatorY_;
        float scrollBarWidth_;
        float scrollBarMinLength_;
        float scrollWheelStep_;
        float scrollAnimationSpeed_;
        float hoverAnimationSpeed_;
        D2D1_COLOR_F backgroundColor_;
        D2D1_COLOR_F textColor_;
        D2D1_COLOR_F selectedColor_;
        D2D1_COLOR_F hoverColor_;
        D2D1_COLOR_F borderColor_;
        D2D1_COLOR_F scrollTrackColor_;
        D2D1_COLOR_F scrollThumbColor_;
        D2D1_COLOR_F scrollHoverThumbColor_;
        float scrollHoverProgress_;
        bool isHovered_;
        bool isScrollBarHovered_;
        bool buttonMode_;
        float buttonSpacing_;
        // 移除 textFormat_，改用 FontManager
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> selectedBrush_;
        ComPtr<ID2D1SolidColorBrush> hoverBrush_;
        ComPtr<ID2D1SolidColorBrush> indicatorBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollThumbBrush_;
    };

    // ==================== 表格视图 TableView ====================
    class TableView : public UIElement {
    public:
        enum class SelectionMode { Cell, Row, Column, None };

        inline static float DefaultHeaderHeight = 26.0f;
        inline static float DefaultRowHeight = 24.0f;
        inline static float DefaultMinColumnWidth = 40.0f;
        inline static D2D1_COLOR_F DefaultBackgroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderBackgroundColor = D2D1::ColorF(0.93f, 0.93f, 0.93f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultSelectedColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultGridLineColor = D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultScrollTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultScrollThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultScrollHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultIndicatorAnimSpeed = 12.0f;
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultScrollAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultWidth = 400.0f;
        inline static float DefaultHeight = 300.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;
        inline static float DefaultColumnResizeHitWidth = 8.0f;

        ZSignal<int, int> CellClicked;          // 单元格点击
        ZSignal<int, int> SelectionChanged;     // 选中变化（参数 row, col）

        TableView()
            : rowCount_(0), colCount_(0),
            selectedRow_(-1), selectedCol_(-1),
            hoveredRow_(-1), hoveredCol_(-1),
            scrollOffsetX_(0.0f), targetScrollOffsetX_(0.0f), maxScrollX_(0.0f),
            scrollOffsetY_(0.0f), targetScrollOffsetY_(0.0f), maxScrollY_(0.0f),
            showVerticalScrollBar_(false), showHorizontalScrollBar_(false),
            isDraggingVertical_(false), isDraggingHorizontal_(false),
            dragStartMouseX_(0.0f), dragStartMouseY_(0.0f),
            dragStartScrollX_(0.0f), dragStartScrollY_(0.0f),
            isResizingColumn_(false), resizeColumnIndex_(-1),
            resizeStartMouseX_(0.0f), resizeStartColumnWidth_(0.0f),
            headerHeight_(DefaultHeaderHeight),
            rowHeight_(DefaultRowHeight),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            indicatorAnimSpeed_(DefaultIndicatorAnimSpeed),
            indicatorY_(0.0f), targetIndicatorY_(0.0f),
            scrollBarWidth_(DefaultScrollBarWidth),
            scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep),
            scrollAnimationSpeed_(DefaultScrollAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            backgroundColor_(DefaultBackgroundColor),
            headerBackgroundColor_(DefaultHeaderBackgroundColor),
            textColor_(DefaultTextColor),
            headerTextColor_(DefaultHeaderTextColor),
            selectedColor_(DefaultSelectedColor),
            hoverColor_(DefaultHoverColor),
            gridLineColor_(DefaultGridLineColor),
            borderColor_(DefaultBorderColor),
            scrollTrackColor_(DefaultScrollTrackColor),
            scrollThumbColor_(DefaultScrollThumbColor),
            scrollHoverThumbColor_(DefaultScrollHoverThumbColor),
            selectionMode_(SelectionMode::Cell),
            verticalScrollHoverProgress_(0.0f),
            horizontalScrollHoverProgress_(0.0f),
            isVerticalHovered_(false),
            isHorizontalHovered_(false) {
            width_ = DefaultWidth;
            height_ = DefaultHeight;
        }

        // 数据模型
        void SetRowCount(int rows) {
            rows = max(0, rows);
            rowCount_ = rows;
            data_.resize(rows);
            for (auto& row : data_) row.resize(colCount_);
            if (selectedRow_ >= rows) selectedRow_ = -1;
            if (hoveredRow_ >= rows) hoveredRow_ = -1;
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            InvalidateLayout();
            RequestRepaint();
        }
        void SetColumnCount(int cols) {
            cols = max(0, cols);
            colCount_ = cols;
            headers_.resize(cols);
            columnWidths_.assign(cols, DefaultMinColumnWidth);
            for (auto& row : data_) row.resize(cols);
            if (selectedCol_ >= cols) selectedCol_ = -1;
            if (hoveredCol_ >= cols) hoveredCol_ = -1;
            UpdateScrollInfo();
            UpdateIndicatorTarget();
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int row, int col, std::shared_ptr<Label> label) {
            if (row < 0 || row >= rowCount_ || col < 0 || col >= colCount_ || !label) return;
            data_[row][col] = label;
            label->SetParent(this);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetItem(int row, int col, const std::wstring& text) {
            auto label = std::make_shared<Label>(text);
            label->SetTextColor(Color(textColor_.r, textColor_.g, textColor_.b, textColor_.a));
            SetItem(row, col, label);
        }
        std::shared_ptr<Label> GetItemLabel(int row, int col) const {
            if (row < 0 || row >= rowCount_ || col < 0 || col >= colCount_) return nullptr;
            if (row >= (int)data_.size() || col >= (int)data_[row].size()) return nullptr;
            return data_[row][col];
        }
        std::wstring GetItemText(int row, int col) const {
            auto label = GetItemLabel(row, col);
            return label ? label->GetText() : L"";
        }
        void SetHorizontalHeaderLabels(const std::vector<std::wstring>& labels) {
            headers_ = labels;
            if (headers_.size() != colCount_) headers_.resize(colCount_);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetColumnWidth(int col, float width) {
            if (col < 0 || col >= colCount_) return;
            columnWidths_[col] = max(DefaultMinColumnWidth, width);
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        float GetColumnWidth(int col) const {
            if (col < 0 || col >= colCount_) return 0;
            return columnWidths_[col];
        }
        void SetRowHeight(float height) { rowHeight_ = height; UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint(); }
        void SetHeaderHeight(float height) { headerHeight_ = height; InvalidateLayout(); RequestRepaint(); }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetIndicatorColor(Color color) { indicatorColor_ = color.ToD2D(); indicatorBrush_.Reset(); RequestRepaint(); }
        void SetIndicatorAnimationSpeed(float speed) { indicatorAnimSpeed_ = speed; }

        // 选择
        void SetSelectionMode(SelectionMode mode) { selectionMode_ = mode; InvalidateLayout(); RequestRepaint(); }
        SelectionMode GetSelectionMode() const { return selectionMode_; }
        void SetCurrentCell(int row, int col) {
            if (row < -1 || row >= rowCount_ || col < -1 || col >= colCount_) return;
            if (selectionMode_ == SelectionMode::None) return;
            if (selectionMode_ == SelectionMode::Row) col = 0;
            else if (selectionMode_ == SelectionMode::Column) row = 0;
            if (selectedRow_ != row || selectedCol_ != col) {
                selectedRow_ = row;
                selectedCol_ = col;
                SelectionChanged(row, col);
                EnsureVisible(row, col);
                UpdateIndicatorTarget();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        int GetCurrentRow() const { return selectedRow_; }
        int GetCurrentColumn() const { return selectedCol_; }

        // 样式
        void SetBackgroundColor(Color color) { backgroundColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetHeaderBackgroundColor(Color color) { headerBackgroundColor_ = color.ToD2D(); headerBgBrush_.Reset(); RequestRepaint(); }
        void SetTextColor(Color color) {
            textColor_ = color.ToD2D();
            for (auto& row : data_) for (auto& label : row) if (label) label->SetTextColor(color);
            textBrush_.Reset();
            RequestRepaint();
        }
        void SetHeaderTextColor(Color color) { headerTextColor_ = color.ToD2D(); headerTextBrush_.Reset(); RequestRepaint(); }
        void SetSelectedColor(Color color) { selectedColor_ = color.ToD2D(); selectedBrush_.Reset(); RequestRepaint(); }
        void SetHoverColor(Color color) { hoverColor_ = color.ToD2D(); hoverBrush_.Reset(); RequestRepaint(); }
        void SetGridLineColor(Color color) { gridLineColor_ = color.ToD2D(); gridLineBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            scrollTrackColor_ = track.ToD2D();
            scrollThumbColor_ = thumb.ToD2D();
            scrollHoverThumbColor_ = hoverThumb.ToD2D();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            RequestRepaint();
        }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetScrollAnimationSpeed(float speed) { scrollAnimationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = speed; }

        // 全局默认样式
        static void SetDefaultHeaderHeight(float height) { DefaultHeaderHeight = height; }
        static void SetDefaultRowHeight(float height) { DefaultRowHeight = height; }
        static void SetDefaultMinColumnWidth(float width) { DefaultMinColumnWidth = width; }
        static void SetDefaultColors(Color bg, Color headerBg, Color text, Color headerText, Color selected, Color hover, Color gridLine, Color border) {
            DefaultBackgroundColor = bg.ToD2D();
            DefaultHeaderBackgroundColor = headerBg.ToD2D();
            DefaultTextColor = text.ToD2D();
            DefaultHeaderTextColor = headerText.ToD2D();
            DefaultSelectedColor = selected.ToD2D();
            DefaultHoverColor = hover.ToD2D();
            DefaultGridLineColor = gridLine.ToD2D();
            DefaultBorderColor = border.ToD2D();
        }
        static void SetDefaultIndicatorColor(Color color) { DefaultIndicatorColor = color.ToD2D(); }
        static void SetDefaultIndicatorWidth(float width) { DefaultIndicatorWidth = width; }
        static void SetDefaultIndicatorHeightRatio(float ratio) { DefaultIndicatorHeightRatio = clamp(ratio, 0.1f, 1.0f); }
        static void SetDefaultIndicatorAnimationSpeed(float speed) { DefaultIndicatorAnimSpeed = speed; }
        static void SetDefaultScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            DefaultScrollTrackColor = track.ToD2D();
            DefaultScrollThumbColor = thumb.ToD2D();
            DefaultScrollHoverThumbColor = hoverThumb.ToD2D();
        }
        static void SetDefaultScrollBarWidth(float width) { DefaultScrollBarWidth = width; }
        static void SetDefaultScrollBarMinLength(float length) { DefaultScrollBarMinLength = length; }
        static void SetDefaultScrollWheelStep(float step) { DefaultScrollWheelStep = step; }
        static void SetDefaultScrollAnimationSpeed(float speed) { DefaultScrollAnimationSpeed = speed; }
        static void SetDefaultHoverAnimationSpeed(float speed) { DefaultHoverAnimationSpeed = speed; }
        static void SetDefaultSize(float width, float height) { DefaultWidth = width; DefaultHeight = height; }
        static void SetDefaultStretchWeights(float horizontal, float vertical) {
            DefaultHorizontalStretchWeight = horizontal;
            DefaultVerticalStretchWeight = vertical;
        }
        static void SetDefaultColumnResizeHitWidth(float width) { DefaultColumnResizeHitWidth = width; }

        // UIElement 接口
        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }
        bool IsFocusable() const override { return true; }

        Size Measure(const Size& availableSize) override { return Size(width_, height_); }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(backgroundColor_);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            float viewportWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);

            D2D1_RECT_F contentClipRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y + headerHeight_,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + viewportHeight);
            rt->PushAxisAlignedClip(contentClipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            int firstRow = (int)(scrollOffsetY_ / rowHeight_);
            int lastRow = (int)((scrollOffsetY_ + viewportHeight - headerHeight_) / rowHeight_);
            lastRow = min(lastRow, rowCount_ - 1);
            if (firstRow < 0) firstRow = 0;

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int col = 0; col < colCount_; ++col) {
                float colWidth = GetEffectiveColumnWidth(col);
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    for (int row = firstRow; row <= lastRow && row < rowCount_; ++row) {
                        float rowY = arrangedRect_.y + headerHeight_ + row * rowHeight_ - Snap(scrollOffsetY_);
                        D2D1_RECT_F cellRect = D2D1::RectF(colX, rowY, colX + colWidth, rowY + rowHeight_);

                        bool isSelected = false;
                        if (selectionMode_ == SelectionMode::Cell && row == selectedRow_ && col == selectedCol_)
                            isSelected = true;
                        else if (selectionMode_ == SelectionMode::Row && row == selectedRow_)
                            isSelected = true;
                        else if (selectionMode_ == SelectionMode::Column && col == selectedCol_)
                            isSelected = true;

                        bool isHovered = false;
                        if (selectionMode_ == SelectionMode::Cell)
                            isHovered = (row == hoveredRow_ && col == hoveredCol_);
                        else if (selectionMode_ == SelectionMode::Row)
                            isHovered = (row == hoveredRow_);
                        else if (selectionMode_ == SelectionMode::Column)
                            isHovered = (col == hoveredCol_);

                        if (isSelected) {
                            if (!selectedBrush_) rt->CreateSolidColorBrush(selectedColor_, selectedBrush_.GetAddressOf());
                            rt->FillRectangle(cellRect, selectedBrush_.Get());
                        }
                        else if (isHovered) {
                            if (!hoverBrush_) rt->CreateSolidColorBrush(hoverColor_, hoverBrush_.GetAddressOf());
                            rt->FillRectangle(cellRect, hoverBrush_.Get());
                        }

                        auto label = GetItemLabel(row, col);
                        if (label && !label->GetText().empty()) {
                            D2D1_RECT_F textRect = cellRect;
                            textRect.left += 4.0f;
                            textRect.right -= 4.0f;
                            if (col == 0) textRect.left += indicatorWidth_ + 4.0f;
                            DrawTextWithEllipsis(rt, label->GetText(), textRect, textColor_, spec, textBrush_, fmt);
                        }
                    }
                    if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                    else gridLineBrush_->SetColor(gridLineColor_);
                    if (gridLineBrush_) {
                        rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y + headerHeight_),
                            D2D1::Point2F(colX, arrangedRect_.y + viewportHeight), gridLineBrush_.Get(), 1.0f);
                    }
                }
                colX += colWidth;
            }

            if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
            else gridLineBrush_->SetColor(gridLineColor_);
            if (gridLineBrush_) {
                for (int row = firstRow; row <= lastRow && row < rowCount_; ++row) {
                    float lineY = arrangedRect_.y + headerHeight_ + (row + 1) * rowHeight_ - Snap(scrollOffsetY_);
                    rt->DrawLine(D2D1::Point2F(arrangedRect_.x, lineY),
                        D2D1::Point2F(arrangedRect_.x + viewportWidth, lineY), gridLineBrush_.Get(), 1.0f);
                }
            }

            if (selectedRow_ >= 0) {
                float indicatorTop = arrangedRect_.y + headerHeight_ + indicatorY_ - Snap(scrollOffsetY_);
                float indicatorHeight = rowHeight_ * indicatorHeightRatio_;
                float indicatorOffset = (rowHeight_ - indicatorHeight) / 2.0f;
                float drawTop = indicatorTop + indicatorOffset;
                float drawBottom = drawTop + indicatorHeight;

                if (drawBottom > arrangedRect_.y + headerHeight_ && drawTop < arrangedRect_.y + viewportHeight) {
                    if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, indicatorBrush_.GetAddressOf());
                    else indicatorBrush_->SetColor(indicatorColor_);
                    D2D1_RECT_F indicatorRect = D2D1::RectF(arrangedRect_.x, drawTop,
                        arrangedRect_.x + indicatorWidth_, drawBottom);
                    rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, indicatorWidth_ / 2, indicatorWidth_ / 2), indicatorBrush_.Get());
                }
            }

            rt->PopAxisAlignedClip();

            DrawHeader(rt, viewportWidth);

            if (showVerticalScrollBar_) DrawVerticalScrollBar(rt, viewportHeight);
            if (showHorizontalScrollBar_) DrawHorizontalScrollBar(rt, viewportWidth);

            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) {
                if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) return this;
                if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) return this;
                return this;
            }
            return nullptr;
        }

        void OnMouseEnter() override { RequestRepaint(); if (MouseEnterHandler) MouseEnterHandler(); }
        void OnMouseLeave() override {
            hoveredRow_ = -1;
            hoveredCol_ = -1;
            isVerticalHovered_ = false;
            isHorizontalHovered_ = false;
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            RequestRepaint();
            if (MouseLeaveHandler) MouseLeaveHandler();
        }
        void OnMouseMove(float x, float y) override {
            if (isResizingColumn_) {
                float dx = x - resizeStartMouseX_;
                float newWidth = max(DefaultMinColumnWidth, resizeStartColumnWidth_ + dx);
                SetColumnWidth(resizeColumnIndex_, newWidth);
                return;
            }
            if (isDraggingVertical_) { HandleVerticalScrollDrag(y); return; }
            if (isDraggingHorizontal_) { HandleHorizontalScrollDrag(x); return; }
            if (!arrangedRect_.Contains(x, y)) return;

            isVerticalHovered_ = (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_);
            isHorizontalHovered_ = (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_);

            if (y <= arrangedRect_.y + headerHeight_) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                bool nearBoundary = false;
                for (int col = 0; col < colCount_ - 1; ++col) {
                    float colWidth = GetEffectiveColumnWidth(col);
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        nearBoundary = true;
                        break;
                    }
                    colX += colWidth;
                }
                if (nearBoundary) SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                else SetCursor(LoadCursor(nullptr, IDC_ARROW));
                hoveredRow_ = -1;
                hoveredCol_ = -1;
                RequestRepaint();
                return;
            }
            else {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }

            float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
            float relY = y - arrangedRect_.y - headerHeight_ + Snap(scrollOffsetY_);
            int row = (int)(relY / rowHeight_);
            int col = GetColumnIndexAtX(relX);
            if (row >= 0 && row < rowCount_ && col >= 0 && col < colCount_) {
                hoveredRow_ = row;
                hoveredCol_ = col;
            }
            else {
                hoveredRow_ = -1;
                hoveredCol_ = -1;
            }
            RequestRepaint();
            if (MouseMoveHandler) MouseMoveHandler(x, y);
        }
        void OnMouseDown(float x, float y) override {
            if (!arrangedRect_.Contains(x, y)) return;

            if (y <= arrangedRect_.y + headerHeight_) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                for (int col = 0; col < colCount_ - 1; ++col) {
                    float colWidth = GetEffectiveColumnWidth(col);
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        isResizingColumn_ = true;
                        resizeColumnIndex_ = col;
                        resizeStartMouseX_ = x;
                        resizeStartColumnWidth_ = colWidth;
                        return;
                    }
                    colX += colWidth;
                }
                return;
            }

            if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) {
                float trackY = arrangedRect_.y;
                float trackHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                float thumbY = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
                D2D1_RECT_F thumbRect = D2D1::RectF(arrangedRect_.x + arrangedRect_.width - scrollBarWidth_, thumbY,
                    arrangedRect_.x + arrangedRect_.width, thumbY + thumbLength);
                if (y >= thumbRect.top && y <= thumbRect.bottom) {
                    isDraggingVertical_ = true;
                    dragStartMouseY_ = y;
                    dragStartScrollY_ = scrollOffsetY_;
                    return;
                }
                else {
                    float ratio = (y - trackY - thumbLength / 2) / (trackHeight - thumbLength);
                    targetScrollOffsetY_ = clamp(ratio * maxScrollY_, 0.0f, maxScrollY_);
                    RequestRepaint();
                    return;
                }
            }

            if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) {
                float trackX = arrangedRect_.x;
                float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
                float thumbX = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
                D2D1_RECT_F thumbRect = D2D1::RectF(thumbX, arrangedRect_.y + arrangedRect_.height - scrollBarWidth_,
                    thumbX + thumbLength, arrangedRect_.y + arrangedRect_.height);
                if (x >= thumbRect.left && x <= thumbRect.right) {
                    isDraggingHorizontal_ = true;
                    dragStartMouseX_ = x;
                    dragStartScrollX_ = scrollOffsetX_;
                    return;
                }
                else {
                    float ratio = (x - trackX - thumbLength / 2) / (trackWidth - thumbLength);
                    targetScrollOffsetX_ = clamp(ratio * maxScrollX_, 0.0f, maxScrollX_);
                    RequestRepaint();
                    return;
                }
            }

            float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
            float relY = y - arrangedRect_.y - headerHeight_ + Snap(scrollOffsetY_);
            int row = (int)(relY / rowHeight_);
            int col = GetColumnIndexAtX(relX);
            if (row >= 0 && row < rowCount_ && col >= 0 && col < colCount_) {
                SetCurrentCell(row, col);
                CellClicked(row, col);
            }
            if (MouseDownHandler) MouseDownHandler(x, y);
        }
        void OnMouseUp(float x, float y) override {
            if (isResizingColumn_) { isResizingColumn_ = false; resizeColumnIndex_ = -1; RequestRepaint(); return; }
            if (isDraggingVertical_) { isDraggingVertical_ = false; RequestRepaint(); return; }
            if (isDraggingHorizontal_) { isDraggingHorizontal_ = false; RequestRepaint(); return; }
            if (MouseUpHandler) MouseUpHandler(x, y);
        }
        bool OnMouseWheel(float deltaX, float deltaY) override {
            bool handled = false;
            if (deltaY != 0 && maxScrollY_ > 0) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                handled = true;
                RequestRepaint();
            }
            if (deltaX != 0 && maxScrollX_ > 0) {
                targetScrollOffsetX_ = clamp(targetScrollOffsetX_ - deltaX * scrollWheelStep_, 0.0f, maxScrollX_);
                handled = true;
                RequestRepaint();
            }
            return handled;
        }
        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsFocusable() || rowCount_ == 0 || colCount_ == 0) return;
            int row = selectedRow_;
            int col = selectedCol_;
            if (selectionMode_ == SelectionMode::Row) col = 0;
            else if (selectionMode_ == SelectionMode::Column) row = 0;
            switch (key) {
            case VK_UP: row = max(0, row - 1); break;
            case VK_DOWN: row = min(rowCount_ - 1, row + 1); break;
            case VK_LEFT: col = max(0, col - 1); break;
            case VK_RIGHT: col = min(colCount_ - 1, col + 1); break;
            case VK_HOME: row = 0; col = 0; break;
            case VK_END: row = rowCount_ - 1; col = colCount_ - 1; break;
            default: return;
            }
            SetCurrentCell(row, col);
            if (KeyDownHandler) KeyDownHandler(key, lParam);
        }
        void OnFocus() override { RequestRepaint(); if (FocusHandler) FocusHandler(); }
        void OnBlur() override { RequestRepaint(); if (BlurHandler) BlurHandler(); }

        void UpdateAnimation(float deltaTime) override {
            if (fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f) {
                scrollOffsetX_ += (targetScrollOffsetX_ - scrollOffsetX_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetX_ - scrollOffsetX_) <= 0.1f) scrollOffsetX_ = targetScrollOffsetX_;
                RequestRepaint();
            }
            else scrollOffsetX_ = targetScrollOffsetX_;

            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                RequestRepaint();
            }
            else scrollOffsetY_ = targetScrollOffsetY_;

            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorAnimSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorY_ - targetIndicatorY_) > 0.01f) RequestRepaint();

            if (isVerticalHovered_ || isDraggingVertical_) {
                if (verticalScrollHoverProgress_ < 1.0f) {
                    verticalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ > 1.0f) verticalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (verticalScrollHoverProgress_ > 0.0f) {
                    verticalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ < 0.0f) verticalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            if (isHorizontalHovered_ || isDraggingHorizontal_) {
                if (horizontalScrollHoverProgress_ < 1.0f) {
                    horizontalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ > 1.0f) horizontalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (horizontalScrollHoverProgress_ > 0.0f) {
                    horizontalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ < 0.0f) horizontalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.1f);
            ConvergeValue(scrollOffsetX_, targetScrollOffsetX_, 0.1f);
            ConvergeValue(scrollOffsetY_, targetScrollOffsetY_, 0.1f);
        }

        bool HasActiveAnimation() const override {
            return fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f ||
                fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f ||
                fabs(indicatorY_ - targetIndicatorY_) > 0.01f ||
                (isVerticalHovered_ && verticalScrollHoverProgress_ < 1.0f) ||
                (!isVerticalHovered_ && verticalScrollHoverProgress_ > 0.0f) ||
                (isHorizontalHovered_ && horizontalScrollHoverProgress_ < 1.0f) ||
                (!isHorizontalHovered_ && horizontalScrollHoverProgress_ > 0.0f);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            headerBgBrush_.Reset();
            textBrush_.Reset();
            headerTextBrush_.Reset();
            selectedBrush_.Reset();
            hoverBrush_.Reset();
            indicatorBrush_.Reset();
            gridLineBrush_.Reset();
            borderBrush_.Reset();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            for (auto& row : data_) for (auto& label : row) if (label) label->ReleaseDeviceResources();
            UIElement::ReleaseDeviceResources();
        }

    private:
        float GetEffectiveColumnWidth(int col) const {
            if (col < 0 || col >= (int)columnWidths_.size()) return DefaultMinColumnWidth;
            return columnWidths_[col];
        }

        int GetColumnIndexAtX(float relX) const {
            float x = 0;
            for (int i = 0; i < colCount_; ++i) {
                float w = GetEffectiveColumnWidth(i);
                if (relX >= x && relX < x + w) return i;
                x += w;
            }
            return -1;
        }

        void UpdateScrollInfo() {
            float totalContentWidth = 0;
            for (int i = 0; i < colCount_; ++i) totalContentWidth += GetEffectiveColumnWidth(i);
            float totalContentHeight = headerHeight_ + rowCount_ * rowHeight_;

            float availWidth = arrangedRect_.width;
            float availHeight = arrangedRect_.height;

            showVerticalScrollBar_ = totalContentHeight > availHeight;
            showHorizontalScrollBar_ = totalContentWidth > availWidth;

            float viewportWidth = availWidth - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = availHeight - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);

            if (!showHorizontalScrollBar_ && totalContentWidth > viewportWidth) {
                showHorizontalScrollBar_ = true;
                viewportHeight = availHeight - scrollBarWidth_;
            }
            if (!showVerticalScrollBar_ && totalContentHeight > viewportHeight) {
                showVerticalScrollBar_ = true;
                viewportWidth = availWidth - scrollBarWidth_;
            }

            maxScrollX_ = max(0.0f, totalContentWidth - viewportWidth);
            maxScrollY_ = max(0.0f, totalContentHeight - viewportHeight);

            scrollOffsetX_ = clamp(scrollOffsetX_, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void EnsureVisible(int row, int col) {
            if (row < 0 || col < 0) return;
            float rowTop = headerHeight_ + row * rowHeight_;
            float rowBottom = rowTop + rowHeight_;
            if (rowTop < scrollOffsetY_) {
                targetScrollOffsetY_ = rowTop;
            }
            else if (rowBottom > scrollOffsetY_ + arrangedRect_.height) {
                targetScrollOffsetY_ = rowBottom - arrangedRect_.height;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);

            float colX = 0;
            for (int i = 0; i < col; ++i) colX += GetEffectiveColumnWidth(i);
            float colWidth = GetEffectiveColumnWidth(col);
            if (colX < scrollOffsetX_) {
                targetScrollOffsetX_ = colX;
            }
            else if (colX + colWidth > scrollOffsetX_ + arrangedRect_.width) {
                targetScrollOffsetX_ = colX + colWidth - arrangedRect_.width;
            }
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
        }

        void HandleVerticalScrollDrag(float mouseY) {
            float trackY = arrangedRect_.y;
            float trackHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            if (trackHeight > thumbLength) {
                float ratio = (mouseY - dragStartMouseY_) / (trackHeight - thumbLength);
                targetScrollOffsetY_ = clamp(dragStartScrollY_ + ratio * maxScrollY_, 0.0f, maxScrollY_);
                RequestRepaint();
            }
        }

        void HandleHorizontalScrollDrag(float mouseX) {
            float trackX = arrangedRect_.x;
            float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            if (trackWidth > thumbLength) {
                float ratio = (mouseX - dragStartMouseX_) / (trackWidth - thumbLength);
                targetScrollOffsetX_ = clamp(dragStartScrollX_ + ratio * maxScrollX_, 0.0f, maxScrollX_);
                RequestRepaint();
            }
        }

        void DrawHeader(ID2D1RenderTarget* rt, float viewportWidth) {
            if (!headerBgBrush_) rt->CreateSolidColorBrush(headerBackgroundColor_, headerBgBrush_.GetAddressOf());
            else headerBgBrush_->SetColor(headerBackgroundColor_);
            if (headerBgBrush_) {
                D2D1_RECT_F headerRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                    arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_);
                rt->FillRectangle(headerRect, headerBgBrush_.Get());
            }

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int col = 0; col < colCount_; ++col) {
                float colWidth = GetEffectiveColumnWidth(col);
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    std::wstring headerText = (col < (int)headers_.size()) ? headers_[col] : L"";
                    if (!headerText.empty()) {
                        D2D1_RECT_F textRect = D2D1::RectF(colX + 4, arrangedRect_.y,
                            colX + colWidth - 4, arrangedRect_.y + headerHeight_);
                        DrawTextWithEllipsis(rt, headerText, textRect, headerTextColor_, spec, headerTextBrush_, fmt);
                    }
                    if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                    else gridLineBrush_->SetColor(gridLineColor_);
                    if (gridLineBrush_) {
                        rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y),
                            D2D1::Point2F(colX, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
                    }
                }
                colX += colWidth;
            }
            if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
            else gridLineBrush_->SetColor(gridLineColor_);
            if (gridLineBrush_) {
                rt->DrawLine(D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y),
                    D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_),
                    gridLineBrush_.Get(), 1.0f);
                rt->DrawLine(D2D1::Point2F(arrangedRect_.x, arrangedRect_.y + headerHeight_),
                    D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_),
                    gridLineBrush_.Get(), 1.0f);
            }
        }

        void DrawVerticalScrollBar(ID2D1RenderTarget* rt, float viewportHeight) {
            float baseTrackWidth = scrollBarWidth_;
            float trackWidth = baseTrackWidth * (1.0f + 0.25f * verticalScrollHoverProgress_);
            float trackX = arrangedRect_.x + arrangedRect_.width - trackWidth;
            float trackY = arrangedRect_.y;
            float trackHeight = viewportHeight;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            if (scrollTrackBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight),
                    trackWidth / 2, trackWidth / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            float thumbPos = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
            float thumbWidth = trackWidth - 2.0f;
            if (thumbWidth < 2.0f) thumbWidth = 2.0f;
            float thumbX = trackX + (trackWidth - thumbWidth) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (verticalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * verticalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * verticalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * verticalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * verticalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            if (scrollThumbBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbX, thumbPos, thumbX + thumbWidth, thumbPos + thumbLength),
                    thumbWidth / 2, thumbWidth / 2), scrollThumbBrush_.Get());
        }

        void DrawHorizontalScrollBar(ID2D1RenderTarget* rt, float viewportWidth) {
            float baseTrackHeight = scrollBarWidth_;
            float trackHeight = baseTrackHeight * (1.0f + 0.25f * horizontalScrollHoverProgress_);
            float trackY = arrangedRect_.y + arrangedRect_.height - trackHeight;
            float trackX = arrangedRect_.x;
            float trackWidth = viewportWidth;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            if (scrollTrackBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + trackHeight),
                    trackHeight / 2, trackHeight / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            float thumbPos = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
            float thumbHeight = trackHeight - 2.0f;
            if (thumbHeight < 2.0f) thumbHeight = 2.0f;
            float thumbY = trackY + (trackHeight - thumbHeight) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (horizontalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * horizontalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            if (scrollThumbBrush_)
                rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbPos, thumbY, thumbPos + thumbLength, thumbY + thumbHeight),
                    thumbHeight / 2, thumbHeight / 2), scrollThumbBrush_.Get());
        }

        void UpdateIndicatorTarget() {
            if (selectedRow_ >= 0 && selectedRow_ < rowCount_) {
                targetIndicatorY_ = selectedRow_ * rowHeight_;
            }
            else {
                targetIndicatorY_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
        }

        int rowCount_, colCount_;
        std::vector<std::vector<std::shared_ptr<Label>>> data_;
        std::vector<std::wstring> headers_;
        std::vector<float> columnWidths_;
        int selectedRow_, selectedCol_;
        int hoveredRow_, hoveredCol_;
        float scrollOffsetX_, targetScrollOffsetX_, maxScrollX_;
        float scrollOffsetY_, targetScrollOffsetY_, maxScrollY_;
        bool showVerticalScrollBar_, showHorizontalScrollBar_;
        bool isDraggingVertical_, isDraggingHorizontal_;
        float dragStartMouseX_, dragStartMouseY_;
        float dragStartScrollX_, dragStartScrollY_;
        bool isResizingColumn_;
        int resizeColumnIndex_;
        float resizeStartMouseX_, resizeStartColumnWidth_;
        float headerHeight_, rowHeight_;
        float indicatorWidth_;
        float indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;
        float indicatorAnimSpeed_;
        float indicatorY_, targetIndicatorY_;
        float scrollBarWidth_, scrollBarMinLength_;
        float scrollWheelStep_, scrollAnimationSpeed_, hoverAnimationSpeed_;
        D2D1_COLOR_F backgroundColor_, headerBackgroundColor_;
        D2D1_COLOR_F textColor_, headerTextColor_;
        D2D1_COLOR_F selectedColor_, hoverColor_, gridLineColor_, borderColor_;
        D2D1_COLOR_F scrollTrackColor_, scrollThumbColor_, scrollHoverThumbColor_;
        SelectionMode selectionMode_;
        float verticalScrollHoverProgress_, horizontalScrollHoverProgress_;
        bool isVerticalHovered_, isHorizontalHovered_;
        // 移除 textFormat_，改用 FontManager
        ComPtr<ID2D1SolidColorBrush> bgBrush_;
        ComPtr<ID2D1SolidColorBrush> headerBgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_;
        ComPtr<ID2D1SolidColorBrush> headerTextBrush_;
        ComPtr<ID2D1SolidColorBrush> selectedBrush_;
        ComPtr<ID2D1SolidColorBrush> hoverBrush_;
        ComPtr<ID2D1SolidColorBrush> indicatorBrush_;
        ComPtr<ID2D1SolidColorBrush> gridLineBrush_;
        ComPtr<ID2D1SolidColorBrush> borderBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollThumbBrush_;
    };

    // ==================== 树视图 TreeView（多列表格形式） ====================
    struct TreeNode {
        std::vector<std::wstring> columns;
        TreeNode* parent = nullptr;
        std::vector<std::shared_ptr<TreeNode>> children;
        bool expanded = false;
        int depth = 0;
        void* userData = nullptr;

        TreeNode(const std::wstring& text) : columns(1, text) {}
        TreeNode(const std::vector<std::wstring>& cols) : columns(cols) {}
    };

    class TreeView : public UIElement {
    public:
        // 默认样式
        inline static float DefaultRowHeight = 24.0f;
        inline static float DefaultIndent = 16.0f;
        inline static float DefaultHeaderHeight = 24.0f;
        inline static float DefaultMinColumnWidth = 40.0f;
        inline static D2D1_COLOR_F DefaultBackgroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderBackgroundColor = D2D1::ColorF(0.93f, 0.93f, 0.93f, 1.0f);
        inline static D2D1_COLOR_F DefaultTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHeaderTextColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultSelectedColor = D2D1::ColorF(0.7f, 0.85f, 1.0f, 1.0f);
        inline static D2D1_COLOR_F DefaultHoverColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
        inline static D2D1_COLOR_F DefaultIndicatorColor = D2D1::ColorF(0.0f, 0.47f, 0.84f, 1.0f);
        inline static D2D1_COLOR_F DefaultBorderColor = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
        inline static D2D1_COLOR_F DefaultGridLineColor = D2D1::ColorF(0.8f, 0.8f, 0.8f, 1.0f);
        inline static D2D1_COLOR_F DefaultScrollTrackColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 0.8f);
        inline static D2D1_COLOR_F DefaultScrollThumbColor = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.9f);
        inline static D2D1_COLOR_F DefaultScrollHoverThumbColor = D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.0f);
        inline static float DefaultIndicatorWidth = 3.0f;
        inline static float DefaultIndicatorHeightRatio = 0.6f;
        inline static float DefaultIndicatorAnimSpeed = 12.0f;
        inline static float DefaultScrollBarWidth = 8.0f;
        inline static float DefaultScrollBarMinLength = 20.0f;
        inline static float DefaultScrollWheelStep = 30.0f;
        inline static float DefaultScrollAnimationSpeed = 10.0f;
        inline static float DefaultHoverAnimationSpeed = 10.0f;
        inline static float DefaultWidth = 400.0f;
        inline static float DefaultHeight = 300.0f;
        inline static float DefaultHorizontalStretchWeight = 1.0f;
        inline static float DefaultVerticalStretchWeight = 1.0f;
        inline static float DefaultColumnResizeHitWidth = 8.0f;

        ZSignal<std::shared_ptr<TreeNode>> SelectionChanged;   // 选中节点变化
        ZSignal<std::shared_ptr<TreeNode>> NodeClicked;        // 节点点击
        ZSignal<std::shared_ptr<TreeNode>, bool> ExpandChanged; // 展开/折叠变化

        TreeView()
            : scrollOffsetX_(0.0f), targetScrollOffsetX_(0.0f), maxScrollX_(0.0f),
            scrollOffsetY_(0.0f), targetScrollOffsetY_(0.0f), maxScrollY_(0.0f),
            showVerticalScrollBar_(false), showHorizontalScrollBar_(false),
            isDraggingVertical_(false), isDraggingHorizontal_(false),
            dragStartMouseX_(0.0f), dragStartMouseY_(0.0f),
            dragStartScrollX_(0.0f), dragStartScrollY_(0.0f),
            isResizingColumn_(false), resizeColumnIndex_(-1),
            resizeStartMouseX_(0.0f), resizeStartColumnWidth_(0.0f),
            selectedNode_(nullptr), hoveredNode_(nullptr),
            indicatorY_(0.0f), targetIndicatorY_(0.0f),
            indicatorX_(0.0f), targetIndicatorX_(0.0f),
            rowHeight_(DefaultRowHeight),
            indent_(DefaultIndent),
            headerHeight_(DefaultHeaderHeight),
            headerVisible_(true),
            columnCount_(1),
            indicatorWidth_(DefaultIndicatorWidth),
            indicatorHeightRatio_(DefaultIndicatorHeightRatio),
            indicatorColor_(DefaultIndicatorColor),
            indicatorAnimSpeed_(DefaultIndicatorAnimSpeed),
            scrollBarWidth_(DefaultScrollBarWidth),
            scrollBarMinLength_(DefaultScrollBarMinLength),
            scrollWheelStep_(DefaultScrollWheelStep),
            scrollAnimationSpeed_(DefaultScrollAnimationSpeed),
            hoverAnimationSpeed_(DefaultHoverAnimationSpeed),
            backgroundColor_(DefaultBackgroundColor),
            headerBackgroundColor_(DefaultHeaderBackgroundColor),
            textColor_(DefaultTextColor),
            headerTextColor_(DefaultHeaderTextColor),
            selectedColor_(DefaultSelectedColor),
            hoverColor_(DefaultHoverColor),
            gridLineColor_(DefaultGridLineColor),
            borderColor_(DefaultBorderColor),
            scrollTrackColor_(DefaultScrollTrackColor),
            scrollThumbColor_(DefaultScrollThumbColor),
            scrollHoverThumbColor_(DefaultScrollHoverThumbColor),
            verticalScrollHoverProgress_(0.0f),
            horizontalScrollHoverProgress_(0.0f),
            isVerticalHovered_(false),
            isHorizontalHovered_(false) {
            width_ = DefaultWidth;
            height_ = DefaultHeight;
            columnWidths_.push_back(DefaultMinColumnWidth);
            headerLabels_.push_back(L"名称");

            indicatorY_ = -1.0f;
            indicatorX_ = -1.0f;
        }

        // ---------- 列设置 ----------
        void SetColumnCount(int count) {
            count = max(1, count);
            columnCount_ = count;
            columnWidths_.assign(count, DefaultMinColumnWidth);
            headerLabels_.resize(count);
            std::function<void(std::shared_ptr<TreeNode>)> updateNode = [&](std::shared_ptr<TreeNode> node) {
                if (!node) return;
                node->columns.resize(count);
                for (auto& child : node->children) updateNode(child);
                };
            for (auto& root : roots_) updateNode(root);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetHeaderLabels(const std::vector<std::wstring>& labels) {
            headerLabels_ = labels;
            if (headerLabels_.size() != columnCount_) headerLabels_.resize(columnCount_);
            InvalidateLayout();
            RequestRepaint();
        }
        void SetColumnWidth(int col, float width) {
            if (col < 0 || col >= columnCount_) return;
            columnWidths_[col] = max(DefaultMinColumnWidth, width);
            UpdateScrollInfo();
            InvalidateLayout();
            RequestRepaint();
        }
        float GetColumnWidth(int col) const {
            if (col < 0 || col >= (int)columnWidths_.size()) return DefaultMinColumnWidth;
            return columnWidths_[col];
        }

        // ---------- 节点操作 ----------
        std::shared_ptr<TreeNode> AddRoot(const std::wstring& text) {
            auto node = std::make_shared<TreeNode>(text);
            node->columns.resize(columnCount_);
            node->depth = 0;
            roots_.push_back(node);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return node;
        }
        std::shared_ptr<TreeNode> AddRoot(const std::vector<std::wstring>& columns) {
            auto node = std::make_shared<TreeNode>(columns);
            node->columns.resize(columnCount_);
            node->depth = 0;
            roots_.push_back(node);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return node;
        }
        std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode> parent, const std::wstring& text) {
            if (!parent) return nullptr;
            auto child = std::make_shared<TreeNode>(text);
            child->columns.resize(columnCount_);
            child->parent = parent.get();
            child->depth = parent->depth + 1;
            parent->children.push_back(child);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return child;
        }
        std::shared_ptr<TreeNode> AddChild(std::shared_ptr<TreeNode> parent, const std::vector<std::wstring>& columns) {
            if (!parent) return nullptr;
            auto child = std::make_shared<TreeNode>(columns);
            child->columns.resize(columnCount_);
            child->parent = parent.get();
            child->depth = parent->depth + 1;
            parent->children.push_back(child);
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
            return child;
        }

        void RemoveNode(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            if (node->parent) {
                auto& siblings = node->parent->children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), node), siblings.end());
            }
            else {
                roots_.erase(std::remove(roots_.begin(), roots_.end(), node), roots_.end());
            }
            if (selectedNode_ == node) selectedNode_ = nullptr;
            if (hoveredNode_ == node) hoveredNode_ = nullptr;
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }

        void Clear() {
            roots_.clear();
            visibleNodes_.clear();
            selectedNode_ = nullptr;
            hoveredNode_ = nullptr;
            scrollOffsetX_ = 0.0f;
            scrollOffsetY_ = 0.0f;
            targetScrollOffsetX_ = 0.0f;
            targetScrollOffsetY_ = 0.0f;
            maxScrollX_ = 0.0f;
            maxScrollY_ = 0.0f;
            InvalidateLayout();
            RequestRepaint();
        }

        // ---------- 展开/折叠 ----------
        void ExpandNode(std::shared_ptr<TreeNode> node, bool expand) {
            if (!node) return;
            if (node->expanded != expand) {
                node->expanded = expand;
                BuildVisibleList();
                ExpandChanged(node, expand);
                InvalidateLayout();
                RequestRepaint();
            }
        }
        void ToggleNode(std::shared_ptr<TreeNode> node) {
            if (!node) return;
            ExpandNode(node, !node->expanded);
        }
        void ExpandNodeRecursive(std::shared_ptr<TreeNode> node, bool expand) {
            if (!node) return;
            if (node->expanded != expand) node->expanded = expand;
            for (auto& child : node->children) {
                ExpandNodeRecursive(child, expand);
            }
            BuildVisibleList();
            InvalidateLayout();
            RequestRepaint();
        }
        bool IsExpanded(std::shared_ptr<TreeNode> node) const {
            return node ? node->expanded : false;
        }

        // ---------- 选择 ----------
        void SetSelectedNode(std::shared_ptr<TreeNode> node) {
            if (selectedNode_ != node) {
                selectedNode_ = node;
                SelectionChanged(selectedNode_);
                EnsureVisible(node);
                UpdateIndicatorTarget();
                InvalidateLayout();
                RequestRepaint();
            }
        }
        std::shared_ptr<TreeNode> GetSelectedNode() const { return selectedNode_; }

        void ScrollToNode(std::shared_ptr<TreeNode> node) {
            int idx = GetVisibleIndex(node);
            if (idx < 0) return;
            float itemTop = idx * rowHeight_;
            float itemBottom = itemTop + rowHeight_;
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float viewportHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            if (itemTop < scrollOffsetY_) {
                targetScrollOffsetY_ = itemTop;
            }
            else if (itemBottom > scrollOffsetY_ + viewportHeight) {
                targetScrollOffsetY_ = itemBottom - viewportHeight;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
            RequestRepaint();
        }

        // ---------- 表头设置 ----------
        void SetHeaderVisible(bool visible) { headerVisible_ = visible; InvalidateLayout(); RequestRepaint(); }

        // ---------- 样式设置 ----------
        void SetRowHeight(float height) { rowHeight_ = height; UpdateScrollInfo(); UpdateIndicatorTarget(); InvalidateLayout(); RequestRepaint(); }
        void SetIndent(float indent) { indent_ = indent; InvalidateLayout(); RequestRepaint(); }
        void SetIndicatorWidth(float width) { indicatorWidth_ = width; RequestRepaint(); }
        void SetIndicatorHeightRatio(float ratio) { indicatorHeightRatio_ = clamp(ratio, 0.1f, 1.0f); RequestRepaint(); }
        void SetIndicatorColor(Color color) { indicatorColor_ = color.ToD2D(); indicatorBrush_.Reset(); RequestRepaint(); }
        void SetIndicatorAnimationSpeed(float speed) { indicatorAnimSpeed_ = speed; }
        void SetBackgroundColor(Color color) { backgroundColor_ = color.ToD2D(); bgBrush_.Reset(); RequestRepaint(); }
        void SetHeaderBackgroundColor(Color color) { headerBackgroundColor_ = color.ToD2D(); headerBgBrush_.Reset(); RequestRepaint(); }
        void SetTextColor(Color color) { textColor_ = color.ToD2D(); textBrush_.Reset(); RequestRepaint(); }
        void SetHeaderTextColor(Color color) { headerTextColor_ = color.ToD2D(); headerTextBrush_.Reset(); RequestRepaint(); }
        void SetSelectedColor(Color color) { selectedColor_ = color.ToD2D(); selectedBrush_.Reset(); RequestRepaint(); }
        void SetHoverColor(Color color) { hoverColor_ = color.ToD2D(); hoverBrush_.Reset(); RequestRepaint(); }
        void SetBorderColor(Color color) { borderColor_ = color.ToD2D(); borderBrush_.Reset(); RequestRepaint(); }
        void SetGridLineColor(Color color) { gridLineColor_ = color.ToD2D(); gridLineBrush_.Reset(); RequestRepaint(); }
        void SetScrollBarColors(Color track, Color thumb, Color hoverThumb) {
            scrollTrackColor_ = track.ToD2D();
            scrollThumbColor_ = thumb.ToD2D();
            scrollHoverThumbColor_ = hoverThumb.ToD2D();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            RequestRepaint();
        }
        void SetScrollBarWidth(float width) { scrollBarWidth_ = width; InvalidateLayout(); RequestRepaint(); }
        void SetScrollWheelStep(float step) { scrollWheelStep_ = step; }
        void SetScrollAnimationSpeed(float speed) { scrollAnimationSpeed_ = speed; }
        void SetHoverAnimationSpeed(float speed) { hoverAnimationSpeed_ = speed; }

        // ---------- UIElement 接口 ----------
        float GetDefaultHorizontalStretchWeight() const override { return DefaultHorizontalStretchWeight; }
        float GetDefaultVerticalStretchWeight() const override { return DefaultVerticalStretchWeight; }
        bool IsFocusable() const override { return true; }

        Size Measure(const Size& availableSize) override {
            return Size(width_, height_);
        }

        void Arrange(const Rect& finalRect) override {
            UIElement::Arrange(finalRect);
            UpdateScrollInfo();
            UpdateIndicatorTarget();
        }

        void Draw(ID2D1RenderTarget* rt) override {
            if (!visible_) return;

            if (!bgBrush_) rt->CreateSolidColorBrush(backgroundColor_, bgBrush_.GetAddressOf());
            else bgBrush_->SetColor(backgroundColor_);
            if (bgBrush_) rt->FillRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), bgBrush_.Get());

            float viewportWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float viewportHeight = arrangedRect_.height - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float headerOffset = headerVisible_ ? headerHeight_ : 0;

            if (headerVisible_) {
                DrawHeader(rt, viewportWidth);
            }

            D2D1_RECT_F clipRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y + headerOffset,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + viewportHeight);
            rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            int firstVisible = (int)(scrollOffsetY_ / rowHeight_);
            int lastVisible = (int)((scrollOffsetY_ + viewportHeight - headerOffset) / rowHeight_);
            lastVisible = min(lastVisible, (int)visibleNodes_.size() - 1);
            if (firstVisible < 0) firstVisible = 0;

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            std::vector<float> colXPositions(columnCount_);
            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int c = 0; c < columnCount_; ++c) {
                colXPositions[c] = colX;
                float colWidth = GetEffectiveColumnWidth(c);
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                    else gridLineBrush_->SetColor(gridLineColor_);
                    rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y + headerOffset),
                        D2D1::Point2F(colX, arrangedRect_.y + viewportHeight), gridLineBrush_.Get(), 1.0f);
                }
                colX += colWidth;
            }

            for (int i = firstVisible; i <= lastVisible && i < (int)visibleNodes_.size(); ++i) {
                auto node = visibleNodes_[i];
                if (!node) continue;

                float itemY = arrangedRect_.y + headerOffset + i * rowHeight_ - Snap(scrollOffsetY_);
                D2D1_RECT_F rowRect = D2D1::RectF(arrangedRect_.x, itemY,
                    arrangedRect_.x + viewportWidth, itemY + rowHeight_);

                if (node == selectedNode_) {
                    if (!selectedBrush_) rt->CreateSolidColorBrush(selectedColor_, selectedBrush_.GetAddressOf());
                    rt->FillRectangle(rowRect, selectedBrush_.Get());
                }
                else if (node == hoveredNode_) {
                    if (!hoverBrush_) rt->CreateSolidColorBrush(hoverColor_, hoverBrush_.GetAddressOf());
                    rt->FillRectangle(rowRect, hoverBrush_.Get());
                }

                for (int c = 0; c < columnCount_; ++c) {
                    float colWidth = GetEffectiveColumnWidth(c);
                    float cellX = colXPositions[c];
                    if (cellX + colWidth < arrangedRect_.x || cellX > arrangedRect_.x + viewportWidth) continue;

                    D2D1_RECT_F cellRect = D2D1::RectF(cellX, itemY, cellX + colWidth, itemY + rowHeight_);
                    if (c == 0) {
                        float indentX = GetNodeTextStartX(node);

                        if (!node->children.empty()) {
                            std::wstring arrow = node->expanded ? L"\u25BC" : L"\u25B6";
                            if (!textBrush_) rt->CreateSolidColorBrush(textColor_, textBrush_.GetAddressOf());
                            else textBrush_->SetColor(textColor_);
                            if (textBrush_ && fmt) {
                                D2D1_RECT_F arrowRect = D2D1::RectF(indentX - 12.0f, cellRect.top, indentX, cellRect.bottom);
                                rt->DrawText(arrow.c_str(), (UINT32)arrow.length(), fmt, arrowRect, textBrush_.Get());
                            }
                        }

                        if (node == selectedNode_) {
                            float barX = indicatorX_;
                            float barTop = cellRect.top + (cellRect.bottom - cellRect.top) * (1.0f - indicatorHeightRatio_) / 2.0f;
                            float barBottom = barTop + (cellRect.bottom - cellRect.top) * indicatorHeightRatio_;
                            if (!indicatorBrush_) rt->CreateSolidColorBrush(indicatorColor_, indicatorBrush_.GetAddressOf());
                            else indicatorBrush_->SetColor(indicatorColor_);
                            D2D1_RECT_F indicatorRect = D2D1::RectF(barX, barTop, barX + indicatorWidth_, barBottom);
                            rt->FillRoundedRectangle(D2D1::RoundedRect(indicatorRect, indicatorWidth_ / 2, indicatorWidth_ / 2), indicatorBrush_.Get());
                        }

                        float textX = indentX + 2.0f + indicatorWidth_ + 4.0f;
                        if (c < (int)node->columns.size() && !node->columns[c].empty()) {
                            if (!textBrush_) rt->CreateSolidColorBrush(textColor_, textBrush_.GetAddressOf());
                            else textBrush_->SetColor(textColor_);
                            D2D1_RECT_F textRect = D2D1::RectF(textX, cellRect.top, cellRect.right - 4.0f, cellRect.bottom);
                            DrawTextWithEllipsis(rt, node->columns[c], textRect, textColor_, spec, textBrush_, fmt);
                        }
                    }
                    else {
                        if (c < (int)node->columns.size() && !node->columns[c].empty()) {
                            if (!textBrush_) rt->CreateSolidColorBrush(textColor_, textBrush_.GetAddressOf());
                            else textBrush_->SetColor(textColor_);
                            D2D1_RECT_F textRect = D2D1::RectF(cellRect.left + 4.0f, cellRect.top,
                                cellRect.right - 4.0f, cellRect.bottom);
                            DrawTextWithEllipsis(rt, node->columns[c], textRect, textColor_, spec, textBrush_, fmt);
                        }
                    }
                }

                if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                else gridLineBrush_->SetColor(gridLineColor_);
                rt->DrawLine(D2D1::Point2F(arrangedRect_.x, itemY + rowHeight_),
                    D2D1::Point2F(arrangedRect_.x + viewportWidth, itemY + rowHeight_), gridLineBrush_.Get(), 1.0f);
            }

            rt->PopAxisAlignedClip();

            if (showVerticalScrollBar_) DrawVerticalScrollBar(rt, viewportHeight, headerOffset);
            if (showHorizontalScrollBar_) DrawHorizontalScrollBar(rt, viewportWidth);

            if (!borderBrush_) rt->CreateSolidColorBrush(borderColor_, borderBrush_.GetAddressOf());
            else borderBrush_->SetColor(borderColor_);
            if (borderBrush_) rt->DrawRoundedRectangle(D2D1::RoundedRect(arrangedRect_.ToD2D(), 4, 4), borderBrush_.Get(), 1.0f);
        }

        UIElement* HitTest(float x, float y) override {
            if (!visible_) return nullptr;
            if (arrangedRect_.Contains(x, y)) {
                if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) return this;
                if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) return this;
                return this;
            }
            return nullptr;
        }

        void OnMouseMove(float x, float y) override {
            if (isResizingColumn_) {
                float dx = x - resizeStartMouseX_;
                float newWidth = max(DefaultMinColumnWidth, resizeStartColumnWidth_ + dx);
                SetColumnWidth(resizeColumnIndex_, newWidth);
                return;
            }
            if (isDraggingVertical_) {
                HandleVerticalScrollDrag(y);
                return;
            }
            if (isDraggingHorizontal_) {
                HandleHorizontalScrollDrag(x);
                return;
            }
            if (!arrangedRect_.Contains(x, y)) {
                hoveredNode_ = nullptr;
                isVerticalHovered_ = false;
                isHorizontalHovered_ = false;
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
                RequestRepaint();
                return;
            }

            isVerticalHovered_ = (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_);
            isHorizontalHovered_ = (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_);

            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            if (headerVisible_ && y <= arrangedRect_.y + headerOffset) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                bool nearBoundary = false;
                for (int c = 0; c < columnCount_ - 1; ++c) {
                    float colWidth = GetEffectiveColumnWidth(c);
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        nearBoundary = true;
                        break;
                    }
                    colX += colWidth;
                }
                if (nearBoundary) SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                else SetCursor(LoadCursor(nullptr, IDC_ARROW));
                hoveredNode_ = nullptr;
                RequestRepaint();
                return;
            }
            else {
                SetCursor(LoadCursor(nullptr, IDC_ARROW));
            }

            float relY = y - arrangedRect_.y - headerOffset + Snap(scrollOffsetY_);
            int idx = (int)(relY / rowHeight_);
            if (idx >= 0 && idx < (int)visibleNodes_.size())
                hoveredNode_ = visibleNodes_[idx];
            else
                hoveredNode_ = nullptr;
            RequestRepaint();
        }

        void OnMouseDown(float x, float y) override {
            if (!arrangedRect_.Contains(x, y)) return;

            float headerOffset = headerVisible_ ? headerHeight_ : 0;

            if (headerVisible_ && y <= arrangedRect_.y + headerOffset) {
                float relX = x - arrangedRect_.x + Snap(scrollOffsetX_);
                float colX = 0;
                for (int c = 0; c < columnCount_ - 1; ++c) {
                    float colWidth = GetEffectiveColumnWidth(c);
                    float boundaryX = colX + colWidth;
                    if (fabs(relX - boundaryX) <= DefaultColumnResizeHitWidth / 2.0f) {
                        isResizingColumn_ = true;
                        resizeColumnIndex_ = c;
                        resizeStartMouseX_ = x;
                        resizeStartColumnWidth_ = colWidth;
                        return;
                    }
                    colX += colWidth;
                }
                return;
            }

            if (showVerticalScrollBar_ && x >= arrangedRect_.x + arrangedRect_.width - scrollBarWidth_) {
                float trackY = arrangedRect_.y + headerOffset;
                float trackHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
                float thumbY = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
                D2D1_RECT_F thumbRect = D2D1::RectF(arrangedRect_.x + arrangedRect_.width - scrollBarWidth_, thumbY,
                    arrangedRect_.x + arrangedRect_.width, thumbY + thumbLength);
                if (y >= thumbRect.top && y <= thumbRect.bottom) {
                    isDraggingVertical_ = true;
                    dragStartMouseY_ = y;
                    dragStartScrollY_ = scrollOffsetY_;
                    return;
                }
                else {
                    float ratio = (y - trackY - thumbLength / 2) / (trackHeight - thumbLength);
                    targetScrollOffsetY_ = clamp(ratio * maxScrollY_, 0.0f, maxScrollY_);
                    RequestRepaint();
                    return;
                }
            }

            if (showHorizontalScrollBar_ && y >= arrangedRect_.y + arrangedRect_.height - scrollBarWidth_) {
                float trackX = arrangedRect_.x;
                float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
                float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
                float thumbX = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
                D2D1_RECT_F thumbRect = D2D1::RectF(thumbX, arrangedRect_.y + arrangedRect_.height - scrollBarWidth_,
                    thumbX + thumbLength, arrangedRect_.y + arrangedRect_.height);
                if (x >= thumbRect.left && x <= thumbRect.right) {
                    isDraggingHorizontal_ = true;
                    dragStartMouseX_ = x;
                    dragStartScrollX_ = scrollOffsetX_;
                    return;
                }
                else {
                    float ratio = (x - trackX - thumbLength / 2) / (trackWidth - thumbLength);
                    targetScrollOffsetX_ = clamp(ratio * maxScrollX_, 0.0f, maxScrollX_);
                    RequestRepaint();
                    return;
                }
            }

            if (headerVisible_ && y <= arrangedRect_.y + headerOffset) return;

            float relY = y - arrangedRect_.y - headerOffset + Snap(scrollOffsetY_);
            int idx = (int)(relY / rowHeight_);
            if (idx >= 0 && idx < (int)visibleNodes_.size()) {
                auto node = visibleNodes_[idx];
                if (!node->children.empty()) {
                    float indentX = GetNodeTextStartX(node);
                    if (x >= indentX - 12.0f && x <= indentX) {
                        ToggleNode(node);
                        return;
                    }
                }
                SetSelectedNode(node);
                NodeClicked(node);
            }
        }

        void OnMouseUp(float x, float y) override {
            if (isResizingColumn_) {
                isResizingColumn_ = false;
                RequestRepaint();
                return;
            }
            if (isDraggingVertical_) { isDraggingVertical_ = false; RequestRepaint(); return; }
            if (isDraggingHorizontal_) { isDraggingHorizontal_ = false; RequestRepaint(); return; }
        }

        void OnMouseLeave() override {
            hoveredNode_ = nullptr;
            isVerticalHovered_ = false;
            isHorizontalHovered_ = false;
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            RequestRepaint();
        }

        bool OnMouseWheel(float deltaX, float deltaY) override {
            bool handled = false;
            if (deltaY != 0 && maxScrollY_ > 0) {
                targetScrollOffsetY_ = clamp(targetScrollOffsetY_ - deltaY * scrollWheelStep_, 0.0f, maxScrollY_);
                handled = true;
                RequestRepaint();
            }
            if (deltaX != 0 && maxScrollX_ > 0) {
                targetScrollOffsetX_ = clamp(targetScrollOffsetX_ - deltaX * scrollWheelStep_, 0.0f, maxScrollX_);
                handled = true;
                RequestRepaint();
            }
            return handled;
        }

        void OnKeyDown(WPARAM key, LPARAM lParam) override {
            if (!IsFocusable() || visibleNodes_.empty()) return;
            int idx = GetVisibleIndex(selectedNode_);
            switch (key) {
            case VK_UP:
                if (idx > 0) SetSelectedNode(visibleNodes_[idx - 1]);
                break;
            case VK_DOWN:
                if (idx < (int)visibleNodes_.size() - 1) SetSelectedNode(visibleNodes_[idx + 1]);
                break;
            case VK_RIGHT:
                if (selectedNode_ && !selectedNode_->children.empty() && !selectedNode_->expanded)
                    ExpandNode(selectedNode_, true);
                break;
            case VK_LEFT:
                if (selectedNode_ && selectedNode_->expanded)
                    ExpandNode(selectedNode_, false);
                else if (selectedNode_ && selectedNode_->parent) {
                    auto parentShared = FindNode(selectedNode_->parent);
                    if (parentShared) SetSelectedNode(parentShared);
                }
                break;
            case VK_HOME:
                if (!visibleNodes_.empty()) SetSelectedNode(visibleNodes_.front());
                break;
            case VK_END:
                if (!visibleNodes_.empty()) SetSelectedNode(visibleNodes_.back());
                break;
            default:
                break;
            }
        }

        void OnFocus() override { RequestRepaint(); if (FocusHandler) FocusHandler(); }
        void OnBlur() override { RequestRepaint(); if (BlurHandler) BlurHandler(); }

        void UpdateAnimation(float deltaTime) override {
            if (fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f) {
                scrollOffsetY_ += (targetScrollOffsetY_ - scrollOffsetY_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetY_ - scrollOffsetY_) <= 0.1f) scrollOffsetY_ = targetScrollOffsetY_;
                RequestRepaint();
            }
            else scrollOffsetY_ = targetScrollOffsetY_;

            if (fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f) {
                scrollOffsetX_ += (targetScrollOffsetX_ - scrollOffsetX_) * min(1.0f, scrollAnimationSpeed_ * deltaTime);
                if (fabs(targetScrollOffsetX_ - scrollOffsetX_) <= 0.1f) scrollOffsetX_ = targetScrollOffsetX_;
                RequestRepaint();
            }
            else scrollOffsetX_ = targetScrollOffsetX_;

            const float lerpFactor = 1.0f - exp(-deltaTime * indicatorAnimSpeed_);
            indicatorY_ += (targetIndicatorY_ - indicatorY_) * lerpFactor;
            indicatorX_ += (targetIndicatorX_ - indicatorX_) * lerpFactor;
            if (fabs(indicatorY_ - targetIndicatorY_) < 0.01f) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorX_ - targetIndicatorX_) < 0.01f) indicatorX_ = targetIndicatorX_;
            if (fabs(indicatorY_ - targetIndicatorY_) > 0.01f || fabs(indicatorX_ - targetIndicatorX_) > 0.01f)
                RequestRepaint();

            if (isVerticalHovered_ || isDraggingVertical_) {
                if (verticalScrollHoverProgress_ < 1.0f) {
                    verticalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ > 1.0f) verticalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (verticalScrollHoverProgress_ > 0.0f) {
                    verticalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (verticalScrollHoverProgress_ < 0.0f) verticalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            if (isHorizontalHovered_ || isDraggingHorizontal_) {
                if (horizontalScrollHoverProgress_ < 1.0f) {
                    horizontalScrollHoverProgress_ += hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ > 1.0f) horizontalScrollHoverProgress_ = 1.0f;
                    RequestRepaint();
                }
            }
            else {
                if (horizontalScrollHoverProgress_ > 0.0f) {
                    horizontalScrollHoverProgress_ -= hoverAnimationSpeed_ * deltaTime;
                    if (horizontalScrollHoverProgress_ < 0.0f) horizontalScrollHoverProgress_ = 0.0f;
                    RequestRepaint();
                }
            }
            ConvergeValue(indicatorY_, targetIndicatorY_, 0.1f);
            ConvergeValue(indicatorX_, targetIndicatorX_, 0.1f);
            ConvergeValue(scrollOffsetX_, targetScrollOffsetX_, 0.1f);
            ConvergeValue(scrollOffsetY_, targetScrollOffsetY_, 0.1f);
            // 末尾添加强制收敛
            const float eps = 0.1f;
            if (fabs(indicatorY_ - targetIndicatorY_) < eps) indicatorY_ = targetIndicatorY_;
            if (fabs(indicatorX_ - targetIndicatorX_) < eps) indicatorX_ = targetIndicatorX_;
            if (fabs(scrollOffsetX_ - targetScrollOffsetX_) < eps) scrollOffsetX_ = targetScrollOffsetX_;
            if (fabs(scrollOffsetY_ - targetScrollOffsetY_) < eps) scrollOffsetY_ = targetScrollOffsetY_;
        }

        bool HasActiveAnimation() const override {
            return fabs(targetScrollOffsetX_ - scrollOffsetX_) > 0.1f ||
                fabs(targetScrollOffsetY_ - scrollOffsetY_) > 0.1f ||
                fabs(indicatorY_ - targetIndicatorY_) > 0.01f ||
                fabs(indicatorX_ - targetIndicatorX_) > 0.01f ||
                (isVerticalHovered_ && verticalScrollHoverProgress_ < 1.0f) ||
                (!isVerticalHovered_ && verticalScrollHoverProgress_ > 0.0f) ||
                (isHorizontalHovered_ && horizontalScrollHoverProgress_ < 1.0f) ||
                (!isHorizontalHovered_ && horizontalScrollHoverProgress_ > 0.0f);
        }

        void ReleaseDeviceResources() override {
            bgBrush_.Reset();
            headerBgBrush_.Reset();
            textBrush_.Reset();
            headerTextBrush_.Reset();
            selectedBrush_.Reset();
            hoverBrush_.Reset();
            indicatorBrush_.Reset();
            gridLineBrush_.Reset();
            borderBrush_.Reset();
            scrollTrackBrush_.Reset();
            scrollThumbBrush_.Reset();
            UIElement::ReleaseDeviceResources();
        }

    private:
        void BuildVisibleList() {
            float oldScrollX = scrollOffsetX_;
            float oldScrollY = scrollOffsetY_;
            visibleNodes_.clear();
            std::function<void(std::shared_ptr<TreeNode>)> traverse = [&](std::shared_ptr<TreeNode> node) {
                if (!node) return;
                visibleNodes_.push_back(node);
                if (node->expanded) {
                    for (auto& child : node->children) {
                        traverse(child);
                    }
                }
                };
            for (auto& root : roots_) {
                traverse(root);
            }
            UpdateScrollInfo();
            scrollOffsetX_ = clamp(oldScrollX, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(oldScrollY, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = scrollOffsetX_;
            targetScrollOffsetY_ = scrollOffsetY_;
            UpdateIndicatorTarget();
        }

        int GetNodeDepth(std::shared_ptr<TreeNode> node) const {
            return node ? node->depth : 0;
        }

        float GetNodeTextStartX(std::shared_ptr<TreeNode> node) const {
            int depth = GetNodeDepth(node);
            const float kBaseOffset = 12.0f;
            float indentX = arrangedRect_.x - Snap(scrollOffsetX_) + kBaseOffset + depth * indent_;
            if (indentX < arrangedRect_.x + 12.0f) {
                indentX = arrangedRect_.x + 12.0f;
            }
            return indentX;
        }

        std::shared_ptr<TreeNode> FindNode(TreeNode* rawPtr) const {
            std::function<std::shared_ptr<TreeNode>(const std::vector<std::shared_ptr<TreeNode>>&)> search =
                [&](const std::vector<std::shared_ptr<TreeNode>>& nodes) -> std::shared_ptr<TreeNode> {
                for (auto& node : nodes) {
                    if (node.get() == rawPtr) return node;
                    auto found = search(node->children);
                    if (found) return found;
                }
                return nullptr;
                };
            return search(roots_);
        }

        int GetVisibleIndex(std::shared_ptr<TreeNode> node) const {
            for (int i = 0; i < (int)visibleNodes_.size(); ++i) {
                if (visibleNodes_[i] == node) return i;
            }
            return -1;
        }

        float GetEffectiveColumnWidth(int col) const {
            if (col < 0 || col >= (int)columnWidths_.size()) return DefaultMinColumnWidth;
            return columnWidths_[col];
        }

        void UpdateScrollInfo() {
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float availWidth = arrangedRect_.width;
            float availHeight = arrangedRect_.height - headerOffset;

            float totalWidth = 0;
            for (float w : columnWidths_) totalWidth += w;
            float contentHeight = visibleNodes_.size() * rowHeight_;

            bool needV = contentHeight > availHeight;
            bool needH = totalWidth > availWidth;

            float viewportWidth = needV ? availWidth - scrollBarWidth_ : availWidth;
            float viewportHeight = needH ? availHeight - scrollBarWidth_ : availHeight;

            if (!needV && contentHeight > viewportHeight) {
                needV = true;
                viewportWidth = availWidth - scrollBarWidth_;
            }
            if (!needH && totalWidth > viewportWidth) {
                needH = true;
                viewportHeight = availHeight - scrollBarWidth_;
            }

            showVerticalScrollBar_ = needV;
            showHorizontalScrollBar_ = needH;

            maxScrollX_ = max(0.0f, totalWidth - viewportWidth);
            maxScrollY_ = max(0.0f, contentHeight - viewportHeight);

            scrollOffsetX_ = clamp(scrollOffsetX_, 0.0f, maxScrollX_);
            scrollOffsetY_ = clamp(scrollOffsetY_, 0.0f, maxScrollY_);
            targetScrollOffsetX_ = clamp(targetScrollOffsetX_, 0.0f, maxScrollX_);
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void EnsureVisible(std::shared_ptr<TreeNode> node) {
            int idx = GetVisibleIndex(node);
            if (idx < 0) return;
            float itemTop = idx * rowHeight_;
            float itemBottom = itemTop + rowHeight_;
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float viewportHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            if (itemTop < scrollOffsetY_) {
                targetScrollOffsetY_ = itemTop;
            }
            else if (itemBottom > scrollOffsetY_ + viewportHeight) {
                targetScrollOffsetY_ = itemBottom - viewportHeight;
            }
            targetScrollOffsetY_ = clamp(targetScrollOffsetY_, 0.0f, maxScrollY_);
        }

        void UpdateIndicatorTarget() {
            int idx = GetVisibleIndex(selectedNode_);
            if (idx >= 0) {
                targetIndicatorY_ = idx * rowHeight_;
                float indentX = GetNodeTextStartX(selectedNode_);
                targetIndicatorX_ = indentX + 2.0f;
            }
            else {
                targetIndicatorY_ = 0.0f;
                targetIndicatorX_ = 0.0f;
            }
            if (indicatorY_ < 0.0f) indicatorY_ = targetIndicatorY_;
            if (indicatorX_ < 0.0f) indicatorX_ = targetIndicatorX_;
        }

        void HandleVerticalScrollDrag(float mouseY) {
            float headerOffset = headerVisible_ ? headerHeight_ : 0;
            float trackY = arrangedRect_.y + headerOffset;
            float trackHeight = arrangedRect_.height - headerOffset - (showHorizontalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            if (trackHeight > thumbLength) {
                float ratio = (mouseY - dragStartMouseY_) / (trackHeight - thumbLength);
                targetScrollOffsetY_ = clamp(dragStartScrollY_ + ratio * maxScrollY_, 0.0f, maxScrollY_);
                RequestRepaint();
            }
        }

        void HandleHorizontalScrollDrag(float mouseX) {
            float trackX = arrangedRect_.x;
            float trackWidth = arrangedRect_.width - (showVerticalScrollBar_ ? scrollBarWidth_ : 0);
            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            if (trackWidth > thumbLength) {
                float ratio = (mouseX - dragStartMouseX_) / (trackWidth - thumbLength);
                targetScrollOffsetX_ = clamp(dragStartScrollX_ + ratio * maxScrollX_, 0.0f, maxScrollX_);
                RequestRepaint();
            }
        }

        void DrawHeader(ID2D1RenderTarget* rt, float viewportWidth) {
            D2D1_RECT_F headerRect = D2D1::RectF(arrangedRect_.x, arrangedRect_.y,
                arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_);
            if (!headerBgBrush_) rt->CreateSolidColorBrush(headerBackgroundColor_, headerBgBrush_.GetAddressOf());
            else headerBgBrush_->SetColor(headerBackgroundColor_);
            rt->FillRectangle(headerRect, headerBgBrush_.Get());

            IDWriteTextFormat* fmt = GetFontFormat();
            FontSpec spec = GetEffectiveFontSpec();

            float colX = arrangedRect_.x - Snap(scrollOffsetX_);
            for (int c = 0; c < columnCount_; ++c) {
                float colWidth = GetEffectiveColumnWidth(c);
                if (colX + colWidth >= arrangedRect_.x && colX <= arrangedRect_.x + viewportWidth) {
                    if (c < (int)headerLabels_.size() && !headerLabels_[c].empty()) {
                        if (!headerTextBrush_) rt->CreateSolidColorBrush(headerTextColor_, headerTextBrush_.GetAddressOf());
                        else headerTextBrush_->SetColor(headerTextColor_);
                        D2D1_RECT_F textRect = D2D1::RectF(colX + 4, arrangedRect_.y,
                            colX + colWidth - 4, arrangedRect_.y + headerHeight_);
                        DrawTextWithEllipsis(rt, headerLabels_[c], textRect, headerTextColor_, spec, headerTextBrush_, fmt);
                    }
                    if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
                    else gridLineBrush_->SetColor(gridLineColor_);
                    rt->DrawLine(D2D1::Point2F(colX, arrangedRect_.y),
                        D2D1::Point2F(colX, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
                }
                colX += colWidth;
            }
            if (!gridLineBrush_) rt->CreateSolidColorBrush(gridLineColor_, gridLineBrush_.GetAddressOf());
            else gridLineBrush_->SetColor(gridLineColor_);
            rt->DrawLine(D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y),
                D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
            rt->DrawLine(D2D1::Point2F(arrangedRect_.x, arrangedRect_.y + headerHeight_),
                D2D1::Point2F(arrangedRect_.x + viewportWidth, arrangedRect_.y + headerHeight_), gridLineBrush_.Get(), 1.0f);
        }

        void DrawVerticalScrollBar(ID2D1RenderTarget* rt, float viewportHeight, float headerOffset) {
            float trackX = arrangedRect_.x + arrangedRect_.width - scrollBarWidth_;
            float trackY = arrangedRect_.y + headerOffset;
            float trackHeight = viewportHeight - headerOffset;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + scrollBarWidth_, trackY + trackHeight),
                scrollBarWidth_ / 2, scrollBarWidth_ / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackHeight * (trackHeight / (maxScrollY_ + trackHeight)));
            float thumbPos = trackY + (trackHeight - thumbLength) * (Snap(scrollOffsetY_) / maxScrollY_);
            float thumbWidth = scrollBarWidth_ - 2.0f;
            float thumbX = trackX + (scrollBarWidth_ - thumbWidth) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (verticalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * verticalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * verticalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * verticalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * verticalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbX, thumbPos, thumbX + thumbWidth, thumbPos + thumbLength),
                thumbWidth / 2, thumbWidth / 2), scrollThumbBrush_.Get());
        }

        void DrawHorizontalScrollBar(ID2D1RenderTarget* rt, float viewportWidth) {
            float trackX = arrangedRect_.x;
            float trackY = arrangedRect_.y + arrangedRect_.height - scrollBarWidth_;
            float trackWidth = viewportWidth;

            if (!scrollTrackBrush_) rt->CreateSolidColorBrush(scrollTrackColor_, scrollTrackBrush_.GetAddressOf());
            else scrollTrackBrush_->SetColor(scrollTrackColor_);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(trackX, trackY, trackX + trackWidth, trackY + scrollBarWidth_),
                scrollBarWidth_ / 2, scrollBarWidth_ / 2), scrollTrackBrush_.Get());

            float thumbLength = max(scrollBarMinLength_, trackWidth * (trackWidth / (maxScrollX_ + trackWidth)));
            float thumbPos = trackX + (trackWidth - thumbLength) * (Snap(scrollOffsetX_) / maxScrollX_);
            float thumbHeight = scrollBarWidth_ - 2.0f;
            float thumbY = trackY + (scrollBarWidth_ - thumbHeight) / 2.0f;

            D2D1_COLOR_F thumbCol = scrollThumbColor_;
            if (horizontalScrollHoverProgress_ > 0.01f) {
                thumbCol = D2D1::ColorF(
                    scrollThumbColor_.r + (scrollHoverThumbColor_.r - scrollThumbColor_.r) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.g + (scrollHoverThumbColor_.g - scrollThumbColor_.g) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.b + (scrollHoverThumbColor_.b - scrollThumbColor_.b) * horizontalScrollHoverProgress_,
                    scrollThumbColor_.a + (scrollHoverThumbColor_.a - scrollThumbColor_.a) * horizontalScrollHoverProgress_);
            }
            if (!scrollThumbBrush_) rt->CreateSolidColorBrush(thumbCol, scrollThumbBrush_.GetAddressOf());
            else scrollThumbBrush_->SetColor(thumbCol);
            rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(thumbPos, thumbY, thumbPos + thumbLength, thumbY + thumbHeight),
                thumbHeight / 2, thumbHeight / 2), scrollThumbBrush_.Get());
        }

        // 成员变量
        std::vector<std::shared_ptr<TreeNode>> roots_;
        std::vector<std::shared_ptr<TreeNode>> visibleNodes_;
        std::shared_ptr<TreeNode> selectedNode_;
        std::shared_ptr<TreeNode> hoveredNode_;
        float scrollOffsetX_, targetScrollOffsetX_, maxScrollX_;
        float scrollOffsetY_, targetScrollOffsetY_, maxScrollY_;
        bool showVerticalScrollBar_, showHorizontalScrollBar_;
        bool isDraggingVertical_, isDraggingHorizontal_;
        float dragStartMouseX_, dragStartMouseY_;
        float dragStartScrollX_, dragStartScrollY_;
        bool isResizingColumn_;
        int resizeColumnIndex_;
        float resizeStartMouseX_, resizeStartColumnWidth_;
        float rowHeight_, indent_, headerHeight_;
        bool headerVisible_;
        int columnCount_;
        std::vector<float> columnWidths_;
        std::vector<std::wstring> headerLabels_;
        float indicatorWidth_, indicatorHeightRatio_;
        D2D1_COLOR_F indicatorColor_;
        float indicatorAnimSpeed_;
        float indicatorY_, targetIndicatorY_;
        float indicatorX_, targetIndicatorX_;
        float scrollBarWidth_, scrollBarMinLength_;
        float scrollWheelStep_, scrollAnimationSpeed_, hoverAnimationSpeed_;
        D2D1_COLOR_F backgroundColor_, headerBackgroundColor_;
        D2D1_COLOR_F textColor_, headerTextColor_;
        D2D1_COLOR_F selectedColor_, hoverColor_, gridLineColor_, borderColor_;
        D2D1_COLOR_F scrollTrackColor_, scrollThumbColor_, scrollHoverThumbColor_;
        float verticalScrollHoverProgress_, horizontalScrollHoverProgress_;
        bool isVerticalHovered_, isHorizontalHovered_;
        // 移除 textFormat_，改用 FontManager
        ComPtr<ID2D1SolidColorBrush> bgBrush_, headerBgBrush_;
        ComPtr<ID2D1SolidColorBrush> textBrush_, headerTextBrush_;
        ComPtr<ID2D1SolidColorBrush> selectedBrush_, hoverBrush_, indicatorBrush_, gridLineBrush_, borderBrush_;
        ComPtr<ID2D1SolidColorBrush> scrollTrackBrush_, scrollThumbBrush_;
    };

} // namespace ZUI