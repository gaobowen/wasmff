#!/bin/bash

#set -eo pipefail

WORKPATH=$(cd $(dirname $0); pwd)

DEMO_PATH=$WORKPATH/demo

echo "WORKPATH"=$WORKPATH

rm -rf ${WORKPATH}/demo/mp4encoder.js ${WORKPATH}/demo/mp4encoder.wasm

FFMPEG_ST=yes

EMSDK=/emsdk

THIRD_DIR=${WORKPATH}/lib/third/build

DEBUG=""
DEBUG="-g -fno-inline -gseparate-dwarf=/code/demo/temp.debug.wasm -s SEPARATE_DWARF_URL=http://localhost:5000/temp.debug.wasm"

#--closure 压缩胶水代码，有可能会造成变量重复定义。生产发布可设为1
OPTIM_FLAGS="-O1 $DEBUG --closure 0"

if [[ "$FFMPEG_ST" != "yes" ]]; then
  EXTRA_FLAGS=(
    -pthread
    -s USE_PTHREADS=1                             # enable pthreads support
    -s PROXY_TO_PTHREAD=1                         # detach main() from browser/UI main thread
    -o ${DEMO_PATH}/mp4encoder.js
  )
else
  EXTRA_FLAGS=(
    -o ${DEMO_PATH}/mp4encoder.js
  )
fi

FLAGS=(
  -I$WORKPATH/lib/ffmpeg-emcc/include -L$WORKPATH/lib/ffmpeg-emcc/lib -I$THIRD_DIR/include -L$THIRD_DIR/lib
  -Wno-deprecated-declarations -Wno-pointer-sign -Wno-implicit-int-float-conversion -Wno-switch -Wno-parentheses -Qunused-arguments
  -lavdevice -lavfilter -lavformat -lavcodec -lswresample -lswscale -lavutil -lpostproc 
  -lm -lharfbuzz -lfribidi -lass -lx264 -lx265 -lvpx -lwavpack -lmp3lame -lfdk-aac -lvorbis -lvorbisenc -lvorbisfile -logg -ltheora -ltheoraenc -ltheoradec -lz -lfreetype -lopus -lwebp
  $DEMO_PATH/encode_v.c
  
  -s FORCE_FILESYSTEM=1
  -s WASM=1
  -s USE_SDL=2                                  # use SDL2
  -s INVOKE_RUN=0                               # not to run the main() in the beginning
  -s EXIT_RUNTIME=1                             # exit runtime after execution
  -s MODULARIZE=1                               # 延迟加载 use modularized version to be more flexible
  -s EXPORT_NAME="createMP4Encoder"             # assign export name for browser
  -s EXPORTED_FUNCTIONS="[_main,_malloc,_free]"  # export main and proxy_main funcs
  -s EXPORTED_RUNTIME_METHODS="[FS, cwrap, ccall, setValue, writeAsciiToMemory]"   # export preamble funcs
  -s INITIAL_MEMORY=134217728                   # 64 KB * 1024 * 16 * 2047 = 2146435072 bytes ~= 2 GB
  -s ALLOW_MEMORY_GROWTH=1
  --pre-js $WORKPATH/pre.js
  --post-js $WORKPATH/post.js
  $OPTIM_FLAGS
  ${EXTRA_FLAGS[@]}
)
echo "FFMPEG_EM_FLAGS=${FLAGS[@]}"

emcc "${FLAGS[@]}"
