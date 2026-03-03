@echo off
:: Build script for XplDrToUdp X-Plane plugin
:: Builds the Debug|x64 configuration (required for X-Plane plugins)

:: Force English output from MSBuild (overrides system locale)
set VSLANG=1033
set PreferredUILang=en-US

set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set PROJECT=XplDrToUdp.vcxproj
set CONFIG=Debug
set PLATFORM=x64

:: Parse optional arguments
if "%1"=="release" (
    set CONFIG=Release
)
if "%1"=="clean" (
    echo Cleaning build output...
    %MSBUILD% %PROJECT% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:PreferredUILang=en-US /t:Clean
    exit /b %ERRORLEVEL%
)

echo.
echo ========================================
echo  XplDrToUdp Build
echo  Config : %CONFIG%^|%PLATFORM%
echo ========================================
echo.

%MSBUILD% %PROJECT% /p:Configuration=%CONFIG% /p:Platform=%PLATFORM% /p:PreferredUILang=en-US /t:Build /m /nologo

if %ERRORLEVEL% == 0 (
    echo.
    echo ========================================
    echo  BUILD SUCCEEDED
    echo  Output: x64\%CONFIG%\win.xpl
    echo ========================================
) else (
    echo.
    echo ========================================
    echo  BUILD FAILED  (exit code %ERRORLEVEL%)
    echo ========================================
)

exit /b %ERRORLEVEL%
