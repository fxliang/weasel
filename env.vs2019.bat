rem Customize your build environment and save the modified copy to env.bat
set WEASEL_ROOT=%CD%

rem REQUIRED: path to Boost source directory
if not defined BOOST_ROOT set BOOST_ROOT=%WEASEL_ROOT%\deps\boost_1_78_0

rem OPTIONAL: Visual Studio version and platform toolset
set BJAM_TOOLSET=msvc-14.2
set PLATFORM_TOOLSET=v142

rem OPTIONAL: path to additional build tools
rem set DEVTOOLS_PATH=%ProgramFiles%\Git\cmd;%ProgramFiles%\Git\usr\bin;%ProgramFiles%\CMake\bin;C:\Python27;
