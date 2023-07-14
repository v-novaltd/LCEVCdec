#!

ffmpeg -y -f lavfi -i "testsrc=duration=2.0:size=qcif:rate=30" -vcodec rawvideo -pix_fmt yuv420p test_176x144_p420.yuv

ffmpeg -y -f rawvideo -pix_fmt yuv420p -s 176x144 -r 30.0 -i test_176x144_p420.yuv -c:v libx264 -b:v 200k test_176x144_h264.ts
ffmpeg -y -f rawvideo -pix_fmt yuv420p -s 176x144 -r 30.0 -i test_176x144_p420.yuv -c:v libx265 -b:v 200k test_176x144_h265.ts

ffmpeg -y -f rawvideo -pix_fmt yuv420p -s 176x144 -r 30.0 -i test_176x144_p420.yuv -c:v lcevc_h264 -base_encoder x264 -b:v 200k test_176x144_lcevc_h264.ts
ffmpeg -y -f rawvideo -pix_fmt yuv420p -s 176x144 -r 30.0 -i test_176x144_p420.yuv -c:v lcevc_h264 -base_encoder x264 -b:v 200k test_176x144_lcevc_h264.h264

ffmpeg -y -f rawvideo -pix_fmt yuv420p -s 176x144 -r 30.0 -i test_176x144_p420.yuv -c:v lcevc_hevc -base_encoder x265 -b:v 200k test_176x144_lcevc_h265.ts
ffmpeg -y -f rawvideo -pix_fmt yuv420p -s 176x144 -r 30.0 -i test_176x144_p420.yuv -c:v lcevc_hevc -base_encoder x265 -b:v 200k test_176x144_lcevc_h265.h265
