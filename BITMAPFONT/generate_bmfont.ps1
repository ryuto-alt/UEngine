param(
    [Parameter(Mandatory=$true)]
    [string]$TtfPath,

    [Parameter(Mandatory=$true)]
    [int]$FontSize,

    [string]$Charset = "kanji",

    [string]$OutputName = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# --- GDI glyph checker (P/Invoke) ---
Add-Type -TypeDefinition @"
using System;
using System.Drawing;
using System.Runtime.InteropServices;

public class GlyphChecker : IDisposable
{
    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    static extern int AddFontResourceExW(string name, uint fl, IntPtr pdv);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    static extern int RemoveFontResourceExW(string name, uint fl, IntPtr pdv);

    [DllImport("gdi32.dll", CharSet = CharSet.Unicode)]
    static extern uint GetGlyphIndicesW(IntPtr hdc, string text, int count, ushort[] indices, uint flags);

    [DllImport("gdi32.dll")]
    static extern IntPtr SelectObject(IntPtr hdc, IntPtr obj);

    [DllImport("gdi32.dll")]
    static extern bool DeleteObject(IntPtr obj);

    const uint FR_PRIVATE = 0x10;
    const uint GGI_MARK_NONEXISTING_GLYPHS = 0x0001;

    string fontPath;

    public GlyphChecker(string path)
    {
        fontPath = path;
        AddFontResourceExW(fontPath, FR_PRIVATE, IntPtr.Zero);
    }

    public bool[] Check(Graphics g, Font font, char[] chars)
    {
        IntPtr hdc = g.GetHdc();
        IntPtr hFont = font.ToHfont();
        IntPtr old = SelectObject(hdc, hFont);

        bool[] supported = new bool[chars.Length];
        int batch = 512;

        for (int i = 0; i < chars.Length; i += batch)
        {
            int n = Math.Min(batch, chars.Length - i);
            string s = new string(chars, i, n);
            ushort[] idx = new ushort[n];
            GetGlyphIndicesW(hdc, s, n, idx, GGI_MARK_NONEXISTING_GLYPHS);
            for (int j = 0; j < n; j++)
                supported[i + j] = idx[j] != 0xFFFF;
        }

        SelectObject(hdc, old);
        DeleteObject(hFont);
        g.ReleaseHdc(hdc);
        return supported;
    }

    public void Dispose()
    {
        RemoveFontResourceExW(fontPath, FR_PRIVATE, IntPtr.Zero);
    }
}
"@ -ReferencedAssemblies System.Drawing

# --- Config ---
$MaxAtlasSize = 4096
$GlyphPadding = 2

# --- Resolve paths ---
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not [System.IO.Path]::IsPathRooted($TtfPath)) {
    $TtfPath = Join-Path $scriptDir $TtfPath
}

if (-not (Test-Path $TtfPath)) {
    Write-Error "TTF file not found: $TtfPath"
    exit 1
}

$ttfFullPath = (Resolve-Path $TtfPath).Path

# --- Load font via PrivateFontCollection ---
$fontCollection = New-Object System.Drawing.Text.PrivateFontCollection
$fontCollection.AddFontFile($ttfFullPath)
$fontFamily = $fontCollection.Families[0]
$font = New-Object System.Drawing.Font(
    $fontFamily, $FontSize,
    [System.Drawing.FontStyle]::Regular,
    [System.Drawing.GraphicsUnit]::Pixel
)

if (-not $OutputName) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($TtfPath)
    $OutputName = "${baseName}_${FontSize}"
}

$outputDir = Join-Path $scriptDir "output"
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

Write-Host "Font     : $($fontFamily.Name)"
Write-Host "Size     : ${FontSize}px"
Write-Host "Charset  : $Charset"
Write-Host "Output   : $OutputName"
Write-Host ""

# ============================================================
#  Build character list
# ============================================================
$charCollection = [System.Collections.Generic.SortedSet[char]]::new()

# ASCII printable (32-126)
for ($i = 32; $i -le 126; $i++) { [void]$charCollection.Add([char]$i) }

