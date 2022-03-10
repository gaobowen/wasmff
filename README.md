# media-wasm
## Introduction
https://developer.mozilla.org/zh-CN/docs/WebAssembly/Concepts  

## Webassembly api
https://emscripten.org/docs/api_reference/index.html

# 编译环境
https://hub.docker.com/r/emscripten/emsdk/tags  
拉取远程镜像
```bash
docker pull emscripten/emsdk:2.0.24
```

# 启动容器
映射自己的目录
```bash
docker run -d -it --name mediawasm -v d:/.../media-wasm:/code  emscripten/emsdk:2.0.24 /bin/bash
```

# 编译
```bash
./build-demo.sh
``` 
会在项目目录下生成 ```mp4encoder.js``` 与 ```mp4encoder.wasm``` 其中js文件文件为胶水代码，使得C++接口能在浏览器js中识别。实际的C++逻辑存放于wasm文件中。


在容器外的项目目录下, 启动web静态服务器查看效果

# 调试
介绍  
https://developer.chrome.com/blog/wasm-debugging-2020/  
下载安装插件  
https://goo.gle/wasm-debugging-extension  
 
1、打开开发者工具，设置(F1) -> 实验 -> 勾选 WebAssembly Debug
![](./pics/debug1.png) 
2、因为项目在Docker下编译，需要把Docker路径映射到本地磁盘
![](./pics/debug2.png) 
![](./pics/debug3.png)
3、打开开发者工具 -> 源代码 -> 左侧file://下即可找到源代码 -> 设置断点 -> 右侧监视添加变量 
![](./pics/debug4.png)



# 参数传递
## 简单参数传递
```js
  //把js string 转成 utf8 array
  var getCStringPtr = function (jstr) {
    var lengthBytes = lengthBytesUTF8(jstr) + 1;
    var p = MP4Encoder._malloc(lengthBytes);
    stringToUTF8(jstr, p, lengthBytes);
    return p;
  }
  //allocateUTF8功能相同

  //值类型可以直接传递，string必须先转array
  var strPtr = getCStringPtr("/tmp/demo2.mp4");
  var ret = MP4Encoder._createH264(strPtr, 1920, 1080, 25);
```
## 大块内存拷贝
```js
    //js 数组内存
    let fileBuffer = new Uint8Array(imagedata.data.buffer);
    //wasm 数组内存
    let bufferPtr = MP4Encoder._malloc(fileBuffer.length);
    //js -> wasm
    MP4Encoder.HEAP8.set(fileBuffer, bufferPtr);
    var ret = MP4Encoder._addFrame(bufferPtr);
```

```_malloc``` 和 ```_free``` 这些系统方法是模块默认导出的。如果想查看其他方法是否可用，可以控制台打印MP4Encoder模块看其挂载的方法

# 文件读写
```html
<input type="file" value="选择文件" onchange="inputJsFile(event)"></input>
```
```js
  var inputJsFile = function (event) {
    let file = event.target.files[0];

    file.arrayBuffer().then(t=>{
      console.log(t)
      //创建文件夹
      FS.mkdir('/working');
      //写入二进制数据到wasm虚拟目录
      FS.writeFile('/working/input.txt', new Uint8Array(t), { flags:'w+' });
      //查看写入成功
      console.log(FS.stat('/working/input.txt'))
      //从wasm读取到js
      var buff = FS.readFile('/working/input.txt', { encoding: 'binary' });
      console.log(buff)
      var pStr = getCStringPtr("/working/input.txt");
      var ret = MP4Encoder._openTestFile(pStr);
    });
  }
```