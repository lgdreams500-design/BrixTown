# Takes every shot of a tour list in one run of the game (UBrixLight's -BrixTour=; Game/README.md "The eye and the
# night"): the light in every kind of place through the day. Offscreen, at High, no sound; about 7 s a shot after the
# town has loaded. Two at once hung the graphics chip of the owner's laptop, so a run waits here until any other
# run of this script on the machine (any checkout, any session) has finished.
#
#   Tools\unreal\light_tour.ps1 -OutDir C:\full\path\shots                 the light as it is
#   Tools\unreal\light_tour.ps1 -OutDir C:\full\path\before -Extra "-BrixLightOff"     as the game mode alone makes it
#   Tools\unreal\light_tour.ps1 -OutDir C:\full\path\pt -List <a list with pt= stops> -PathTrace   the path tracer's shots too
#
# Then Tools\unreal\shot_sheet.py puts shots side by side. The engine's path is this machine's (docs/setup/DEV_ENVIRONMENT.md).
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$List = (Join-Path $PSScriptRoot "light_tour.txt"),
    [string]$Extra = "",
    [switch]$PathTrace,   # ray tracing compiled for this run, for a list's pt= stops (the first run compiles its shaders: long)
    [string]$Editor = "C:\Users\leena\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
)
$Project = Join-Path (Split-Path (Split-Path $PSScriptRoot)) "Game\BrixTown.uproject"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Out = (Resolve-Path $OutDir).Path.Replace("\", "/")
$Log = Join-Path $OutDir "tour.log"
$ArgLine = "`"$Project`" -game -windowed -ResX=1066 -ResY=666 -RenderOffscreen -nosound -nosplash -unattended -BrixNoDesigner -BrixNoSave -BrixLightRoom -ExecCmds=`"scalability 2`" -BrixTour=$((Resolve-Path $List).Path.Replace('\','/')) -BrixTourOut=$Out -abslog=`"$Log`" $Extra"
if ($PathTrace) { $ArgLine += " -ini:Engine:[/Script/Engine.RendererSettings]:r.RayTracing=True,[/Script/Engine.RendererSettings]:r.PathTracing=True,[/Script/Engine.RendererSettings]:r.SkinCache.CompileShaders=True" }
$One = New-Object System.Threading.Mutex($false, "Global\BrixTownUnrealRun")
try { if (-not $One.WaitOne(0)) { "another Unreal run is going: waiting for it"; [void]$One.WaitOne() } } catch [System.Threading.AbandonedMutexException] { }
try {
    $T0 = Get-Date
    $P = Start-Process -FilePath $Editor -ArgumentList $ArgLine -PassThru
    if (-not $P.WaitForExit(1800000)) { $P.Kill(); "the tour did not finish in half an hour" }
} finally { $One.ReleaseMutex() }
"{0} pictures in {1:n0} s" -f (Get-ChildItem (Join-Path $OutDir "*.png")).Count, ((Get-Date) - $T0).TotalSeconds
Select-String -Path $Log -Pattern "Brix Town: (tour stop|the tour|a tour|the eye)" | ForEach-Object { $_.Line -replace '^.*Brix Town: ', '' }
