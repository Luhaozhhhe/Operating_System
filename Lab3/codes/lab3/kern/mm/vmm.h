#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <sync.h>

// 预定义的结构体
struct mm_struct;

/*
`vma_struct` 结构体描述一段连续的虚拟地址空间，从 `vm_start` 到 `vm_end`。
通过包含一个 `list_entry_t` 成员，我们可以将同一个页表对应的多个 `vma_struct` 结构体串成一个链表，
并在链表中按区间的起始点对它们进行排序。
*/

struct vma_struct {
    struct mm_struct *vm_mm; // 指向使用相同页目录表 (PDT) 的 vma 集合
    uintptr_t vm_start;      // vma 的起始地址
    uintptr_t vm_end;        // vma 的结束地址，不包含 `vm_end` 本身
    uint_t vm_flags;         // vma 的标志，描述权限（如可读、可写、可执行等）
    // `vm_flags` 表示一段虚拟地址对应的权限，这些权限也会在页表项中进行相应的设置
    list_entry_t list_link;  // 用于将 vma 结构体按起始地址排序连接成线性链表
};

// 宏 `le2vma` 用于将链表项转换为 `vma_struct` 结构体
#define le2vma(le, member)                  \
    to_struct((le), struct vma_struct, member)

// 定义虚拟内存区域 (VMA) 的权限标志
#define VM_READ                 0x00000001 // 可读权限
#define VM_WRITE                0x00000002 // 可写权限
#define VM_EXEC                 0x00000004 // 可执行权限

// 控制使用相同页目录表 (PDT) 的一组 vma 的结构体
struct mm_struct {
    list_entry_t mmap_list;        // 按 vma 起始地址排序的线性链表链接
    struct vma_struct *mmap_cache; // 当前访问的 vma，用于加快查找速度
    pde_t *pgdir;                  // 这些 vma 的页目录表 (PDT)
    int map_count;                 // 这些 vma 的数量
    void *sm_priv;                 // 用于页面置换管理器的私有数据
};


/* find_vma - 根据地址查找对应的 vma 结构体
 * 该函数用于查找 `mm` 中包含地址 `addr` 的 vma，如果找不到，则返回第一个大于该地址的 vma。
 */
struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);

/* vma_create - 创建一个新的 vma 结构体
 * 该函数用于创建一个新的 vma，包含起始地址、结束地址和权限标志。
 */
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint_t vm_flags);

/* insert_vma_struct - 将 vma 插入到 mm 的 vma 链表中
 * 该函数用于将新的 vma 插入到 `mm` 的 vma 链表中，确保链表按地址顺序排序。
 */
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma);

/* mm_create - 创建一个新的 mm_struct 结构体
 * 该函数用于为进程或线程创建一个新的内存管理结构体，用于管理该进程的虚拟内存区域。
 */
struct mm_struct *mm_create(void);

/* mm_destroy - 销毁一个 mm_struct 结构体
 * 该函数用于释放 `mm` 结构体中的所有资源，销毁与该进程相关的虚拟内存区域。
 */
void mm_destroy(struct mm_struct *mm);

/* vmm_init - 初始化虚拟内存管理
 * 该函数用于初始化内核中的虚拟内存管理系统。
 */
void vmm_init(void);

/* do_pgfault - 处理页故障
 * 当页故障发生时，调用该函数进行处理。`mm` 是内存管理结构体，`error_code` 是错误码，`addr` 是发生页故障的地址。
 */
int do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr);

// 外部变量声明

extern volatile unsigned int pgfault_num; // 记录页故障的数量
extern struct mm_struct *check_mm_struct;  // 用于调试的 mm_struct 指针

#endif /* !__KERN_MM_VMM_H__ */
