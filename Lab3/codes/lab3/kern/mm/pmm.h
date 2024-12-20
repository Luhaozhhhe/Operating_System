#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include <assert.h>
#include <atomic.h>
#include <defs.h>
#include <memlayout.h>
#include <mmu.h>

/*
 * pmm_manager 是物理内存管理类，通过实现 pmm_manager 中的方法，
 * 一个特定的物理内存管理器（如 XXX_pmm_manager）可以被 uCore 用于管理物理内存空间。
 * 主要功能包括内存初始化、分配和释放页框等操作。
 */
struct pmm_manager {
    const char *name;  // 物理内存管理器的名称
    // 初始化管理器的内部描述和数据结构（如空闲块列表和空闲块数量）
    void (*init)(void);
    // 根据初始的空闲物理内存设置描述和管理数据结构
    void (*init_memmap)(struct Page *base, size_t n);
    // 分配 >=n 页的内存，依赖于具体的内存分配算法
    struct Page *(*alloc_pages)(size_t n);
    // 释放 >=n 页的内存，基于 Page 描述符的起始地址
    void (*free_pages)(struct Page *base, size_t n);
    // 返回当前空闲的页数量
    size_t (*nr_free_pages)(void);
    // 检查物理内存管理器的正确性
    void (*check)(void);
};

// 外部声明的一些全局变量和函数，供其他模块使用
extern const struct pmm_manager *pmm_manager;
extern pde_t *boot_pgdir;
extern const size_t nbase; // 基准物理页号偏移
extern uintptr_t boot_cr3;

void pmm_init(void);

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t nr_free_pages(void);

// 为单页的分配和释放定义简便宏
#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

// 获取某线性地址的页表项，必要时创建页表项
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);
// 获取某线性地址所对应的物理页面，存储页表项的指针
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store);
// 删除页表项，从页表中移除页面
void page_remove(pde_t *pgdir, uintptr_t la);
// 将页面插入页表并设置权限
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);

// 刷新某地址的 TLB 缓存
void tlb_invalidate(pde_t *pgdir, uintptr_t la);
// 为某线性地址分配一个页并插入页表
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);

/*
 * PADDR - 将内核虚拟地址转换为物理地址
 * 如果传入的地址不是内核地址，会触发 panic。
 */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE) {                                  \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    })

/*
 * KADDR - 将物理地址转换为内核虚拟地址
 * 如果传入的地址无效，会触发 panic。
 */
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage) {                                  \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    })

extern struct Page *pages;       // 管理物理页面的 Page 结构数组
extern size_t npage;             // 总的物理页数量
extern uint_t va_pa_offset;      // 虚拟地址和物理地址之间的偏移

/*
 * page2ppn - 获取页面结构对应的物理页号
 * pages 是一个 Page 结构体数组的起始地址，通过指针运算可以得到物理页号
 */
static inline ppn_t page2ppn(struct Page *page) { 
    return page - pages + nbase; 
}

/*
 * page2pa - 获取页面结构对应的物理地址
 * 将物理页号左移页大小位数即可得到页面的起始物理地址
 */
static inline uintptr_t page2pa(struct Page *page) {
    return page2ppn(page) << PGSHIFT;
}

/*
 * pa2page - 通过物理地址得到所在的页面结构
 * 从物理地址获取物理页号，然后通过 pages 数组获取 Page 结构
 */
static inline struct Page *pa2page(uintptr_t pa) {
    if (PPN(pa) >= npage) {
        panic("pa2page called with invalid pa");
    }
    return &pages[PPN(pa) - nbase];
}

/*
 * page2kva - 获取页面对应的内核虚拟地址
 */
static inline void *page2kva(struct Page *page) {
    return KADDR(page2pa(page));
}

/*
 * kva2page - 获取内核虚拟地址对应的页面结构
 */
static inline struct Page *kva2page(void *kva) {
    return pa2page(PADDR(kva));
}

/*
 * pte2page - 从页表项中获取对应的物理页面
 * PTE_V 检查页表项是否有效，PTE_ADDR 获取页面的物理地址
 */
static inline struct Page *pte2page(pte_t pte) {
    if (!(pte & PTE_V)) {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}

/*
 * pde2page - 获取页目录项指向的页面
 * 类似于 pte2page，PDE_ADDR 宏用于获取页面地址
 */
static inline struct Page *pde2page(pde_t pde) {
    return pa2page(PDE_ADDR(pde));
}

// 页的引用计数相关操作，用于跟踪某个页是否被共享或者引用
static inline int page_ref(struct Page *page) {
    return page->ref;
}

static inline void set_page_ref(struct Page *page, int val) {
    page->ref = val;
}

static inline int page_ref_inc(struct Page *page) {
    page->ref += 1;
    return page->ref;
}

static inline int page_ref_dec(struct Page *page) {
    page->ref -= 1;
    return page->ref;
}

// 刷新整个 TLB 缓存
static inline void flush_tlb() {
    asm volatile("sfence.vma");
}

// 构造页表项（PTE）函数，通过物理页号和权限标志创建页表项
static inline pte_t pte_create(uintptr_t ppn, int type) {
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}

// 创建页目录项（PTD），和 PTE 类似
static inline pte_t ptd_create(uintptr_t ppn) {
    return pte_create(ppn, PTE_V);
}

// 声明内核引导栈的起始和结束位置
extern char bootstack[], bootstacktop[];

// 动态内存分配函数
extern void *kmalloc(size_t n);
extern void kfree(void *ptr, size_t n);

#endif /* !__KERN_MM_PMM_H__ */
