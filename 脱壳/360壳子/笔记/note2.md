## 前言
数字壳子最重要的部分还是自己实现的虚拟机，所以分析下其虚拟机的实现。


## 问题

1. 壳在执行完初始化工作后，如何将程序入口设置到源程序
2. 现在程序有两个虚拟机，一个系统本身的art虚拟机，一个是NUM_VM，这两个虚拟机如何分工，协调工作。
2. 360vm是如何装载dex，加载类的


## 类加载模块
数字壳加载dex，以及将dex与app的classloader绑定步骤
long sub_1401C(JNIEnv *a1, const char *dexPath, const char *oatPath, signed int oatVersion)
void sub_117A0(JNIEnv *a1, int num, int mCookie, int oatVersion)

1. sub_1410c加载dex
2. sub_117A0将加载的dex与app的classloader绑定
    1. com/stub/StupApp.getClassLoader 获取加载app的classLoader，称为ACalssLoader吧。
    2. 获取AClassLoader对应的DexPathList
    2. 获取到该DexPathList的DexElements
    3. 取出DexElements的第一个元素Element
    4. 取出该Element中的DexFile
    5. 取出该DexFile的 mCookie，以及 mFileName
    6. 调用DexFile.loadDex(mFileName)新建一个 DexFile结构
    7. 将新建的DexFile中的mCookie替换为加载源app的mCookie
    8. 新建Element
    9. 新建DexElelments，该DexElements[0]为新建的Element， DexElements[1]为原来DexElements[0]
    10. 将AClassloader的DexPathlist对应的Dexelements替换为新建的Element
