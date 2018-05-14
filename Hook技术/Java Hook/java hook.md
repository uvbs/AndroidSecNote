## java hook
hook的本质是对方法调用的拦截，要想从高的层次看java hook，那么必须对java方法有深入的理解。我们知道在c的世界里，方法就是一段汇编代码，方法调用就是按照函数调用约定，放函数参数(压入栈中，或者放入寄存器)，设置返回值（压栈，或者放入LR寄存器), 设置pc跳转到调用的方法执行代码。那么对于java世界有如下几个问题：

1. java方法具体的表现形式是什么
2. java是如何完成实现方法调用的
3. 如何介入上面过程，完成hook

这篇笔记主要是依托于android6.0源码，在ART模式下解答上面的问题。

## java方法的表现形式
在编译型语言中，编译后的方法就是一段机器码，执行方法就是执行这段机器码。对于java这种基于虚拟机的语言，表现形式就复杂了很多。一段代码在android ART中被执行要经过如下过程：

java源码 -> dex -> oat -> ART加载oat执行方法

在不同的阶段，java方法有不同的表现形式，下面依次叙述在不同阶段java方法的表现形式。

### java方法在dex文件中的表现形式
dex文件是Android Dalvik虚拟机的可执行文件。dex文件中除了保存方法的代码外，还利用其他数据结构来描述java方法。具体如下：
``` cpp
struct DexMethod {
    u4 methodIdx;       //指向 DexMethodId的索引，具体描述了方法所在的类，方法的声明类型，方法名称
    u4 accessFlags;     //方法的访问标志
    u4 codeOff;         //指向Dexcode结构，具体描述了方法使用的寄存器个数，以及具体方法的指令集等信息
};
struct DexMethodId {
    u2 classIdx;        //描述方法所属的类
    u2 protoIdx;        //方法声明类型
    u4 nameIdx;         //方法名
};
struct DexCode {
    u2 registerSize;    //方法使用寄存器个数
    u2 insSize;         //方法参数个数
    u2 outsSize;        //调用其他方法使用的寄存器个数
    u2 triesSize;       //Try/Catch 个数
    u4 debugInfoOff;    //调试信息
    u4 insnsSize;       //指令集长度，2字节为单位
    u2 insns[1];        //指令集
};
```
上面这些结构很详尽的描述了一个方法的信息，Dalvik虚拟机通过解析上面结构，就可以解释执行一个方法。

### java方法在oat文件中的表现形式
ART虚拟机相比较Dalvik虚拟机最大的区别就是ART虚拟机会在程序运行前，将程序中的Dalvik指令转换为本地指令，这样程序的运行速度会大大提升。oat文件是ART虚拟机的可执行文件，是由dex文件加工而来，oat文件中包含着完整的dex文件，除此之外oat文件还增添了一些结构来描述dex文件，描述dex文件中的类以及方法。
``` cpp
//android6.0 oat版本064
struct OatMethodOffsets {
    u4 code_offset;  //方法编译后的native code 所在偏移。
};
```
每个被编译的方法，都对应有这样一个结构，记录了该方法对应的本地机器指令的偏移。

### java方法在ART虚拟机中的表现形式
在看完java方法在dex文件和oat文件的静态表现形式，接下来就看看java方法在被ART虚拟机加载后的表现形式。ART虚拟机解析oat文件，对于java方法，会把相关的信息利用OatMethod类来保存，具体解析的过程就不看了，直接看解析的结果OatMehod结构：

``` cpp
class OatMehod {
public:
    uint32_t GetCodeOffset() const { return code_offset_; }
    //等一些其他public方法
private:
    //获取该方法的本地机器指令在内存的位置
    template<class T>
    T GetOatPointer(uint32_t offset) const {
      if (offset == 0) {
        return nullptr;
      }
      return reinterpret_cast<T>(begin_ + offset);
    }
    const uint8_t* begin_;  //oat文件的oatHeader在内存的位置
    uint32_t code_offset_;  //该方法的本地机器指令相对oatHeader的偏移位置
};
```

ART虚拟机在运行时，可以通过OatMethod对象的GetOatPointer方法来找到每个方法对应的本地机器指令在内存的位置。OatMethod结构相当于一个java方法最底层的表现形式了。然后我们可以梳理下还有那些其他抽象层高的表现，可以从我们如何调用一个方法来梳理，有如下几类：
1. java层，利用反射来调用一个java方法，这时候我们使用java.lang.reflect.Method类来表示一个java方法。
2. native层，利用jni来调用一个java方法，这时候我们使用jmethodID结构来表示一个java方法。

然后我们看下java.lang.reflect.Method、jmehodID分别是什么样的，它们又和OatMethod类有什么联系。

java.lang.reflect.Method类是







## 参考文章
[Android运行时ART加载OAT文件的过程分析](https://blog.csdn.net/luoshengyang/article/details/39307813)

[Android运行时ART加载类和方法的过程分析](https://blog.csdn.net/luoshengyang/article/details/39533503)

《android软件安全与逆向分析》
