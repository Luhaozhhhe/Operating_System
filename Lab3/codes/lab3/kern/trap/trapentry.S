#include <riscv.h> // 包含 RISC-V 架构的定义

// 定义宏 SAVE_ALL，用于保存所有寄存器状态
    .macro SAVE_ALL
    .align 2 // 对齐到 4 字节边界

    // 将当前栈指针保存到 sscratch 寄存器
    csrw sscratch, sp

    // 为保存寄存器腾出栈空间，sp 向下移动 36 个寄存器的大小
    addi sp, sp, -36 * REGBYTES

    # 保存通用寄存器（x1 到 x31）
    STORE  x1, 1*REGBYTES(sp)
    STORE  x3, 3*REGBYTES(sp)
    STORE  x4, 4*REGBYTES(sp)
    STORE  x5, 5*REGBYTES(sp)
    STORE  x6, 6*REGBYTES(sp)
    STORE  x7, 7*REGBYTES(sp)
    STORE  x8, 8*REGBYTES(sp)
    STORE  x9, 9*REGBYTES(sp)
    STORE  x10, 10*REGBYTES(sp)
    STORE  x11, 11*REGBYTES(sp)
    STORE  x12, 12*REGBYTES(sp)
    STORE  x13, 13*REGBYTES(sp)
    STORE  x14, 14*REGBYTES(sp)
    STORE  x15, 15*REGBYTES(sp)
    STORE  x16, 16*REGBYTES(sp)
    STORE  x17, 17*REGBYTES(sp)
    STORE  x18, 18*REGBYTES(sp)
    STORE  x19, 19*REGBYTES(sp)
    STORE  x20, 20*REGBYTES(sp)
    STORE  x21, 21*REGBYTES(sp)
    STORE  x22, 22*REGBYTES(sp)
    STORE  x23, 23*REGBYTES(sp)
    STORE  x24, 24*REGBYTES(sp)
    STORE  x25, 25*REGBYTES(sp)
    STORE  x26, 26*REGBYTES(sp)
    STORE  x27, 27*REGBYTES(sp)
    STORE  x28, 28*REGBYTES(sp)
    STORE  x29, 29*REGBYTES(sp)
    STORE  x30, 30*REGBYTES(sp)
    STORE  x31, 31*REGBYTES(sp)

    # 保存特殊寄存器：sscratch、sstatus、sepc、badvaddr（地址错误寄存器）、scause
    csrr s0, sscratch
    csrr s1, sstatus
    csrr s2, sepc
    csrr s3, 0x143 // 访问寄存器 stval，用于记录错误虚拟地址（旧版称为 sbadvaddr）
    csrr s4, scause

    # 将特殊寄存器保存到栈中
    STORE s0, 2*REGBYTES(sp)
    STORE s1, 32*REGBYTES(sp)
    STORE s2, 33*REGBYTES(sp)
    STORE s3, 34*REGBYTES(sp)
    STORE s4, 35*REGBYTES(sp)
    .endm

// 定义宏 RESTORE_ALL，用于恢复所有寄存器状态
    .macro RESTORE_ALL

    # 从栈中恢复特殊寄存器 sstatus 和 sepc
    LOAD s1, 32*REGBYTES(sp)
    LOAD s2, 33*REGBYTES(sp)

    # 恢复 sstatus 和 sepc 的值
    csrw sstatus, s1
    csrw sepc, s2

    // 恢复通用寄存器（x1 到 x31）
    LOAD  x1, 1*REGBYTES(sp)
    LOAD  x3, 3*REGBYTES(sp)
    LOAD  x4, 4*REGBYTES(sp)
    LOAD  x5, 5*REGBYTES(sp)
    LOAD  x6, 6*REGBYTES(sp)
    LOAD  x7, 7*REGBYTES(sp)
    LOAD  x8, 8*REGBYTES(sp)
    LOAD  x9, 9*REGBYTES(sp)
    LOAD  x10, 10*REGBYTES(sp)
    LOAD  x11, 11*REGBYTES(sp)
    LOAD  x12, 12*REGBYTES(sp)
    LOAD  x13, 13*REGBYTES(sp)
    LOAD  x14, 14*REGBYTES(sp)
    LOAD  x15, 15*REGBYTES(sp)
    LOAD  x16, 16*REGBYTES(sp)
    LOAD  x17, 17*REGBYTES(sp)
    LOAD  x18, 18*REGBYTES(sp)
    LOAD  x19, 19*REGBYTES(sp)
    LOAD  x20, 20*REGBYTES(sp)
    LOAD  x21, 21*REGBYTES(sp)
    LOAD  x22, 22*REGBYTES(sp)
    LOAD  x23, 23*REGBYTES(sp)
    LOAD  x24, 24*REGBYTES(sp)
    LOAD  x25, 25*REGBYTES(sp)
    LOAD  x26, 26*REGBYTES(sp)
    LOAD  x27, 27*REGBYTES(sp)
    LOAD  x28, 28*REGBYTES(sp)
    LOAD  x29, 29*REGBYTES(sp)
    LOAD  x30, 30*REGBYTES(sp)
    LOAD  x31, 31*REGBYTES(sp)

    # 最后恢复栈指针寄存器
    LOAD  x2, 2*REGBYTES(sp)
    .endm

// 对齐到 16 字节边界
    .align 4
    .globl __alltraps
__alltraps:
    SAVE_ALL  // 保存所有寄存器的状态

    move  a0, sp  // 将当前栈指针（包含陷阱帧的内容）传递给 a0
    jal trap  // 调用 C 函数 trap，处理陷阱

    // sp 应该在调用 trap 之后保持不变
    .globl __trapret
__trapret:
    RESTORE_ALL // 恢复所有寄存器的状态

    // 返回到发生陷阱的位置
    sret // 使用 sret 指令从异常返回