if ($Charset -ne "ascii") {
    # Hiragana U+3041-U+3096
    for ($i = 0x3041; $i -le 0x3096; $i++) { [void]$charCollection.Add([char]$i) }
    # Katakana U+30A1-U+30FA + cho-on
    for ($i = 0x30A1; $i -le 0x30FA; $i++) { [void]$charCollection.Add([char]$i) }
    [void]$charCollection.Add([char]0x30FC)

    # Full-width punctuation
    $fwPunct = @(
        0x3000, 0x3001, 0x3002, 0x3003, 0x300C, 0x300D, 0x300E, 0x300F,
        0x3010, 0x3011, 0x3014, 0x3015, 0x301C, 0x30FB,
        0xFF01, 0xFF08, 0xFF09, 0xFF0C, 0xFF0E, 0xFF1A, 0xFF1B, 0xFF1F,
        0xFF5B, 0xFF5D, 0xFF5E, 0x2015, 0x2026, 0x2025
    )
    foreach ($c in $fwPunct) { [void]$charCollection.Add([char]$c) }

    # Half-width katakana
    for ($i = 0xFF65; $i -le 0xFF9F; $i++) { [void]$charCollection.Add([char]$i) }

    if ($Charset -ne "kana") {
        if ($Charset -eq "kanji") {
            $kanjiFile = Join-Path $scriptDir "charsets\kanji.txt"
            if (Test-Path $kanjiFile) {
                Write-Host "Loading kanji from: $kanjiFile"
                $text = [System.IO.File]::ReadAllText($kanjiFile, [System.Text.Encoding]::UTF8)
                foreach ($c in $text.ToCharArray()) {
                    if (-not [char]::IsWhiteSpace($c) -and -not [char]::IsControl($c)) {
                        [void]$charCollection.Add($c)
                    }
                }
            } else {
                Write-Host "Including CJK Unified Ideographs (U+4E00-U+9FFF) ..."
                for ($i = 0x4E00; $i -le 0x9FFF; $i++) { [void]$charCollection.Add([char]$i) }
            }
        } else {
            # Treat Charset as a file path
            $charsetPath = $Charset
            if (-not [System.IO.Path]::IsPathRooted($charsetPath)) {
                $charsetPath = Join-Path $scriptDir $charsetPath
            }
            if (-not (Test-Path $charsetPath)) {
                Write-Error "Charset file not found: $charsetPath"
                exit 1
            }
            Write-Host "Loading charset from: $charsetPath"
            $text = [System.IO.File]::ReadAllText($charsetPath, [System.Text.Encoding]::UTF8)
            foreach ($c in $text.ToCharArray()) {
                if (-not [char]::IsControl($c) -or $c -eq ' ') {
                    [void]$charCollection.Add($c)
                }
            }
        }
    }
}

# Convert SortedSet to char array
$candidateChars = [char[]]::new($charCollection.Count)
$charCollection.CopyTo($candidateChars)

Write-Host "Candidate characters: $($candidateChars.Length)"

# ============================================================
#  Filter unsupported glyphs via GDI GetGlyphIndices
# ============================================================
Write-Host "Checking font coverage..."

$checker = New-Object GlyphChecker($ttfFullPath)
$tempBmp = New-Object System.Drawing.Bitmap(16, 16)
$tempGfx = [System.Drawing.Graphics]::FromImage($tempBmp)

$supported = $checker.Check($tempGfx, $font, $candidateChars)

$tempGfx.Dispose()
$tempBmp.Dispose()
$checker.Dispose()

# Build filtered array (always keep ASCII space)
$filteredList = [System.Collections.Generic.List[char]]::new()
$skipped = 0
for ($i = 0; $i -lt $candidateChars.Length; $i++) {
    if ($supported[$i] -or [int]$candidateChars[$i] -eq 32) {
        $filteredList.Add($candidateChars[$i])
    } else {
        $skipped++
    }
}

$charArray  = $filteredList.ToArray()
$glyphCount = $charArray.Length

Write-Host "Supported : $glyphCount  (skipped $skipped unsupported)"
Write-Host ""

# ============================================================
#  Measure all glyphs
# ============================================================
$tempBmp = New-Object System.Drawing.Bitmap(512, 512)
$tempGfx = [System.Drawing.Graphics]::FromImage($tempBmp)
$tempGfx.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias

