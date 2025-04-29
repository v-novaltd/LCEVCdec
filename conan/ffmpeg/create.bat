@echo off
setlocal

conan profile list
if %ERRORLEVEL% neq 0 (
    exit /b %ERRORLEVEL%
)
echo Enter a profile from the above list, leave blank for default native release build
set /p "profile=>"

if "%profile%"=="" (
    set "profile=default"
)

conan create conan/ffmpeg/conanfile.py ffmpeg/7.1@ --profile %profile% --build=missing -o shared=True -o postproc=False -o with_asm=False -o with_zlib=False -o with_bzip2=False -o with_lzma=False -o with_libiconv=False -o with_freetype=False -o with_openjpeg=False -o with_openh264=False -o with_opus=False -o with_vorbis=False -o with_zeromq=False -o with_sdl=False -o with_libx264=False -o with_libx265=False -o with_libvpx=False -o with_libmp3lame=False -o with_libfdk_aac=False -o with_libwebp=False -o with_ssl=False -o with_programs=False

endlocal
pause
