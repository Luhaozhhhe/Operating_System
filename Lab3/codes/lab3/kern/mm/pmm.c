#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>

// virtual address of physical page array
struct Page *pages; 
/*
 * pages: 虚拟地址指针，指向整个物理页管理的结构体数组（Page结构体数组）。
 * 用于管理物理内存中每一个物理页面的状态。
 */

// amount of physical memory (in pages)
size_t npage = 0; 
/*
 * npage: 物理内存的总页面数量。每一页的大小是固定的，通常为 4KB。
 * 这个变量记录了系统中共有多少页可供管理。
 */

// The kernel image is mapped at VA=KERNBASE and PA=info.base
uint_t va_pa_offset; 
/*
 * va_pa_offset: 虚拟地址到物理地址的偏移。
 * 内核映像被映射到某个特定的虚拟地址（KERNBASE），通过这个偏移可以将内核的虚拟地址转换为物理地址。
 */

// memory starts at 0x80000000 in RISC-V
const size_t nbase = DRAM_BASE / PGSIZE; 
/*
 * nbase: 物理内存基地址，以页的数量表示。
 * DRAM_BASE 表示物理内存的起始地址（通常为 0x80000000），nbase 表示物理内存基地址相对于页大小（PGSIZE）的位置。
 */

// virtual address of boot-time page directory
pde_t *boot_pgdir = NULL; 
/*
 * boot_pgdir: 内核启动时的页目录的虚拟地址。
 * 这是最初引导阶段的页表，用于将虚拟地址映射到物理地址。
 */

// physical address of boot-time page directory
uintptr_t boot_cr3; 
/*
 * boot_cr3: 启动时页目录的物理地址。
 * 在启动过程中，CPU需要知道页表的物理地址，boot_cr3就是用来保存这个地址。
 */

// physical memory management
const struct pmm_manager *pmm_manager; 
/*
 * pmm_manager: 指向物理内存管理器的指针。
 * 它用于管理物理内存的分配和回收，系统在启动时会选择并初始化一个具体的物理内存管理器。
 */

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    // 初始化物理内存管理器，设置 pmm_manager 为默认的物理内存管理器
    pmm_manager = &default_pmm_manager;
    // 打印内存管理器的名称，以确认初始化成功
    cprintf("memory management: %s\n", pmm_manager->name);
    // 调用 pmm_manager 的 init 方法，初始化内存管理器的内部数据结构
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    // 调用物理内存管理器的 init_memmap 方法，初始化从 base 开始的 n 个页面
    // 该方法会设置这些页面为可用状态
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;  // 指向分配的物理页
    bool intr_flag;            // 中断标志，用于保存中断状态

    while (1) {
        // 保存当前的中断状态，并禁止中断
        local_intr_save(intr_flag);
        {
            // 调用物理内存管理器的 alloc_pages 方法，尝试分配 n 个连续的页面
            page = pmm_manager->alloc_pages(n);
        }
        // 恢复之前的中断状态
        local_intr_restore(intr_flag);

        // 如果成功分配页面，或者请求的页面数量大于1，或者交换机制没有初始化，则结束循环
        if (page != NULL || n > 1 || swap_init_ok == 0) {
            break;
        }

        // 如果页面不足且交换机制已初始化，调用 swap_out 函数进行页面置换
        extern struct mm_struct *check_mm_struct;
        // cprintf("page %x, call swap_out in alloc_pages %d\n",page, n);
        swap_out(check_mm_struct, n, 0);
    }
    // 返回分配的页面的指针
    // cprintf("n %d,get page %x, No %d in alloc_pages\n",n,page,(page-pages));
    return page;
}


// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;

    // 保存当前中断状态，并禁止中断
    local_intr_save(intr_flag);
    { 
        // 调用物理内存管理器的 free_pages 函数，释放从 base 开始的 n 个页面
        pmm_manager->free_pages(base, n); 
    }
    // 恢复之前的中断状态
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;

    // 保存当前中断状态，并禁止中断
    local_intr_save(intr_flag);
    { 
        // 调用物理内存管理器的 nr_free_pages 函数，返回当前空闲内存的页数
        ret = pmm_manager->nr_free_pages(); 
    }
    // 恢复之前的中断状态
    local_intr_restore(intr_flag);

    return ret;
}