$sf = New-Object System.Drawing.StringFormat([System.Drawing.StringFormat]::GenericTypographic)
$sf.FormatFlags = [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces

$lineHeight = [int][Math]::Ceiling($font.GetHeight($tempGfx))
$emHeight   = $fontFamily.GetEmHeight([System.Drawing.FontStyle]::Regular)
$ascent     = [int][Math]::Ceiling(
    $fontFamily.GetCellAscent([System.Drawing.FontStyle]::Regular) * $font.Size / $emHeight
)

# Measure space width
$spaceWidth = [int][Math]::Ceiling(
    $tempGfx.MeasureString("A A", $font, [System.Drawing.PointF]::Empty, $sf).Width -
    $tempGfx.MeasureString("AA",  $font, [System.Drawing.PointF]::Empty, $sf).Width
)
if ($spaceWidth -lt 1) { $spaceWidth = [int]($FontSize * 0.3) }

Write-Host "Measuring glyphs..."

$glyphId   = [int[]]::new($glyphCount)
$glyphW    = [int[]]::new($glyphCount)
$glyphH    = [int[]]::new($glyphCount)
$glyphXAdv = [int[]]::new($glyphCount)
$glyphX    = [int[]]::new($glyphCount)
$glyphY    = [int[]]::new($glyphCount)
$glyphPage = [int[]]::new($glyphCount)

$progressStep = [Math]::Max(1, [int]($glyphCount / 40))

for ($i = 0; $i -lt $glyphCount; $i++) {
    $c = $charArray[$i]
    $glyphId[$i] = [int]$c

    if ([int]$c -eq 32) {
        $glyphW[$i]    = $spaceWidth
        $glyphH[$i]    = $lineHeight
        $glyphXAdv[$i] = $spaceWidth
    } else {
        $sizeF = $tempGfx.MeasureString([string]$c, $font, [System.Drawing.PointF]::Empty, $sf)
        $w = [int][Math]::Ceiling($sizeF.Width)
        $h = [int][Math]::Ceiling($sizeF.Height)
        if ($w -lt 1) { $w = 1 }
        if ($h -lt 1) { $h = $lineHeight }
        $glyphW[$i]    = $w
        $glyphH[$i]    = $h
        $glyphXAdv[$i] = $w
    }

    if (($i % $progressStep) -eq 0) {
        $pct = [int]($i * 100 / $glyphCount)
        Write-Host "`r  $pct% ($i / $glyphCount)    " -NoNewline
    }
}
Write-Host "`r  100% ($glyphCount / $glyphCount)    "

$tempGfx.Dispose()
$tempBmp.Dispose()

Write-Host "Line Height: $lineHeight, Ascent: $ascent"

# ============================================================
#  Pack glyphs into atlas pages (row-based, height-sorted)
# ============================================================
Write-Host "Packing atlas..."

# Sort indices by height descending for better packing
$sortOrder = 0..($glyphCount - 1) | Sort-Object { -$glyphH[$_] }

$currentPage = 0
$packX = $GlyphPadding
$packY = $GlyphPadding
$rowH  = 0

foreach ($idx in $sortOrder) {
    $w = $glyphW[$idx]
    $h = $glyphH[$idx]

    if (($packX + $w + $GlyphPadding) -gt $MaxAtlasSize) {
        $packX  = $GlyphPadding
        $packY += $rowH + $GlyphPadding
        $rowH   = 0
    }

    if (($packY + $h + $GlyphPadding) -gt $MaxAtlasSize) {
        $currentPage++
        $packX = $GlyphPadding
        $packY = $GlyphPadding
        $rowH  = 0
    }

    $glyphX[$idx]    = $packX
    $glyphY[$idx]    = $packY
    $glyphPage[$idx] = $currentPage

    if ($h -gt $rowH) { $rowH = $h }
    $packX += $w + $GlyphPadding
}

$pageCount = $currentPage + 1
Write-Host "Pages: $pageCount"

# ============================================================
#  Determine atlas dimensions per page (power of 2)
# ============================================================
function Get-NextPow2([int]$v) {
    $p = 1
    while ($p -lt $v -and $p -lt $script:MaxAtlasSize) { $p *= 2 }
    return $p
}

$pageW = [int[]]::new($pageCount)
$pageH = [int[]]::new($pageCount)

for ($p = 0; $p -lt $pageCount; $p++) {
    $mxW = 0; $mxH = 0
    for ($i = 0; $i -lt $glyphCount; $i++) {
        if ($glyphPage[$i] -eq $p) {
            $right  = $glyphX[$i] + $glyphW[$i] + $GlyphPadding
            $bottom = $glyphY[$i] + $glyphH[$i] + $GlyphPadding
            if ($right  -gt $mxW) { $mxW = $right }
            if ($bottom -gt $mxH) { $mxH = $bottom }
        }
    }
    $pageW[$p] = Get-NextPow2 $mxW
    $pageH[$p] = Get-NextPow2 $mxH
    Write-Host "  Page ${p}: $($pageW[$p])x$($pageH[$p])"
}

# For FNT header: use the largest page dimensions
$scaleW = ($pageW | Measure-Object -Maximum).Maximum
$scaleH = ($pageH | Measure-Object -Maximum).Maximum

# ============================================================
#  Render atlas pages
# ============================================================
Write-Host ""
Write-Host "Rendering..."

$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)

