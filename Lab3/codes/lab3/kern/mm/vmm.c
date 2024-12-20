#include <vmm.h>
#include <sync.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <error.h>
#include <pmm.h>
#include <riscv.h>
#include <swap.h>

/* 
  虚拟内存管理 (VMM) 设计包含两个部分：mm_struct (mm) 和 vma_struct (vma)
  mm 是用于管理一组具有相同页目录表 (PDT) 的连续虚拟内存区域的内存管理器；
  vma 是一个连续的虚拟内存区域。
  在 mm 中有一个用于 vma 的线性链表和一个用于 vma 的红黑树链表。
---------------
  mm 相关函数：
   全局函数
     struct mm_struct * mm_create(void)                 // 创建一个 mm_struct 结构体
     void mm_destroy(struct mm_struct *mm)              // 销毁一个 mm_struct 结构体
     int do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) // 处理页故障
--------------
  vma 相关函数：
   全局函数
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)  // 创建一个 vma 结构体
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)       // 将 vma 插入到 mm 中
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)         // 查找包含给定地址的 vma
   局部函数
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)  // 检查 vma 是否重叠
---------------
  校验正确性的函数
     void check_vmm(void);                   // 校验虚拟内存管理的正确性
     void check_vma_struct(void);            // 校验 vma 结构体的正确性
     void check_pgfault(void);               // 校验页故障的处理
*/

// szx 函数：print_vma 和 print_mm

// 打印 vma_struct 结构体的信息
void print_vma(char *name, struct vma_struct *vma) {
    // 打印函数调用的名称，方便调试
    cprintf("-- %s print_vma --\n", name);
    // 打印 vma 结构体所属的 mm_struct 指针地址
    cprintf("   mm_struct: %p\n", vma->vm_mm);
    // 打印 vma 的起始和结束地址
    cprintf("   vm_start,vm_end: %x,%x\n", vma->vm_start, vma->vm_end);
    // 打印 vma 的权限标志
    cprintf("   vm_flags: %x\n", vma->vm_flags);
    // 打印 vma 链表链接的地址
    cprintf("   list_entry_t: %p\n", &vma->list_link);
}

// 打印 mm_struct 结构体的信息
void print_mm(char *name, struct mm_struct *mm) {
    // 打印函数调用的名称，方便调试
    cprintf("-- %s print_mm --\n", name);
    // 打印 mm 结构体的 mmap_list 链表的地址
    cprintf("   mmap_list: %p\n", &mm->mmap_list);
    // 打印 mm 结构体中管理的 vma 数量
    cprintf("   map_count: %d\n", mm->map_count);

    // 遍历 mm 结构体中的 vma 链表，逐个打印每个 vma 的信息
    list_entry_t *list = &mm->mmap_list;
    for (int i = 0; i < mm->map_count; i++) {
        // 获取下一个 vma 的链表节点
        list = list_next(list);
        // 通过宏 le2vma 将链表节点转换为 vma 结构体，并打印其信息
        print_vma(name, le2vma(list, list_link));
    }
}


static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

// mm_create - 分配一个 mm_struct 结构体并初始化它
struct mm_struct *
mm_create(void) {
    // 使用 kmalloc 为 mm_struct 分配内存空间
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL) {
        // 初始化 mmap_list 链表头节点
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;  // 将 mmap_cache 初始化为 NULL（用于缓存最近访问的 vma）
        mm->pgdir = NULL;       // 初始化页目录表指针为 NULL
        mm->map_count = 0;      // 初始化 vma 计数器为 0

        // 如果页面置换机制初始化成功，则初始化页面置换相关字段
        if (swap_init_ok) {
            swap_init_mm(mm);   // 初始化页面置换管理器的私有数据
        } else {
            mm->sm_priv = NULL; // 否则设置页面置换的私有数据为 NULL
        }
    }
    return mm; // 返回初始化后的 mm_struct 结构体指针
}

// vma_create - 分配一个 vma_struct 结构体并初始化它（地址范围：vm_start ~ vm_end）
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint_t vm_flags) {
    // 使用 kmalloc 为 vma_struct 分配内存空间
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL) {
        vma->vm_start = vm_start; // 设置 vma 的起始地址
        vma->vm_end = vm_end;     // 设置 vma 的结束地址
        vma->vm_flags = vm_flags; // 设置 vma 的标志（如可读、可写、可执行等权限）
    }
    return vma; // 返回初始化后的 vma_struct 结构体指针
}



