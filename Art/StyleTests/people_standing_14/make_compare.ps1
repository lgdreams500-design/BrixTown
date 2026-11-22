# One standing person: the reference figure beside the slice's, under the reference's own light (index.html?studio=1), before and after
# people's fill light was matched to it. The studio captures are screenshots of Prototype/play taken on 2026-11-22.
#   powershell -NoProfile -ExecutionPolicy Bypass -File Art/StyleTests/people_standing_14/make_compare.ps1
Add-Type -AssemblyName System.Drawing
$here = $PSScriptRoot; $H = 520; $top = 34; $gap = 12
$items = @(@("reference_player.png", 0, 30, 250, 500, "Reference"), @("studio_1_before.jpg", 290, 105, 220, 440, "Slice, same light: before"), @("studio_2_after.jpg", 290, 105, 220, 440, "Slice, same light: after"))
$parts = @(); foreach ($it in $items) { $img = [System.Drawing.Image]::FromFile((Join-Path $here $it[0])); $b = New-Object System.Drawing.Bitmap ([int]$it[3]), ([int]$it[4]); $g = [System.Drawing.Graphics]::FromImage($b)
  $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $it[3], $it[4]), (New-Object System.Drawing.Rectangle $it[1], $it[2], $it[3], $it[4]), [System.Drawing.GraphicsUnit]::Pixel); $g.Dispose(); $img.Dispose(); $parts += ,$b }
$ws = @(); foreach ($p in $parts) { $ws += [int]($p.Width * $H / $p.Height) }
$bmp = New-Object System.Drawing.Bitmap ([int](($ws | Measure-Object -Sum).Sum + $gap * 2)), ($H + $top)
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic; $g.Clear([System.Drawing.Color]::FromArgb(28, 27, 31))
$font = New-Object System.Drawing.Font "Segoe UI", 12, ([System.Drawing.FontStyle]::Bold); $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 226, 214))
$x = 0; for ($k = 0; $k -lt $parts.Count; $k++) { $g.DrawImage($parts[$k], $x, $top, $ws[$k], $H); $g.DrawString($items[$k][5], $font, $brush, ($x + 4), 6); $x += $ws[$k] + $gap }
$bmp.Save((Join-Path $here "standing_reference_before_after.png"), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose(); foreach ($p in $parts) { $p.Dispose() }; "wrote standing_reference_before_after.png"
