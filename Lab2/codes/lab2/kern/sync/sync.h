#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>

/*
它通过读取处理器状态寄存器（sstatus）的值，并检查其中的 SSTATUS_SIE 位（Supervisor Interrupt Enable）。
如果该位为 1，表示中断是启用的。在这种情况下，函数会调用 intr_disable 函数禁用中断，并返回 true。
如果中断本来就是禁用的，则直接返回 false。
也就是，判断当前的禁用状态，如果启用的情况就开启，同时返回true
*/
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

/*
它接受一个布尔类型的参数 flag。
如果 flag 为 true，表示之前中断是启用的，因此函数会调用 intr_enable 函数来重新启用中断。
如果 flag 为 false，则不进行任何操作。
*/
static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

/*
方便地保存当前中断状态到一个变量x中
*/
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)

/*
这个宏定义用于恢复之前保存的中断状态为x
*/
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
