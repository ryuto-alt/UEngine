<#
.SYNOPSIS
    C++プロジェクトの統計情報を表示するスクリプト
.DESCRIPTION
    クラス数、関数数、ファイル数、行数を集計します
.PARAMETER Path
    分析対象のディレクトリパス（デフォルト: カレントディレクトリ）
.PARAMETER ExcludeDirs
    除外するディレクトリ名（カンマ区切り）
.EXAMPLE
    .\project_stats.ps1
    .\project_stats.ps1 -Path "C:\MyProject"
    .\project_stats.ps1 -ExcludeDirs "external,vendor,third_party"
#>

param(
    [string]$Path = ".",
    [string]$ExcludeDirs = "external,vendor,third_party,node_modules,build,out,x64,Debug,Release,.git"
)

$ErrorActionPreference = "SilentlyContinue"

# 絶対パスに変換
$Path = (Resolve-Path $Path).Path

# 色付き出力
function Write-Header { param($text) Write-Host "`n$text" -ForegroundColor Cyan }
function Write-Stat { param($label, $value) Write-Host ("  {0,-30} : " -f $label) -NoNewline; Write-Host $value -ForegroundColor Yellow }

# 除外パターン作成
$excludeList = $ExcludeDirs -split ","
$excludePattern = ($excludeList | ForEach-Object { [regex]::Escape($_.Trim()) }) -join "|"

# ファイル取得（除外ディレクトリをスキップ）
function Get-FilteredFiles {
    param([string[]]$Extensions)
    $results = @()
    foreach ($ext in $Extensions) {
        $files = Get-ChildItem -Path $Path -Recurse -Filter $ext -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch "\\($excludePattern)\\" }
        $results += $files
    }
    return $results
}

Write-Host ""
Write-Host "===============================================" -ForegroundColor Magenta
Write-Host "        C++ Project Statistics Tool           " -ForegroundColor Magenta
Write-Host "===============================================" -ForegroundColor Magenta
Write-Host ""
Write-Host "Target: " -NoNewline
Write-Host $Path -ForegroundColor Green
Write-Host "Exclude: " -NoNewline
Write-Host $ExcludeDirs -ForegroundColor DarkGray

# ファイル収集
$headerFiles = @(Get-FilteredFiles @("*.h", "*.hpp", "*.hxx"))
$sourceFiles = @(Get-FilteredFiles @("*.cpp", "*.cxx", "*.cc", "*.c"))
$allFiles = $headerFiles + $sourceFiles

if ($allFiles.Count -eq 0) {
    Write-Host "`nNo C/C++ files found in the specified path." -ForegroundColor Red
    exit
}

# ========== ファイル統計 ==========
Write-Header "FILE STATISTICS"

$hCount = $headerFiles.Count
$cppCount = $sourceFiles.Count
$totalFiles = $hCount + $cppCount

Write-Stat "Header files (.h/.hpp)" $hCount
Write-Stat "Source files (.cpp/.c)" $cppCount
Write-Stat "Total files" $totalFiles

# ========== 行数統計 ==========
Write-Header "LINE STATISTICS"

$totalLines = 0
$codeLines = 0
$blankLines = 0
$commentLines = 0

foreach ($file in $allFiles) {
    $content = Get-Content $file.FullName -ErrorAction SilentlyContinue
    if ($content) {
        $totalLines += $content.Count
        foreach ($line in $content) {
            $trimmed = $line.Trim()
            if ([string]::IsNullOrEmpty($trimmed)) {
                $blankLines++
            } elseif ($trimmed.StartsWith("//") -or $trimmed.StartsWith("/*") -or $trimmed.StartsWith("*")) {
                $commentLines++
            } else {
                $codeLines++
            }
        }
    }
}

Write-Stat "Total lines" ("{0:N0}" -f $totalLines)
Write-Stat "Code lines" ("{0:N0}" -f $codeLines)
Write-Stat "Comment lines" ("{0:N0}" -f $commentLines)
Write-Stat "Blank lines" ("{0:N0}" -f $blankLines)

