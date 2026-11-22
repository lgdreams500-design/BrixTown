# The reference's neutral face beside the slice's two faces (index.html?studio=1&face=1 and ?studio=2&face=1), 2026-11-22.
Add-Type -AssemblyName System.Drawing
$here = $PSScriptRoot; $H = 400; $top = 34; $gap = 12
$items = @(@("reference_face.png", "Reference (face_expressions.png)"), @("face_after_player.png", "Slice: first player"), @("face_after_second.png", "Slice: second player"))
$imgs = @(); foreach ($it in $items) { $imgs += ,([System.Drawing.Image]::FromFile((Join-Path $here $it[0]))) }
$ws = @(); foreach ($i in $imgs) { $ws += [int]($i.Width * $H / $i.Height) }
$bmp = New-Object System.Drawing.Bitmap ([int](($ws | Measure-Object -Sum).Sum + $gap * 2)), ($H + $top)
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic; $g.Clear([System.Drawing.Color]::FromArgb(28, 27, 31))
$font = New-Object System.Drawing.Font "Segoe UI", 12, ([System.Drawing.FontStyle]::Bold); $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 226, 214))
$x = 0; for ($k = 0; $k -lt $imgs.Count; $k++) { $g.DrawImage($imgs[$k], $x, $top, $ws[$k], $H); $g.DrawString($items[$k][1], $font, $brush, ($x + 4), 6); $x += $ws[$k] + $gap }
$bmp.Save((Join-Path $here "faces_reference_and_slice.png"), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose(); foreach ($i in $imgs) { $i.Dispose() }; "wrote faces_reference_and_slice.png"