// find_vma - 查找包含地址 addr 的 vma（虚拟内存区域）
// 条件为 vma->vm_start <= addr < vma->vm_end
// 如果返回 NULL，说明查询的虚拟地址不存在或不合法，既不对应任何内存页，也不对应硬盘上可以换进来的页
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;

    // 检查 mm 是否为 NULL，确保有效的内存管理结构体
    if (mm != NULL) {
        vma = mm->mmap_cache;  // 从 mmap_cache 中获取最近使用的 vma
        // 检查 mmap_cache 中的 vma 是否包含目标地址 addr
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
            // 如果缓存中没有找到包含该地址的 vma，遍历 mmap_list
            bool found = 0;  // 标志位，表示是否找到包含地址的 vma
            list_entry_t *list = &(mm->mmap_list), *le = list;

            // 遍历 mmap_list 链表，查找包含 addr 的 vma
            while ((le = list_next(le)) != list) {
                vma = le2vma(le, list_link);
                if (vma->vm_start <= addr && addr < vma->vm_end) {
                    found = 1;  // 找到包含该地址的 vma
                    break;
                }
            }

            // 如果没有找到包含该地址的 vma，则将 vma 设置为 NULL
            if (!found) {
                vma = NULL;
            }
        }

        // 如果找到有效的 vma，将其设置为 mmap_cache 以便下次快速访问
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }

    // 返回找到的 vma 或者 NULL（如果没有找到）
    return vma;
}

// check_vma_overlap - 检查两个 vma 是否重叠
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) {
    // 确保 prev 的起始地址小于结束地址，地址范围合法
    assert(prev->vm_start < prev->vm_end);

    // 确保 prev 的结束地址小于等于 next 的起始地址，表示 prev 和 next 不重叠
    assert(prev->vm_end <= next->vm_start);

    // 确保 next 的起始地址小于结束地址，地址范围合法
    assert(next->vm_start < next->vm_end);
    // 这里顺便检查了要插入的区间 next 的合法性，确保 start < end
}



