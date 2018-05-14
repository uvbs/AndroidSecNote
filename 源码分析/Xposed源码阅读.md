# Xposed源码阅读
## Xposed项目结构
- XposedInstaller，这是Xposed的插件管理和功能控制APP，也就是说Xposed整体管控功能就是由这个APP来完成的，它包括启用Xposed插件功能，下载和启用指定插件APP，还可以禁用Xposed插件功能等。注意，这个app要正常无误得运行必须能拿到root权限。
- Xposed，这个项目属于Xposed框架，其实它就是单独搞了一套xposed版的zygote。这个zygote会替换系统原生的zygote。所以，它需要由 XposedInstaller在root之后放到/system/bin下。
- XposedBridge，这个项目也是Xposed框架，它属于Xposed框架的Java部分，编译出来是一个XposedBridge.jar包。
- XposedTools，Xposed和XposedBridge编译依赖于Android源码，而且还有一些定制化的东西。所以XposedTools就是用来帮助我们编译Xposed和XposedBridge的。

## Xposed工程
Xposed核心原理就是实现了自己的Zygote进程，源码结构如下：
- app_main.cpp: app_process程序入口，sdk_version < 21
- app_main2.cpp: app_process程序入口，sdk_version >= 21



### app_main2.cpp
Zygote可以创建三种进程：Zygote进程，system_server进程， 应用程序进程。Xposed在Zygote处于创建Zygote进程时介入。

``` cpp
int main(int argc, char* const argv[])
{
    //节选
    if (zygote) {
        //xposed介入Zygote创建过程
        isXposedLoaded = xposed::initialize(true, startSystemServer, NULL, argc, argv);
        runtimeStart(runtime, isXposedLoaded ? XPOSED_CLASS_DOTS_ZYGOTE : "com.android.internal.os.ZygoteInit", args, zygote);
    } else if (className) {
        isXposedLoaded = xposed::initialize(false, false, className, argc, argv);
        runtimeStart(runtime, isXposedLoaded ? XPOSED_CLASS_DOTS_TOOLS : "com.android.internal.os.RuntimeInit", args, zygote);
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
        return 10;
    }
}

