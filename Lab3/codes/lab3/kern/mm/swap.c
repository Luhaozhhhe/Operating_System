#include <swap.h>
#include <swapfs.h>
#include <swap_fifo.h>
#include <swap_clock.h>
#include <stdio.h>
#include <string.h>
#include <memlayout.h>
#include <pmm.h>
#include <mmu.h>

// 定义检查中有效虚拟页的数量为 5
#define CHECK_VALID_VIR_PAGE_NUM 5
// 定义开始检查的虚拟地址为 0x1000
#define BEING_CHECK_VALID_VADDR 0X1000
// 定义检查的有效虚拟地址的最大值，注意，这里有 CHECK_VALID_VIR_PAGE_NUM 个页的大小
#define CHECK_VALID_VADDR (CHECK_VALID_VIR_PAGE_NUM + 1) * 0x1000
// 定义用于检查的物理页的最大数量
#define CHECK_VALID_PHY_PAGE_NUM 4
// 定义最大访问序列号
#define MAX_SEQ_NO 10

// 定义指向页面置换管理器的指针
static struct swap_manager *sm;
// 最大的交换区偏移量
size_t max_swap_offset;
// 标记交换区是否初始化
volatile int swap_init_ok = 0;

// 用于存储虚拟页的页面地址
unsigned int swap_page[CHECK_VALID_VIR_PAGE_NUM];
// 用于记录页面交换的入和出序列号
unsigned int swap_in_seq_no[MAX_SEQ_NO], swap_out_seq_no[MAX_SEQ_NO];

// 声明检查页面置换算法的函数
static void check_swap(void);

// swap_out - 实现将 n 个页面换出内存的功能
int swap_out(struct mm_struct *mm, int n, int in_tick)
{
    int i;
    for (i = 0; i != n; ++i)
    {
        uintptr_t v; // 保存虚拟地址
        struct Page *page; // 被选中的页面对象
        // 调用页面置换管理器的 swap_out_victim 函数，找一个可以换出的页面
        int r = sm->swap_out_victim(mm, &page, in_tick);
        // 如果返回值 r 不为 0，表示无法找到可换出的页面
        if (r != 0) {
            cprintf("i %d, swap_out: call swap_out_victim failed\n", i);
            break;
        }

        // 选择了一个被换出的页面
        cprintf("SWAP: choose victim page 0x%08x\n", page);

        // 获取要换出的页面的虚拟地址
        v = page->pra_vaddr;
        // 获取该虚拟地址对应的页表项
        pte_t *ptep = get_pte(mm->pgdir, v, 0);
        // 确认页表项是有效的（即页面存在于内存中）
        assert((*ptep & PTE_V) != 0);

        // 将要换出的物理页面内容写入到硬盘上的交换区
        if (swapfs_write((page->pra_vaddr / PGSIZE + 1) << 8, page) != 0) {
            // 如果写入失败，保持页面的可交换状态
            cprintf("SWAP: failed to save\n");
            sm->map_swappable(mm, v, page, 0);
            continue;
        } else {
            // 写入成功，更新页表项，使其指向磁盘上的交换区条目
            cprintf("swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d\n", i, v, page->pra_vaddr / PGSIZE + 1);
            *ptep = (page->pra_vaddr / PGSIZE + 1) << 8;
            // 释放内存页面
            free_page(page);
        }

        // 页表项改变后，需要刷新 TLB（转换后备缓冲区）
        tlb_invalidate(mm->pgdir, v);
    }
    return i; // 返回实际换出的页面数量
}

// swap_in - 将页面从交换区换入内存
int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result)
{
    // 分配一个新的物理页面用于将内容换入
    struct Page *result = alloc_page();
    assert(result != NULL);

    // 获取虚拟地址对应的页表项
    pte_t *ptep = get_pte(mm->pgdir, addr, 0);

    int r;
    // 从磁盘的交换区中读取页面的内容，并加载到新分配的物理页面中
    if ((r = swapfs_read((*ptep), result)) != 0) {
        // 如果读取失败，程序直接停止
        assert(r != 0);
    }

    // 打印交换入的调试信息
    cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n", (*ptep) >> 8, addr);
    // 返回结果页面
    *ptr_result = result;
    return 0;
}

