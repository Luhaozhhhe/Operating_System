#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include <defs.h>

// 定义寄存器保存结构体，用于保存陷阱发生时的寄存器状态
struct pushregs {
    uintptr_t zero;  // 硬连线的零寄存器，恒为 0
    uintptr_t ra;    // 返回地址寄存器
    uintptr_t sp;    // 栈指针寄存器
    uintptr_t gp;    // 全局指针寄存器
    uintptr_t tp;    // 线程指针寄存器
    uintptr_t t0;    // 临时寄存器
    uintptr_t t1;    // 临时寄存器
    uintptr_t t2;    // 临时寄存器
    uintptr_t s0;    // 保存寄存器/帧指针
    uintptr_t s1;    // 保存寄存器
    uintptr_t a0;    // 函数参数/返回值
    uintptr_t a1;    // 函数参数/返回值
    uintptr_t a2;    // 函数参数
    uintptr_t a3;    // 函数参数
    uintptr_t a4;    // 函数参数
    uintptr_t a5;    // 函数参数
    uintptr_t a6;    // 函数参数
    uintptr_t a7;    // 函数参数
    uintptr_t s2;    // 保存寄存器
    uintptr_t s3;    // 保存寄存器
    uintptr_t s4;    // 保存寄存器
    uintptr_t s5;    // 保存寄存器
    uintptr_t s6;    // 保存寄存器
    uintptr_t s7;    // 保存寄存器
    uintptr_t s8;    // 保存寄存器
    uintptr_t s9;    // 保存寄存器
    uintptr_t s10;   // 保存寄存器
    uintptr_t s11;   // 保存寄存器
    uintptr_t t3;    // 临时寄存器
    uintptr_t t4;    // 临时寄存器
    uintptr_t t5;    // 临时寄存器
    uintptr_t t6;    // 临时寄存器
};

// 定义陷阱帧结构体，用于保存陷阱发生时的所有 CPU 状态
struct trapframe {
    struct pushregs gpr;    // 保存通用寄存器的状态
    uintptr_t status;       // 保存状态寄存器（`sstatus`）
    uintptr_t epc;          // 保存异常程序计数器（`sepc`），指向发生异常的指令地址
    uintptr_t badvaddr;     // 保存导致异常的虚拟地址（`stval`），用于页故障
    uintptr_t cause;        // 保存异常原因寄存器（`scause`），表示异常类型
};


/* trap - 处理发生的陷阱（异常/中断）
 * 当陷阱发生时，保存 CPU 的状态到 trapframe，然后调用该函数进行处理。
 * 该函数会根据陷阱的类型（中断或异常）调用相应的处理程序。
 */
void trap(struct trapframe *tf);

/* idt_init - 初始化中断描述符表（IDT）
 * 用于设置中断描述符表，将所有中断服务例程（ISR）加载到中断向量表中。
 */
void idt_init(void);

/* print_trapframe - 打印陷阱帧的详细信息
 * 用于调试陷阱处理时打印陷阱帧的内容，包括寄存器、状态等信息。
 */
void print_trapframe(struct trapframe *tf);

/* print_regs - 打印寄存器的内容
 * 用于调试陷阱处理时打印寄存器的状态。
 */
void print_regs(struct pushregs* gpr);

/* trap_in_kernel - 判断异常是否发生在内核模式
 * 用于检查陷阱帧的状态寄存器来判断陷阱发生时 CPU 是否在内核模式。
 */
bool trap_in_kernel(struct trapframe *tf);

#endif /* !__KERN_TRAP_TRAP_H__ */
