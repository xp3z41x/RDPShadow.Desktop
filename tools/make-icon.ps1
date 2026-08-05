# Generates src/app.ico — a multi-resolution Windows icon rendered vectorially
# at each size so it stays crisp from 16 px (Explorer list) to 256 px (jumbo).
# Design: a Fluent rounded blue tile with a white monitor and a faint second
# screen behind it, hinting at session mirroring ("shadow"). Run with:
#   powershell -ExecutionPolicy Bypass -File tools\make-icon.ps1
param(
    [string]$OutIco     = "$PSScriptRoot\..\src\app.ico",
    [string]$OutPreview = "$PSScriptRoot\icon-preview.png"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function RRPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [float]($r * 2)
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc([float]($x + $w - $d), $y, $d, $d, 270, 90)
    $p.AddArc([float]($x + $w - $d), [float]($y + $h - $d), $d, $d, 0, 90)
    $p.AddArc($x, [float]($y + $h - $d), $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}
function FillRR($g, [float]$x, [float]$y, [float]$w, [float]$h, [float]$r, $brush) {
    $p = RRPath $x $y $w $h $r
    $g.FillPath($brush, $p); $p.Dispose()
}
function Solid([int]$a, [int]$r, [int]$gg, [int]$b) {
    return New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb($a, $r, $gg, $b))
}

function Draw([int]$N) {
    $bmp = New-Object System.Drawing.Bitmap($N, $N, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.PixelOffsetMode   = 'HighQuality'

    $m    = [float]($N * 0.055)
    $tile = [float]($N * 0.89)
    $trad = [float]($N * 0.223)

    # rounded tile with a diagonal blue gradient
    $rect = New-Object System.Drawing.RectangleF($m, $m, $tile, $tile)
    $c1 = [System.Drawing.Color]::FromArgb(255, 0x69, 0xCB, 0xFF)
    $c2 = [System.Drawing.Color]::FromArgb(255, 0x11, 0x63, 0xA6)
    $grad = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $c1, $c2, [single]55)
    FillRR $g $m $m $tile $tile $trad $grad
    $grad.Dispose()

    # soft top gloss for depth
    $half = [float]($tile / 2)
    $grect = New-Object System.Drawing.RectangleF($m, $m, $tile, $half)
    $gloss = New-Object System.Drawing.Drawing2D.LinearGradientBrush($grect,
        [System.Drawing.Color]::FromArgb(48, 255, 255, 255),
        [System.Drawing.Color]::FromArgb(0, 255, 255, 255), [single]90)
    FillRR $g $m $m $tile $half $trad $gloss
    $gloss.Dispose()

    # monitor geometry
    $sw  = [float]($N * 0.52)
    $sh  = [float]($N * 0.37)
    $sx  = [float](($N - $sw) / 2)
    $sy  = [float]($N * 0.235)
    $rad = [float]($N * 0.055)

    # faint mirrored screen behind, offset up-right (skip when too small to read)
    if ($N -ge 28) {
        $off = [float]($N * 0.075)
        $ghost = Solid 70 255 255 255
        FillRR $g ([float]($sx + $off)) ([float]($sy - $off)) $sw $sh $rad $ghost
        $ghost.Dispose()
    }

    $white  = Solid 255 255 255 255
    $screen = Solid 255 0x37 0xB4 0xFF

    # bezel + inner screen
    FillRR $g $sx $sy $sw $sh $rad $white
    $b = [float]($N * 0.045)
    FillRR $g ([float]($sx + $b)) ([float]($sy + $b)) ([float]($sw - $b * 2)) ([float]($sh - $b * 2)) ([float]($N * 0.03)) $screen

    # small white cursor glint on larger sizes
    if ($N -ge 32) {
        $gl = [float]($N * 0.05)
        FillRR $g ([float]($sx + $sw * 0.52)) ([float]($sy + $sh * 0.32)) $gl ([float]($gl * 1.4)) ([float]($N * 0.012)) $white
    }

    # stand + base
    $stemW = [float]($N * 0.05)
    FillRR $g ([float](($N - $stemW) / 2)) ([float]($sy + $sh - $rad)) $stemW ([float]($N * 0.11)) ([float]($N * 0.01)) $white
    $baseW = [float]($N * 0.24)
    FillRR $g ([float](($N - $baseW) / 2)) ([float]($sy + $sh + $N * 0.05)) $baseW ([float]($N * 0.045)) ([float]($N * 0.02)) $white

    $white.Dispose(); $screen.Dispose(); $g.Dispose()
    return $bmp
}