# ========== クラス/構造体統計 ==========
Write-Header "CLASS / STRUCT STATISTICS"

$classCount = 0
$structCount = 0
$enumCount = 0

foreach ($file in $headerFiles) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($content) {
        # class定義（前方宣言とenum classを除外）
        $classMatches = [regex]::Matches($content, '^\s*class\s+(\w+)\s*[:{]', [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $classCount += $classMatches.Count

        # struct定義（前方宣言を除外）
        $structMatches = [regex]::Matches($content, '^\s*struct\s+(\w+)\s*[:{]', [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $structCount += $structMatches.Count

        # enum/enum class
        $enumMatches = [regex]::Matches($content, '^\s*enum\s+(class\s+)?(\w+)', [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $enumCount += $enumMatches.Count
    }
}

Write-Stat "Classes" $classCount
Write-Stat "Structs" $structCount
Write-Stat "Enums (including enum class)" $enumCount
Write-Stat "Total types" ($classCount + $structCount + $enumCount)

# ========== 関数統計 ==========
Write-Header "FUNCTION STATISTICS"

$memberFuncDefs = 0
$headerFuncDecls = 0

# cppファイル内のメンバ関数定義 (ClassName::FunctionName パターン)
foreach ($file in $sourceFiles) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($content) {
        $matches = [regex]::Matches($content, '^\w[\w:<>*&\s]+\s+[\w:]+::\w+\s*\([^;]*\)\s*(const)?\s*(noexcept)?\s*\{?', [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $memberFuncDefs += $matches.Count
    }
}

# ヘッダファイル内の関数宣言
foreach ($file in $headerFiles) {
    $content = Get-Content $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($content) {
        $matches = [regex]::Matches($content, '^\s*(virtual\s+)?(static\s+)?(inline\s+)?[\w:<>*&\s]+\s+\w+\s*\([^)]*\)\s*(const)?\s*(override)?\s*(noexcept)?\s*(=\s*(0|default|delete))?\s*;', [System.Text.RegularExpressions.RegexOptions]::Multiline)
        $headerFuncDecls += $matches.Count
    }
}

Write-Stat "Member function definitions" $memberFuncDefs
Write-Stat "Function declarations (headers)" $headerFuncDecls
Write-Stat "Estimated total functions" ([Math]::Max($memberFuncDefs, $headerFuncDecls))

# ========== サマリー ==========
Write-Header "SUMMARY"
Write-Host ""
Write-Host "  +----------------------------------+----------+" -ForegroundColor DarkCyan
Write-Host "  |  Metric                          |   Value  |" -ForegroundColor DarkCyan
Write-Host "  +----------------------------------+----------+" -ForegroundColor DarkCyan
Write-Host ("  |  Total Files                     | {0,8} |" -f $totalFiles) -ForegroundColor White
Write-Host ("  |  Total Lines                     | {0,8} |" -f ("{0:N0}" -f $totalLines)) -ForegroundColor White
Write-Host ("  |  Code Lines                      | {0,8} |" -f ("{0:N0}" -f $codeLines)) -ForegroundColor White
Write-Host ("  |  Classes + Structs               | {0,8} |" -f ($classCount + $structCount)) -ForegroundColor White
Write-Host ("  |  Functions                       | {0,8} |" -f ([Math]::Max($memberFuncDefs, $headerFuncDecls))) -ForegroundColor White
Write-Host "  +----------------------------------+----------+" -ForegroundColor DarkCyan
Write-Host ""

# 平均統計
if ($totalFiles -gt 0) {
    $avgLinesPerFile = [math]::Round($totalLines / $totalFiles, 1)
    Write-Host "  Average lines per file: $avgLinesPerFile" -ForegroundColor DarkGray
}

Write-Host ""
