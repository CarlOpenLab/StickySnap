# extract sample frames from flameshot.gif (ASCII only)
Add-Type -AssemblyName System.Drawing
$gif = [System.Drawing.Image]::FromFile("C:\Users\73624\.zcode\workspace\draft-bb469cf6-a2b1-4083-ad2a-3453eb6d1d5e\StickySnap\test\ref\flameshot.gif")
$fd = New-Object System.Drawing.Imaging.FrameDimension $gif.FrameDimensionsList[0]
$n = $gif.GetFrameCount($fd)
Write-Host "frames: $n"
$picks = @(0, 100, 200, 300, ($n - 1))
foreach ($i in $picks) {
  if ($i -lt 0 -or $i -ge $n) { continue }
  $gif.SelectActiveFrame($fd, $i) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap $gif.Width, $gif.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.DrawImage($gif, 0, 0, $gif.Width, $gif.Height)
  $out = "C:\Users\73624\.zcode\workspace\draft-bb469cf6-a2b1-4083-ad2a-3453eb6d1d5e\StickySnap\test\ref\frame_$i.png"
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  Write-Host "saved $out"
  $g.Dispose(); $bmp.Dispose()
}
$gif.Dispose()