// insert_vma_struct - 将 vma 插入到 mm 的 mmap_list 链表中
// 该函数用于插入一个新的 `vma_struct`，保持链表按区间起始地址有序。
// 我们可以通过插入操作添加新的 `vma_struct`，也可以用该函数查找某个虚拟地址是否已经有对应的 `vma_struct`。
void
insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma) {
    // 确保插入的 vma 起始地址小于结束地址
    assert(vma->vm_start < vma->vm_end);

    // 获取 mm 结构体中的 mmap_list 链表
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    // 从链表头开始遍历，找到合适的插入位置，保证 vma 的有序性
    list_entry_t *le = list;
    while ((le = list_next(le)) != list) {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        // 找到第一个起始地址大于当前 vma 起始地址的位置，准备插入
        if (mmap_prev->vm_start > vma->vm_start) {
            break;
        }
        le_prev = le;
    }

    // 找到的插入位置是 le_prev，获取它的下一个节点
    le_next = list_next(le_prev);

    // 检查插入后的 vma 是否与相邻的 vma 存在重叠
    if (le_prev != list) {
        // 检查与前一个 vma 是否重叠
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list) {
        // 检查与后一个 vma 是否重叠
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    // 将 vma 的 mm 字段指向当前 mm
    vma->vm_mm = mm;
    // 将 vma 插入到 le_prev 后面
    list_add_after(le_prev, &(vma->list_link));

    // 更新 mm 结构体中的 vma 数量
    mm->map_count++;
}

// mm_destroy - 释放 mm 结构体及其内部的字段
void
mm_destroy(struct mm_struct *mm) {
    list_entry_t *list = &(mm->mmap_list), *le;

    // 遍历 mmap_list，逐个释放链表中的每个 vma
    while ((le = list_next(list)) != list) {
        // 从链表中删除 le 节点
        list_del(le);
        // 释放 le 对应的 vma 结构体
        kfree(le2vma(le, list_link), sizeof(struct vma_struct));
    }

    // 释放 mm 结构体的内存
    kfree(mm, sizeof(struct mm_struct));
    // 将 mm 设置为 NULL，避免悬空指针
    mm = NULL;
}


// vmm_init - 初始化虚拟内存管理
//          - 目前仅调用 check_vmm 函数来检查虚拟内存管理的正确性
void
vmm_init(void) {
    // 调用 check_vmm 函数，验证虚拟内存管理系统的正确性
    check_vmm();
}

// check_vmm - 检查虚拟内存管理系统的正确性
static void
check_vmm(void) {
    // 记录当前空闲页的数量
    size_t nr_free_pages_store = nr_free_pages();

    // 检查虚拟内存区域（vma）的结构是否正确
    check_vma_struct();

    // 检查页故障处理机制的正确性
    check_pgfault();

    // Sv39 三级页表机制占用了一个额外的内存页，因此减去一个页
    nr_free_pages_store--;

    // 确保操作后的空闲页数与预期一致，以验证内存管理系统没有出现内存泄漏或错误
    assert(nr_free_pages_store == nr_free_pages());

    // 打印检查成功的信息
    cprintf("check_vmm() succeeded.\n");
}


// check_vma_struct - 检查虚拟内存区域（VMA）结构的正确性
static void
check_vma_struct(void) {
    // 记录当前空闲页的数量，以便在检查结束后验证内存管理的正确性
    size_t nr_free_pages_store = nr_free_pages();

    // 创建一个内存管理结构体（mm_struct）
    struct mm_struct *mm = mm_create();
    assert(mm != NULL);  // 确保创建成功

    int step1 = 10, step2 = step1 * 10;

    int i;
    // 插入 step1 个 vma，从地址 5 * step1 开始递减
    for (i = step1; i >= 1; i--) {
        // 创建一个 vma，起始地址为 i * 5，结束地址为 i * 5 + 2
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);  // 确保创建成功
        // 将创建的 vma 插入到 mm 的 mmap_list 中
        insert_vma_struct(mm, vma);
    }

    // 插入 step2 - step1 个 vma，从 step1 + 1 到 step2
    for (i = step1 + 1; i <= step2; i++) {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    // 检查 mmap_list 链表中的 vma 是否按顺序插入
    list_entry_t *le = list_next(&(mm->mmap_list));
    for (i = 1; i <= step2; i++) {
        // 确保没有越过链表的末尾
        assert(le != &(mm->mmap_list));
        struct vma_struct *mmap = le2vma(le, list_link);
        // 验证 vma 的起始地址和结束地址是否正确
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);  // 移动到下一个节点
    }

    // 查找虚拟内存地址，验证 vma 的存在性
    for (i = 5; i <= 5 * step2; i += 5) {
        // 查找包含地址 i 的 vma
        struct vma_struct *vma1 = find_vma(mm, i);
        assert(vma1 != NULL);  // 确保找到了对应的 vma
        // 查找包含地址 i + 1 的 vma
        struct vma_struct *vma2 = find_vma(mm, i + 1);
        assert(vma2 != NULL);  // 确保找到了对应的 vma
        // 查找包含地址 i + 2 的 vma，应该不存在
        struct vma_struct *vma3 = find_vma(mm, i + 2);
        assert(vma3 == NULL);
        // 查找包含地址 i + 3 的 vma，应该不存在
        struct vma_struct *vma4 = find_vma(mm, i + 3);
        assert(vma4 == NULL);
        // 查找包含地址 i + 4 的 vma，应该不存在
        struct vma_struct *vma5 = find_vma(mm, i + 4);
        assert(vma5 == NULL);

        // 验证找到的 vma 的起始地址和结束地址是否正确
        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
    }

    // 查找地址小于 5 的 vma，验证是否不存在
    for (i = 4; i >= 0; i--) {
        struct vma_struct *vma_below_5 = find_vma(mm, i);
        if (vma_below_5 != NULL) {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start, vma_below_5->vm_end);
        }
        assert(vma_below_5 == NULL);  // 确保找不到对应的 vma
    }

    // 销毁 mm 结构体，释放所有内存
    mm_destroy(mm);

    // 验证销毁后空闲页的数量是否与最初相同，确保没有内存泄漏
    assert(nr_free_pages_store == nr_free_pages());

    // 打印成功信息
    cprintf("check_vma_struct() succeeded!\n");
}


struct mm_struct *check_mm_struct;

