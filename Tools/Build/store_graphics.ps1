# Builds Google Play store graphics and the website's screenshots from the capture screenshots.
# Run Tools\Validation\capture.ps1 and capture.ps1 -Store first.
# Output: Build/Android/PlayStore/ (feature graphic 1024x500, screenshots within 2:1) and Website/assets/shots/.
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."
$src = Join-Path $root 'Saved\Screenshots\WindowsEditor'
$out = Join-Path $root 'Build\Android\PlayStore'
$web = Join-Path $root 'Website\assets\shots'
New-Item -ItemType Directory -Force $out, $web | Out-Null

$jpeg = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$q = New-Object System.Drawing.Imaging.EncoderParameters 1
$q.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), 92L

function Load([string]$name) {
    $p = Join-Path $src "Echoes_$name.png"
    if (-not (Test-Path $p)) { throw "missing screenshot $p - run Tools\Validation\capture.ps1 (and -Phone) first" }
    [System.Drawing.Image]::FromFile($p)
}

# Draws $img into a WxH frame, cropping to fill around a focus point (0..1).
function Fill([System.Drawing.Image]$img, [int]$W, [int]$H, [double]$fx = 0.5, [double]$fy = 0.5) {
    $bmp = New-Object System.Drawing.Bitmap $W, $H, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
    $s = [Math]::Max($W / $img.Width, $H / $img.Height)
    $sw = $W / $s; $sh = $H / $s
    $sx = ($img.Width - $sw) * $fx; $sy = ($img.Height - $sh) * $fy
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), [float]$sx, [float]$sy, [float]$sw, [float]$sh, 'Pixel')
    return @($bmp, $g)
}

function Centered($g, [string]$text, $font, $brush, [double]$cx, [double]$cy) {
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = 'Center'; $fmt.LineAlignment = 'Center'
    $g.DrawString($text, $font, $brush, (New-Object System.Drawing.RectangleF ([float]($cx - 1000)), ([float]($cy - 200)), 2000, 400), $fmt)
}

# ---- Feature graphic 1024 x 500: the Factory full of Echoes, the title with its two Echo copies ----
$img = Load 'desktop_57_assembly_clean'
$r = Fill $img 1024 500 0.3 0.8
$bmp = $r[0]; $g = $r[1]
$shade = New-Object System.Drawing.Drawing2D.LinearGradientBrush (New-Object System.Drawing.Rectangle 0, 0, 1024, 500), ([System.Drawing.Color]::FromArgb(200, 6, 10, 20)), ([System.Drawing.Color]::FromArgb(30, 6, 10, 20)), 0
$g.FillRectangle($shade, 0, 0, 1024, 500)
$title = New-Object System.Drawing.Font 'Segoe UI Black', 96, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
$sub = New-Object System.Drawing.Font 'Segoe UI Semibold', 24, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
Centered $g 'ECHOES' $title (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(150, 255, 176, 32))) 284 208
Centered $g 'ECHOES' $title (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(140, 255, 61, 154))) 308 202
Centered $g 'ECHOES' $title (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 244, 247, 255))) 296 205
Centered $g 'YOU ARE YOUR ONLY TEAMMATE' $sub (New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 61, 242, 255))) 296 290
$g.Dispose(); $img.Dispose()
$bmp.Save((Join-Path $out 'feature-graphic-1024x500.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# ---- Screenshots (16:9, and 2:1 for the phone shot: both within Play's 2:1 limit) ----
$shots = [ordered]@{
    '01-title' = @('desktop_01_title', 1600, 900)
    '02-first-step' = @('desktop_13_l1_door_held', 1600, 900)
    '03-touch-controls' = @('store_55_assembly', 1440, 720)
    '04-stepping-stone' = @('desktop_20_l2_stack', 1600, 900)
    '05-clockwork' = @('desktop_33_clockwork', 1600, 900)
    '06-hot-wire' = @('desktop_50_hotwire', 1600, 900)
    '07-assembly-line' = @('desktop_55_assembly', 1600, 900)
    '08-the-core' = @('desktop_56_core', 1600, 900)
}
Get-ChildItem $out -Filter 'screenshot-*.jpg' -ErrorAction SilentlyContinue | Remove-Item
foreach ($k in $shots.Keys) {
    $s = $shots[$k]
    $img = Load $s[0]
    $r = Fill $img $s[1] $s[2] 0.5 0.5
    $r[1].Dispose(); $img.Dispose()
    $r[0].Save((Join-Path $out "screenshot-$k.jpg"), $jpeg, $q)
    $r[0].Dispose()
}

# ---- Website screenshots ----
$webShots = @{ 'lab' = 'desktop_33_clockwork'; 'factory' = 'desktop_55_assembly'; 'core' = 'desktop_56_core' }
foreach ($k in $webShots.Keys) {
    $img = Load $webShots[$k]
    $r = Fill $img 960 540 0.5 0.5
    $r[1].Dispose(); $img.Dispose()
    $r[0].Save((Join-Path $web "$k.jpg"), $jpeg, $q)
    $r[0].Dispose()
}
Get-ChildItem $out, $web | Select-Object Name, Length
