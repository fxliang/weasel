@echo off

setlocal

if not exist env.bat copy env.bat.template env.bat

if exist env.bat call env.bat

if not defined WEASEL_ROOT set WEASEL_ROOT=%CD%

if not defined VERSION_MAJOR set VERSION_MAJOR=0
if not defined VERSION_MINOR set VERSION_MINOR=17
if not defined VERSION_PATCH set VERSION_PATCH=0

if not defined WEASEL_VERSION set WEASEL_VERSION=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_PATCH%
if not defined WEASEL_BUILD set WEASEL_BUILD=0

rem use numeric build version for release build
set PRODUCT_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%
rem for non-release build, try to use git commit hash as product build version
if not defined RELEASE_BUILD (
  rem check if git is installed and available, then get the short commit id of head
  git --version >nul 2>&1
  if not errorlevel 1 (
    for /f "delims=" %%i in ('git tag --sort=-creatordate ^| findstr /r "%WEASEL_VERSION%"') do (
      set LAST_TAG=%%i
      goto found_tag
    )
    :found_tag
    for /f "delims=" %%i in ('git rev-list %LAST_TAG%..HEAD --count') do (
      set WEASEL_BUILD=%%i
    )
    rem get short commmit id of head
    for /F %%i in ('git rev-parse --short HEAD') do (set PRODUCT_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%.%%i)
  )
)

rem FILE_VERSION is always 4 numbers; same as PRODUCT_VERSION in release build
if not defined FILE_VERSION set FILE_VERSION=%WEASEL_VERSION%.%WEASEL_BUILD%

echo PRODUCT_VERSION=%PRODUCT_VERSION%
echo WEASEL_VERSION=%WEASEL_VERSION%
echo WEASEL_BUILD=%WEASEL_BUILD%
echo WEASEL_ROOT=%WEASEL_ROOT%
echo WEASEL_BUNDLED_RECIPES=%WEASEL_BUNDLED_RECIPES%
echo.

if defined GITHUB_ENV (
  setlocal enabledelayedexpansion
  echo git_ref_name=%PRODUCT_VERSION%>>!GITHUB_ENV!
)

if defined BOOST_ROOT (
  if exist "%BOOST_ROOT%\boost" goto boost_found
)
echo Error: Boost not found! Please set BOOST_ROOT in env.bat.
exit /b 1

:boost_found
echo BOOST_ROOT=%BOOST_ROOT%
echo.

if not defined BJAM_TOOLSET (
  rem the number actually means platform toolset, not %VisualStudioVersion%
  set BJAM_TOOLSET=msvc-14.2
)

if not defined PLATFORM_TOOLSET (
  set PLATFORM_TOOLSET=v142
)

if defined DEVTOOLS_PATH set PATH=%DEVTOOLS_PATH%%PATH%

set build_config=Release
set build_option=/t:Build
set build_boost=0
set boost_build_variant=release
set build_data=0
set build_opencc=0
set build_rime=0
set rime_build_variant=release
set build_weasel=0
set build_installer=0
set build_arm64=0

rem parse the command line options
:parse_cmdline_options
  if "%1" == "" goto end_parsing_cmdline_options
  if "%1" == "debug" (
    set build_config=Debug
    set boost_build_variant=debug
    set rime_build_variant=debug
  )
  if "%1" == "release" (
    set build_config=Release
    set boost_build_variant=release
    set rime_build_variant=release
  )
  if "%1" == "rebuild" set build_option=/t:Rebuild
  if "%1" == "boost" set build_boost=1
  if "%1" == "data" set build_data=1
  if "%1" == "opencc" set build_opencc=1
  if "%1" == "rime" set build_rime=1
  if "%1" == "librime" set build_rime=1
  if "%1" == "weasel" set build_weasel=1
  if "%1" == "installer" set build_installer=1
  if "%1" == "arm64" set build_arm64=1
  if "%1" == "all" (
    set build_boost=1
    set build_data=1
    set build_opencc=1
    set build_rime=1
    set build_weasel=1
    set build_installer=1
    set build_arm64=1
  )
  shift
  goto parse_cmdline_options
:end_parsing_cmdline_options

if %build_weasel% == 0 (
if %build_boost% == 0 (
if %build_data% == 0 (
if %build_opencc% == 0 (
if %build_rime% == 0 (
  set build_weasel=1
)))))

rem quit WeaselServer.exe before building
cd /d %WEASEL_ROOT%
if exist output\weaselserver.exe (
  output\weaselserver.exe /q
)

