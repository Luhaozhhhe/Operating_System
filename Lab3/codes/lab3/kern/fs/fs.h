#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include <mmu.h>

#define SECTSIZE            512//一个磁盘扇区的大小
#define PAGE_NSECT          (PGSIZE / SECTSIZE)  //一共需要8个磁盘扇区

// 交换设备号定义为1
#define SWAP_DEV_NO         1

#endif /* !__KERN_FS_FS_H__ */

//`fs`全称为file system,我们这里其实并没有“文件”的概念，
//这个模块称作`fs`只是说明它是“硬盘”和内核之间的接口。