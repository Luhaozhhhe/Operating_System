#include <swap.h>
#include <swapfs.h>
#include <mmu.h>
#include <fs.h>
#include <ide.h>
#include <pmm.h>
#include <assert.h>


//做一些检查
// 初始化交换文件系统
void
swapfs_init(void) {
    static_assert((PGSIZE % SECTSIZE) == 0);// 检查页大小是否是扇区大小的倍数
    // 检查是否有有效的交换设备（即是否有用于存储交换数据的设备）
    if (!ide_device_valid(SWAP_DEV_NO)) {
        panic("swap fs isn't available.\n");
    }
    //swap.c/swap.h里的全局变量
    // 初始化全局变量 max_swap_offset，用于表示交换空间的最大偏移量
    max_swap_offset = ide_device_size(SWAP_DEV_NO) / (PGSIZE / SECTSIZE);
}

// 从交换文件系统读取数据
int
swapfs_read(swap_entry_t entry, struct Page *page) {
    // ide_read_secs 函数用于从交换设备中读取数据，读取的数据大小为 PAGE_NSECT 扇区，存放到物理页的虚拟地址中
    return ide_read_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}

// 向交换文件系统写入数据
int
swapfs_write(swap_entry_t entry, struct Page *page) {
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}

