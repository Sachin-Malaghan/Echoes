# Launches the game (editor build, -game) and runs the -ECCapture script: the autopilot plays the levels,
# every screen and effect is photographed to Saved/Screenshots/WindowsEditor/Echoes_*.png, then it quits.
#   -Phone   20:9 window with the touch pads forced on
#   -Store   2:1 window with the touch pads (Google Play's widest allowed screenshot)
param([int]$Width = 1600, [int]$Height = 900, [switch]$Phone, [switch]$Store)
$engine = if ($env:UE_ROOT) { $env:UE_ROOT } else { 'C:\Program Files\Epic Games\UE_5.8' }
$root = Resolve-Path "$PSScriptRoot\..\.."
$project = Join-Path $root 'Echoes.uproject'
$extra = @()
$tag = 'desktop'
if ($Phone) { $Width = 1600; $Height = 720; $extra += '-ECForceTouch'; $tag = 'phone' }
if ($Store) { $Width = 1440; $Height = 720; $extra += '-ECForceTouch'; $tag = 'store' }
& "$engine\Engine\Binaries\Win64\UnrealEditor.exe" "$project" -game -windowed "-ResX=$Width" "-ResY=$Height" -nosplash -unattended `
    -ECCapture "-ECCaptureTag=$tag" @extra | Out-Null
Get-ChildItem (Join-Path $root 'Saved\Screenshots') -Recurse -Filter "Echoes_${tag}_*.png" | Select-Object Name, Length
