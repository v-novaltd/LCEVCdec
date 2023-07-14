@echo off

set script_dir="%~dp0"

set cwd="%cd%"

python --version
python -m venv env
call env\Scripts\activate.bat
cd %script_dir%
pip install --extra-index-url "https://%NEXUS_USER%:%NEXUS_PASSWORD%@nexus.dev.v-nova.com/repository/pypi-all/simple" -r ..\..\requirements.txt
pip install --extra-index-url "https://%NEXUS_USER%:%NEXUS_PASSWORD%@nexus.dev.v-nova.com/repository/pypi-all/simple" -r requirements.txt
cd %cwd%

if "%NUMBER_OF_PROCESSORS%" == "" set NUMBER_OF_PROCESSORS=1

python -m pytest %~dp0\\tests^
    --bin-dir %~dp0\\..\\..\\build\\x64\\Windows\\Debug^
    --resource-dir %ASSETS%^
    -rfE -n %NUMBER_OF_PROCESSORS% --basetemp cpu^ :: -m "not slow and not performance and not gpu"^
    --conan-profile vs-2019-MT-Release^
    %* || goto :error

goto :EOF

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%