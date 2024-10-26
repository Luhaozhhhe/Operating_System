#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* All physical memory mapped at this address */
#define KERNBASE            0xFFFFFFFFC0200000 // = 0x80200000(物理内存里内核的起始位置, KERN_BEGIN_PADDR) + 0xFFFFFFFF40000000(偏移量, PHYSICAL_MEMORY_OFFSET)
//把原有内存映射到虚拟内存空间的最后一页
#define KMEMSIZE            0x7E00000          // the maximum amount of physical memory
// 0x7E00000 = 0x8000000 - 0x200000
// QEMU 缺省的RAM为 0x80000000到0x88000000, 128MiB, 0x80000000到0x80200000被OpenSBI占用
#define KERNTOP             (KERNBASE + KMEMSIZE) // 0x88000000对应的虚拟地址

#define PHYSICAL_MEMORY_END         0x88000000     //物理地址的结束
#define PHYSICAL_MEMORY_OFFSET      0xFFFFFFFF40000000  //虚拟地址的偏移量
#define KERNEL_BEGIN_PADDR          0x80200000  //物理地址的开始
#define KERNEL_BEGIN_VADDR          0xFFFFFFFFC0200000  //虚拟地址的开始


#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

typedef uintptr_t pte_t;
typedef uintptr_t pde_t;

/* *
 * struct Page - 页描述符结构。每个 Page 描述一个物理页。
 在 kern/mm/pmm.h 中，你可以找到许多有用的函数，这些函数可以将 Page 转换为其他数据类型，例如物理地址。
 * */
struct Page {
    int ref;                        // 页帧的引用计数器
    uint64_t flags;                 // 描述页帧状态的标志数组
    unsigned int property;          // 空闲块的数量，在首次适应内存管理器中使用
    list_entry_t page_link;         // 空闲列表链接
};

/* 描述页帧状态的标志 */
#define PG_reserved                 0       //如果此位为 1：该页被保留给内核，不能在 alloc/free_pages 中使用；否则，此位为 0

#define PG_property                 1       //如果此位为 1：该页是一个空闲内存块的头页（包含一些连续地址的页），并且可以在 alloc_pages 中使用；
                                            //如果此位为 0：如果该页是一个空闲内存块的头页，那么这个页和内存块已被分配。或者该页不是头页。

#define SetPageReserved(page)       set_bit(PG_reserved, &((page)->flags))
#define ClearPageReserved(page)     clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page)          test_bit(PG_reserved, &((page)->flags))
#define SetPageProperty(page)       set_bit(PG_property, &((page)->flags))
#define ClearPageProperty(page)     clear_bit(PG_property, &((page)->flags))
#define PageProperty(page)          test_bit(PG_property, &((page)->flags))

// 将列表项转换为页
#define le2page(le, member)                 \
    to_struct((le), struct Page, member)

/* free_area_t - 维护一个双向链表来记录空闲（未使用）的页*/
typedef struct {
    list_entry_t free_list;         // 链表头
    unsigned int nr_free;           // 列表中空闲的页数
} free_area_t;


#define MAX_BUDDY_ORDER 14 // 0x7cb9 31929
typedef struct
{
    unsigned int max_order;                       // 实际最大块的大小
    list_entry_t free_array[15];                  // 伙伴堆数组
    unsigned int nr_free;                         // 伙伴系统中剩余的空闲块
} buddy_system_t;



#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
