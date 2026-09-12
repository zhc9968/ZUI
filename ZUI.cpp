// main.cpp - ZUI 综合自动化布局测试（使用 Connect 自动管理连接）
#include "ZDataViewer.h"

using namespace ZUI;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Window win;
    if (!win.Create(1000, 700, L"ZUI 自动化布局综合测试 1.5.1")) {
        MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_ICONERROR);
        return 1;
    }

    // 创建全局右键菜单（与信号无关）
    auto globalMenu = std::make_shared<Menu>();
    globalMenu->AddItem(L"显示信息", []() {
        MessageBoxW(nullptr, L"这是全局右键菜单", L"提示", MB_OK);
        });
    globalMenu->AddSeparator();
    auto subMenu = std::make_shared<Menu>();
    subMenu->AddItem(L"子项1", []() { MessageBoxW(nullptr, L"点击了子项1", L"提示", MB_OK); });
    subMenu->AddItem(L"子项2", []() { MessageBoxW(nullptr, L"点击了子项2", L"提示", MB_OK); });
    globalMenu->AddSubmenu(L"更多操作", subMenu);
    globalMenu->AddSeparator();
    globalMenu->AddItem(L"关于", []() {
        MessageBoxW(nullptr, L"ZUI 综合测试程序 v1.0", L"关于", MB_OK);
        });
    win.SetContextMenu(globalMenu);

    // 获取默认根布局（ColumnBox）
    auto root = win.GetRootColumnBox();
    if (!root) return 1;
    root->SetSpacing(10);

    // 主水平布局：左侧导航 + 右侧内容
    auto mainRow = std::make_shared<RowBox>();
    mainRow->SetSpacing(10);
    mainRow->SetFillHeight(true);
    mainRow->SetFillWidth(true);
    root->AddChild(mainRow);

    // 左侧导航：使用 ListView 按钮模式
    auto navList = std::make_shared<ListView>();
    navList->SetWidth(160);
    navList->SetFillHeight(true);
    navList->SetButtonMode(true);
    navList->SetButtonSpacing(4.0f);
    navList->SetItemHeight(36.0f);
    navList->AddItem(L"基础控件");
    navList->AddItem(L"输入与滚动");
    navList->AddItem(L"嵌套与联动");
    navList->AddItem(L"列表视图");
    navList->AddItem(L"表格视图");
    navList->AddItem(L"树形视图");
    navList->SetSelectedIndex(0);
    mainRow->AddChild(navList);

    // 右侧页面容器
    auto mainHost = std::make_shared<PageHost>();
    mainHost->SetTransitionDirection(PageHost::TransitionDirection::Left);
    mainHost->SetFillWidth(true);
    mainHost->SetFillHeight(true);
    mainRow->AddChild(mainHost);
    mainHost->SetUseCache(false);  // PageHost 本身也不应缓存

    // ---------- 页面1：基础控件 ----------
    auto page1 = std::make_shared<Page>();
    auto grid1 = page1->GetLayoutAs<GridLayout>();
    if (grid1) {
        grid1->SetSpacing(10, 10);

        auto title1 = std::make_shared<Label>(L"基础控件测试");
        title1->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid1->AddChild(title1, 0, 0, 1, 2);

        auto btn1 = std::make_shared<Button>(L"普通按钮12345678901234567890123456789012345678901234567890");
        auto btn2 = std::make_shared<Button>(L"彩色按钮");
        btn2->SetColors(Color::FromArgb(255, 180, 60, 60), Color::FromArgb(255, 200, 80, 80), Color::FromArgb(255, 160, 40, 40));
        btn1->Connect(btn1->Clicked, []() { MessageBoxW(nullptr, L"按钮1被点击", L"提示", MB_OK); });
        btn2->Connect(btn2->Clicked, []() { MessageBoxW(nullptr, L"按钮2被点击", L"提示", MB_OK); });
        grid1->AddChild(btn1, 1, 0);
        grid1->AddChild(btn2, 1, 1);

        auto toggle1 = std::make_shared<ToggleSwitch>(false);
        auto lblToggleState = std::make_shared<Label>(L"开关状态：关");
        toggle1->Connect(toggle1->Toggled, [lblToggleState](bool on) {
            lblToggleState->SetText(on ? L"开关状态：开" : L"开关状态：关");
            lblToggleState->SetTextColor(on ? Color::FromArgb(255, 0, 120, 0) : Color::FromArgb(255, 120, 0, 0));
            });
        grid1->AddChild(toggle1, 2, 0);
        grid1->AddChild(lblToggleState, 2, 1);

        auto slider1 = std::make_shared<Slider>();
        slider1->SetRange(0.0f, 1.0f);
        slider1->SetValue(0.4f);
        auto progress1 = std::make_shared<ProgressBar>();
        progress1->SetValue(0.4f);
        slider1->Connect(slider1->ValueChanged, [progress1](float val) {
            progress1->SetValue(val);
            });
        grid1->AddChild(slider1, 3, 0);
        grid1->AddChild(progress1, 3, 1);

        auto combo1 = std::make_shared<ComboBox>();
        combo1->AddItem(L"选项 A");
        combo1->AddItem(L"选项 B");
        combo1->AddItem(L"选项 C");
        auto lblCombo = std::make_shared<Label>(L"当前选择：选项 A");
        combo1->Connect(combo1->SelectionChanged, [lblCombo, combo1](int index) {
            lblCombo->SetText(L"当前选择：" + combo1->GetSelectedText());
            });
        grid1->AddChild(combo1, 4, 0);
        grid1->AddChild(lblCombo, 4, 1);
    }

    // ---------- 页面2：输入与滚动 ----------
    auto page2 = std::make_shared<Page>();
    auto grid2 = page2->GetLayoutAs<GridLayout>();
    if (grid2) {
        grid2->SetSpacing(10, 10);

        auto title2 = std::make_shared<Label>(L"输入控件与滚动容器");
        title2->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid2->AddChild(title2, 0, 0, 1, 2);

        auto textBox1 = std::make_shared<TextBox>();
        textBox1->SetPlaceholder(L"输入文本...");
        auto lblText = std::make_shared<Label>(L"你输入的内容将显示在这里");
        textBox1->Connect(textBox1->TextChanged, [lblText](const std::wstring& text) {
            if (text.empty())
                lblText->SetText(L"你输入的内容将显示在这里");
            else
                lblText->SetText(L"输入：" + text);
            });
        lblText->SetWidth(100);  // 固定宽度，文本超出即省略
        lblText->SetTextOverflow(Label::TextOverflow::Ellipsis);
        grid2->AddChild(textBox1, 1, 0);
        grid2->AddChild(lblText, 1, 1);

        auto passwordBox = std::make_shared<TextBox>();
        passwordBox->SetPlaceholder(L"密码");
        passwordBox->SetPasswordMode(true);
        passwordBox->SetMaxLength(16);
        grid2->AddChild(passwordBox, 2, 0);

        auto numBox = std::make_shared<TextBox>();
        numBox->SetPlaceholder(L"数字");
        numBox->SetMaxLength(10);
        grid2->AddChild(numBox, 2, 1);

        auto scrollView = std::make_shared<ScrollViewer>();
        scrollView->SetHeight(150);
        scrollView->SetVerticalScrollEnabled(true);
        scrollView->SetHorizontalScrollEnabled(false);

        auto scrollContent = std::make_shared<ColumnBox>();
        scrollContent->SetSpacing(6);
        for (int i = 1; i <= 30; i++) {
            auto line = std::make_shared<Label>(L"滚动条目 " + std::to_wstring(i));
            line->SetTextColor(Color::FromArgb(255, 60, 60, 60));
            scrollContent->AddChild(line);
        }
        scrollView->SetContent(scrollContent);
        grid2->AddChild(scrollView, 3, 0, 1, 2);
    }

    // ---------- 页面3：嵌套与联动 ----------
    auto page3 = std::make_shared<Page>();
    auto grid3 = page3->GetLayoutAs<GridLayout>();
    if (grid3) {
        grid3->SetSpacing(10, 10);

        auto title3 = std::make_shared<Label>(L"嵌套 PageHost 与控件联动");
        title3->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid3->AddChild(title3, 0, 0, 1, 2);

        auto btnNestedA = std::make_shared<Button>(L"嵌套 A");
        auto btnNestedB = std::make_shared<Button>(L"嵌套 B");
        auto btnNestedC = std::make_shared<Button>(L"嵌套 C");
        auto nestedNav = std::make_shared<RowBox>();
        nestedNav->SetSpacing(10);
        nestedNav->AddChild(btnNestedA);
        nestedNav->AddChild(btnNestedB);
        nestedNav->AddChild(btnNestedC);
        grid3->AddChild(nestedNav, 1, 0, 1, 2);

        auto nestedHost = std::make_shared<PageHost>();
        nestedHost->SetTransitionDirection(PageHost::TransitionDirection::Up);
        grid3->AddChild(nestedHost, 2, 0, 1, 2);

        auto nestedPageA = std::make_shared<Page>();
        auto nestedLayoutA = nestedPageA->GetLayoutAs<GridLayout>();
        if (nestedLayoutA) {
            nestedLayoutA->SetSpacing(8, 8);

            auto lblA = std::make_shared<Label>(L"嵌套 A：滑块控制进度条");
            nestedLayoutA->AddChild(lblA, 0, 0, 1, 2);

            auto sliderA = std::make_shared<Slider>();
            sliderA->SetRange(0.0f, 100.0f);
            sliderA->SetValue(50.0f);
            auto progressA = std::make_shared<ProgressBar>();
            progressA->SetValue(0.5f);
            sliderA->Connect(sliderA->ValueChanged, [progressA](float val) {
                progressA->SetValue(val / 100.0f);
                });
            nestedLayoutA->AddChild(sliderA, 1, 0);
            nestedLayoutA->AddChild(progressA, 1, 1);

            auto btnA = std::make_shared<Button>(L"重置为 50%");
            btnA->Connect(btnA->Clicked, [sliderA]() {
                sliderA->SetValue(50.0f);
                });
            nestedLayoutA->AddChild(btnA, 2, 0, 1, 2);
        }

        auto nestedPageB = std::make_shared<Page>();
        auto nestedLayoutB = nestedPageB->GetLayoutAs<GridLayout>();
        if (nestedLayoutB) {
            nestedLayoutB->SetSpacing(8, 8);

            auto lblB = std::make_shared<Label>(L"嵌套 B：下拉框控制标签");
            nestedLayoutB->AddChild(lblB, 0, 0, 1, 2);

            auto comboB = std::make_shared<ComboBox>();
            comboB->AddItem(L"红色");
            comboB->AddItem(L"绿色");
            comboB->AddItem(L"蓝色");
            auto displayLabel = std::make_shared<Label>(L"当前颜色：红色");
            displayLabel->SetTextColor(Color::FromArgb(255, 255, 0, 0));
            comboB->Connect(comboB->SelectionChanged, [displayLabel, comboB](int index) {
                std::wstring colorName = comboB->GetSelectedText();
                displayLabel->SetText(L"当前颜色：" + colorName);
                if (colorName == L"红色")
                    displayLabel->SetTextColor(Color::FromArgb(255, 255, 0, 0));
                else if (colorName == L"绿色")
                    displayLabel->SetTextColor(Color::FromArgb(255, 0, 128, 0));
                else
                    displayLabel->SetTextColor(Color::FromArgb(255, 0, 0, 255));
                });
            nestedLayoutB->AddChild(comboB, 1, 0);
            nestedLayoutB->AddChild(displayLabel, 1, 1);

            auto btnB = std::make_shared<Button>(L"重置选择");
            btnB->Connect(btnB->Clicked, [comboB]() {
                comboB->SetSelectedIndex(0);
                });
            nestedLayoutB->AddChild(btnB, 2, 0, 1, 2);
        }
        // ---------- 新增：嵌套页面 C（密集交叉测试） ----------
        auto nestedPageC = std::make_shared<Page>();
        auto nestedLayoutC = nestedPageC->GetLayoutAs<GridLayout>();
        if (nestedLayoutC) {
            nestedLayoutC->SetSpacing(6, 6);

            auto lblC = std::make_shared<Label>(L"密集交叉测试：多个下拉框与控件混合");
            lblC->SetTextColor(Color::FromArgb(255, 40, 40, 40));
            nestedLayoutC->AddChild(lblC, 0, 0, 1, 4);

            // 第一行：下拉框1、按钮A、下拉框2、标签
            auto combo1 = std::make_shared<ComboBox>();
            combo1->AddItem(L"选项1-A");
            combo1->AddItem(L"选项1-B");
            combo1->AddItem(L"选项1-C");
            nestedLayoutC->AddChild(combo1, 1, 0);

            auto btnA = std::make_shared<Button>(L"按钮A");
            btnA->Connect(btnA->Clicked, []() { MessageBoxW(nullptr, L"按钮A被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnA, 1, 1);

            auto combo2 = std::make_shared<ComboBox>();
            combo2->AddItem(L"选项2-A");
            combo2->AddItem(L"选项2-B");
            combo2->AddItem(L"选项2-C");
            nestedLayoutC->AddChild(combo2, 1, 2);

            auto lblStatus1 = std::make_shared<Label>(L"状态1");
            nestedLayoutC->AddChild(lblStatus1, 1, 3);

            // 第二行：标签、下拉框3、滑块、按钮B
            auto lblStatic = std::make_shared<Label>(L"固定文本");
            nestedLayoutC->AddChild(lblStatic, 2, 0);

            auto combo3 = std::make_shared<ComboBox>();
            combo3->AddItem(L"选项3-A");
            combo3->AddItem(L"选项3-B");
            combo3->AddItem(L"选项3-C");
            nestedLayoutC->AddChild(combo3, 2, 1);

            auto sliderC = std::make_shared<Slider>();
            sliderC->SetRange(0.0f, 100.0f);
            sliderC->SetValue(50.0f);
            nestedLayoutC->AddChild(sliderC, 2, 2);

            auto btnB = std::make_shared<Button>(L"按钮B");
            btnB->Connect(btnB->Clicked, []() { MessageBoxW(nullptr, L"按钮B被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnB, 2, 3);

            // 第三行：进度条、下拉框4、按钮C、下拉框5
            auto progressC = std::make_shared<ProgressBar>();
            progressC->SetValue(0.7f);
            nestedLayoutC->AddChild(progressC, 3, 0);

            auto combo4 = std::make_shared<ComboBox>();
            combo4->AddItem(L"选项4-A");
            combo4->AddItem(L"选项4-B");
            combo4->AddItem(L"选项4-C");
            nestedLayoutC->AddChild(combo4, 3, 1);

            auto btnC = std::make_shared<Button>(L"按钮C");
            btnC->Connect(btnC->Clicked, []() { MessageBoxW(nullptr, L"按钮C被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnC, 3, 2);

            auto combo5 = std::make_shared<ComboBox>();
            combo5->AddItem(L"选项5-A");
            combo5->AddItem(L"选项5-B");
            combo5->AddItem(L"选项5-C");
            nestedLayoutC->AddChild(combo5, 3, 3);

            // 第四行：开关、标签、下拉框6、按钮D
            auto toggleC = std::make_shared<ToggleSwitch>(false);
            nestedLayoutC->AddChild(toggleC, 4, 0);

            auto lblToggle = std::make_shared<Label>(L"开关");
            nestedLayoutC->AddChild(lblToggle, 4, 1);

            auto combo6 = std::make_shared<ComboBox>();
            combo6->AddItem(L"选项6-A");
            combo6->AddItem(L"选项6-B");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            combo6->AddItem(L"选项6-C");
            nestedLayoutC->AddChild(combo6, 4, 2);

            auto btnD = std::make_shared<Button>(L"按钮D");
            btnD->Connect(btnD->Clicked, []() { MessageBoxW(nullptr, L"按钮D被点击", L"提示", MB_OK); });
            nestedLayoutC->AddChild(btnD, 4, 3);
        }

        nestedHost->AddPage(nestedPageA);
        nestedHost->AddPage(nestedPageB);
        nestedHost->AddPage(nestedPageC);   // 新增

        // 嵌套页面切换：根据目标索引设置左右方向
        auto navigateNested = [nestedHost](int target) {
            int current = nestedHost->GetCurrentIndex();
            if (target > current) {
                nestedHost->SetTransitionDirection(PageHost::TransitionDirection::Left);
            }
            else if (target < current) {
                nestedHost->SetTransitionDirection(PageHost::TransitionDirection::Right);
            }
            nestedHost->NavigateTo(target);
            };

        btnNestedA->Connect(btnNestedA->Clicked, [navigateNested]() { navigateNested(0); });
        btnNestedB->Connect(btnNestedB->Clicked, [navigateNested]() { navigateNested(1); });
        btnNestedC->Connect(btnNestedC->Clicked, [navigateNested]() { navigateNested(2); });
    }

    // ---------- 页面4：列表视图 ----------
    auto page4 = std::make_shared<Page>();
    auto grid4 = page4->GetLayoutAs<GridLayout>();
    if (grid4) {
        grid4->SetSpacing(10, 10);

        auto title4 = std::make_shared<Label>(L"列表视图 (ListView)");
        title4->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid4->AddChild(title4, 0, 0, 1, 2);

        auto listView = std::make_shared<ListView>();
        listView->SetHeight(250);
        listView->SetWidth(300);
        for (int i = 1; i <= 50; i++) {
            listView->AddItem(L"列表项 " + std::to_wstring(i));
        }
        listView->SetSelectedIndex(0);

        auto lblListInfo = std::make_shared<Label>(L"当前选中：列表项 1");
        listView->Connect(listView->SelectionChanged, [lblListInfo](int index) {
            if (index >= 0)
                lblListInfo->SetText(L"当前选中：列表项 " + std::to_wstring(index + 1));
            else
                lblListInfo->SetText(L"无选中项");
            });

        auto btnClearList = std::make_shared<Button>(L"清空列表");
        auto btnAddItem = std::make_shared<Button>(L"添加一项");
        btnClearList->Connect(btnClearList->Clicked, [listView]() { listView->Clear(); });
        btnAddItem->Connect(btnAddItem->Clicked, [listView]() {
            static int counter = 51;
            listView->AddItem(L"新增项 " + std::to_wstring(counter++));
            });

        auto listButtons = std::make_shared<RowBox>();
        listButtons->SetSpacing(10);
        listButtons->AddChild(btnAddItem);
        listButtons->AddChild(btnClearList);

        grid4->AddChild(listView, 1, 0);
        grid4->AddChild(lblListInfo, 1, 1);
        grid4->AddChild(listButtons, 2, 0, 1, 2);
        listView->SetUseCache(false);
    }

    // ---------- 页面5：表格视图 ----------
    auto page5 = std::make_shared<Page>();
    auto grid5 = page5->GetLayoutAs<GridLayout>();
    if (grid5) {
        grid5->SetSpacing(10, 10);

        auto title5 = std::make_shared<Label>(L"表格视图 (TableView)");
        title5->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid5->AddChild(title5, 0, 0, 1, 2);

        auto tableView = std::make_shared<TableView>();
        tableView->SetWidth(600);
        tableView->SetHeight(250);
        tableView->SetRowCount(20);
        tableView->SetColumnCount(4);
        tableView->SetHorizontalHeaderLabels({ L"姓名", L"年龄", L"城市", L"备注" });
        tableView->SetColumnWidth(0, 120);
        tableView->SetColumnWidth(1, 80);
        tableView->SetColumnWidth(2, 120);
        tableView->SetColumnWidth(3, 200);

        for (int row = 0; row < 20; row++) {
            tableView->SetItem(row, 0, L"用户 " + std::to_wstring(row + 1));
            tableView->SetItem(row, 1, std::to_wstring(20 + row % 30));
            tableView->SetItem(row, 2, (row % 3 == 0) ? L"北京" : (row % 3 == 1) ? L"上海" : L"广州");
            tableView->SetItem(row, 3, L"备注信息 " + std::to_wstring(row));
        }

        tableView->SetSelectionMode(TableView::SelectionMode::Row);
        tableView->SetCurrentCell(0, 0);

        auto lblTableInfo = std::make_shared<Label>(L"点击单元格查看信息");
        tableView->Connect(tableView->CellClicked, [lblTableInfo](int row, int col) {
            lblTableInfo->SetText(L"选中：行 " + std::to_wstring(row) + L", 列 " + std::to_wstring(col));
            });

        auto btnRowMode = std::make_shared<Button>(L"行选择");
        auto btnColMode = std::make_shared<Button>(L"列选择");
        auto btnCellMode = std::make_shared<Button>(L"单元格选择");
        btnRowMode->Connect(btnRowMode->Clicked, [tableView]() { tableView->SetSelectionMode(TableView::SelectionMode::Row); });
        btnColMode->Connect(btnColMode->Clicked, [tableView]() { tableView->SetSelectionMode(TableView::SelectionMode::Column); });
        btnCellMode->Connect(btnCellMode->Clicked, [tableView]() { tableView->SetSelectionMode(TableView::SelectionMode::Cell); });

        auto modeButtons = std::make_shared<RowBox>();
        modeButtons->SetSpacing(10);
        modeButtons->AddChild(btnRowMode);
        modeButtons->AddChild(btnColMode);
        modeButtons->AddChild(btnCellMode);

        grid5->AddChild(tableView, 1, 0, 1, 2);
        grid5->AddChild(lblTableInfo, 2, 0, 1, 2);
        grid5->AddChild(modeButtons, 3, 0, 1, 2);
        tableView->SetUseCache(false);
    }

    // ---------- 页面6：树形视图 ----------
    auto page6 = std::make_shared<Page>();
    auto grid6 = page6->GetLayoutAs<GridLayout>();
    if (grid6) {
        grid6->SetSpacing(10, 10);

        auto title6 = std::make_shared<Label>(L"树形视图 (TreeView)");
        title6->SetTextColor(Color::FromArgb(255, 40, 40, 40));
        grid6->AddChild(title6, 0, 0, 1, 2);

        auto treeView = std::make_shared<TreeView>();
        treeView->SetWidth(500);
        treeView->SetHeight(300);
        treeView->SetColumnCount(2);
        treeView->SetHeaderLabels({ L"名称", L"类型" });
        treeView->SetColumnWidth(0, 250);
        treeView->SetColumnWidth(1, 150);

        // 构建树结构
        auto root1 = treeView->AddRoot(std::vector<std::wstring>{ L"根节点1", L"文件夹" });
        auto child1_1 = treeView->AddChild(root1, std::vector<std::wstring>{ L"子节点1-1", L"文件" });
        auto child1_2 = treeView->AddChild(root1, std::vector<std::wstring>{ L"子节点1-2", L"文件夹" });
        treeView->AddChild(child1_2, std::vector<std::wstring>{ L"子节点1-2-1", L"文件" });
        treeView->AddChild(child1_2, std::vector<std::wstring>{ L"子节点1-2-2", L"文件" });

        auto root2 = treeView->AddRoot(std::vector<std::wstring>{ L"根节点2", L"文件夹" });
        treeView->AddChild(root2, std::vector<std::wstring>{ L"子节点2-1", L"文件" });

        treeView->SetSelectedNode(root1);
        treeView->ExpandNode(root1, true);
        treeView->ExpandNode(child1_2, true);

        auto lblTreeInfo = std::make_shared<Label>(L"点击节点查看信息");
        treeView->Connect(treeView->SelectionChanged, [lblTreeInfo](std::shared_ptr<TreeNode> node) {
            if (node) {
                std::wstring text = node->columns.size() > 0 ? node->columns[0] : L"";
                lblTreeInfo->SetText(L"选中节点：" + text);
            }
            else {
                lblTreeInfo->SetText(L"无选中节点");
            }
            });
        treeView->Connect(treeView->NodeClicked, [lblTreeInfo](std::shared_ptr<TreeNode> node) {
            if (node) {
                std::wstring text = node->columns.size() > 0 ? node->columns[0] : L"";
                lblTreeInfo->SetText(L"点击节点：" + text);
            }
            });

        auto btnExpandAll = std::make_shared<Button>(L"展开全部");
        auto btnCollapseAll = std::make_shared<Button>(L"折叠全部");
        btnExpandAll->Connect(btnExpandAll->Clicked, [treeView, root1, root2]() {
            treeView->ExpandNodeRecursive(root1, true);
            treeView->ExpandNodeRecursive(root2, true);
            });
        btnCollapseAll->Connect(btnCollapseAll->Clicked, [treeView, root1, root2]() {
            treeView->ExpandNodeRecursive(root1, false);
            treeView->ExpandNodeRecursive(root2, false);
            });

        auto treeButtons = std::make_shared<RowBox>();
        treeButtons->SetSpacing(10);
        treeButtons->AddChild(btnExpandAll);
        treeButtons->AddChild(btnCollapseAll);

        grid6->AddChild(treeView, 1, 0, 1, 2);
        grid6->AddChild(lblTreeInfo, 2, 0, 1, 2);
        grid6->AddChild(treeButtons, 3, 0, 1, 2);
        treeView->SetUseCache(false);
    }

    // 所有页面加入 PageHost
    mainHost->AddPage(page1);
    mainHost->AddPage(page2);
    mainHost->AddPage(page3);
    mainHost->AddPage(page4);
    mainHost->AddPage(page5);
    mainHost->AddPage(page6);

    // 主页面导航：记录当前索引，根据相对位置设置上下方向
    auto currentMainIndex = std::make_shared<int>(0);
    navList->Connect(navList->SelectionChanged, [mainHost, currentMainIndex](int index) {
        int oldIndex = *currentMainIndex;
        if (index > oldIndex) {
            mainHost->SetTransitionDirection(PageHost::TransitionDirection::Up);
        }
        else if (index < oldIndex) {
            mainHost->SetTransitionDirection(PageHost::TransitionDirection::Down);
        }
        *currentMainIndex = index;
        mainHost->NavigateTo(index);
        });

    win.SetMinSize(800, 600);
    win.Run();

    return 0;
}