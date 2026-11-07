# Excel


## 构建与产物

```bat
Excel\build.bat
```

需要 Visual Studio 2022 的 MSVC x64 工具链。

- **产物：`Excel\Excel.exe`**（就在本目录，不再输出到上级）
- 中间对象：`Excel\obj\*.obj`；`sqlite3.obj`、`icon.res` 也留在本目录
- 运行时数据：`settings.ini`、`session\` 都在 exe 同目录
- 编译选项 `/std:c++17 /utf-8 /W3 /O2 /MT /EHsc`。C++17 是必需的，
  跨翻译单元共享状态用 `inline` 变量保证唯一实体
- 第三方依赖：`third_party\sqlite\sqlite3.c / .h`（SQLite amalgamation）
- 资源：`icon.ico` / `icon.rc`

## 目录结构

```
Excel/
├─ Excel.exe              构建产物
├─ settings.ini           运行配置（与 exe 同目录）
├─ build.bat              自洽构建脚本
├─ README.md
├─ icon.ico / icon.rc / icon.res
├─ sqlite3.obj / obj/
├─ third_party/sqlite/    sqlite3.c, sqlite3.h
└─ src/
   ├─ core.h              第 0 层：不可变契约
   ├─ api_pN_*.h          第 1 层：各模块自己的状态与接口（10 份）
   ├─ state.cpp           应用级全局状态的唯一定义处
   ├─ p1_core.cpp         平台：设置、主题、DPI、字体、GDI 原语
   ├─ p2_doc.cpp          文档模型：Cell/Doc/快照、SQLite 持久化、媒体拷贝
   ├─ p3_formula.cpp      公式引擎、样式应用、引用平移
   ├─ p4_image.cpp        WIC 解码、帧动画、图像缓存
   ├─ p5_popup.cpp        菜单弹窗、下拉列表、调色板、已安装字体
   ├─ p6a_grid_paint.cpp  网格度量、排版、滚动条、单元格绘制、HintSpec
   ├─ p6b_grid_input.cpp  编辑、选区、鼠标、键盘、拖动、视口策略
   ├─ p7_chrome.cpp       标题栏/菜单栏/工具栏/标签栏/状态栏控件
   ├─ p8a_commands.cpp    命令、模态对话框、导入、插入、求和、属性
   └─ p8b_app.cpp         菜单构建、UiRoot 编排、窗口过程、消息循环
```
