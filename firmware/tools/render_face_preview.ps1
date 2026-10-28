param(
    [Parameter(Mandatory)][string]$InputJson,
    [Parameter(Mandatory)][string]$OutputPath
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$faces = Get-Content -LiteralPath $InputJson -Raw | ConvertFrom-Json
$bitmap = [System.Drawing.Bitmap]::new(960, 604)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([System.Drawing.Color]::FromArgb(246, 248, 251))
$font = [System.Drawing.Font]::new('Yu Gothic UI', 14, [System.Drawing.FontStyle]::Bold)
$small = [System.Drawing.Font]::new('Yu Gothic UI', 10)
$labelBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(35, 45, 61))
function Convert-Color565([int]$v) {
    [System.Drawing.Color]::FromArgb([int]((($v -shr 11) -band 31) * 255 / 31), [int]((($v -shr 5) -band 63) * 255 / 63), [int](($v -band 31) * 255 / 31))
}
function New-RoundPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $r = [Math]::Min($r, [Math]::Min($w, $h) / 2)
    $d = 2 * $r
    $path.AddArc($x, $y, $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}
try {
    $labels = @{Sleep='Sleep / 眠り'; Ready='Ready / 待機'; Listening='Listening / 聞く'; Thinking='Thinking / 考える'; Speaking='Speaking / 話す'}
    for ($i = 0; $i -lt $faces.Count; $i++) {
        $x = ($i % 3) * 320
        $y = [Math]::Floor($i / 3) * 282
        $graphics.DrawString($labels[$faces[$i].mode], $font, $labelBrush, [float]($x + 10), [float]($y + 3))
        $state = $graphics.Save()
        $graphics.TranslateTransform([float]($x + 8), [float]($y + 37))
        $graphics.ScaleTransform(0.95, 0.95)
        foreach ($command in $faces[$i].commands.Split(';', [StringSplitOptions]::RemoveEmptyEntries)) {
            $parts = $command.Split(',')
            $v = @($parts[1..($parts.Length - 1)] | ForEach-Object { [int]$_ })
            $color = Convert-Color565 $v[-1]
            $brush = [System.Drawing.SolidBrush]::new($color)
            $pen = [System.Drawing.Pen]::new($color, 1)
            try {
                switch ($parts[0]) {
                    'bg' { $graphics.FillRectangle($brush, 0, 0, 320, 240) }
                    'rr' { $path = New-RoundPath $v[0] $v[1] $v[2] $v[3] $v[4]; try { $graphics.FillPath($brush, $path) } finally { $path.Dispose() } }
                    'rrOutline' { $path = New-RoundPath $v[0] $v[1] $v[2] $v[3] $v[4]; try { $graphics.DrawPath($pen, $path) } finally { $path.Dispose() } }
                    'circle' { $graphics.FillEllipse($brush, $v[0]-$v[2], $v[1]-$v[2], 2*$v[2], 2*$v[2]) }
                    'circleOutline' { $graphics.DrawEllipse($pen, $v[0]-$v[2], $v[1]-$v[2], 2*$v[2], 2*$v[2]) }
                    'arc' {
                        $radius = ($v[2] + $v[3]) / 2.0
                        $pen.Width = [Math]::Max(1, [Math]::Abs($v[3] - $v[2]))
                        $graphics.DrawArc($pen, [float]($v[0]-$radius), [float]($v[1]-$radius), [float](2*$radius), [float](2*$radius), [float]$v[4], [float]($v[5]-$v[4]))
                    }
                    default { throw 'Unknown avatar drawing operation' }
                }
            } finally { $brush.Dispose(); $pen.Dispose() }
        }
        $graphics.Restore($state)
    }
    $graphics.DrawString('実コードの描画形状プレビュー。Sleepの実機輝度・液晶の見え方とは異なります。', $small, $labelBrush, 10, 575)
    $output = [System.IO.Path]::GetFullPath($OutputPath)
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($output)) | Out-Null
    $bitmap.Save($output, [System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output ('Preview saved: ' + $output)
} finally {
    $graphics.Dispose(); $bitmap.Dispose(); $font.Dispose(); $small.Dispose(); $labelBrush.Dispose()
}
