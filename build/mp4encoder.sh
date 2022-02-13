# replace it with your own path
EMSDK=/Users/gaobowen/Desktop/src/blog/emsdk

WORKPATH=$(pwd)

FFMPEG_PATH=$(pwd)/ffmpeg-4.3.1

X264_PATH=$(pwd)/x264

export TOTAL_MEMORY=33554432

cd ${EMSDK}

./emsdk activate latest

source ./emsdk_env.sh

echo 'build MP4Encoder'

cd ${WORKPATH}

rm -rf ../bin/mp4encoder.js ../bin/mp4encoder.wasm

emcc -s EXPORT_NAME="'MP4Encoder'" \
  -O3 \
  -s FORCE_FILESYSTEM=1 \
  -s WASM=1 \
  -s TOTAL_MEMORY=${TOTAL_MEMORY} \
  -s ASSERTIONS=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s LLD_REPORT_UNDEFINED \
  ${WORKPATH}/encode_v.c \
  ${FFMPEG_PATH}/lib/libavformat.a \
  ${FFMPEG_PATH}/lib/libavcodec.a \
  ${FFMPEG_PATH}/lib/libswscale.a \
  ${FFMPEG_PATH}/lib/libavutil.a \
  ${X264_PATH}/lib/libx264.a \
  -I "${FFMPEG_PATH}/include" \
  -o ../bin/mp4encoder.js