rem build booost
if %build_boost% == 1 (
  call :build_boost
  if errorlevel 1 exit /b 1
  cd /d %WEASEL_ROOT%
)

rem -------------------------------------------------------------------------
rem build librime x64 and Win32
if %build_rime% == 1 (
  if not exist librime\build.bat (
    git submodule update --init --recursive
    if errorlevel 1 goto error
  )
  cd %WEASEL_ROOT%\librime
  if defined RIME_PLUGINS (
    pushd %WEASEL_ROOT%\librime
    call .\action-install-plugins-windows.bat || echo "action-install-plugins-windows.bat failed, continuing..."
    popd
  )
  rem build x64 librime
  call :build_librime_platform x64 %WEASEL_ROOT%\lib64 %WEASEL_ROOT%\output
  if errorlevel 1 goto error
  rem build Win32 librime
  call :build_librime_platform x86 %WEASEL_ROOT%\lib %WEASEL_ROOT%\output\Win32
  if errorlevel 1 goto error
)

rem -------------------------------------------------------------------------
if %build_weasel% == 1 (
  if not exist output\data\essay.txt (
    set build_data=1
  )
  if not exist output\data\opencc\TSCharacters.ocd* (
    set build_opencc=1
  )
)
if %build_data% == 1 call :build_data
if %build_opencc% == 1 call :build_opencc_data

if %build_weasel% == 0 goto end

cd /d %WEASEL_ROOT%

set WEASEL_PROJECT_PROPERTIES=BOOST_ROOT^
  PLATFORM_TOOLSET^
  VERSION_MAJOR^
  VERSION_MINOR^
  VERSION_PATCH^
  PRODUCT_VERSION^
  FILE_VERSION

cscript.exe render.js weasel.props %WEASEL_PROJECT_PROPERTIES%

del msbuild*.log

if %build_arm64% == 1 (

  msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="ARM" /fl6
  if errorlevel 1 goto error
  msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="ARM64" /fl5
  if errorlevel 1 goto error
)

msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="x64" /fl2
if errorlevel 1 goto error
msbuild.exe weasel.sln %build_option% /p:Configuration=%build_config% /p:Platform="Win32" /fl1
if errorlevel 1 goto error

if %build_arm64% == 1 (
  pushd arm64x_wrapper
  call build.bat
  if errorlevel 1 goto error
  popd

  copy arm64x_wrapper\weaselARM64X.dll output
  if errorlevel 1 goto error
  copy arm64x_wrapper\weaselARM64X.ime output
  if errorlevel 1 goto error
)

if %build_installer% == 1 (
  "%ProgramFiles(x86)%"\NSIS\Bin\makensis.exe ^
  /DWEASEL_VERSION=%WEASEL_VERSION% ^
  /DWEASEL_BUILD=%WEASEL_BUILD% ^
  /DPRODUCT_VERSION=%PRODUCT_VERSION% ^
  output\install.nsi
  if errorlevel 1 goto error
)

goto end

rem -------------------------------------------------------------------------
rem build boost
:build_boost
  set BJAM_OPTIONS_COMMON=-j%NUMBER_OF_PROCESSORS%^
    --with-filesystem^
    --with-json^
    --with-locale^
    --with-regex^
    --with-serialization^
    --with-system^
    --with-thread^
    define=BOOST_USE_WINAPI_VERSION=0x0603^
    toolset=%BJAM_TOOLSET%^
    link=static^
    runtime-link=static^
    --build-type=complete
  
  set BJAM_OPTIONS_X86=%BJAM_OPTIONS_COMMON%^
    architecture=x86^
    address-model=32
  
  set BJAM_OPTIONS_X64=%BJAM_OPTIONS_COMMON%^
    architecture=x86^
    address-model=64
  
  set BJAM_OPTIONS_ARM32=%BJAM_OPTIONS_COMMON%^
    define=BOOST_USE_WINAPI_VERSION=0x0A00^
    architecture=arm^
    address-model=32
  
  set BJAM_OPTIONS_ARM64=%BJAM_OPTIONS_COMMON%^
    define=BOOST_USE_WINAPI_VERSION=0x0A00^
    architecture=arm^
    address-model=64
  
  cd /d %BOOST_ROOT%
  if not exist b2.exe call bootstrap.bat
  if errorlevel 1 goto error
  b2 %BJAM_OPTIONS_X86% stage %BOOST_COMPILED_LIBS%
  if errorlevel 1 goto error
  b2 %BJAM_OPTIONS_X64% stage %BOOST_COMPILED_LIBS%
  if errorlevel 1 goto error
  
  if %build_arm64% == 1 (
    b2 %BJAM_OPTIONS_ARM32% stage %BOOST_COMPILED_LIBS%
    if errorlevel 1 goto error
    b2 %BJAM_OPTIONS_ARM64% stage %BOOST_COMPILED_LIBS%
    if errorlevel 1 goto error
  )
  exit /b

