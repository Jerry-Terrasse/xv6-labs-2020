## Xv6 Labs

我的Xv6实验代码，基于[原仓库](git://g.csail.mit.edu/xv6-labs-2020)

### 实验简介

#### Lab Utilities

分支 [`util`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/util)

- 实现Xv6的编译、启动
- 编写`sleep` `find`等一些用户态应用程序，以熟悉Xv6。

#### Lab System Calls

分支 [`syscall`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/syscall)

- 实现`trace`用于跟踪系统调用
- 实现`sysinfo`用于获取系统信息。

#### Lab Page Tables

分支 [`pagetable`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/pagetable)

- 实现`vmprint`用于打印页表
- 实现各进程独立的内核页表
- 将用户页表中的映射内容同步到内核页表，以便内核态可以直接访问用户态的内存。

#### Lab Traps

分支 [`traps`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/traps)

- 实现`backtrace`用于打印函数调用栈
- 利用时钟中断实现自定义定时器`alarm`

#### Lab Lazy Allocation

分支 [`lazy`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/lazy)

- 基于`page fault`实现惰性内存分配

#### Lab Copy-on-Write

分支 [`cow`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/cow)

- 基于`page fault`实现写时复制，优化`fork`的性能
- 基于引用计数维护内存页的生命周期

#### Lab Multithreading

分支 [`thread`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/thread)

- 实现用户态线程切换`uthread`
- (非Xv6) 利用`pthread_mutex`修复并发bug
- (非Xv6) 利用`pthread_cond`实现`barrier`

#### Lab Lock

分支 [`lock`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/lock)

- 实现多核内存分配器，优化`kalloc`性能
- 利用哈希桶优化`bcache`，减少锁的竞争

#### Lab File System

分支 [`fs`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/fs)

- 为原有`inode`添加二级间接索引，以支持更大的文件
- 实现符号链接文件`symlink`，并在其他系统调用中提供支持

#### Lab mmap

分支 [`mmap`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/mmap)

- 实现`mmap`系统调用，以支持内存映射文件
- 实现`munmap`系统调用，以支持解除内存映射
- 需要支持最基本的`mmap`特性(`MAP_PRIVATE` `MAP_SHARED`)
- `munmap`需要支持部分解除映射，必要时立即将修改写回文件

#### Lab Network Driver

分支 [`net`](https://github.com/Jerry-Terrasse/xv6-labs-2020/tree/net)

- 补全`e1000`网卡驱动，以支持Xv6的网络功能
- 需要根据手册`e1000_transmit` `e1000_receive`功能，即发送和接收数据包

### 相关资料

- [MIT 6.S081: Operating System Engineering](https://pdos.csail.mit.edu/6.828/2020/index.html)
- [Xv6 Book](https://pdos.csail.mit.edu/6.828/2020/xv6/book-riscv-rev1.pdf)
- [HITSZ OS2023 实验指导书](https://hitsz-cslab.gitee.io/os-labs/)