/* page_init - initialize the physical memory management */
static void page_init(void) {
    extern char kern_entry[];

    // 计算虚拟地址和物理地址的偏移量
    va_pa_offset = KERNBASE - 0x80200000;

    // 物理内存起始地址
    uint64_t mem_begin = KERNEL_BEGIN_PADDR;

    // 物理内存大小
    uint64_t mem_size = PHYSICAL_MEMORY_END - KERNEL_BEGIN_PADDR;

    // 物理内存结束地址
    uint64_t mem_end = PHYSICAL_MEMORY_END; // 这里硬编码了物理内存结束地址，取代了 sbi_query_memory() 接口

    // 打印物理内存的起始、结束地址和大小
    cprintf("membegin %llx memend %llx mem_size %llx\n", mem_begin, mem_end, mem_size);
    cprintf("physcial memory map:\n");
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin, mem_end - 1);

    // 限制最大物理地址到 KERNTOP，即内核映射的顶部地址
    uint64_t maxpa = mem_end;
    if (maxpa > KERNTOP) {
        maxpa = KERNTOP;
    }

    // 获取内核结束的地址
    extern char end[];

    // 计算物理内存的总页数
    npage = maxpa / PGSIZE;

    // 内核启动页表已经在内核结束地址之后分配了一部分物理页，所以需要跳过这些页
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);

    // 将所有页面标记为 "Reserved"，防止错误使用
    for (size_t i = 0; i < npage - nbase; i++) {
        SetPageReserved(pages + i);
    }

    // 获取可以分配的空闲内存的起始地址，需包含页面管理结构体的空间
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));

    // 计算可用内存的开始和结束地址，并确保是页面对齐的
    mem_begin = ROUNDUP(freemem, PGSIZE);
    mem_end = ROUNDDOWN(mem_end, PGSIZE);

    // 如果有空闲的物理内存，则初始化这些内存页
    if (freemem < mem_end) {
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
    }
}

/* enable_paging - enable paging by writing to the satp CSR */
static void enable_paging(void) {
    // 将页表的物理地址写入 RISC-V 的 satp 寄存器，启用分页机制
    // 0x8000000000000000 表示使用 SV39 分页模式
    write_csr(satp, (0x8000000000000000) | (boot_cr3 >> RISCV_PGSHIFT));
}