// check_pgfault - 检查页故障处理程序的正确性
static void
check_pgfault(void) {
    // 保存当前空闲页数量，以便在测试完成后验证是否有内存泄漏
    size_t nr_free_pages_store = nr_free_pages();

    // 创建一个内存管理结构体 mm，并将其赋值给全局变量 check_mm_struct
    check_mm_struct = mm_create();
    assert(check_mm_struct != NULL);  // 确保 mm 创建成功

    struct mm_struct *mm = check_mm_struct;

    // 设置 mm 的页目录表指针为系统的启动页目录
    pde_t *pgdir = mm->pgdir = boot_pgdir;

    // 确保页目录的第一个页表项为 0，即尚未映射
    assert(pgdir[0] == 0);

    // 创建一个虚拟内存区域 vma，地址范围从 0 到 PTSIZE，且具有写权限
    struct vma_struct *vma = vma_create(0, PTSIZE, VM_WRITE);
    assert(vma != NULL);  // 确保 vma 创建成功

    // 将 vma 插入到 mm 的虚拟内存区域链表中
    insert_vma_struct(mm, vma);

    // 设置一个虚拟地址 addr，确保可以在 mm 中找到该地址对应的 vma
    uintptr_t addr = 0x100;
    assert(find_vma(mm, addr) == vma);

    // 向 addr 开始的内存地址写入数据，确保页故障处理正常工作
    int i, sum = 0;
    for (i = 0; i < 100; i++) {
        *(char *)(addr + i) = i;  // 写入数据
        sum += i;                 // 计算写入数据的和
    }

    // 读取之前写入的数据，确保所有数据都能正确读取
    for (i = 0; i < 100; i++) {
        sum -= *(char *)(addr + i);  // 读取数据并减去
    }
    assert(sum == 0);  // 确保读取的数据与写入的数据一致

    // 移除地址 addr 对应的页表项
    page_remove(pgdir, ROUNDDOWN(addr, PGSIZE));

    // 释放页表的第一级页表页
    free_page(pde2page(pgdir[0]));

    // 重置页目录的第一个页表项为 0
    pgdir[0] = 0;

    // 将 mm 的页目录表指针设为 NULL，销毁 mm 结构体，释放所有资源
    mm->pgdir = NULL;
    mm_destroy(mm);

    // 将全局变量 check_mm_struct 设为 NULL，避免悬空指针
    check_mm_struct = NULL;

    // Sv39 三级页表会多占用一个内存页，因此在验证空闲页数量时需要减去 1
    nr_free_pages_store--;

    // 确保内存管理的空闲页数量与预期一致，验证是否有内存泄漏
    assert(nr_free_pages_store == nr_free_pages());

    // 打印成功信息，表示页故障处理测试通过
    cprintf("check_pgfault() succeeded!\n");
}

// 页故障次数统计变量
volatile unsigned int pgfault_num = 0;


/* do_pgfault - 页故障异常的中断处理程序
 * @mm         : 控制结构体，用于管理使用相同页目录表（PDT）的一组虚拟内存区域（VMA）
 * @error_code : 页故障发生时，x86 硬件在 trapframe->tf_err 中记录的错误代码
 * @addr       : 导致内存访问异常的地址（CR2 寄存器的内容）

 * 调用流程: trap --> trap_dispatch --> pgfault_handler --> do_pgfault
 * 处理器为 uCore 的 do_pgfault 函数提供了两个信息，以帮助诊断异常并进行恢复：
 *   (1) CR2 寄存器的内容。处理器会将导致异常的 32 位线性地址加载到 CR2 寄存器中。
 *       do_pgfault 函数可以使用该地址找到相应的页目录和页表项。
 *   (2) 内核栈上的错误代码。页故障的错误代码格式与其他异常不同。错误代码为异常处理程序提供了以下三点信息：
 *         -- P 标志（位 0）指示异常是由于页面不存在（0），还是由于访问权限违规或使用保留位（1）而引起的。
 *         -- W/R 标志（位 1）指示导致异常的内存访问是读（0）还是写（1）。
 *         -- U/S 标志（位 2）指示在发生异常时，处理器是在用户模式（1）下执行，还是在特权模式（0）下执行。
 */

/*
`do_pgfault()`函数在`vmm.c`定义，是页面置换机制的核心。如果过程可行，没有错误值返回，
我们就可对页表做对应的修改，通过加入对应的页表项，并把硬盘上的数据换进内存，
这时还可能涉及到要把内存里的一个页换出去，而`do_pgfault()`函数就实现了这些功能。
如果你对这部分内容相当了解的话，那可以说你对于页面置换机制的掌握程度已经很棒了。
*/


// do_pgfault - 处理页故障异常的中断处理函数
// @mm         : 虚拟内存区域（VMA）使用相同页目录表（PDT）的管理结构体
// @error_code : 页故障发生时记录在 trapframe->tf_err 中的错误代码
// @addr       : 引发内存访问异常的地址

