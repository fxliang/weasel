setlocal

if not defined RIME_ROOT set RIME_ROOT=%CD%

if not defined boost_version set boost_version=1.90.0
set boost_x_y_z=%boost_version:.=_%

if not defined BOOST_ROOT set BOOST_ROOT=%RIME_ROOT%\deps\boost_%boost_x_y_z%

if not exist "%RIME_ROOT%\deps" ( mkdir %RIME_ROOT%\deps )
if exist "%BOOST_ROOT%\boost" goto boost_found
for %%I in ("%BOOST_ROOT%\.") do set src_dir=%%~dpI
rem download boost source with powershell
powershell -Command "Invoke-WebRequest -Uri 'https://archives.boost.io/release/%boost_version%/source/boost_%boost_x_y_z%.7z' -OutFile '%src_dir%\boost_%boost_x_y_z%.7z' -UseBasicParsing"
pushd %src_dir%
7z x boost_%boost_x_y_z%.7z
popd
:boost_found

call .\build.bat boost
