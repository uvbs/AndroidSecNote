# dex注入技术
有时候需要注入的代码要完成的一些事情，c/c++不好实现，比如要设置目标app的一些组件属性，这时候用java代码就很方便。所以需要用到dex注入技术
## dex注入流程
1. 将java代码编译为dex文件
2. 编写用来加载java的so
3. 向目标进程注入上一步中用来加载java代码的so


## 将java代码编译为dex
1. javac 将java代码编译为class文件
2. 利用dx工具将class文件编译为dex

``` bash
javac -encoding UTF-8 -source 1.7 -target 1.7 -bootclasspath E:\AS_SDK\platforms\android-25/android.jar ./com/expample/tools/*.java
E:\AS_SDK\build-tools\25.0.2\dx.bat --dex --output=./inject.dex ./com/expample/tools/*
```

## 编写用来加载dex的so
1. 获取jni_env
2. 创建DexClassLoader加载dex
3. 利用DexClassLoader.loadClass加载所需类
4. 调用加载类中方法

## 注入用来加载dex的so
详见so注入