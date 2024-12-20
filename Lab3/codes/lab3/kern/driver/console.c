#include <sbi.h>
#include <sync.h>
#include <defs.h>
#include <console.h>

/* kbd_intr - try to feed input characters from keyboard */
// 键盘中断处理函数，尝试从键盘获取输入字符
void kbd_intr(void) {}

/* serial_intr - try to feed input characters from serial port */
// 串口中断处理函数，尝试从串口获取输入字符
void serial_intr(void) {}

/* cons_init - initializes the console devices */
// 控制台初始化函数，初始化控制台设备
void cons_init(void) {}

/* cons_putc - print a single character @c to console devices */
// cons_putc - 将单个字符 @c 输出到控制台设备
void cons_putc(int c) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        sbi_console_putchar((unsigned char)c);
    }
    local_intr_restore(intr_flag);
}

/* *
 * cons_getc - return the next input character from console,
 * or 0 if none waiting.
 * */
// cons_getc - 返回控制台的下一个输入字符，若没有则返回 0
int cons_getc(void) {
    int c = 0;
    bool intr_flag;
    // 关闭本地中断，并保存当前中断状态
    local_intr_save(intr_flag);
    {
        // 使用 SBI 调用从控制台获取输入字符
        c = sbi_console_getchar();
    }
    // 恢复之前的中断状态
    local_intr_restore(intr_flag);
    return c;
}
