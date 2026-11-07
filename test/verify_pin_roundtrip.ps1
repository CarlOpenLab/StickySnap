param(
  [switch]$Pin,
  [switch]$Quit,
  [switch]$Inspect
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class S {
  public delegate bool Proc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(Proc p, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool OpenClipboard(IntPtr h);
  [DllImport("user32.dll")] public static extern bool CloseClipboard();
  [DllImport("user32.dll")] public static extern bool EmptyClipboard();
  [DllImport("user32.dll")] public static extern IntPtr SetClipboardData(uint fmt, IntPtr mem);
  [DllImport("kernel32.dll")] public static extern IntPtr GlobalAlloc(uint flags, UIntPtr bytes);
  [DllImport("kernel32.dll")] public static extern IntPtr GlobalLock(IntPtr mem);
  [DllImport("kernel32.dll")] public static extern bool GlobalUnlock(IntPtr mem);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

$proc = Get-Process StickySnap -ErrorAction SilentlyContinue
if (-not $proc) { "StickySnap is not running"; exit 1 }
$target = $proc.Id

function Get-Windows([string]$cls) {
  $script:found = @()
  $cb = [S+Proc]{
    param($h, $l)
    $procId = 0
    [void][S]::GetWindowThreadProcessId($h, [ref]$procId)
    if ($procId -eq $target) {
      $sb = New-Object System.Text.StringBuilder 256
      [void][S]::GetClassName($h, $sb, 256)
      if ($sb.ToString() -eq $cls) { $script:found += $h }
    }
    return $true
  }
  [void][S]::EnumWindows($cb, [IntPtr]::Zero)
  return $script:found
}

$main = @(Get-Windows 'StickySnap.Main')
if ($main.Count -eq 0) { "main window not found"; exit 1 }
$main = $main[0]

if ($Pin) {
  $w = 200; $hgt = 120
  $bmp = New-Object System.Drawing.Bitmap $w, $hgt
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.Clear([System.Drawing.Color]::Orange)
  $g.FillEllipse([System.Drawing.Brushes]::Navy, 30, 30, 80, 50)
  $g.Dispose()
  $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $hgt
  $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $px = New-Object byte[] ($data.Stride * $hgt)
  [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $px, 0, $px.Length)
  $bmp.UnlockBits($data)
  $bmp.Dispose()

  $size = 40 + $px.Length
  $hMem = [S]::GlobalAlloc(0x0002, [UIntPtr]::new([uint64]$size))
  $p = [S]::GlobalLock($hMem)
  $header = New-Object byte[] 40
  [BitConverter]::GetBytes([UInt32]40).CopyTo($header, 0)
  [BitConverter]::GetBytes([Int32]$w).CopyTo($header, 4)
  [BitConverter]::GetBytes([Int32](-$hgt)).CopyTo($header, 8)
  [BitConverter]::GetBytes([UInt16]1).CopyTo($header, 12)
  [BitConverter]::GetBytes([UInt16]32).CopyTo($header, 14)
  [BitConverter]::GetBytes([UInt32]$px.Length).CopyTo($header, 20)
  [System.Runtime.InteropServices.Marshal]::Copy($header, 0, $p, 40)
  [System.Runtime.InteropServices.Marshal]::Copy($px, 0, [IntPtr]::Add($p, 40), $px.Length)
  [void][S]::GlobalUnlock($hMem)
  if (-not [S]::OpenClipboard([IntPtr]::Zero)) { "open clipboard failed"; exit 2 }
  [void][S]::EmptyClipboard()
  [void][S]::SetClipboardData(8, $hMem)
  [void][S]::CloseClipboard()
  [void][S]::SendMessage($main, 0x0312, [IntPtr]2, [IntPtr]::Zero)  # WM_HOTKEY pin
  Start-Sleep -Milliseconds 1600
  "pinned; pin windows: " + @(Get-Windows 'StickySnap.Pin').Count
}

if ($Inspect) {
  $pins = @(Get-Windows 'StickySnap.Pin')
  "pin windows: $($pins.Count)"
  $i = 0
  foreach ($h in $pins) {
    $r = New-Object S+RECT
    [void][S]::GetWindowRect($h, [ref]$r)
    $w = $r.Right - $r.Left; $hgt = $r.Bottom - $r.Top
    "pin[$i] rect=($($r.Left),$($r.Top)) size=${w}x${hgt}"
    $shot = New-Object System.Drawing.Bitmap $w, $hgt
    $g = [System.Drawing.Graphics]::FromImage($shot)
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size $w, $hgt))
    $g.Dispose()
    foreach ($pt in @(@(0,0),@(1,1),@(2,2),@(($w-1),($hgt-1)),@([int]($w/2),0),@([int]($w/2),1))) {
      $c = $shot.GetPixel($pt[0], $pt[1])
      "  px($($pt[0]),$($pt[1])) = R$($c.R) G$($c.G) B$($c.B)"
    }
    $shot.Dispose()
    $i++
  }
}

if ($Quit) {
  [void][S]::SendMessage($main, 0x0111, [IntPtr]103, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 1500
  "quit sent; remaining process: " + @(Get-Process StickySnap -ErrorAction SilentlyContinue).Count
}
