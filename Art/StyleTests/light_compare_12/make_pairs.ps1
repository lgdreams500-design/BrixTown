# Light and shadow comparison 12: puts each reference render beside the slice's closest view.
#   powershell -NoProfile -ExecutionPolicy Bypass -File Art/StyleTests/light_compare_12/make_pairs.ps1
# The slice captures (slice_*.jpg) are screenshots of Prototype/play taken on 2026-11-22; retake them after the light changes.
Add-Type -AssemblyName System.Drawing
$here = $PSScriptRoot
$ref = Join-Path $here "..\..\Reference"
$H = 500

function Get-Part([string]$path, $crop) {          # an image, or a part of it given as x, y, width, height in shares of the whole
  $img = [System.Drawing.Image]::FromFile($path)
  if (-not $crop) { return $img }
  $r = New-Object System.Drawing.Rectangle ([int]($crop[0] * $img.Width)), ([int]($crop[1] * $img.Height)), ([int]($crop[2] * $img.Width)), ([int]($crop[3] * $img.Height))
  $bmp = New-Object System.Drawing.Bitmap $r.Width, $r.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $r.Width, $r.Height), $r, [System.Drawing.GraphicsUnit]::Pixel)
  $g.Dispose(); $img.Dispose(); return $bmp
}

function New-Pair([string]$out, [string]$refFile, $refCrop, [string]$refLabel, [string]$sliceFile, [string]$sliceLabel) {
  $a = Get-Part (Join-Path $ref $refFile) $refCrop
  $b = Get-Part (Join-Path $here $sliceFile) $null
  $wa = [int]($a.Width * $H / $a.Height); $wb = [int]($b.Width * $H / $b.Height); $gap = 12; $top = 34
  $bmp = New-Object System.Drawing.Bitmap ($wa + $wb + $gap), ($H + $top)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.Clear([System.Drawing.Color]::FromArgb(28, 27, 31))
  $g.DrawImage($a, 0, $top, $wa, $H); $g.DrawImage($b, $wa + $gap, $top, $wb, $H)
  $font = New-Object System.Drawing.Font "Segoe UI", 13, ([System.Drawing.FontStyle]::Bold)
  $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 226, 214))
  $g.DrawString($refLabel, $font, $brush, 6, 5); $g.DrawString($sliceLabel, $font, $brush, ($wa + $gap + 6), 5)
  $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
  $p = New-Object System.Drawing.Imaging.EncoderParameters 1
  $p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]90)
  $bmp.Save((Join-Path $here $out), $codec, $p)
  $g.Dispose(); $bmp.Dispose(); $a.Dispose(); $b.Dispose()
  "wrote $out"
}

New-Pair "pair_1_alley_morning.jpg" "world_alley_morning_raw.png" $null "Reference: alley, morning" "slice_alley_morning.jpg" "Slice: start alley, 07:20"
New-Pair "pair_2_alley_dusk.jpg" "world_overview.png" @(0.007, 0.506, 0.488, 0.483) "Reference: alley, dusk" "slice_alley_dusk.jpg" "Slice: start alley, 19:54"
New-Pair "pair_3_street.jpg" "world_street_kit_light.png" $null "Reference: street, kit daylight" "slice_street_morning.jpg" "Slice: Linden Street, 09:00"

# Three across: the reference, the slice as it is, and the slice with the trial light (dial gi, key G; ADR-0018).
function New-Row([string]$out, $items) {              # items: (file, in Reference or here, label)
  $imgs = @(); foreach ($it in $items) { $dir = $ref; if ($it[1] -eq "here") { $dir = $here }; $imgs += ,(Get-Part (Join-Path $dir $it[0]) $null) }
  $gap = 12; $top = 34; $ws = @(); foreach ($i in $imgs) { $ws += [int]($i.Width * $H / $i.Height) }
  $total = ($ws | Measure-Object -Sum).Sum + $gap * ($imgs.Count - 1)
  $bmp = New-Object System.Drawing.Bitmap ([int]$total), ($H + $top)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.Clear([System.Drawing.Color]::FromArgb(28, 27, 31))
  $font = New-Object System.Drawing.Font "Segoe UI", 13, ([System.Drawing.FontStyle]::Bold)
  $brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(232, 226, 214))
  $x = 0; for ($k = 0; $k -lt $imgs.Count; $k++) { $g.DrawImage($imgs[$k], $x, $top, $ws[$k], $H); $g.DrawString($items[$k][2], $font, $brush, ($x + 6), 5); $x += $ws[$k] + $gap }
  $codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq "image/jpeg" }
  $p = New-Object System.Drawing.Imaging.EncoderParameters 1
  $p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), ([long]90)
  $bmp.Save((Join-Path $here $out), $codec, $p); $g.Dispose(); $bmp.Dispose(); foreach ($i in $imgs) { $i.Dispose() }
  "wrote $out"
}
New-Row "trial_1_alley_morning.jpg" @(@("world_alley_morning_raw.png", "ref", "Reference: alley, morning"), @("slice_alley_bench_before.jpg", "here", "Slice now (bench added)"), @("slice_alley_bench_trial.jpg", "here", "Slice with the trial light"))