# ---- render frames -----------------------------------------------------------
# Standard shell sizes; Windows scales intermediate DPI sizes (20/40/…) from these.
$sizes = 16, 24, 32, 48, 64, 256
$frames = New-Object System.Collections.ArrayList
foreach ($s in $sizes) { [void]$frames.Add((Draw $s)) }

# ---- pack into a .ico (BMP DIB for <=48, PNG for the big frames) --------------
function BmpFrameBytes($bmp) {
    $n = $bmp.Width
    $lb = $bmp.LockBits((New-Object System.Drawing.Rectangle(0, 0, $n, $n)),
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $px = New-Object byte[] ($n * $n * 4)
    [System.Runtime.InteropServices.Marshal]::Copy($lb.Scan0, $px, 0, $px.Length)
    $bmp.UnlockBits($lb)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([uint32]40); $bw.Write([int32]$n); $bw.Write([int32]($n * 2))
    $bw.Write([uint16]1);  $bw.Write([uint16]32)
    $bw.Write([uint32]0);  $bw.Write([uint32]0)
    $bw.Write([int32]0); $bw.Write([int32]0); $bw.Write([uint32]0); $bw.Write([uint32]0)
    for ($y = $n - 1; $y -ge 0; $y--) { $bw.Write($px, ($y * $n * 4), ($n * 4)) }
    $maskRow = [int]([math]::Floor(($n + 31) / 32) * 4)
    for ($y = $n - 1; $y -ge 0; $y--) {
        $bytes = New-Object byte[] $maskRow
        for ($x = 0; $x -lt $n; $x++) {
            if ($px[($y * $n + $x) * 4 + 3] -lt 128) {
                $bi = [int][math]::Floor($x / 8)
                $bytes[$bi] = [byte]($bytes[$bi] -bor (0x80 -shr ($x % 8)))
            }
        }
        $bw.Write($bytes, 0, $maskRow)
    }
    $bw.Flush(); $r = $ms.ToArray(); $bw.Dispose(); $ms.Dispose(); return , $r
}
function PngFrameBytes($bmp) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $r = $ms.ToArray(); $ms.Dispose(); return , $r
}

# BMP DIB for the tiny sizes Explorer likes uncompressed; PNG for >=48 keeps
# the file small (a 48px BMP frame is ~9.6 KB; its PNG is a few hundred bytes).
$blobs = New-Object System.Collections.ArrayList
foreach ($f in $frames) {
    if ($f.Width -ge 48) { [void]$blobs.Add((PngFrameBytes $f)) }
    else                 { [void]$blobs.Add((BmpFrameBytes $f)) }
}

$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter($out)
$w.Write([uint16]0); $w.Write([uint16]1); $w.Write([uint16]$frames.Count)
$offset = [uint32](6 + 16 * $frames.Count)
for ($i = 0; $i -lt $frames.Count; $i++) {
    $n = $frames[$i].Width
    $dim = [byte]$(if ($n -ge 256) { 0 } else { $n })
    $w.Write($dim); $w.Write($dim); $w.Write([byte]0); $w.Write([byte]0)
    $w.Write([uint16]1); $w.Write([uint16]32)
    $w.Write([uint32]$blobs[$i].Length)
    $w.Write([uint32]$offset)
    $offset = [uint32]($offset + $blobs[$i].Length)
}
foreach ($b in $blobs) { $w.Write($b, 0, $b.Length) }
$w.Flush()
$icoPath = [System.IO.Path]::GetFullPath($OutIco)
[System.IO.File]::WriteAllBytes($icoPath, $out.ToArray())
$w.Dispose(); $out.Dispose()

# ---- preview sheet: each size on dark and light swatches ----------------------
$pw = 16; foreach ($s in $sizes) { $pw += $s + 16 }
$prev = New-Object System.Drawing.Bitmap([int]$pw, 300)
$pg = [System.Drawing.Graphics]::FromImage($prev)
$pg.Clear([System.Drawing.Color]::FromArgb(255, 32, 32, 32))
$pg.FillRectangle((Solid 255 235 235 235), 0, 150, [int]$pw, 150)
$x = 16
foreach ($f in $frames) {
    $pg.DrawImage($f, [int]$x, [int]((150 - $f.Height) / 2))
    $pg.DrawImage($f, [int]$x, [int](150 + (150 - $f.Height) / 2))
    $x += $f.Width + 16
}
$pg.Dispose(); $prev.Save($OutPreview); $prev.Dispose()
foreach ($f in $frames) { $f.Dispose() }
Write-Output ("Wrote {0} ({1} bytes, {2} frames)" -f $icoPath, (Get-Item $icoPath).Length, $sizes.Count)
