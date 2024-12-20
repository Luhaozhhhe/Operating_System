#ifndef __KERN_MM_SWAP_H__
#define __KERN_MM_SWAP_H__

#include <defs.h>
#include <memlayout.h>
#include <pmm.h>
#include <vmm.h>

/* *
 * swap_entry_t
 * --------------------------------------------
 * |         offset        |   reserved   | 0 |
 * --------------------------------------------
 *           24 bits            7 bits    1 bit
 *
 * swap_entry_t 是一个 32 位的数据类型，用于标识交换条目。
 * 它的结构如下：
 * - offset (24 bits)：表示交换区中的偏移量，用于标识数据在交换区中的位置。
 * - reserved (7 bits)：保留位，暂未使用。
 * - 最后一位 (1 bit)：通常为 0。
 * */

#define MAX_SWAP_OFFSET_LIMIT (1 << 24) // 交换区的最大偏移量为 2^24，即最大交换条目数

extern size_t max_swap_offset; // 最大的交换偏移量，受物理设备大小的限制

/* *
 * swap_offset - 取出 swap_entry（保存在 pte 中），并返回对应的交换区中的偏移量
 * 该宏定义通过右移 8 位取出 offset，检查是否有效后返回偏移量。
 * */
#define swap_offset(entry) ({                                     \
               size_t __offset = (entry >> 8);                    \
               if (!(__offset > 0 && __offset < max_swap_offset)) { \
                    panic("invalid swap_entry_t = %08x.\n", entry); \
               }                                                  \
               __offset;                                          \
          })

/* swap_manager 结构体定义了交换管理器的接口和相关函数指针 */
struct swap_manager
{
    const char *name; // 交换管理器的名称
    /* 交换管理器的全局初始化函数 */
    int (*init) (void);
    /* 初始化 mm_struct 结构体中的私有数据 */
    int (*init_mm) (struct mm_struct *mm);
    /* 当时钟中断发生时调用的函数 */
    int (*tick_event) (struct mm_struct *mm);
    /* 当将可交换页面映射到 mm_struct 中时调用 */
    int (*map_swappable) (struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in);
    /* 当页面被标记为共享时，删除该地址条目 */
    int (*set_unswappable) (struct mm_struct *mm, uintptr_t addr);
    /* 尝试将一个页面换出，并返回被选中的页面（即受害页面） */
    int (*swap_out_victim) (struct mm_struct *mm, struct Page **ptr_page, int in_tick);
    /* 检查页面替换算法的正确性 */
    int (*check_swap) (void);
};

/* 交换管理模块初始化成功的标志 */
extern volatile int swap_init_ok;

// 交换管理器的全局函数声明
int swap_init(void); // 初始化交换管理器
int swap_init_mm(struct mm_struct *mm); // 初始化 mm_struct 中与交换管理相关的数据
int swap_tick_event(struct mm_struct *mm); // 在时钟中断时调用的事件处理函数
int swap_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in); // 将页面设置为可交换
int swap_set_unswappable(struct mm_struct *mm, uintptr_t addr); // 将页面设置为不可交换
int swap_out(struct mm_struct *mm, int n, int in_tick); // 尝试换出 n 个页面
int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result); // 将页面从交换区换入

//#define MEMBER_OFFSET(m,t) ((int)(&((t *)0)->m))
//#define FROM_MEMBER(m,t,a) ((t *)((char *)(a) - MEMBER_OFFSET(m,t)))

#endif
