# capture current screen and crop pin area (ASCII only)
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$vs = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap $vs.Width, $vs.Height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($vs.X, $vs.Y, 0, 0, $bmp.Size)
$out = "C:\Users\73624\.zcode\workspace\draft-bb469cf6-a2b1-4083-ad2a-3453eb6d1d5e\StickySnap\test\out\D_pinned.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
$src = [System.Drawing.Image]::FromFile($out)
$crop = New-Object System.Drawing.Bitmap 600, 200
$g2 = [System.Drawing.Graphics]::FromImage($crop)
$g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g2.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, 600, 200), (New-Object System.Drawing.Rectangle 600, 400, 300, 100), [System.Drawing.Drawing2D.GraphicsUnit]::Pixel)
$crop2Path = "C:\Users\73624\.zcode\workspace\draft-bb469cf6-a2b1-4083-ad2a-3453eb6d1d5e\StickySnap\test\out\crop_pin.png"
$crop.Save($crop2Path, [System.Drawing.Imaging.ImageFormat]::Png)
$g2.Dispose(); $crop.Dispose(); $src.Dispose(); $bmp.Dispose()
Write-Host "pin shot done"
