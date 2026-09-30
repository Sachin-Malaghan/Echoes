# Draws the ECHOES app icon (Subject 7 with two Echoes trailing behind, inside the loop clock, in the
# Lab palette) and writes every size Android, iOS and the Play Store need. Re-run after changing the design.
Add-Type -AssemblyName System.Drawing
$root = Resolve-Path "$PSScriptRoot\..\.."

function C([int]$a, [int]$rgb) { [System.Drawing.Color]::FromArgb($a, ($rgb -shr 16) -band 255, ($rgb -shr 8) -band 255, $rgb -band 255) }

function New-Icon([int]$N) {
    $bmp = New-Object System.Drawing.Bitmap $N, $N
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $s = $N / 1024.0
    function P([double]$x, [double]$y) { New-Object System.Drawing.PointF ([float]($x * $s)), ([float]($y * $s)) }
    function Glow([double]$cx, [double]$cy, [double]$r, [int]$alpha, [int]$rgb) {
        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        $path.AddEllipse([float](($cx - $r) * $s), [float](($cy - $r) * $s), [float](2 * $r * $s), [float](2 * $r * $s))
        $b = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
        $b.CenterColor = C $alpha $rgb
        $b.SurroundColors = [System.Drawing.Color[]]@((C 0 $rgb))
        $g.FillPath($b, $path)
    }

    # Background: the Lab's near-black blue, lighter toward the floor, with a cyan haze in the middle.
    $rect = New-Object System.Drawing.RectangleF 0, 0, $N, $N
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, (C 255 0x0A1220), (C 255 0x14243D), 90
    $g.FillRectangle($bg, $rect)
    Glow 512 520 520 70 0x3DF2FF

    # The loop clock: a thin ring with ticks, the remaining time as a bright arc, a rewind arrowhead.
    $cx = 512; $cy = 500; $R = 400
    $thin = New-Object System.Drawing.Pen (C 60 0x3DF2FF), ([float](10 * $s))
    $g.DrawEllipse($thin, [float](($cx - $R) * $s), [float](($cy - $R) * $s), [float](2 * $R * $s), [float](2 * $R * $s))
    for ($i = 0; $i -lt 60; $i++) {
        $a = $i * [Math]::PI * 2 / 60
        $r0 = if ($i % 5 -eq 0) { $R - 46 } else { $R - 26 }
        $tp = New-Object System.Drawing.Pen (C $(if ($i % 5 -eq 0) { 120 } else { 55 }) 0x3DF2FF), ([float](6 * $s))
        $g.DrawLine($tp, (P ($cx + [Math]::Cos($a) * $r0) ($cy + [Math]::Sin($a) * $r0)), (P ($cx + [Math]::Cos($a) * ($R - 10)) ($cy + [Math]::Sin($a) * ($R - 10))))
    }
    $arc = New-Object System.Drawing.Pen (C 255 0x3DF2FF), ([float](26 * $s))
    $arc.StartCap = 'Round'; $arc.EndCap = 'Round'
    $g.DrawArc($arc, [float](($cx - $R) * $s), [float](($cy - $R) * $s), [float](2 * $R * $s), [float](2 * $R * $s), -90, 250)
    # Arrowhead at the arc's start (top), pointing back: time rewinding.
    $g.FillPolygon((New-Object System.Drawing.SolidBrush (C 255 0x3DF2FF)), [System.Drawing.PointF[]]@((P ($cx - 70) ($cy - $R)), (P ($cx + 10) ($cy - $R - 58)), (P ($cx + 10) ($cy - $R + 58))))

    # Floor line
    Glow 512 860 420 60 0x3DF2FF
    $g.FillRectangle((New-Object System.Drawing.SolidBrush (C 255 0x0B1526)), [float](0), [float](862 * $s), [float]$N, [float]$N)
    $g.FillRectangle((New-Object System.Drawing.SolidBrush (C 220 0x3DF2FF)), [float](0), [float](858 * $s), [float]$N, [float](8 * $s))

    # Subject 7 (and its Echoes): feet at (fx, 860), facing right. K = pixels per game tile.
    function Figure([double]$fx, [double]$K, [int]$rgb, [bool]$glass) {
        function Q([double]$u, [double]$v) { P ($fx + $u * $K) (860 - $v * $K) }
        $body = if ($glass) { New-Object System.Drawing.SolidBrush (C 70 $rgb) } else { New-Object System.Drawing.SolidBrush (C 255 0xF4F7FF) }
        $edge = New-Object System.Drawing.Pen (C $(if ($glass) { 235 } else { 150 }) $rgb), ([float]($(if ($glass) { 0.035 } else { 0.05 }) * $K * $s))
        $edge.LineJoin = 'Round'
        $leg = New-Object System.Drawing.Pen $(if ($glass) { C 170 $rgb } else { C 255 0xDCE5F2 }), ([float](0.12 * $K * $s))
        $leg.StartCap = 'Round'; $leg.EndCap = 'Round'
        $g.DrawLine($leg, (Q 0 0.62), (Q 0.2 0.05))
        $g.DrawLine($leg, (Q 0 0.62), (Q -0.16 0.05))
        $coat = [System.Drawing.PointF[]]@((Q -0.19 1.14), (Q 0.17 1.14), (Q 0.2 0.74), (Q 0.26 0.41), (Q 0.18 0.33), (Q 0.1 0.4), (Q 0.0 0.3), (Q -0.1 0.39), (Q -0.22 0.27), (Q -0.3 0.38), (Q -0.42 0.34), (Q -0.21 0.74))
        $g.FillPolygon($body, $coat)
        $g.DrawPolygon($edge, $coat)
        $peak = [System.Drawing.PointF[]]@((Q -0.13 1.51), (Q -0.36 1.36), (Q -0.11 1.22))
        $g.FillPolygon($body, $peak)
        $hx = $fx - 0.205 * $K; $hy = 860 - (1.355 + 0.215) * $K
        $g.FillEllipse($body, [float]($hx * $s), [float]($hy * $s), [float](0.41 * $K * $s), [float](0.43 * $K * $s))
        $g.DrawEllipse($edge, [float]($hx * $s), [float]($hy * $s), [float](0.41 * $K * $s), [float](0.43 * $K * $s))
        $face = New-Object System.Drawing.SolidBrush (C $(if ($glass) { 150 } else { 255 }) 0x060B16)
        $g.FillEllipse($face, [float](($fx + (0.085 - 0.118) * $K) * $s), [float]((860 - (1.33 + 0.132) * $K) * $s), [float](0.236 * $K * $s), [float](0.264 * $K * $s))
        $eye = if ($glass) { $rgb } else { 0x3DF2FF }
        Glow ($fx + 0.115 * $K) (860 - 1.335 * $K) (0.2 * $K) 160 $eye
        Glow ($fx + 0.07 * $K) (860 - 0.97 * $K) (0.32 * $K) 190 $eye
        $eb = New-Object System.Drawing.SolidBrush (C 255 0xE8FFFF)
        foreach ($ex in 0.075, 0.155) {
            $g.FillEllipse($eb, [float](($fx + ($ex - 0.026) * $K) * $s), [float]((860 - (1.335 + 0.036) * $K) * $s), [float](0.052 * $K * $s), [float](0.072 * $K * $s))
        }
        $g.FillEllipse($eb, [float](($fx + 0.025 * $K) * $s), [float]((860 - 1.015 * $K) * $s), [float](0.09 * $K * $s), [float](0.09 * $K * $s))
    }

    Figure 250 300 0xFFB020 $true    # Echo 1, amber
    Figure 390 318 0xFF3D9A $true    # Echo 2, magenta
    Glow 600 600 300 90 0x3DF2FF
    Figure 560 340 0x3DF2FF $false   # Subject 7

    $g.Dispose()
    return $bmp
}

