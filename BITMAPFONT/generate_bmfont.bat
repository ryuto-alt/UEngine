@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo =========================================
echo   BMFont Generator - UnoEngine
echo =========================================
echo.

set "TTF_PATH=%~1"
set "FONT_SIZE=%~2"
set "CHARSET=%~3"
set "OUTPUT_NAME=%~4"

:: TTF未指定 → ヘルプ表示
if "%TTF_PATH%"=="" goto :SHOW_HELP

:: 拡張子チェック
set "EXT=%~x1"
if /i "%EXT%"==".ttf" goto :EXT_OK
if /i "%EXT%"==".ttc" goto :EXT_OK
if /i "%EXT%"==".otf" goto :EXT_OK
echo [ERROR] TTF/TTC/OTF ファイルを指定してください: %~nx1
echo.
pause
exit /b 1

:EXT_OK
:: ファイル存在チェック
if not exist "%TTF_PATH%" goto :FILE_NOT_FOUND

echo Font: %~nx1
echo.

:: フォントサイズ未指定 → 対話入力
if not "%FONT_SIZE%"=="" goto :SIZE_OK
set /p "FONT_SIZE=フォントサイズ (px) [32]: "
if "!FONT_SIZE!"=="" set "FONT_SIZE=32"

:SIZE_OK
:: 数値チェック
set /a "_check=%FONT_SIZE%" 2>nul
if %_check% LEQ 0 goto :BAD_SIZE

:: 文字セット未指定 → 対話選択
if not "%CHARSET%"=="" goto :CHARSET_OK
echo 文字セット:
echo   1. kanji  - ASCII + かな + 漢字 (CJK全域)
echo   2. kana   - ASCII + ひらがな + カタカナ
echo   3. ascii  - ASCII のみ
echo.
set /p "CHOICE=選択 [1]: "
if "!CHOICE!"=="" set "CHOICE=1"
if "!CHOICE!"=="1" set "CHARSET=kanji"
if "!CHOICE!"=="2" set "CHARSET=kana"
if "!CHOICE!"=="3" set "CHARSET=ascii"
if "!CHARSET!"=="" set "CHARSET=kanji"

:CHARSET_OK
echo.
echo --- 設定 ---
echo   Font    : %~nx1
echo   Size    : %FONT_SIZE%px
echo   Charset : %CHARSET%
echo -------------
echo.

set "SCRIPT_DIR=%~dp0"

powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%generate_bmfont.ps1" -TtfPath "%TTF_PATH%" -FontSize %FONT_SIZE% -Charset "%CHARSET%" -OutputName "%OUTPUT_NAME%"

if %ERRORLEVEL% NEQ 0 goto :FAILED

echo.
echo [OK] ビットマップフォント生成完了
echo Output: %SCRIPT_DIR%output\
echo.
pause
exit /b 0

:: ============================================================
::  エラーハンドラ
:: ============================================================

:SHOW_HELP
echo Usage: generate_bmfont.bat [ttf_path] [font_size] [charset] [output_name]
echo.
echo   TTF/TTC/OTF をドラッグ＆ドロップするだけでも OK
echo.
echo   font_size : ピクセルサイズ (省略時は対話入力, デフォルト 32)
echo   charset   : ascii / kana / kanji (省略時は対話選択)
echo   ascii     - ASCII (32-126)
echo   kana      - ASCII + ひらがな + カタカナ + 句読点
echo   kanji     - kana + CJK統合漢字 (U+4E00-U+9FFF)
echo.
pause
exit /b 1

:FILE_NOT_FOUND
echo [ERROR] ファイルが見つかりません: %TTF_PATH%
echo.
pause
exit /b 1

:BAD_SIZE
echo [ERROR] フォントサイズは正の整数で指定してください: %FONT_SIZE%
echo.
pause
exit /b 1

:FAILED
echo.
echo [ERROR] 生成に失敗しました。
echo.
pause
exit /b 1
