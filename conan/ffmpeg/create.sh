echo "Building FFmpeg via conan, please enter your OS to select the correct build options"
echo "1) Linux"
echo "2) Apple (macOS, iOS, tvOS)"
echo "3) Other (Android, Windows or anything else)"
read -p ">" choice

conan profile list || exit
echo "Enter a profile from the above list, leave blank for default native release build"
read -p ">" profile

if [[ "${profile}" == "" ]]; then
  profile=default
fi

case $choice in
    1)
        conan create conan/ffmpeg/conanfile.py ffmpeg/7.1@ --profile:host ${profile} --profile:build=default --build=missing -o shared=True -o postproc=False -o with_asm=False -o with_zlib=False -o with_bzip2=False -o with_lzma=False -o with_libiconv=False -o with_freetype=False -o with_openjpeg=False -o with_openh264=False -o with_opus=False -o with_vorbis=False -o with_zeromq=False -o with_sdl=False -o with_libx264=False -o with_libx265=False -o with_libvpx=False -o with_libmp3lame=False -o with_libfdk_aac=False -o with_libwebp=False -o with_ssl=False -o with_programs=False -o with_libalsa=False -o with_pulse=False -o with_vaapi=False -o with_vdpau=False -o with_vulkan=False -o with_xcb=False
        ;;
    2)
        conan create conan/ffmpeg/conanfile.py ffmpeg/7.1@ --profile:host ${profile} --profile:build=default --build=missing -o shared=True -o postproc=False -o with_asm=False -o with_zlib=False -o with_bzip2=False -o with_lzma=False -o with_libiconv=False -o with_freetype=False -o with_openjpeg=False -o with_openh264=False -o with_opus=False -o with_vorbis=False -o with_zeromq=False -o with_sdl=False -o with_libx264=False -o with_libx265=False -o with_libvpx=False -o with_libmp3lame=False -o with_libfdk_aac=False -o with_libwebp=False -o with_ssl=False -o with_programs=False -o with_coreimage=False -o with_audiotoolbox=False -o with_videotoolbox=False
        ;;
    3)
        conan create conan/ffmpeg/conanfile.py ffmpeg/7.1@ --profile:host ${profile} --profile:build=default --build=missing -o shared=True -o postproc=False -o with_asm=False -o with_zlib=False -o with_bzip2=False -o with_lzma=False -o with_libiconv=False -o with_freetype=False -o with_openjpeg=False -o with_openh264=False -o with_opus=False -o with_vorbis=False -o with_zeromq=False -o with_sdl=False -o with_libx264=False -o with_libx265=False -o with_libvpx=False -o with_libmp3lame=False -o with_libfdk_aac=False -o with_libwebp=False -o with_ssl=False -o with_programs=False
        ;;
    *)
        echo "Invalid choice. Please enter 1, 2, or 3."
        ;;
esac
