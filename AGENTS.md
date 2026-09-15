# AGENTS / 开发原则

## 协作流程规则（必须遵守）

- **提交/推送前必须先经用户同意**：不要自动 `git commit` / `git push`（含 Gitee/GitHub/gh-pages），等到用户明确说“提交/推送”再执行。
- **不要自动更新文档**：API 文档、README、更新日志等，只有用户明确指定时才更新；不要顺手自动改。
- 每次提交信息要如实描述本次改动。

## Bug 处理原则（最高优先级）

遇到任何 bug（无论轻重缓急），**先定位根源，再从根源修复**：

1. 绝不允许用“新机制”让 bug 无法触发，来掩盖问题（那是掩盖，不是修复）。
2. 如果根源确实在外部（Windows API / 系统 / 第三方库），才考虑绕过它，并在文档中明确标注“这是对外部问题的规避”。
3. 如果根源在本项目内部代码，必须**找到它并从根源修复**。
4. 如果找不到根源：**如实说明“找不到，未定位”**，交由用户（或用户的其他分析）定位；找到后再修复。**绝不猜测、绝不用“最可疑”下结论、绝不靠引入机制来掩盖。**

## 依赖与约束（避免埋雷）

- `UIElement::GetChildren()` 返回的是**元素内部缓冲的引用**，其有效期为“该元素的子列表或可视状态发生变化 / 再次调用该元素的 `GetChildren()` 之前”。遍历期间**不得对同一个元素再次调用 `GetChildren()`**（容器递归自己子元素时用的是各自独立的缓冲，安全）。
- 元素内部用**窗口 id**（不是裸指针）记录所属窗口；窗口销毁后 `GetWindow()` 返回 `nullptr`。
- `UIElement::Connect(signal, slot)` **不返回** `Connection`；连接登记在元素的 `ConnectionGroup`，随元素析构自动断开。需要手动断开请直接用 `signal.connect(...)`。
- 所有与窗口相关的全局信号（`UIZSignals::*`）都携带 `Window*`；订阅者必须用 `GetWindow()` 过滤，禁止跨窗口处理。
- 数据视图（List/Table/Tree）的按项/按行元数据要么绑定到项目对象、要么在排序/增删时重映射，禁止用会失效的下标或裸指针悬挂。

## 构建

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" "C:\project\ZUI\ZUI.vcxproj" /p:Configuration=Release /p:Platform=x64 /m /nologo /v:minimal
```
（构建前先 `Stop-Process -Name ZUI -Force`，否则会因文件占用链接失败。）
