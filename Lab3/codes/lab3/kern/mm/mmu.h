#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif /* !__ASSEMBLER__ */

// 线性地址 'la' 的四部分结构如下：
//
// +--------9-------+-------9--------+-------9--------+---------12----------+
// | Page Directory | Page Directory |   Page Table   | Offset within Page  |
// |     Index 1    |    Index 2     |                |                     |
// +----------------+----------------+----------------+---------------------+
//  \-- PDX1(la) --/ \-- PDX0(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
//  \-------------------PPN(la)----------------------/
//
// 宏 `PDX1`、`PDX0`、`PTX`、`PGOFF` 和 `PPN` 用于分解线性地址。
// 要通过 `PDX(la)`、`PTX(la)` 和 `PGOFF(la)` 构造线性地址 `la`，请使用 `PGADDR(PDX(la), PTX(la), PGOFF(la))`。

// RISC-V 使用 39 位的虚拟地址访问 56 位的物理地址！
// Sv39 虚拟地址：
// +----9----+----9---+----9---+---12--+
// |  VPN[2] | VPN[1] | VPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 物理地址：
// +----26---+----9---+----9---+---12--+
// |  PPN[2] | PPN[1] | PPN[0] | PGOFF |
// +---------+----+---+--------+-------+
//
// Sv39 页表项：
// +----26---+----9---+----9---+---2----+-------8-------+
// |  PPN[2] | PPN[1] | PPN[0] |Reserved|D|A|G|U|X|W|R|V|
// +---------+----+---+--------+--------+---------------+

// 页目录索引
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)

// 页表索引
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)

// 地址中的页号字段
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

// 页内偏移
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

// 根据索引和偏移构造线性地址
#define PGADDR(d1, d0, t, o) ((uintptr_t)((d1) << PDX1SHIFT | (d0) << PDX0SHIFT | (t) << PTXSHIFT | (o)))

// 从页表项 pte 中提取页面的基地址，去掉低10位的标志位，并左移以得到物理地址中的页帧号字段
#define PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
// 从页目录项 pde 中提取页面的基地址
#define PDE_ADDR(pde)   PTE_ADDR(pde)  

// 页目录和页表常量
#define NPDEENTRY       512                    // 每个页目录的页目录项数量
#define NPTEENTRY       512                    // 每个页表的页表项数量

#define PGSIZE          4096                   // 每页的字节数
#define PGSHIFT         12                     // log2(PGSIZE)，页大小的移位值
#define PTSIZE          (PGSIZE * NPTEENTRY)   // 每个页目录项映射的字节数
#define PTSHIFT         21                     // log2(PTSIZE)，页目录项映射大小的移位值

#define PTXSHIFT        12                     // 线性地址中页表索引的偏移量
#define PDX0SHIFT       21                     // 线性地址中二级页目录索引的偏移量
#define PDX1SHIFT       30                     // 线性地址中一级页目录索引的偏移量
#define PTE_PPN_SHIFT   10                     // 物理地址中的页号偏移量

// 页表项 (PTE) 字段
#define PTE_V     0x001 // 有效位
#define PTE_R     0x002 // 读权限
#define PTE_W     0x004 // 写权限
#define PTE_X     0x008 // 执行权限
#define PTE_U     0x010 // 用户模式权限
#define PTE_G     0x020 // 全局位
#define PTE_A     0x040 // 访问位
#define PTE_D     0x080 // 脏位
#define PTE_SOFT  0x300 // 供软件保留使用的位

// 页表项权限组合
#define PAGE_TABLE_DIR (PTE_V)                        // 页表项有效
#define READ_ONLY (PTE_R | PTE_V)                     // 只读
#define READ_WRITE (PTE_R | PTE_W | PTE_V)            // 读写
#define EXEC_ONLY (PTE_X | PTE_V)                     // 仅执行
#define READ_EXEC (PTE_R | PTE_X | PTE_V)             // 可读可执行
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V) // 可读、可写、可执行

#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V) // 用户权限下可读、可写、可执行且有效

#endif /* !__KERN_MM_MMU_H__ */

/*
上述代码定义了一些与内存管理单元（Memory Management Unit, MMU）相关的宏和常量，
用于操作线性地址和物理地址，以及页表项的字段。
*/
