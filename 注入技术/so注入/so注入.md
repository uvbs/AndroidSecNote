# Android SO 注入技术

## 参考文章:

<https://bbs.pediy.com/thread-220783.htm?source=1>

<https://bbs.pediy.com/thread-216096.htm?source=1>

<https://bbs.pediy.com/thread-141355.htm?source=1>

so注入就是讲自定义的so注入到目标进程中，让目标进程执行我们自定义的代码，so注入的方式有很多种，主要是直接利用ptrace注入，还有一些其他方法，如参考文章中列举的伪造so；利用virtualapp注入等。

## 利用ptrace实现so注入流程
1. 利用ptrace在目标进程中分配空间,用来写shellcode。
2. 利用ptrace写入shellcode，shellcode是用来加载我们的so。
3. 利用ptrace使目标进程执行shellcode。