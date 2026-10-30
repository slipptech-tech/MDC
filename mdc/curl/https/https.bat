
@echo off
setlocal EnableExtensions
title MDC HTTPS Client

set "API=https://api.github.com/repos/slipptech-tech/MDC"
set "OUTPUT=MDC-response.json"

echo.
echo  
echo       mdc https
echo  
echo.
echo  [INFO] Initializing HTTPS connection...
echo  [INFO] Target: github.com
echo  [INFO] Protocol: HTTPS / TLS
echo.

curl.exe --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CURL is not available.
    pause
    exit /b 1
)

echo [INFO] Sending GET request...
echo.

curl.exe --location ^
    --request GET ^
    "%API%" ^
    --header "Accept: application/vnd.github+json" ^
    --header "User-Agent: MDC-Client/1.0" ^
    --header "X-GitHub-Api-Version: 2022-11-28" ^
    --connect-timeout 15 ^
    --max-time 60 ^
    --retry 3 ^
    --silent ^
    --show-error ^
    --output "%OUTPUT%" ^
    --write-out "HTTP Status: %%{http_code}\nTime: %%{time_total}s\n"

if errorlevel 1 (
    echo.
    echo [ERROR] HTTPS request failed.
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Response saved to %OUTPUT%
echo [INFO] Connection completed.
echo.

pause
