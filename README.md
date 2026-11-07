<div align="center">

<img src="assets/app-icon.png" width="112" alt="StickySnap logo">

# StickySnap

**把截图钉在桌面上。**<br>
*Pin a screenshot to your desktop.*

Snipaste 式截图 + 贴图工具 · Windows 原生 + macOS Qt 6 预览 · 免费无广告 · 纯本地

[官网](https://carlopenlab.github.io/StickySnap/) · [下载](https://github.com/CarlOpenLab/StickySnap/releases) · [产品文档](docs/PRD-M0.md) · [问题反馈](https://github.com/CarlOpenLab/StickySnap/issues)

[![windows-build](https://github.com/CarlOpenLab/StickySnap/actions/workflows/windows.yml/badge.svg)](https://github.com/CarlOpenLab/StickySnap/actions/workflows/windows.yml)
[![pages](https://github.com/CarlOpenLab/StickySnap/actions/workflows/pages.yml/badge.svg)](https://github.com/CarlOpenLab/StickySnap/actions/workflows/pages.yml)
![platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64%20%7C%20macOS%2014%2B-2F6FED)
![version](https://img.shields.io/badge/version-v0.0.1-2F6FED)
![milestone](https://img.shields.io/badge/milestone-M0-2F6FED)
![runtime](https://img.shields.io/badge/runtime-Win32%20%7C%20Qt%206-4ADE80)
![license](https://img.shields.io/badge/license-MIT-2F6FED)

</div>

---

## 它是什么

StickySnap 是一个把「贴图」当作第一优先级刚需的截图工具。核心循环只有三个动作：

**F1 截图 → Enter 贴到桌面 → F3 把剪贴板贴出来**

贴图是常驻的桌面工作台：可拖动、缩放、调透明度、鼠标穿透，重启后自动恢复。所有处理都在本地完成，不联网、无广告、无账号。

> English: StickySnap is a Snipaste-style screenshot and pin tool. The Windows build is native C++17/Win32; the cross-platform build uses C++20 and Qt 6. Press `F1` to capture with window snapping, `Enter` to pin to the desktop, `F3` to pin the clipboard. Pins persist across reboots. Local-only, free, no ads.

## 平台与状态

| 平台 | 实现 | 状态 |
|---|---|---|
| Windows 10/11 x64 | C++17 / Win32 | ✅ M0 主力版本，功能最完整 |
| macOS 14+ | C++20 / Qt 6.5+ | 🚧 Qt 预览版，目前从源码构建 |
| Linux | Qt 6 | ⬜ 规划中 |

Qt 预览版与 Windows 版共享同一套选区、标注、设置和会话模型（`portable/src/core`）；Windows 原生版仍是参考实现。

## 当前状态：M0

| 能力 | Windows 原生 | macOS Qt 预览 |
|---|---|---|
| 全局热键截图 / 贴剪贴板 | ✅ 默认 `F1` / `F3`，可配置组合键 | ✅ `⌃⌘A` / `F3`，可在设置中修改 |
| 全屏冻结遮罩选区（多显示器、HiDPI） | ✅ | ✅ |
| 窗口检测高亮（单击吸附） | ✅ | ✅ |
| 放大镜 + 十六进制取色 + 尺寸提示 | ✅ | ✅ |
| Enter/双击贴图，复制、取消、选区微调 | ✅ | ✅ |
| 贴图窗：置顶、拖动、滚轮缩放、透明度、穿透 | ✅ | ✅ |
| 贴图常显边框：1px 黑白双层描边，悬停变品牌色 | ✅ | — |
| 贴图右键菜单：穿透 / 复制 / 另存为 / 关闭 | ✅ | ✅ |
| 贴图不进入后续截图 | ✅ | — |
| 标注工具栏（6 种工具 + 撤销 + 4 个动作） | ✅ | ✅ |
| 标注引擎：矩形/椭圆/箭头/画笔/荧光笔/马赛克，9 色 × 3 粗细，Ctrl+Z 撤销 | ✅ | ✅ |
| 会话持久化（贴图图片 + 位置自动恢复） | ✅ | ✅ |
| 托盘：左键单击直接截图，右键菜单 | ✅ | ✅（macOS 菜单栏） |
| 设置对话框（强调色 / 热键 / 遮罩浓度 / 贴图边框 / 开机自启） | ✅ | — |
| 开机自启（写当前用户启动项） | ✅ | — |
| 单元测试（CTest）+ CI 跑测试 | ✅ | ✅（Qt Test） |
| GIF/APNG 贴图、贴图上标注、标注二次编辑、UI 元素级吸附 | ⬜ M1 | ⬜ M1 |

## 和常见工具对比

只看「贴图」这条主线和几个关键能力，数据来自各家公开文档。**「—」表示未确认或该项目不适用**；竞品能力以实际版本为准，本表不列版本号。

| 维度 | StickySnap | Snipaste | PixPin | ShareX | CleanShot X |
|---|---|---|---|---|---|
| 贴图持久化（重启恢复） | ✅ | ❌ | ❌ | ❌ | ❌ |
| 贴图不进截图 | ✅ | — | — | — | — |
| 点穿（鼠标穿透） | ✅ | ✅ | ✅ | — | — |
| 长截图 | ❌ | ❌ | ✅ | ✅ | ✅ |
| OCR | ❌ | — | ✅ | — | ✅ |
| 平台 | Windows（macOS 预览） | Windows / macOS / Linux | Windows / macOS | Windows | macOS |
| 价格 | 免费（MIT 开源） | 免费 / 专业版付费 | 免费 / 会员增值 | 免费开源 | 付费 |

StickySnap 的取舍很明确：**贴图优先**。长截图、OCR 这类能力还没做，就不写进功能表——本仓库只承诺已经能用起来的东西。贴图「重启后还在」「不进截图」「可点穿」三件事，是目前和上面这些工具拉开差距的地方。

## 下载

- **Windows 体验版**：[Releases](https://github.com/CarlOpenLab/StickySnap/releases)（推送 `v*` tag 后由 CI 自动构建并发布）
- **Windows CI 产物**：[windows-build](https://github.com/CarlOpenLab/StickySnap/actions/workflows/windows.yml) 每次 push 都会上传 `StickySnap-windows-x64.zip`（需登录 GitHub 下载）
- **macOS Qt 预览版**：目前从源码构建，见下方「macOS（Qt 6）」。尚未提供签名/公证的安装包。

Windows 可执行文件未做代码签名，首次运行若被 SmartScreen 拦截，选择「更多信息 → 仍要运行」。macOS 首次截图需要在「系统设置 → 隐私与安全性 → 屏幕录制」中授权 StickySnap。

## 使用

1. 运行 `StickySnap.exe`（Windows）或 `StickySnap.app`（macOS），无主窗口，托盘/菜单栏出现图标
2. **左键单击托盘图标 = 直接截图**；右键菜单：取消所有穿透 / 设置 / 开机自启 / 打开日志 / 关于
3. **F1**（默认热键，可在设置中修改；macOS 为 **⌃⌘A**）：进入截图 → 拖拽选区（或单击高亮窗口）→ **Enter** 贴图 / **Ctrl+C**（macOS **⌘C**）复制 / **Esc** 取消
4. **F3**（默认热键，可修改）：把剪贴板中的图片直接贴上桌面
5. 贴图上：拖动移动、滚轮缩放、**Ctrl+滚轮**（macOS **⌘+滚轮**）调透明度、右键菜单、双击关闭；贴图常显 1px 黑白双层边框，鼠标悬停时变品牌色
6. 重启后贴图自动恢复

### 快捷键

| 按键（Windows） | 按键（macOS） | 作用 |
|---|---|---|
| `F1`（默认） | `⌃⌘A` | 截图（拖拽选区 / 单击吸附窗口） |
| `Enter` / 双击 | `Enter` / 双击 | 把当前选区贴到桌面 |
| `Ctrl+C` | `⌘C` | 复制选区（含标注）到剪贴板 |
| `Ctrl+Z` | `⌘Z` | 撤销上一笔标注 |
| `Esc` | `Esc` | 取消截图 |
| `F3`（默认） | `F3` | 把剪贴板图片贴到桌面 |
| `Ctrl+滚轮` | `⌘+滚轮` | 调整贴图透明度 |

两个全局热键都能在「设置」里改成组合键，写法如 `Ctrl+Alt+A`、`Shift+F1`、`Shift+Win+S`。修饰键统一按 **Ctrl → Alt → Shift → Win** 的顺序显示与解析，键名大小写不敏感；非法值回退默认。

macOS 上功能键若被系统占用，可在「系统设置 → 键盘 → 键盘快捷键」中调整，或在 StickySnap 设置里改成其它组合键。

### 配置

配置文件为 `%APPDATA%\StickySnap\config.ini`，首次运行自动生成；也可以在托盘右键 →「设置」里改，保存后写回该文件。

| 键 | 默认值 | 说明 |
|---|---|---|
| `accent` | `#2F6FED` | 界面强调色（`#RRGGBB`），用于工具栏/标注的激活色等界面元素 |
| `hotkey_shot` | `F1` | 截图热键，支持 `Ctrl+Alt+A` 形式的组合键 |
| `hotkey_pin` | `F3` | 贴剪贴板热键，写法同上 |
| `dim_opacity` | `55` | 截图遮罩变暗百分比，范围 10–90 |
| `pin_border` | `1` | 贴图常显边框开关（`1` 开 / `0` 关），默认开启 |
| `autostart` | `0` | 开机自启（写当前用户启动项），默认关闭 |

```ini
# %APPDATA%\StickySnap\config.ini（# 开头为注释）
accent=#2F6FED
hotkey_shot=Ctrl+Alt+A
hotkey_pin=F3
dim_opacity=55
pin_border=1
autostart=0
```

## 构建

### Windows（MSVC）

```bat
build-local.bat        :: 自动探测 Visual Studio（vswhere / 常见安装路径），产物 build\StickySnap.exe
```

### Windows（CMake / MinGW-w64）

```bat
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure   :: 单元测试
```

MinGW 手动编译：

```bat
windres res/app.rc -O coff -o app_res.o
g++ -std=c++17 -O2 -municode -mwindows src/*.cpp app_res.o -o StickySnap.exe ^
  -luser32 -lgdi32 -lshell32 -ldwmapi -lmsimg32 -lcomdlg32 -lgdiplus -lole32
```

### macOS（Qt 6）

需要 macOS 14+、Xcode Command Line Tools、CMake 3.21+ 和 Qt 6.5+：

```bash
brew install qt cmake
cmake -S portable -B build-qt -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-qt -j
ctest --test-dir build-qt --output-on-failure
open build-qt/StickySnap.app
```

调试时可用 `--capture` 和 `--pin-clipboard` 参数直接触发截图/贴剪贴板。

### CI

| 工作流 | 触发 | 作用 |
|---|---|---|
| [`windows.yml`](.github/workflows/windows.yml) | push / PR / 手动 | 构建 Windows 版、跑 `ctest` 单元测试并上传 zip artifact |
| [`release.yml`](.github/workflows/release.yml) | 推送 `v*` tag | 构建并发布 GitHub Release |
| [`pages.yml`](.github/workflows/pages.yml) | `site/` 变更 / 手动 | 部署落地页到 GitHub Pages |

## 架构

```
src/                 Windows 原生版（Win32）
  main.cpp           入口：单实例互斥、Per-Monitor-V2 DPI、COM/GDI+ 初始化
  app.{h,cpp}        主窗口（隐藏）、热键调度、模块编排
  capture.{h,cpp}    虚拟屏幕一次性捕获（GDI BitBlt，32bpp top-down DIB）
  overlay.{h,cpp}    冻结遮罩选区窗（窗口检测/放大镜/选区标注）
  toolbar.{h,cpp}    选区旁图标工具栏
  pin.{h,cpp}        贴图窗（layered window：缩放/透明度/穿透/边框/菜单）
  pinmanager.{h,cpp} 贴图集合管理 + 延迟保存
  session.h          会话读写（index.txt，崩溃安全：tmp + MoveFileEx 替换）
  clipboard.{h,cpp}  CF_DIB + PNG 双格式剪贴板图片读写
  tray.{h,cpp}       托盘图标与菜单
  settings.h         用户设置（config.ini 读写、热键文本格式化）
  settings_dialog.{h,cpp}  设置对话框（强调色/热键/遮罩浓度/贴图边框/开机自启）
  gdiutil.h          GDI+ 封装（PNG/JPEG 编解码）
  config.h           常量（类名/热键/缩放参数/菜单 ID/品牌色）
  util.h             DPI、单实例、%APPDATA%、ExcludeFromCapture 等

portable/            Qt 6 跨平台版
  src/core/          选区模型、标注模型、设置、会话（与 UI 无关）
  src/ui/            Qt 遮罩窗、工具栏、贴图窗、托盘、设置对话框
  src/platform/      平台抽象；macOS 已实现（ScreenCaptureKit / Carbon 热键）
  tests/             Qt Test 单元测试与离屏渲染测试
```

两个关键设计：

- 坐标一律使用**物理像素**（Windows 版声明 Per-Monitor-V2 aware），多屏与 HiDPI 下不串位
- 贴图图片在创建时即落盘，会话文件只存引用——进程崩溃也不丢贴图

## 路线图

- **M0（当前）**：贴图成立的最小集 + M0.6 打磨（边框、热键组合、设置、自启、单测），见上表
- **M1**：贴图标注、GIF/APNG 动图贴图、文本/颜色贴图、贴图分组、UI 元素级检测（UI Automation）、OCR（Windows.Media.Ocr）、macOS Qt 预览与 Windows 功能对齐
- **M2**：虚拟桌面绑定、Solo 模式、批量操作、AI 功能（按 PRD）、Linux（Qt 6）
- **平台**：Windows ✅ → macOS 🚧（Qt 预览）→ Linux ⬜

详细验收标准见 [docs/PRD-M0.md](docs/PRD-M0.md)。

## 项目结构

```
.github/workflows/   CI：Windows 构建、Release、Pages
assets/              品牌源图与生成的应用图标
site/                落地页（GitHub Pages 源目录）
src/                 Windows 原生版源码
portable/            Qt 6 跨平台版（macOS 平台层已实现）
res/                 Windows 图标与工具栏资源
docs/                产品文档
test/                手工测试脚本、参考图与 CTest 单元测试
tools/               图标与品牌资源生成脚本
```

品牌资源统一由 `python3 tools/make_brand_assets.py` 从 `assets/logo.png` 生成。

## 反馈

遇到问题或有功能建议，欢迎开 [Issue](https://github.com/CarlOpenLab/StickySnap/issues)。反馈截图类问题时，附上系统版本、显示器缩放比例和复现步骤会很有帮助。

## 许可

本项目以 [MIT License](LICENSE) 开源，Copyright (c) 2026 CarlOpenLab。

你可以自由使用、修改、分发本项目（包括商业用途），只需保留版权声明与许可文本。项目按「现状」提供，不附带任何明示或暗示的担保。