function Save([System.Drawing.Bitmap]$src, [int]$size, [string]$path) {
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    $dst = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($dst)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}

$icon = New-Icon 1024

# Android launcher icons (Unreal copies Build/Android/res over its defaults when packaging)
$android = @{ 'drawable-ldpi' = 36; 'drawable-mdpi' = 48; 'drawable-hdpi' = 72; 'drawable-xhdpi' = 96; 'drawable-xxhdpi' = 144; 'drawable-xxxhdpi' = 192; 'drawable' = 192 }
foreach ($k in $android.Keys) { Save $icon $android[$k] (Join-Path $root "Build\Android\res\$k\icon.png") }
Save $icon 512 (Join-Path $root 'Build\Android\PlayStore\icon-512.png')

# iOS: single 1024 icon in the asset catalog, plus the legacy Graphics folder
$appicon = Join-Path $root 'Build\IOS\Resources\Assets.xcassets\AppIcon.appiconset'
Save $icon 1024 (Join-Path $appicon 'Icon1024.png')
Set-Content (Join-Path $appicon 'Contents.json') -Encoding ascii -Value @'
{
  "images" : [ { "filename" : "Icon1024.png", "idiom" : "universal", "platform" : "ios", "size" : "1024x1024" } ],
  "info" : { "author" : "xcode", "version" : 1 }
}
'@
Set-Content (Join-Path $root 'Build\IOS\Resources\Assets.xcassets\Contents.json') -Encoding ascii -Value '{ "info" : { "author" : "xcode", "version" : 1 } }'
Save $icon 1024 (Join-Path $root 'Build\IOS\Resources\Graphics\Icon1024.png')

# Full-size master for store listings / website later
Save $icon 1024 (Join-Path $root 'Build\Icon\echoes-icon-1024.png')
$icon.Dispose()
'icons written'