// check_content_set - 设置页面内容并验证页错误次数
static inline void
check_content_set(void)
{
    // 访问地址 0x1000 并设置其内容为 0x0a，同时验证页错误次数增加
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 1);
    *(unsigned char *)0x1010 = 0x0a;
    assert(pgfault_num == 1);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 2);
    *(unsigned char *)0x2010 = 0x0b;
    assert(pgfault_num == 2);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 3);
    *(unsigned char *)0x3010 = 0x0c;
    assert(pgfault_num == 3);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4010 = 0x0d;
    assert(pgfault_num == 4);
}

// check_content_access - 验证页面置换算法是否正确工作
static inline int
check_content_access(void)
{
    // 调用页面置换管理器的 check_swap 函数，检查页面置换逻辑
    int ret = sm->check_swap();
    return ret;
}

struct Page *check_rp[CHECK_VALID_PHY_PAGE_NUM];
pte_t *check_ptep[CHECK_VALID_PHY_PAGE_NUM];
unsigned int check_swap_addr[CHECK_VALID_VIR_PAGE_NUM];

extern free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

// check_swap - 检查页面置换的正确性
static void
check_swap(void)
{
    // 备份当前内存的自由页面状态
    int ret, count = 0, total = 0, i;
    list_entry_t *le = &free_list;

    // 统计当前自由页面的数量和总数
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count++, total += p->property;
    }
    assert(total == nr_free_pages());
    cprintf("BEGIN check_swap: count %d, total %d\n", count, total);
    
    // 创建新的内存管理结构体，用于测试页面置换
    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    // 确保检查用的 mm_struct 没有被其他地方占用
    extern struct mm_struct *check_mm_struct;
    assert(check_mm_struct == NULL);

    check_mm_struct = mm;

    // 使用页表目录来初始化 mm 结构
    pde_t *pgdir = mm->pgdir = boot_pgdir;
    assert(pgdir[0] == 0);

    // 创建一个虚拟地址区间 vma，范围为 BEING_CHECK_VALID_VADDR 到 CHECK_VALID_VADDR
    struct vma_struct *vma = vma_create(BEING_CHECK_VALID_VADDR, CHECK_VALID_VADDR, VM_WRITE | VM_READ);
    assert(vma != NULL);

    // 将新创建的 vma 插入到 mm 结构中
    insert_vma_struct(mm, vma);

    // 设置虚拟地址 0x1000 的页表，确保页表项存在
    cprintf("setup Page Table for vaddr 0X1000, so alloc a page\n");
    pte_t *temp_ptep = NULL;
    temp_ptep = get_pte(mm->pgdir, BEING_CHECK_VALID_VADDR, 1);
    assert(temp_ptep != NULL);
    cprintf("setup Page Table vaddr 0~4MB OVER!\n");
    
    // 分配物理页面，用于模拟页面分配和置换
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_rp[i] = alloc_page();
        assert(check_rp[i] != NULL);
        assert(!PageProperty(check_rp[i]));
    }

    // 存储并清空当前自由页面列表，以便测试
    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    
    unsigned int nr_free_store = nr_free;
    nr_free = 0;
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        free_pages(check_rp[i], 1);
    }
    assert(nr_free == CHECK_VALID_PHY_PAGE_NUM);
    
    cprintf("set up init env for check_swap begin!\n");
    
    // 设置虚拟页的内容，并验证页错误次数
    pgfault_num = 0;
    check_content_set();
    assert(nr_free == 0); 

    // 初始化页面换入和换出的序列号数组
    for (i = 0; i < MAX_SEQ_NO; i++) {
        swap_out_seq_no[i] = swap_in_seq_no[i] = -1;
    }
    
    // 检查页面对应的页表项
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        check_ptep[i] = 0;
        check_ptep[i] = get_pte(pgdir, (i + 1) * 0x1000, 0);
        assert(check_ptep[i] != NULL);
        assert(pte2page(*check_ptep[i]) == check_rp[i]);
        assert((*check_ptep[i] & PTE_V));          
    }
    cprintf("set up init env for check_swap over!\n");
    
    // 访问虚拟页以测试页面置换算法
    ret = check_content_access();
    assert(ret == 0);
    
    // 恢复内存环境，释放所有分配的页面
    for (i = 0; i < CHECK_VALID_PHY_PAGE_NUM; i++) {
        free_pages(check_rp[i], 1);
    }

    mm_destroy(mm);
    
    // 恢复自由页面列表和自由页面数
    nr_free = nr_free_store;
    free_list = free_list_store;

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count--, total -= p->property;
    }
    cprintf("count is %d, total is %d\n", count, total);
    
    cprintf("check_swap() succeeded!\n");
}
