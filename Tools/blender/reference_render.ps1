# Render a reference place with Tools/blender/hours_50.py, from the checkout this script is in.
#
#   Tools/blender/reference_render.ps1 -Place mall_entrance -Review -Out <scratch folder>       # a review round
#   Tools/blender/reference_render.ps1 -Place mall_entrance -Review -Hours Midday -Out <scratch> # one setting first
#   Tools/blender/reference_render.ps1 -Place mall_entrance -Out <scratch>                      # full quality, once approved
#
# -Review: low quality (half size, 24 samples), for every round the owner reviews (FEATURE_PROCESS 6).
# -Out: where the pictures go; leave it out only to write Art/StyleTests/hours_50/.
# Blender is $env:BLENDER if set, else blender-launcher.exe. The launcher shows no output of its own, so the run's log
# (hours_50_<place>_log.txt in -Out) is printed at the end: how long the build took, each picture's time, any FAILED.
param(
    [Parameter(Mandatory = $true)][string]$Place,
    [switch]$Review,
    [string]$Hours = "",
    [string]$Out = ""
)
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$blender = if ($env:BLENDER) { $env:BLENDER } else { "blender-launcher.exe" }
$env:WASTED_SCRIPT = "hours_50.py"
$env:WASTED_PLACE = $Place
$env:WASTED_HOURS = $Hours
if ($Review) { $env:WASTED_SCALE = "50"; $env:WASTED_SAMPLES = "24" } else { $env:WASTED_SCALE = ""; $env:WASTED_SAMPLES = "" }
if ($Out) {
    New-Item -ItemType Directory -Force $Out | Out-Null
    $env:WASTED_OUT = (Resolve-Path $Out).Path.Replace("\", "/")
} else { $env:WASTED_OUT = "" }
$t = Get-Date
& $blender -b --factory-startup --python (Join-Path $here "run_here.py") | Out-Null
$log = if ($Out) { Join-Path $Out "hours_50_$($Place)_log.txt" } else { Join-Path $here "..\..\Art\StyleTests\hours_50\hours_50_$($Place)_log.txt" }
if (Test-Path $log) { Get-Content $log } else { Write-Output "no log at ${log}: Blender did not start the script" }
Write-Output ("whole run: {0:N0} s" -f ((Get-Date) - $t).TotalSeconds)
