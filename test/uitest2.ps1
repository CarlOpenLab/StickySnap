# StickySnap step-by-step UI test (ASCII only; PowerShell 5.1 safe)
$OutDir = "C:\Users\73624\.zcode\workspace\draft-bb469cf6-a2b1-4083-ad2a-3453eb6d1d5e\StickySnap\test\out"
Write-Host "OutDir=$OutDir"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir | Out-Null }
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Native2 {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}
"@
[Native2]::SetProcessDPIAware() | Out-Null
function Shot($path) {
  $vs = [System.Windows.Forms.SystemInformation]::VirtualScreen
  $bmp = New-Object System.Drawing.Bitmap $vs.Width, $vs.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($vs.X, $vs.Y, 0, 0, $bmp.Size)
  $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
}
function Tap($vk) {
  [Native2]::keybd_event($vk, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60
  [Native2]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
}
function Click($x, $y) {
  [Native2]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 120
  [Native2]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
  [Native2]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Drag($x1, $y1, $x2, $y2) {
  [Native2]::SetCursorPos($x1, $y1) | Out-Null; Start-Sleep -Milliseconds 150
  [Native2]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
  for ($i = 1; $i -le 25; $i++) {
    $x = $x1 + [int](($x2 - $x1) * $i / 25); $y = $y1 + [int](($y2 - $y1) * $i / 25)
    [Native2]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 16
  }
  Start-Sleep -Milliseconds 150
  [Native2]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
Tap 0x70
Start-Sleep -Milliseconds 1500
Drag 600 400 1400 900
Start-Sleep -Milliseconds 800
Shot (Join-Path $OutDir "A_toolbar.png")
Click 994 928
Start-Sleep -Milliseconds 600
Shot (Join-Path $OutDir "B_after_click.png")
Drag 850 600 1150 700
Start-Sleep -Milliseconds 600
Shot (Join-Path $OutDir "C_after_draw.png")
Write-Host "step test done"
