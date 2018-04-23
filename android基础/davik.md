# Dalvik虚拟机学习笔记
## 前言
学习Dalvik虚拟机最主要的原因就是之前分析了下数字公司的vmp壳子，想知道虚拟机在拿到一个方法的DexCode后，如何解释执行这个方法。这篇笔记也是围绕这个问题写的。分析的源码版本为：android-4.0.4

## 概述
在Dalvik模式下，android系统中运行的每一个应用程序都需要对应于一个Dalvik虚拟机实例，因此在运行应用程序之前要创建好虚拟机。而创建虚拟机实例的任务由Zygote进程负责。
Zygote是一个虚拟机进程，也是一个虚拟机实例的孵化器。它在系统启动时运行，完成虚拟机的初始化，库的加载等操作。当系统要求执行一个android应用程序，Zygote就会fork出一个子程序来执行该应用程序，将自己的Dalvik虚拟机实例复制到应用程序进程中去，用来解释执行应用程序。
总的来说，一个应用程序被顺利执行，要有下面三个必须的步骤：

1. Zygote进程的创建
2. Zygote进程孵化出用来执行应用程序的Dalvik虚拟机进程
3. 虚拟机实例执行应用程序

## Zygote进程创建
参考老罗的源码分析：

[Android系统进程Zygote启动过程的源代码分析（1）](http://blog.51cto.com/shyluo/966541)

[Android系统进程Zygote启动过程的源代码分析（2）](http://blog.51cto.com/shyluo/966546)

总结Zygote的创建与工作流程如下：
1. init进程创建Zygote进程，用来执行app_process程序。
2. app_process生成AppRuntime对象并调用其start方法。在start方法中完成如下三件事：
    1. 调用startVM创建Dalvik虚拟机。
    2. 调用startReg注册JNI函数。
    3. 利用前两步创建的虚拟机与jNI环境，执行ZygoteInit类的main函数。

3. ZygoteInit类实现了Zygote的核心功能，其执行流程如下：
    1. 调用registerZygoteSocket函数创建了socket接口，绑定了socket套接字，用来接收孵化新app的请求。
    2. 调用preload函数加载Android Framework中常用的类和资源。
    3. 调用startSystemServer函数启动SystemServer组件。
    4. 调用runSelectLoopMode函数进入循环，监听socket端口等待孵化新app的请求。

## Zygote进程孵化出Dalvik虚拟机进程
Zygote进程可以孵化出三种进程：
1. 应用程序进程
2. system_server进程
3. Zygote子进程

这里我们只关心孵化android应用程序。Zyogte进程通过dalvik.system.Zygote类的静态成员函数forkAndSpecialize来创建新Android应用程序进程的，有如下三个步骤：

1. Zygote.forkAndSpecialize
2. forkAndSpecializeCommon fork创建虚拟机进程
3. dvmInitAfterZygote 初始化拷贝自Zygote的虚拟机实例

因为虚拟机进程为Zygote进程fork出来的子进程，所以虚拟机进程在创建之初就可以和Zygote进程共享很多Java和Android核心类库（dex文件）及其JNI方法（so文件），只需在必要时(copy on write), 复制出自己的一份。