/**
 * @brief      设置并启用分页机制
 *
 * @param      pgdir  页目录
 * @param[in]  la     需要映射的内存线性地址
 * @param[in]  size   内存大小
 * @param[in]  pa     内存的物理地址
 * @param[in]  perm   内存的权限
 */
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm) {
    // 确保线性地址和物理地址的偏移相同，即保证内存对齐
    assert(PGOFF(la) == PGOFF(pa));

    // 计算需要映射的页数，ROUNDUP 用于保证所有需要的大小都包含在这些页内
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;

    // 将线性地址和物理地址向下对齐到页边界，方便后续逐页映射
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);

    // 循环为每个页设置页表项
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE) {
        // 获取线性地址 la 对应的页表项指针，如果页表不存在则创建页表
        pte_t *ptep = get_pte(pgdir, la, 1);
        // 确保页表项不为空，表示创建成功
        assert(ptep != NULL);

        // 使用 pte_create 创建页表项，包含物理页号和权限标志
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - 使用 pmm->alloc_pages(1) 分配一个页面
// 返回值：已分配页面的内核虚拟地址
// 注意：此函数用于为页目录表（Page Directory Table, PDT）和页表（Page Table, PT）分配内存
static void *boot_alloc_page(void) {
    // 使用物理内存管理器分配一个页面
    struct Page *p = alloc_page();

    // 如果分配失败，则触发内核崩溃
    if (p == NULL) {
        panic("boot_alloc_page failed.\n");
    }

    // 返回分配的物理页对应的内核虚拟地址
    return page2kva(p);
}

// pmm_init - 设置一个物理内存管理器来管理物理内存，构建页目录和页表以设置分页机制
//         - 检查物理内存管理器和分页机制的正确性，打印页目录和页表
void pmm_init(void) {
    // 初始化一个物理内存管理器
    // 我们需要一个用于分配和释放物理内存的管理器（粒度为 4KB 或其他大小）
    // 因此我们定义了一个物理内存管理器结构体（struct pmm_manager）
    // 初始化一个基于框架的物理内存管理器
    init_pmm_manager();

    // 探测物理内存空间，保留已使用的内存，然后使用 pmm->init_memmap 创建空闲页面列表
    page_init();

    // 使用物理内存管理器的 check 函数，验证分配和释放函数的正确性
    check_alloc_page();

    // 创建 boot_pgdir，一个初始页目录表（Page Directory Table, PDT）
    extern char boot_page_table_sv39[];
    boot_pgdir = (pte_t*)boot_page_table_sv39; // 使用预定义的静态页表
    boot_cr3 = PADDR(boot_pgdir); // 获取页表的物理地址，用于分页机制

    // 检查页目录的正确性
    check_pgdir();

    // 确保内核基地址和顶端都是页目录项大小的倍数，以便进行整页映射
    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // 映射所有物理内存到线性地址，线性地址从 KERNBASE 开始，大小为 KMEMSIZE
    // KERNBASE ~ KERNBASE+KMEMSIZE 的线性地址 = 物理地址 0 ~ KMEMSIZE
    // 但在 enable_paging() 和 gdt_init() 完成之前，不应使用此映射
    // boot_map_segment(boot_pgdir, KERNBASE, KMEMSIZE, PADDR(KERNBASE), READ_WRITE_EXEC);

    // 临时映射：
    // 虚拟地址 3G ~ 3G+4M = 线性地址 0 ~ 4M = 线性地址 3G ~ 3G+4M =
    // 物理地址 0 ~ 4M
    // boot_pgdir[0] = boot_pgdir[PDX(KERNBASE)];

    // enable_paging();

    // 现在基本的虚拟内存映射（见 memlayout.h）已建立
    // 检查基本虚拟内存映射的正确性
    check_boot_pgdir();
}


// get_pte - 获取页表项并返回该页表项对应的内核虚拟地址
//        - 如果页表不存在该页表项，则为页表分配一个新的页面
// 参数：
//  pgdir:  页目录表的内核虚拟基地址
//  la:     需要映射的线性地址
//  create: 一个逻辑值，决定是否为页表分配一个新的页面
// 返回值：该页表项的内核虚拟地址
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create) {
    /*
     * 如果需要访问一个物理地址，请使用 KADDR() 宏。
     * 请阅读 pmm.h 以了解有用的宏。
     *
     * 可能您需要帮助注释，下面的注释可以帮助您完成代码。
     *
     * 以下是一些有用的宏和定义，您可以在下面的实现中使用它们。
     * 宏或函数：
     *   PDX(la) = 获取虚拟地址 la 的页目录项的索引。
     *   KADDR(pa): 传入一个物理地址并返回对应的内核虚拟地址。
     *   set_page_ref(page, 1): 表示此页面被引用了一次。
     *   page2pa(page): 获取此（struct Page *）page 管理的内存的物理地址。
     *   struct Page * alloc_page(): 分配一个页面。
     *   memset(void *s, char c, size_t n): 将指针 s 指向的内存区域的前 n 个字节设置为指定值 c。
     * 定义：
     *   PTE_P 0x001 - 页表/页目录项标志位：存在（Present）
     *   PTE_W 0x002 - 页表/页目录项标志位：可写（Writeable）
     *   PTE_U 0x004 - 页表/页目录项标志位：用户可访问（User can access）
     */
    
    // 获取一级页目录项指针
    pde_t *pdep1 = &pgdir[PDX1(la)];
    
    // 如果一级页目录项不包含有效页表
    if (!(*pdep1 & PTE_V)) {
        struct Page *page;

        // 如果没有允许创建，或者分配新页面失败，则返回 NULL
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }

        // 将该页面的引用计数设置为 1，表示页面被引用了一次
        set_page_ref(page, 1);

        // 获取页面的物理地址
        uintptr_t pa = page2pa(page);

        // 将分配的物理页面对应的内核虚拟地址清零，大小为 PGSIZE
        memset(KADDR(pa), 0, PGSIZE);

        // 设置一级页目录项，指向分配的页面，设置权限为用户访问和有效
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }

    // 获取二级页目录项指针，使用一级页目录项获取二级页目录的内核虚拟地址
    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];

    // 如果二级页目录项不包含有效页表
    if (!(*pdep0 & PTE_V)) {
        struct Page *page;

        // 如果没有允许创建，或者分配新页面失败，则返回 NULL
        if (!create || (page = alloc_page()) == NULL) {
            return NULL;
        }

        // 将该页面的引用计数设置为 1，表示页面被引用了一次
        set_page_ref(page, 1);

        // 获取页面的物理地址
        uintptr_t pa = page2pa(page);

        // 将分配的物理页面对应的内核虚拟地址清零，大小为 PGSIZE
        memset(KADDR(pa), 0, PGSIZE);

        // 设置二级页目录项，指向分配的页面，设置权限为用户访问和有效
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }

    // 最终返回三级页表项的内核虚拟地址
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}


