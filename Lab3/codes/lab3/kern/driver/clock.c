#include <clock.h>
#include <defs.h>
#include <sbi.h>
#include <stdio.h>
#include <riscv.h>

volatile size_t ticks;

// 读取当前的时钟周期数
static inline uint64_t get_cycles(void) {
#if __riscv_xlen == 64
    uint64_t n;
    __asm__ __volatile__("rdtime %0" : "=r"(n));
    return n;
#else
    uint32_t lo, hi, tmp;
    __asm__ __volatile__(
        "1:\n"
        "rdtimeh %0\n"  // 读取时间高位
        "rdtime %1\n"  // 读取时间低位
        "rdtimeh %2\n"  // 再次读取时间高位
        "bne %0, %2, 1b"  // 如果高位不相等，重新读取
        : "=&r"(hi), "=&r"(lo), "=&r"(tmp));

    // 返回合成的 64 位时间值
    return ((uint64_t)hi << 32) | lo;
#endif
}

static uint64_t timebase;

/* *
 * clock_init - 初始化 8253 时钟，使它每秒中断 100 次，
 * 然后启用 IRQ_TIMER 事件。
 * */
void clock_init(void) {
    // 分开 500 （在 Spike 中使用2MHz频率）
    // 分开 100 （在 QEMU 中使用10MHz频率）
    timebase = 1e7 / 100;
    // 设置下次时间事件
    clock_set_next_event();
    // 启用介入启动 STIP 事件
    set_csr(sie, MIP_STIP);

    // 将时间计数器 'ticks' 初始化为 0
    ticks = 0;
    // 输出初始化信息
    cprintf("++ setup timer interrupts\n");
}
// 设置下一次时间事件
// 返回当前周期数 + 定义的周期基准，以此设置下一次时间事件
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
