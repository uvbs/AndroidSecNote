# 如何阅读Dalvik字节码
最近学习Dalvik虚拟机，需要学会阅读Dalvik字节码。以示例记录下如何阅读Dalvik字节码。

在Dex文件的DexCode结构中，有如下一条Dalvik字节码：6F 20 60 00 43 00

1. 将字节码以16位为单位，转换为大端： 206F 0060 0043
2. 取出第一个16位的低8位的opcode: 6F
3. 在[官方文档](https://source.android.google.cn/devices/tech/dalvik/dalvik-bytecode?hl=zh-cn)中查询opcode对应的指令为invoke-super，指令格式标识：35c
4. 在[官方文档](https://source.android.google.cn/devices/tech/dalvik/instruction-formats?hl=zh-cn)中查询该指令格式标识，对应的位描述：A|G|op BBBB F|E|D|C， [A=2] op {vC, vD}, kind@BBBB。
5. 对应为：A = 2，G = 0，op = 6F，BBBB = 0060，F = 0，E = 0，D = 4，C = 3。
6. 所以该指令解释为：invoke-super{v3， v4} method@0060
7. 在dex中查找methodID为0x60的函数：Activity->onCreate

关于指令格式标识35c的解释：
1. 第一个表示指令由多少个16位组成： 3
2. 第二个数字表示指令最多使用多少个寄存器: 5
3. 第三个字母为类型码，表示指令额外用到的数据类型: c表示常量池索引