// get_page - 使用页目录表 pgdir 获取线性地址 la 对应的 Page 结构体
// 参数：
//  pgdir: 页目录表指针
//  la: 线性地址
//  ptep_store: 页表项指针的存储位置（可选），如果不为空，将返回找到的页表项
// 返回值：la 对应的 Page 结构体指针，如果找不到则返回 NULL
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store) {
    // 获取页表项指针
    pte_t *ptep = get_pte(pgdir, la, 0);
    
    // 如果 ptep_store 不为空，将找到的页表项指针存储到 ptep_store 中
    if (ptep_store != NULL) {
        *ptep_store = ptep;
    }

    // 如果页表项存在且有效（PTE_V），返回对应的 Page 结构体
    if (ptep != NULL && *ptep & PTE_V) {
        return pte2page(*ptep);
    }

    // 否则返回 NULL
    return NULL;
}

// page_remove_pte - 释放与线性地址 la 相关联的 Page 结构体
//                - 清除（失效）与线性地址 la 相关联的页表项
// 注意：页表发生改变，需要手动失效 TLB 条目
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep) {
    /*
     * 检查 ptep 是否有效，如果映射更新则必须手动更新 TLB。
     *
     * 以下是一些有用的宏和定义，您可以在下面的实现中使用它们：
     * 宏或函数：
     *   struct Page *page pte2page(*ptep): 通过页表项的值获取对应的 Page 结构体
     *   free_page: 释放一个页面
     *   page_ref_dec(page): 减少 page 的引用计数。如果 page->ref == 0，那么应该释放此页面。
     *   tlb_invalidate(pde_t *pgdir, uintptr_t la): 使 TLB 条目失效，但仅当正在编辑的页表是当前处理器正在使用的页表时才这样做。
     * 定义：
     *   PTE_V 0x001 - 页表项的标志位：有效（Present）
     */
    
    // (1) 检查页表项是否有效
    if (*ptep & PTE_V) {
        // (2) 获取页表项对应的 Page 结构体
        struct Page *page = pte2page(*ptep);

        // (3) 减少页面的引用计数
        page_ref_dec(page);

        // (4) 如果引用计数为 0，则释放该页面
        if (page_ref(page) == 0) {
            free_page(page);
        }

        // (5) 清除页表项
        *ptep = 0;

        // (6) 失效 TLB 条目
        tlb_invalidate(pgdir, la);
    }
}

// page_remove - 释放与线性地址 la 相关联的 Page，并删除对应的有效页表项
void page_remove(pde_t *pgdir, uintptr_t la) {
    // 获取 la 对应的页表项指针
    pte_t *ptep = get_pte(pgdir, la, 0);

    // 如果页表项不为空，则删除该页表项及其映射
    if (ptep != NULL) {
        page_remove_pte(pgdir, la, ptep);
    }
}


// page_insert - 建立物理页面和线性地址的映射
// 参数：
//  pgdir: 页目录表基址（内核虚拟地址）
//  page:  需要映射的物理页面（Page 结构体）
//  la:    需要映射的线性地址
//  perm:  页表项中的权限设置
// 返回值：始终为 0
// 注意：页表发生改变，需要手动刷新 TLB
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm) {
    // pgdir 是页表基地址（satp），page 是对应的物理页面，la 是虚拟地址

    // 先找到对应页表项的位置，如果原先不存在，get_pte() 会分配页表项的内存
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL) {
        return -E_NO_MEM;
    }

    // 指向这个物理页面的虚拟地址增加了一个引用计数
    page_ref_inc(page);

    // 如果原先存在映射（页表项有效）
    if (*ptep & PTE_V) {
        struct Page *p = pte2page(*ptep);

        // 如果这个映射原先就存在且没有改变物理页面
        if (p == page) {
            page_ref_dec(page); // 物理页面的引用计数不增加
        } else {
            // 如果原先这个虚拟地址映射到其他物理页面，那么需要删除旧的映射
            page_remove_pte(pgdir, la, ptep);
        }
    }

    // 构造新的页表项，将线性地址映射到物理页面
    *ptep = pte_create(page2ppn(page), PTE_V | perm);

    // 页表发生改变之后刷新 TLB
    tlb_invalidate(pgdir, la);

    return 0;
}