rem ---------------------------------------------------------------------------
:build_data
  copy %WEASEL_ROOT%\LICENSE.txt output\
  copy %WEASEL_ROOT%\README.md output\README.txt
  copy %WEASEL_ROOT%\plum\rime-install.bat output\
  set plum_dir=plum
  set rime_dir=output/data
  set WSLENV=plum_dir:rime_dir
  bash plum/rime-install %WEASEL_BUNDLED_RECIPES%
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------
:build_opencc_data
  if not exist %WEASEL_ROOT%\librime\deps_x64\share\opencc\TSCharacters.ocd2 (
    cd %WEASEL_ROOT%\librime
    call %WEASEL_ROOT%\msvc-latest.bat x64
    set build_dir=build_x64
    set deps_install_prefix=%WEASEL_ROOT%\librime\deps_x64
    set rime_install_prefix=%WEASEL_ROOT%\librime\dist_x64
    rem try ninja to build librime
    call :TraySetNinja
    cd %WEASEL_ROOT%\librime
    if not exist env.bat copy %WEASEL_ROOT%\env.bat env.bat
    call build.bat deps %rime_build_variant% 
    if errorlevel 1 goto error
  )
  cd %WEASEL_ROOT%
  if not exist output\data\opencc mkdir output\data\opencc
  copy %WEASEL_ROOT%\librime\deps_x64\share\opencc\*.* output\data\opencc\
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------
:TraySetNinja
    rem try ninja to build librime
    ninja --version >nul 2>&1
    rem ninja available
    if %errorlevel% equ 0 (
      echo Ninja is available
      cd %WEASEL_ROOT%
      xcopy /Y %WEASEL_ROOT%\env.bat %WEASEL_ROOT%\librime\
      powershell -Command "(Get-Content 'librime\env.bat') | Where-Object {$_ -notmatch '^.*set PLATFORM_TOOLSET=.*$'} | Set-Content 'librime\env.bat'"
      powershell -Command "(Get-Content 'librime\env.bat') | Where-Object {$_ -notmatch '^.*set CMAKE_GENERATOR=.*$'} | Set-Content 'librime\env.bat'"
      powershell -Command "(Get-Content 'librime\env.bat') | Where-Object {$_ -notmatch '^.*set ARCH=.*$'} | Set-Content 'librime\env.bat'"
      set CMAKE_GENERATOR=Ninja
      set ARCH=
      set PLATFORM_TOOLSET=
    )
    exit /b

rem ---------------------------------------------------------------------------
rem %1 : ARCH x86 or x64
rem %2 : target_path of rime.lib, base %WEASEL_ROOT% or abs path
rem %3 : target_path of rime.dll, base %WEASEL_ROOT% or abs path
:build_librime_platform
  call %WEASEL_ROOT%\msvc-latest.bat %1
  set build_dir=build_%1
  set deps_install_prefix=%WEASEL_ROOT%\librime\deps_%1
  set rime_install_prefix=%WEASEL_ROOT%\librime\dist_%1

  if "%1"=="x64" (
    rem try ninja to build librime
    call :TraySetNinja
  )

  cd %WEASEL_ROOT%\librime
  if not exist env.bat copy %WEASEL_ROOT%\env.bat env.bat

  if not exist %deps_install_prefix%\lib\opencc.lib (
    call build.bat deps %rime_build_variant%
    if errorlevel 1 goto error
  )
  call build.bat %rime_build_variant%
  if errorlevel 1 goto error

  cd %WEASEL_ROOT%\librime
  if "%1"=="x86" (
    call :copye %WEASEL_ROOT%\librime\dist_%1\include\rime_*.h %WEASEL_ROOT%\include\
  )
  call :copye %WEASEL_ROOT%\librime\dist_%1\lib\rime.lib %2\
  call :copye %WEASEL_ROOT%\librime\dist_%1\lib\rime.dll %3\
  call :copye %WEASEL_ROOT%\librime\dist_%1\lib\rime.pdb %3\

  exit /b
rem ---------------------------------------------------------------------------
rem %1 src
rem %2 dest
:copye
  xcopy /Y %1 %2
  if errorlevel 1 goto error
  exit /b

rem ---------------------------------------------------------------------------

:error

echo error building weasel...
exit 1

:end
cd %WEASEL_ROOT%
