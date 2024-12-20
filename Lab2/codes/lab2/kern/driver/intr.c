#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt */
//用于启用中断。
//具体来说，它设置处理器状态寄存器（sstatus）中的 SSTATUS_SIE（Supervisor Interrupt Enable）位，从而允许处理器响应外部中断请求。
//intr_enable函数通过调用 set_csr函数来设置 sstatus 寄存器的特定位。set_csr函数可能是一个用于设置特定控制状态寄存器（CSR）值的函数，
//它接受两个参数，第一个参数指定要设置的 CSR 寄存器，这里是 sstatus，第二个参数指定要设置的位或值，这里是 SSTATUS_SIE。
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
//用于禁用中断。
//它清除处理器状态寄存器（sstatus）中的 SSTATUS_SIE 位，使得处理器不再响应外部中断请求。
//intr_disable函数通过调用 clear_csr函数来清除 sstatus 寄存器的特定位。clear_csr函数可能是一个用于清除特定 CSR 寄存器中特定位的函数，
//它接受两个参数，第一个参数指定要操作的 CSR 寄存器，这里是 sstatus，第二个参数指定要清除的位，这里是 SSTATUS_SIE。
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
