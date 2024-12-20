#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <pmm.h>
#include <vmm.h>
#include <ide.h>
#include <swap.h>
#include <kmonitor.h>

// `kern_init` 是内核的主要初始化函数。
// `noreturn` 属性表示该函数不会返回。
int kern_init(void) __attribute__((noreturn));

// 用于测试堆栈回溯的函数原型。
void grade_backtrace(void);

// 内核初始化函数
int
kern_init(void) {
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    // grade_backtrace();

    pmm_init();   // 初始化内存管理空间

    idt_init();  // 用于初始化处理器中断控制器（PIC）和中断描述符表（IDT）

    vmm_init();  // 虚拟内存管理机制的初始化

    ide_init();  // 对用于页面换入和换出的硬盘（通常称为swap硬盘）的初始化工作——新加的！！
    swap_init(); // 初始化页面置换算法

    clock_init();  // 初始化时钟初始化
    // intr_enable(); // enable irq interrupt



    /* do nothing */
    while (1);
}

void __attribute__((noinline))
grade_backtrace2(int arg0, int arg1, int arg2, int arg3) {
    mon_backtrace(0, NULL, NULL);
}

void __attribute__((noinline))
grade_backtrace1(int arg0, int arg1) {
    grade_backtrace2(arg0, (sint_t)&arg0, arg1, (sint_t)&arg1);
}

void __attribute__((noinline))
grade_backtrace0(int arg0, sint_t arg1, int arg2) {
    grade_backtrace1(arg0, arg2);
}

void
grade_backtrace(void) {
    grade_backtrace0(0, (sint_t)kern_init, 0xffff0000);
}

static void
lab1_print_cur_status(void) {
    static int round = 0;
    round ++;
}


