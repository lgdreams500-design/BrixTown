# People comparison 13: the reference lineup above the same six people in the slice (index.html?lineup=1).
#   powershell -NoProfile -ExecutionPolicy Bypass -File Art/StyleTests/people_compare_13/make_compare.ps1
# slice_lineup_after.png is a screenshot of Prototype/play taken on 2026-11-22; retake it after people.js changes.
Add-Type -AssemblyName System.Drawing
$here = $PSScriptRoot
$a = [System.Drawing.Image]::FromFile((Join-Path $here "..\..\Reference\character_lineup.png"))
$b = [System.Drawing.Image]::FromFile((Join-Path $here "slice_lineup_after.png"))
$W = 1400; $top = 34; $gap = 12
$ha = [int]($a.Height * $W / $a.Width); $hb = [int]($b.Height * $W / $b.Width)
$bmp = New-Object System.Drawing.Bitmap $W, ($ha + $hb + 2 * $top + $gap)
$g = [System.Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.Color]::FromArgb(28, 27, 31))
$font = New-Object System.Drawing.Font "Segoe UI", 13, ([System.Drawing.FontStyle]::Bold)
$brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 226, 214))
$g.DrawString("Reference: character_lineup.png", $font, $brush, 6, 5); $g.DrawImage($a, 0, $top, $W, $ha)
$g.DrawString("Slice: the same six people, built from the reference script's measurements", $font, $brush, 6, ($top + $ha + $gap + 5)); $g.DrawImage($b, 0, (2 * $top + $ha + $gap), $W, $hb)
$bmp.Save((Join-Path $here "lineup_reference_and_slice.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose(); $a.Dispose(); $b.Dispose(); "wrote lineup_reference_and_slice.png"