int
do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    int ret = -E_INVAL; // 初始化返回值，默认设置为无效地址错误
    // 查找包含地址 addr 的 vma
    struct vma_struct *vma = find_vma(mm, addr);

    pgfault_num++; // 增加页故障计数
    // 如果地址不在任何 vma 的范围内，则认为地址无效
    if (vma == NULL || vma->vm_start > addr) {
        cprintf("not valid addr %x, and can not find it in vma\n", addr);
        goto failed; // 跳到错误处理
    }

    /* 如果：
     * - 写入已存在的地址，或者
     * - 写入一个不存在的地址，并且该地址可写，或者
     * - 读取一个不存在的地址，并且该地址可读
     * 则继续处理页故障
     */
    uint32_t perm = PTE_U; // 基础权限为用户访问权限
    if (vma->vm_flags & VM_WRITE) {
        perm |= (PTE_R | PTE_W); // 如果 vma 有写权限，添加读写权限
    }
    addr = ROUNDDOWN(addr, PGSIZE); // 将地址对齐到页大小

    ret = -E_NO_MEM; // 如果后续操作失败，默认返回内存不足错误

    pte_t *ptep = NULL;
    // 尝试找到页表项，如果页表不存在，则创建页表
    ptep = get_pte(mm->pgdir, addr, 1);
    if (*ptep == 0) { // 如果页表项为空，表示页面未分配
        // 分配一个新的物理页面并建立地址映射
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL) {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    } else { // 页表项不是空，可能是一个交换条目
        if (swap_init_ok) { // 检查是否已经初始化了页面置换
            struct Page *page = NULL;

            // (1) 根据 mm 和 addr，将正确的磁盘页面加载到内存中
            int result = swap_in(mm, addr, &page); // 调用 swap_in 函数从磁盘加载页面
            if (result != 0) {
                cprintf("swap_in failed\n");
                goto failed; // 如果加载失败，跳到错误处理
            }

            // (2) 根据 mm、addr 和 page 设置物理地址与逻辑地址之间的映射,将页面插入页表
            if (page_insert(mm->pgdir, page, addr, perm) != 0) {
                cprintf("page_insert failed\n");
                goto failed; // 如果插入失败，跳到错误处理
            }

            // (3) 将页面设置为可交换
            if (swap_map_swappable(mm, addr, page, 1) != 0) {
                cprintf("swap_map_swappable failed\n");
                goto failed; // 如果设置为可交换失败，跳到错误处理
            }

            page->pra_vaddr = addr; // 设置页面的虚拟地址为 addr
        } else {
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed; // 如果页面置换未初始化且页表项不为空，跳到错误处理
        }
    }

    ret = 0; // 成功处理页故障
failed:
    return ret; // 返回处理结果
}


/*
请描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。
#### 潜在用处

页目录项（Page Directory Entry）和页表项（Page Table Entry）中的合法位可以用来判断该页面是否存在，
还有一些其他的权限位比如可读可写，可以在CLOCK算法或LRU算法中进行调用。

表项中 PTE_A 表示内存页是否被访问过，PTE_D 表示内存页是否被修改过，借助着两位标志位可以实现 **Enhanced Clock 算法**。

**改进的时钟（Enhanced Clock）页替换算法**：
在时钟置换算法中，淘汰一个页面时只考虑了页面是否被访问过，但在实际情况中，还应考虑被淘汰的页面是否被修改过。
因为淘汰修改过的页面还需要写回硬盘，使得其置换代价大于未修改过的页面，所以优先淘汰没有修改的页，减少磁盘操作次数。
改进的时钟置换算法除了考虑页面的访问情况，还需考虑页面的修改情况。即该算法不但希望淘汰的页面是最近未使用的页，
而且还希望被淘汰的页是在主存驻留期间其页面内容未被修改过的。这需要为每一页的对应页表项内容中增加一位引用位和一位修改位。
当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当该页被“写”时，CPU 中的 MMU 硬件将把修改位置“1”。
这样这两位就存在四种可能的组合情况：（0，0）表示最近未被引用也未被修改，首先选择此页淘汰；（0，1）最近未被使用，
但被修改，其次选择；（1，0）最近使用而未修改，再次选择；（1，1）最近使用且修改，最后选择。该算法与时钟算法相比，
可进一步减少磁盘的 I/O 操作次数，但为了查找到一个尽可能适合淘汰的页面，可能需要经过多次扫描，增加了算法本身的执行开销。




*/