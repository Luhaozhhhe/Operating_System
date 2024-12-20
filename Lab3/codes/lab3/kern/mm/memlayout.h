#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* 该文件包含操作系统中内存管理的定义。 */

/* *
 * 虚拟内存映射：                                            权限
 *                                                          内核/用户
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         空内存 (*)              |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            | 当前页表（内核，读写）          | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |         无效内存 (*)            | --/--
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    重映射的物理内存             | RW/-- KMEMSIZE
 *                            |                                 |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |                                 |
 *                            |                                 |
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (*) 注意：内核确保“无效内存” *永远* 不会被映射。
 *     “空内存”通常是未映射的，但用户程序可以根据需要在此处映射页面。
 *
 * */

/* 所有物理内存都映射到这个地址 */

#define KERNBASE            0xFFFFFFFFC0200000 
// = 0x80200000(物理内存里内核的起始位置, KERN_BEGIN_PADDR) + 0xFFFFFFFF40000000(偏移量, PHYSICAL_MEMORY_OFFSET)
//把原有内存映射到虚拟内存空间的最后一页
#define KMEMSIZE            0x7E00000          // 物理内存的最大容量
// 0x7E00000 = 0x8000000 - 0x200000
// QEMU 缺省的RAM为 0x80000000到0x88000000, 128MiB, 0x80000000到0x80200000被OpenSBI占用
#define KERNTOP             (KERNBASE + KMEMSIZE) // 0x88000000对应的虚拟地址

#define PHYSICAL_MEMORY_END         0x88000000 // 物理内存的结束地址
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000 // 物理内存的虚拟地址偏移量
#define KERNEL_BEGIN_PADDR          0x80200000 // 内核在物理内存中的起始地址
#define KERNEL_BEGIN_VADDR          0xFFFFFFFFC0200000 // 内核在虚拟内存中的起始地址


#define KSTACKPAGE          2                           // 内核栈的页数
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // 内核栈的大小

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

typedef uintptr_t pte_t;      // 页表项的类型
typedef uintptr_t pde_t;      // 页目录项的类型
typedef pte_t swap_entry_t;   // 页表项也可以是交换项

/* *
 * struct Page - 页描述符结构。每个 Page 描述一个物理页。
 * 在 kern/mm/pmm.h 中可以找到许多有用的函数，用于将 Page 转换为其他数据类型，例如物理地址。
 * */
struct Page {
    int ref;                        // 页帧的引用计数
    uint_t flags;                   // 标志数组，描述页帧的状态
    uint_t visited;                 // 访问标记（用于页面访问追踪）
    unsigned int property;          // 空闲块的页数，在首次适应内存分配管理器中使用
    list_entry_t page_link;         // 用于连接到空闲列表的链表节点
    list_entry_t pra_page_link;     // 用于页面置换算法的链表节点
    uintptr_t pra_vaddr;            // 用于页面置换算法的虚拟地址
};

/* 描述页帧状态的标志 */
#define PG_reserved                 0       
// 如果此位为1：表示该页被保留用于内核，不能用于分配/释放页面；否则该位为0
#define PG_property                 1       
// 如果此位为1：该页是一个空闲内存块的头页（包含多个连续的地址页），可以用于分配页面；
//如果为0，则表示该页已被分配，或不是空闲块的头页

// 设置或清除页的标志位，检查页的标志位状态
#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))  // 设置页面的 PG_reserved 标志
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags)) // 清除页面的 PG_reserved 标志
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))  // 检查页面的 PG_reserved 标志
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))   // 设置页面的 PG_property 标志
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags)) // 清除页面的 PG_property 标志
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))  // 检查页面的 PG_property 标志

// 将链表节点转换为 Page 结构体
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/* free_area_t - 维护一个双向链表来记录空闲（未使用）页面 */
typedef struct {
    list_entry_t free_list;         // 链表的头节点
    unsigned int nr_free;           // 该链表中空闲页面的数量
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */

