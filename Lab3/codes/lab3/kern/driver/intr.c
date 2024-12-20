#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt */
// intr_enable - 启用IRQ中断
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
// intr_disable - 禁用IRQ中断
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
