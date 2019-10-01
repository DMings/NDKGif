# android ndk使用giflib高性能显示gif
ndk giflib转bitmap显示完整使用示例，非DGifSlurp缓慢方式，包含从asset读取、外部目录读取方式，速度秒开，内存占用小
![在这里插入图片描述](https://github.com/DMings/NDKGif/blob/master/show-gif/demo.gif)
### 什么是giflib?
giflib是使用c源码编写的一个开源库，在android源码中已被使用的[android external](http://androidxref.com/9.0.0_r3/xref/external/giflib/)。
要学习如何用giflib在ndk层次进行gif解码，必须要了解gif格式，不然是不知道库如何用的，学习gif格式：[gif格式](https://blog.csdn.net/poisx/article/details/79122506)。
gif格式也没有想象中那么困难，它就是颜色表和数据索引的组合，有全局颜色表和每帧数据的颜色表，每帧数据记录的不是RGB而是颜色表中的索引，解码帧的时候是拿索引查询颜色表中的数据，然后就能知道那个像素点的颜色。帧与帧之间延时的单位是10ms，最为注意是透明像素问题，不理解会出现奇奇怪怪的问题。
### 写这个giflib意义
网上也有不少的giflib显示gif库，但是也就是简单使用，千篇一律，基本都是使用DGifSlurp进行解码，DGifSlurp用法虽然简单，但是缺点也是很明显，需要一次性把gif解码完，这个过程是耗时不少的，如果gif比较大，十几M，耗时是相当明显的，不能做到秒开效果。所以要实现高性能显示就必须要清楚解码流程，使用while循环结合DGifGetRecordType函数进行解码。
### giflib中gif解码方式
giflib中对gif的打开方式有三种：

 1. DGifOpenFileName(const char *GifFileName, int *Error)
 2. DGifOpenFileHandle(int GifFileHandle, int *Error)
 3. DGifOpen(void *userPtr, InputFunc readFunc, int *Error)
 
第一种根据路径打开文件，第二种是用文件句柄打开，第一和第二种打开方式都是差不多的。
第三种就不同了可以根据InputFunc这个回调回来的数据进行解码，灵活很多。也就是说giflib的解码可以从文件中读取进行解码，也可以从内存中读取，当然也可以是流。
giflib中对gif的解码流程有两种方法，第一种是用while循环结合DGifGetRecordType函数，根据gif储存格式进行解码，第二种是用高阶函数DGifSlurp，一次性把gif进行解码，然后输出解码后的数据，也就是对第一种流程进行封装摆了。
### 使用giflib从asset目录中读取文件，或者用内存中读取
要实现从网络下载显示，或者从流中显示，就需要用到：DGifOpen(void *userPtr, InputFunc readFunc, int *Error)，以下就是asset中读取示例：
asset传入GifFile->UserData中，然后读取流。内存读取也是差不多就不在叙述。
```
int fileRead(GifFileType *gif, GifByteType *buf, int size) {
    AAsset *asset = (AAsset *) gif->UserData;
    return AAsset_read(asset, buf, (size_t) size);
}

AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
AAsset *asset = AAssetManager_open(mgr, filename, AASSET_MODE_STREAMING);
DGifOpen(asset, fileRead, &Error));
```
### 如何使用giflib实现秒开，高性能
要实现秒开，不能使用DGifSlurp方式，需要用while循环结合DGifGetRecordType函数，接收到帧的时候就要将其进行显示；要实现性能，不能每次都要读取文件进行显示，在一开始读取文件的时候要把每帧数据缓存起来，当然缓存的不是RGB数据，这样数据量会很大，缓存也只是颜色表和颜色索引，同时记录延时等少量信息，这样内存会少很多。

### 演示：
#### 秒开：
![在这里插入图片描述](https://github.com/DMings/NDKGif/blob/master/show-gif/test_demo.gif)
#### 不透明背景：
![在这里插入图片描述](https://github.com/DMings/NDKGif/blob/master/show-gif/mao.gif)
#### 透明背景：
![在这里插入图片描述](https://github.com/DMings/NDKGif/blob/master/show-gif/mogutou.gif)