for ($p = 0; $p -lt $pageCount; $p++) {
    $bmp = New-Object System.Drawing.Bitmap(
        $pageW[$p], $pageH[$p],
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $gfx = [System.Drawing.Graphics]::FromImage($bmp)
    $gfx.Clear([System.Drawing.Color]::Transparent)
    $gfx.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    $gfx.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $rendered = 0
    for ($i = 0; $i -lt $glyphCount; $i++) {
        if ($glyphPage[$i] -ne $p) { continue }
        $c = $charArray[$i]
        if ([int]$c -ne 32) {
            $gfx.DrawString(
                [string]$c, $font, $brush,
                [float]$glyphX[$i], [float]$glyphY[$i], $sf
            )
        }
        $rendered++
    }

    $pngPath = Join-Path $outputDir "${OutputName}_${p}.png"
    $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $gfx.Dispose()
    $bmp.Dispose()
    Write-Host "  Saved: $pngPath  ($rendered glyphs)"
}

$brush.Dispose()

# ============================================================
#  Write BMFont .fnt file (text format)
# ============================================================
Write-Host ""
Write-Host "Writing .fnt..."

$fntPath = Join-Path $outputDir "${OutputName}.fnt"
$sb = [System.Text.StringBuilder]::new(1024 * 64)

[void]$sb.AppendLine(
    "info face=`"$($fontFamily.Name)`" size=$FontSize bold=0 italic=0 charset=`"`" unicode=1 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=$GlyphPadding,$GlyphPadding outline=0"
)
[void]$sb.AppendLine(
    "common lineHeight=$lineHeight base=$ascent scaleW=$scaleW scaleH=$scaleH pages=$pageCount packed=0 alphaChnl=0 redChnl=4 greenChnl=4 blueChnl=4"
)
for ($p = 0; $p -lt $pageCount; $p++) {
    [void]$sb.AppendLine("page id=$p file=`"${OutputName}_${p}.png`"")
}
[void]$sb.AppendLine("chars count=$glyphCount")

# Write char entries (already sorted since we used SortedSet + filtered in order)
for ($i = 0; $i -lt $glyphCount; $i++) {
    [void]$sb.AppendLine(
        "char id=$($glyphId[$i]) x=$($glyphX[$i]) y=$($glyphY[$i]) width=$($glyphW[$i]) height=$($glyphH[$i]) xoffset=0 yoffset=0 xadvance=$($glyphXAdv[$i]) page=$($glyphPage[$i]) chnl=15"
    )
}

[System.IO.File]::WriteAllText($fntPath, $sb.ToString(), [System.Text.Encoding]::UTF8)
Write-Host "  Saved: $fntPath"

# ============================================================
#  Cleanup
# ============================================================
$sf.Dispose()
$font.Dispose()
$fontCollection.Dispose()

Write-Host ""
Write-Host "=== Complete ==="
Write-Host "  FNT  : $fntPath"
Write-Host "  Pages: $pageCount"
Write-Host "  Chars: $glyphCount"