// 使特定的 TLB 条目失效，但仅当编辑的页表是当前处理器正在使用的页表时才这样做
void tlb_invalidate(pde_t *pgdir, uintptr_t la) {
    flush_tlb();  // 刷新 TLB
}

// pgdir_alloc_page - 调用 alloc_page 和 page_insert 函数来
//                  - 分配一个页面大小的内存并建立地址映射
//                  - pa <-> la，使用线性地址 la 和页目录表 pgdir
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm) {
    // 分配一个物理页面
    struct Page *page = alloc_page();
    if (page != NULL) {
        // 将物理页面与线性地址 la 建立映射
        if (page_insert(pgdir, page, la, perm) != 0) {
            // 如果映射失败，则释放该页面并返回 NULL
            free_page(page);
            return NULL;
        }

        // 如果启用了页面置换机制，将该页面标记为可换出
        if (swap_init_ok) {
            swap_map_swappable(check_mm_struct, la, page, 0);
            page->pra_vaddr = la; // 保存虚拟地址
            assert(page_ref(page) == 1);
        }
    }

    return page; // 返回分配的页面
}


static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void) {
    // assert(npage <= KMEMSIZE / PGSIZE);
    // The memory starts at 2GB in RISC-V
    // so npage is always larger than KMEMSIZE / PGSIZE
    assert(npage <= KERNTOP / PGSIZE);
    //boot_pgdir是页表的虚拟地址
    assert(boot_pgdir != NULL && (uint32_t)PGOFF(boot_pgdir) == 0);
    assert(get_page(boot_pgdir, 0x0, NULL) == NULL);
    //get_page()尝试找到虚拟内存0x0对应的页，现在当然是没有的，返回NULL

    struct Page *p1, *p2;
    p1 = alloc_page();//拿过来一个物理页面
    assert(page_insert(boot_pgdir, p1, 0x0, 0) == 0);//把这个物理页面通过多级页表映射到0x0
    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir, PGSIZE, 0) == ptep);
    //get_pte查找某个虚拟地址对应的页表项，如果不存在这个页表项，会为它分配各级的页表

    p2 = alloc_page();
    assert(page_insert(boot_pgdir, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir[0])) == 1);
    free_page(pde2page(boot_pgdir[0]));
    boot_pgdir[0] = 0;//清除测试的痕迹

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void) {
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store=nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE) {
        assert((ptep = get_pte(boot_pgdir, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }


    assert(boot_pgdir[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1=boot_pgdir,*pd0=page2kva(pde2page(boot_pgdir[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir[0] = 0;

    assert(nr_free_store==nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// kern/mm/pmm.c
void *kmalloc(size_t n) { //分配至少n个连续的字节，这里实现得不精细，占用的只能是整数个页。
    void *ptr = NULL;
    struct Page *base = NULL;
    assert(n > 0 && n < 1024 * 0124);
    int num_pages = (n + PGSIZE - 1) / PGSIZE; //向上取整到整数个页
    base = alloc_pages(num_pages); 
    assert(base != NULL); //如果分配失败就直接panic
    ptr = page2kva(base); //分配的内存的起始位置（虚拟地址），
    //page2kva, 就是page_to_kernel_virtual_address
    return ptr;
}

void kfree(void *ptr, size_t n) { //从某个位置开始释放n个字节
    assert(n > 0 && n < 1024 * 0124);
    assert(ptr != NULL);
    struct Page *base = NULL;
    int num_pages = (n + PGSIZE - 1) / PGSIZE; 
    /*计算num_pages和kmalloc里一样，
    但是如果程序员写错了呢？调用kfree的时候传入的n和调用kmalloc传入的n不一样？
    就像你平时在windows/linux写C语言一样，会出各种奇奇怪怪的bug。
    */
    base = kva2page(ptr);//kernel_virtual_address_to_page
    free_pages(base, num_pages);
}