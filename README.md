<div align="center">

# StickySnap

**把截图钉在桌面上。**<br>
*Pin a screenshot to your desktop.*

Snipaste 式截图 + 贴图工具 · Windows 原生 · C++/Win32 · 免费无广告 · 纯本地

[官网](https://carlopenlab.github.io/StickySnap/) · [下载](https://github.com/CarlOpenLab/StickySnap/releases) · [产品文档](docs/PRD-M0.md) · [问题反馈](https://github.com/CarlOpenLab/StickySnap/issues)

[![windows-build](https://github.com/CarlOpenLab/StickySnap/actions/workflows/windows.yml/badge.svg)](https://github.com/CarlOpenLab/StickySnap/actions/workflows/windows.yml)
[![pages](https://github.com/CarlOpenLab/StickySnap/actions/workflows/pages.yml/badge.svg)](https://github.com/CarlOpenLab/StickySnap/actions/workflows/pages.yml)
![platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D6)
![milestone](https://img.shields.io/badge/milestone-M0-FF4B3E)
![dependencies](https://img.shields.io/badge/runtime-none%20beyond%20Win32-4ADE80)

</div>

---

## 它是什么

StickySnap 是一个把「贴图」当作第一优先级刚需的 Windows 截图工具。核心循环只有三个动作：

**F1 截图 → Enter 贴到桌面 → F3 把剪贴板贴出来**

贴图是常驻的桌面工作台：可拖动、缩放、调透明度、鼠标穿透，重启后自动恢复。所有处理都在本地完成，不联网、无广告、无账号。

> English: StickySnap is a Snipaste-style screenshot and pin tool for Windows, built with C++17 and Win32. Press `F1` to capture with window snapping, `Enter` to pin to the desktop, `F3` to pin the clipboard. Pins persist across reboots. Local-only, free, no ads.

## 当前状态：M0

| 能力 | 状态 |
|---|---|
| 全局热键 F1 截图 / F3 贴剪贴板 | ✅ |
| 全屏冻结遮罩选区（多显示器、HiDPI 物理像素） | ✅ |
| 窗口检测高亮（z-order 枚举，跳过贴图/桌面/隐身窗） | ✅ |
| 放大镜 + 十六进制取色 + 尺寸提示 | ✅ |
| Enter/双击 = 贴图，Ctrl+C = 复制，Esc = 取消，单击 = 选中窗口 | ✅ |
| 贴图窗：置顶、拖动、滚轮缩放、Ctrl+滚轮透明度、双击关闭 | ✅ |
| 贴图右键菜单：鼠标穿透 / 复制 / 另存为 / 关闭 | ✅ |
| 贴图不进入后续截图（`WDA_EXCLUDEFROMCAPTURE`） | ✅ |
| Snipaste 式图标工具栏（6 种标注工具 + 撤销 + 贴图/复制/保存/取消） | ✅ |
| 标注引擎：矩形/椭圆/箭头/画笔/荧光笔/马赛克，9 色 × 3 粗细，Ctrl+Z 撤销 | ✅ |
| 贴图/复制/保存输出合成标注后的图像 | ✅ |
| 会话持久化（`%APPDATA%\StickySnap\index.txt` + `pins\*.png`） | ✅ |
| 托盘菜单 + explorer 重启自动恢复图标 | ✅ |
| GIF/APNG 贴图、贴图上标注、标注二次编辑、UI 元素级吸附（MSAA/UIA） | ⬜ M1 |

## 下载

- **体验版**：[Releases](https://github.com/CarlOpenLab/StickySnap/releases)（推送 `v*` tag 后由 CI 自动构建并发布）
- **CI 产物**：[windows-build](https://github.com/CarlOpenLab/StickySnap/actions/workflows/windows.yml) 每次 push 都会上传 `StickySnap-windows-x64.zip`（需登录 GitHub 下载）

当前可执行文件未做代码签名，首次运行若被 SmartScreen 拦截，选择「更多信息 → 仍要运行」。仅支持 Windows 10/11 x64。

## 使用

1. 运行 `StickySnap.exe`（无主窗口，托盘出现图标）
2. **F1**：进入截图 → 拖拽选区（或单击高亮窗口）→ **Enter** 贴图 / **Ctrl+C** 复制 / **Esc** 取消
3. **F3**：把剪贴板中的图片直接贴上桌面
4. 贴图上：拖动移动、滚轮缩放、**Ctrl+滚轮** 调透明度、右键菜单、双击关闭
5. 重启后贴图自动恢复

### 快捷键

| 按键 | 作用 |
|---|---|
| `F1` | 截图（拖拽选区 / 单击吸附窗口） |
| `Enter` / 双击 | 把当前选区贴到桌面 |
| `Ctrl+C` | 复制选区（含标注）到剪贴板 |
| `Ctrl+Z` | 撤销上一笔标注 |
| `Esc` | 取消截图 |
| `F3` | 把剪贴板图片贴到桌面 |
| `Ctrl+滚轮` | 调整贴图透明度 |

## 构建

### 本地（MSVC）

```bat
build-local.bat        :: 需要 VS Build Tools（vcvars64），产物 build\StickySnap.exe
```

### CMake（MSVC 或 MinGW-w64）

```bat
cmake -S . -B build
cmake --build build --config Release
```

MinGW 手动编译：

```bat
windres res/app.rc -O coff -o app_res.o
g++ -std=c++17 -O2 -municode -mwindows src/*.cpp app_res.o -o StickySnap.exe ^
  -luser32 -lgdi32 -lshell32 -ldwmapi -lmsimg32 -lcomdlg32 -lgdiplus -lole32
```

### CI

| 工作流 | 触发 | 作用 |
|---|---|---|
| [`windows.yml`](.github/workflows/windows.yml) | push / PR / 手动 | 构建 Windows 版并上传 zip artifact |
| [`release.yml`](.github/workflows/release.yml) | 推送 `v*` tag | 构建并发布 GitHub Release |
| [`pages.yml`](.github/workflows/pages.yml) | `site/` 变更 / 手动 | 部署落地页到 GitHub Pages |

## 架构

```
src/
  main.cpp          入口：单实例互斥、Per-Monitor-V2 DPI、COM/GDI+ 初始化
  app.{h,cpp}       主窗口（隐藏）、热键调度、模块编排
  capture.{h,cpp}   虚拟屏幕一次性捕获（GDI BitBlt，32bpp top-down DIB）
  overlay.{h,cpp}   冻结遮罩选区窗（窗口检测/放大镜/选区标注）
  toolbar.{h,cpp}   选区旁图标工具栏
  pin.{h,cpp}       贴图窗（layered window：缩放/透明度/穿透/菜单）
  pinmanager.{h,cpp} 贴图集合管理 + 延迟保存
  session.h         会话读写（index.txt，崩溃安全：tmp + MoveFileEx 替换）
  clipboard.{h,cpp} CF_DIB + PNG 双格式剪贴板图片读写
  tray.{h,cpp}      托盘图标与菜单
  settings.h        用户设置
  gdiutil.h         GDI+ 封装（PNG/JPEG 编解码）
  config.h          常量（类名/热键/缩放参数/菜单 ID）
  util.h            DPI、单实例、%APPDATA%、ExcludeFromCapture 等
```

两个关键设计：

- 坐标一律使用**物理像素**（进程声明 Per-Monitor-V2 aware），多屏与 HiDPI 下不串位
- 贴图图片在创建时即落盘，会话文件只存引用——进程崩溃也不丢贴图

## 路线图

- **M0（当前）**：贴图成立的最小集，见上表
- **M1**：贴图标注、GIF/APNG 动图贴图、文本/颜色贴图、贴图分组、热键自定义、UI 元素级检测（UI Automation）、OCR（Windows.Media.Ocr）
- **M2**：虚拟桌面绑定、Solo 模式、批量操作、AI 功能（按 PRD）
- **平台**：Windows ✅ → macOS（Swift/AppKit + NSPanel）→ Linux（X11 满配 / KDE+wlroots layer-shell / GNOME 降级）

详细验收标准见 [docs/PRD-M0.md](docs/PRD-M0.md)。

## 项目结构

```
.github/workflows/   CI：Windows 构建、Release、Pages
site/                落地页（GitHub Pages 源目录）
src/                 应用源码
res/                 图标与工具栏资源
docs/                产品文档
test/                手工测试与截图脚本
tools/               图标生成脚本
```

## 反馈

遇到问题或有功能建议，欢迎开 [Issue](https://github.com/CarlOpenLab/StickySnap/issues)。反馈截图类问题时，附上 Windows 版本、显示器缩放比例和复现步骤会很有帮助。

## 许可

当前仓库尚未添加开源许可证文件；在补充 `LICENSE` 之前，保留所有权利。
