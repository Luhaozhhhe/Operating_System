#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_fifo.h>
#include <list.h>

/* 
 * FIFO (First-In, First-Out) 页面置换算法：
 * FIFO 是最简单的页面置换算法之一。它的基本思想是：
 * 最早进入内存的页面最早被替换。我们将所有进入内存的页面按时间顺序存储在队列中，
 * 队列头部是最早进入的页面，队列尾部是最新进入的页面。当需要置换时，选取队列头部的页面进行替换。
 * 虽然 FIFO 算法开销小、实现简单，但它在实际应用中的性能不佳，容易出现 Belady 异象（置换的页面增加会导致缺页增多）。
 */

// 页面置换队列的头节点
list_entry_t pra_list_head;

/*
 * _fifo_init_mm - 初始化页面置换的链表并将 mm 结构体中的 sm_priv 指向这个链表
 * @mm: 内存管理的结构体
 * 通过将 mm_struct 的 sm_priv 指针指向 pra_list_head，我们可以从内存管理结构中访问 FIFO 页面置换相关的数据
 */
static int
_fifo_init_mm(struct mm_struct *mm)
{     
    // 初始化 pra_list_head 链表，用于管理所有可替换的页面
    list_init(&pra_list_head);
    // 将 mm->sm_priv 指向 pra_list_head 的地址，以便在后续操作中访问页面置换队列
    mm->sm_priv = &pra_list_head;
    return 0;
}

/*
 * _fifo_map_swappable - 将最新到达的页面链接到 pra_list_head 的队列末尾
 * @mm: 内存管理结构体
 * @addr: 虚拟地址
 * @page: 页面结构体
 * @swap_in: 是否是从磁盘换入
 * 根据 FIFO 算法，我们需要将新到达的页面添加到 pra_list_head 队列的末尾。
 */
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    // 获取 pra_list_head 链表的头节点
    list_entry_t *head = (list_entry_t*) mm->sm_priv;
    // 获取要插入的页面的链表节点
    list_entry_t *entry = &(page->pra_page_link);
 
    // 确保 entry 和 head 均不为 NULL
    assert(entry != NULL && head != NULL);

    // 将新页面插入到 pra_list_head 队列的末尾（链表的头部表示队列的末尾）
    list_add(head, entry);
    return 0;
}

/*
 * _fifo_swap_out_victim - 从 pra_list_head 队列中选择最早到达的页面进行置换
 * @mm: 内存管理结构体
 * @ptr_page: 指向被选中的页面的指针
 * @in_tick: 是否在时钟中断中
 * 根据 FIFO 页面置换算法，我们应当移除 pra_list_head 队列中最早到达的页面，然后将该页面的地址赋值给 ptr_page。
 */
static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    // 获取 pra_list_head 链表的头节点
    list_entry_t *head = (list_entry_t*) mm->sm_priv;
    assert(head != NULL);
    // 确保 in_tick 为 0，表示不是在时钟中断中进行置换
    assert(in_tick == 0);

    // 选择最早到达的页面，即 pra_list_head 链表中的最后一个节点（队列头）
    list_entry_t* entry = list_prev(head);
    if (entry != head) {
        // 从队列中删除该页面，并将页面地址赋值给 ptr_page
        list_del(entry);
        *ptr_page = le2page(entry, pra_page_link);
    } else {
        *ptr_page = NULL; // 如果队列为空，设置 ptr_page 为 NULL
    }
    return 0;
}

/*
 * _fifo_check_swap - 检查 FIFO 页面置换是否正确
 * 在这个函数中，我们通过写入不同虚拟页面，模拟页面置换并验证页错误次数（pgfault_num）。
 */
static int
_fifo_check_swap(void) {
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);

    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);

    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);

    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);

    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);

    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);

    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);

    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 7);

    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 8);

    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 9);

    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 10);

    cprintf("write Virt Page a in fifo_check_swap\n");
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 11);
    
    return 0;
}

/*
 * _fifo_init - 初始化 FIFO 页面置换管理器
 * 由于 FIFO 算法相对简单，这里不需要额外的初始化工作，直接返回 0。
 */
static int
_fifo_init(void)
{
    return 0;
}

/*
 * _fifo_set_unswappable - 设置页面不可换出
 * 由于 FIFO 算法不考虑页面共享的情况，这里直接返回 0。
 */
static int
_fifo_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

/*
 * _fifo_tick_event - 时钟中断事件处理
 * FIFO 算法不受时钟中断的影响，因此这里直接返回 0。
 */
static int
_fifo_tick_event(struct mm_struct *mm)
{ 
    return 0; 
}

// 定义 FIFO 页面置换管理器的函数接口
struct swap_manager swap_manager_fifo =
{
    .name            = "fifo swap manager",  // 管理器的名称
    .init            = &_fifo_init,          // 初始化函数
    .init_mm         = &_fifo_init_mm,       // 初始化 mm 结构体函数
    .tick_event      = &_fifo_tick_event,    // 时钟事件处理函数
    .map_swappable   = &_fifo_map_swappable, // 映射可换页面函数
    .set_unswappable = &_fifo_set_unswappable,// 设置不可换页面函数
    .swap_out_victim = &_fifo_swap_out_victim, // 选择被置换页面的函数
    .check_swap      = &_fifo_check_swap,    // 页面置换检查函数
};
