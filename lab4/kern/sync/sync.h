#ifndef __KERN_SYNC_SYNC_H__
#define __KERN_SYNC_SYNC_H__

#include <defs.h>
#include <intr.h>
#include <riscv.h>


/*
保存中断状态并禁用中断。具体步骤如下：

1、read_csr(sstatus)用于读取sstatus寄存器的值，该寄存器存储了中断使能位（SIE）的状态。
2、& SSTATUS_SIE用于将读取到的sstatus寄存器的值与SSTATUS_SIE进行按位与操作，以判断中断使能位是否被设置。
3、如果中断使能位被设置，说明当前中断是开启状态，需要禁用中断。intr_disable()函数用于禁用中断。
4、返回值为1，表示中断状态已保存并被禁用。
5、如果中断使能位未被设置，说明当前中断是关闭状态，无需进行任何操作。
6、返回值为0，表示中断状态未保存。
*/
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}


/*
local_intr_save(x) 宏将当前的中断状态保存到变量 x 中，
并在保存完成后立即恢复中断。这样做的目的是为了在一段代码中禁用中断，
执行完后再恢复原来的中断状态。

local_intr_restore(x) 宏用于恢复之前保存的中断状态，
将变量 x 中保存的中断状态恢复到处理器中。
这样做的目的是为了在禁用中断后，恢复到之前的中断状态，以保证系统正常运行。
*/
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

#endif /* !__KERN_SYNC_SYNC_H__ */
