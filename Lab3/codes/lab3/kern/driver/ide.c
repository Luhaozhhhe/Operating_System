#include <assert.h>
#include <defs.h>
#include <fs.h>
#include <ide.h>
#include <stdio.h>
#include <string.h>
#include <trap.h>
#include <riscv.h>

//Integrated Drive Electronics的意思，表示的是一种标准的硬盘接口。
// ide_init - 初始化IDE设备（这里只是一个空函数，实际没有操作）
void ide_init(void) {}

#define MAX_IDE 2  // 最大支持的IDE设备数量
#define MAX_DISK_NSECS 56  // 模拟磁盘的最大扇区数
static char ide[MAX_DISK_NSECS * SECTSIZE];// 模拟的IDE磁盘存储空间

// ide_device_valid - 检查给定的设备号是否有效
bool ide_device_valid(unsigned short ideno) { return ideno < MAX_IDE; }

// ide_device_size - 返回给定设备的磁盘大小（以扇区为单位）
size_t ide_device_size(unsigned short ideno) { return MAX_DISK_NSECS; }

// ide_read_secs - 从指定的扇区读取数据到dst中
// ideno: 磁盘编号（这里只模拟一个磁盘，这个参数没有实际用途）
// secno: 要读取的起始扇区号
// dst: 读取数据的目标地址
// nsecs: 要读取的扇区数量
int ide_read_secs(unsigned short ideno, uint32_t secno, void *dst,
                  size_t nsecs) {
    //ideno: 假设挂载了多块磁盘，选择哪一块磁盘 这里我们其实只有一块“磁盘”，这个参数就没用到
    int iobase = secno * SECTSIZE;
    memcpy(dst, &ide[iobase], nsecs * SECTSIZE);
    return 0;
}

// ide_write_secs - 将src中的数据写入到指定的扇区
// ideno: 磁盘编号（这里只模拟一个磁盘，这个参数没有实际用途）
// secno: 要写入的起始扇区号
// src: 要写入的数据源地址
// nsecs: 要写入的扇区数量
int ide_write_secs(unsigned short ideno, uint32_t secno, const void *src,
                   size_t nsecs) {
    int iobase = secno * SECTSIZE;// 计算起始位置的偏移量
/*
可以看到，我们这里所谓的“硬盘IO”，只是在内存里用`memcpy`把数据复制来复制去。
同时为了逼真地模仿磁盘，我们只允许以磁盘扇区为数据传输的基本单位，
也就是一次传输的数据必须是512字节的倍数，并且必须对齐。
*/
    memcpy(&ide[iobase], src, nsecs * SECTSIZE);
    return 0;
}

