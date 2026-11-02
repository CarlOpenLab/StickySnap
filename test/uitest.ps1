# StickySnap 自动化 UI 冒烟测试
# 用法: powershell -NoProfile -File uitest.ps1 <输出目录>
# 前置: StickySnap 已以 STICKYSNAP_VISIBLE_CAPTURE=1 启动（截屏能看到遮罩层）
param([Parameter(Mandatory=$true)][string]$OutDir)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Native {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}
"@

[Native]::SetProcessDPIAware() | Out-Null   # 之后全部使用物理像素

function Shot($path) {
  $vs = [System.Windows.Forms.SystemInformation]::VirtualScreen
  $bmp = New-Object System.Drawing.Bitmap $vs.Width, $vs.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($vs.X, $vs.Y, 0, 0, $bmp.Size)
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
}
function Tap($vk) {
  [Native]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [Native]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
}
function Click($x, $y) {
  [Native]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 100
  [Native]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [Native]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Drag($x1, $y1, $x2, $y2) {
  [Native]::SetCursorPos($x1, $y1) | Out-Null; Start-Sleep -Milliseconds 150
  [Native]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
  $steps = 25
  for ($i = 1; $i -le $steps; $i++) {
    $x = $x1 + [int](($x2 - $x1) * $i / $steps)
    $y = $y1 + [int](($y2 - $y1) * $i / $steps)
    [Native]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 16
  }
  Start-Sleep -Milliseconds 150
  [Native]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}

# ---- 场景 1：F1 → 拖框选区 → 工具栏出现 ----
Tap 0x70                        # F1
Start-Sleep -Milliseconds 1500  # 等捕获 + 遮罩
Drag 600 400 1400 900
Start-Sleep -Milliseconds 800
Shot (Join-Path $OutDir "1_overlay_toolbar.png")

# ---- 场景 2：点矩形工具 → 在选区内画一个标注 ----
# 工具栏预期出现在选区右下角下方：tx = 1400-428=972, ty = 910
Click 994 928                   # 第一个工具按钮（矩形）
Start-Sleep -Milliseconds 400
Drag 850 600 1150 700
Start-Sleep -Milliseconds 500
Shot (Join-Path $OutDir "2_annotated.png")

# ---- 场景 3：Enter → 合成标注并贴图 ----
Tap 0x0D                        # Enter
Start-Sleep -Milliseconds 800
Shot (Join-Path $OutDir "3_pinned.png")

Write-Host "uitest done -> $OutDir"
