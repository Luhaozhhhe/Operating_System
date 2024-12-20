#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <list.h>

/* 
 * 这个文件实现了 "时钟 (Clock)" 页面置换算法。Clock 算法是 FIFO 的一种改进版本，
 * 它用一个时钟指针遍历页面集合，通过访问位 (visited) 来决定哪个页面可以被置换。
 */

// 全局变量，表示页面链表的头节点和当前指针位置
list_entry_t pra_list_head, *curr_ptr;

/*
 * _clock_init_mm - 初始化页面置换算法
 * @mm: 内存管理结构体
 * 这个函数初始化页面链表，并将 mm 结构体的私有数据成员指向页面链表头节点。
 */
static int
_clock_init_mm(struct mm_struct *mm)
{
    // 初始化 pra_list_head 链表为空链表，用于管理所有可换出的页面
    list_init(&pra_list_head);
    
    // 初始化当前指针 curr_ptr，指向 pra_list_head，表示当前页面替换位置为链表头
    curr_ptr = &pra_list_head;

    // 将 mm 的私有成员 sm_priv 指针指向 pra_list_head，用于后续的页面替换算法操作
    mm->sm_priv = &pra_list_head;

    return 0;
}

/*
 * _clock_map_swappable - 将页面插入到页面链表中以便进行替换
 * @mm: 内存管理结构体
 * @addr: 虚拟地址
 * @page: 页面结构体
 * @swap_in: 是否是从磁盘换入的页面
 * 这个函数按照 Clock 页面替换算法，将最新到达的页面插入到 pra_list_head 队列末尾。
 */
static int
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    // 获取页面的链表节点
    list_entry_t *entry = &(page->pra_page_link);

    // 确保 entry 和 curr_ptr 都不为 NULL
    assert(entry != NULL && curr_ptr != NULL);

    // 将页面 page 插入到页面链表 pra_list_head 的末尾
    list_add_before(&pra_list_head, entry);

    // 将页面的 visited 标志置为 1，表示该页面已被访问
    page->visited = 1;

    return 0;
}

/*
 * _clock_swap_out_victim - 从页面链表中选择要换出的页面
 * @mm: 内存管理结构体
 * @ptr_page: 指向被选中的页面的指针
 * @in_tick: 是否在时钟中断中
 * 根据 Clock 页面替换算法，选择一个未被访问的页面进行置换。
 */
static int
_clock_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    // 获取页面链表的头节点
    list_entry_t *head = (list_entry_t*) mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);

    // 开始遍历链表，找到一个可以换出的页面
    while (1) {
        // 将 curr_ptr 移动到下一个页面
        curr_ptr = list_next(curr_ptr);

        // 如果回到了链表头节点，则继续从头开始
        if (curr_ptr == &pra_list_head) {
            curr_ptr = list_next(curr_ptr);
        }

        // 获取当前页面的结构体指针
        struct Page *page = le2page(curr_ptr, pra_page_link);

        // 如果页面未被访问
        if (page->visited == 0) {
            cprintf("curr_ptr %p\n", curr_ptr);

            // 将该页面从页面链表中删除
            list_del(curr_ptr);

            // 将该页面指针赋值给 ptr_page，表示这个页面将被换出
            *ptr_page = page;

            break; // 找到要换出的页面后跳出循环
        } else {
            // 如果页面已被访问，则将 visited 标志置为 0，以便下次可以被换出
            page->visited = 0;
        }
    }

    return 0;
}

/*
 * _clock_check_swap - 检查 Clock 页面置换算法是否正确
 * 在这个函数中，我们通过模拟访问不同虚拟页面来检查页错误次数是否符合预期。
 */
static int
_clock_check_swap(void) {
#ifdef ucore_test
    int score = 0, totalscore = 5;
    cprintf("%d\n", &score);
    ++score; cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    ++score; cprintf("grading %d/%d points", score, totalscore);
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    ++score; cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    ++score; cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    ++score; cprintf("grading %d/%d points", score, totalscore);
#else
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
#endif
    return 0;
}

/*
 * _clock_init - 初始化 Clock 页面置换管理器
 * 由于 Clock 算法相对简单，这里不需要额外的初始化工作，直接返回 0。
 */
static int
_clock_init(void)
{
    return 0;
}

/*
 * _clock_set_unswappable - 设置页面不可换出
 * 由于 Clock 算法不考虑页面共享的情况，这里直接返回 0。
 */
static int
_clock_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

/*
 * _clock_tick_event - 时钟中断事件处理
 * Clock 算法不受时钟中断的影响，因此这里直接返回 0。
 */
static int
_clock_tick_event(struct mm_struct *mm)
{
    return 0;
}

// 定义 Clock 页面置换管理器的函数接口
struct swap_manager swap_manager_clock = {
    .name            = "clock swap manager",  // 管理器的名称
    .init            = &_clock_init,          // 初始化函数
    .init_mm         = &_clock_init_mm,       // 初始化 mm 结构体函数
    .tick_event      = &_clock_tick_event,    // 时钟事件处理函数
    .map_swappable   = &_clock_map_swappable, // 映射可换页面函数
    .set_unswappable = &_clock_set_unswappable, // 设置不可换页面函数
    .swap_out_victim = &_clock_swap_out_victim, // 选择被置换页面的函数
    .check_swap      = &_clock_check_swap,    // 页面置换检查函数
};
