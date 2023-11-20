# Lab4进程管理

小组成员：蒋薇（2110957） 李鑫（2112047） 丁岩（2111840）

## 实验目的

1. 了解内核线程创建和执行的管理过程

2. 了解内核线程的切换和基本调度过程

## 实验内容

### 练习1:分配并初始化一个进程控制块

alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，需要完成这个初始化过程。

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明proc_struct中`struct context context`和`struct trapframe *tf`成员变量含义和在本实验中的作用是啥？（提示：通过看代码和编程调试可以判断出来）

#### 答：

alloc_proc初始化了某个进程所需的进程控制块（PCB）结构体，即为PCB结构体中的各个属性赋值。

```c
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
    //LAB4:EXERCISE1 
    /*
     * below fields in proc_struct need to be initialized
     *       enum proc_state state;        // 进程所处的状态
     *       int pid;                      // 进程的ID
     *       int runs;                     // the running times of Proces
     *       uintptr_t kstack;             // 进程的内核栈的位置
     *       volatile bool need_resched;   // bool value:是否需要调度释放CPU资源
     *       struct proc_struct *parent;   // 父进程
     *       struct mm_struct *mm;         // 进程虚拟内存的结构体
     *       struct context context;       //进程的上下文，用与进程切换
     *       struct trapframe *tf;         // 中断帧的指针，总是指向内核栈的某个位置
     *       uintptr_t cr3;                // 记录当前使用的页表地址（PDT）
     *       uint32_t flags;               // Process flag
     *       char name[PROC_NAME_LEN + 1]; // Process name
     */
        proc->state = PROC_UNINIT;//未分配PCB对应的资源，状态设为初始状态
        proc->pid = -1;
        proc->runs = 0;//处于分配阶段没有运行
        proc->kstack = 0;//暂未分配内核栈
        proc->need_resched = 0;//不需调度其他进程，CPU资源未分配
        proc->parent = NULL;//无父进程
        proc->mm = NULL;//当前未分配内存
        memset(&(proc->context), 0, sizeof(struct context));//上下文置0
        proc->tf = NULL;//无中断帧
        proc->cr3 = boot_cr3;//共享内核空间，页表相同
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
    }
    return proc;
}
```

context和tf成员变量含义和在本实验中的作用：

context：表示进程上下文信息，包括程序计数器、寄存器状态、内存管理信息等。它记录了进程执行的环境和状态，当进程被切换时，需要保存当前进程的上下文信息，并加载新进程的上下文信息。上下文切换是操作系统进行进程调度和管理的关键操作。因此，"context" 是用于描述和保存进程状态的重要数据结构。

tf：表示中断上下文信息，它是在处理中断或异常时，用于保存被中断进程的状态的数据结构。"tf" 包含了被中断进程在被中断时的寄存器状态、指令指针和其他相关信息，以便在中断处理程序执行完毕后能够恢复被中断进程的执行现场。

二者相互配合实现进程切换的。proc_run函数中调用的switch_to函数，使用context保存原进程上下文并恢复现进程上下文。然后，由于在初始化context时将其ra设置为forkret函数入口，所以会返回到forkret函数，它封装了forkrets函数，而该函数的参数是当前进程的tf，该函数调用了__trapret来恢复所有寄存器的值。需要注意的是，在初始化tf时将其epc设置为了kernel_thread_entry，这个函数基于s0（新进程的函数）和s1（传给函数的参数）寄存器，实现了当前进程即initproc的功能，即输出“Hello World!”。

### 练习2：为新创建的内核线程分配资源

kernel_thread函数通过调用**do_fork**函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们**实际需要"fork"的东西就是stack和trapframe**。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

请回答如下问题：

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

#### 答：

dokernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。
——————————————————————————

调用 alloc_proc ，首先获得一块用户信息块。

为进程分配一个内核栈。

复制原进程的内存管理信息到新进程（但内核线程不必做此事）

复制原进程上下文到新进程

将新进程添加到进程列表

唤醒新进程

返回新进程号v

```c
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC; 
    struct proc_struct *proc;

    // 检查进程数是否已达到上限
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }

    ret = -E_NO_MEM; // 设置错误码为内存不足

    // LAB4:EXERCISE2
    /*
       1、调用 alloc_proc 函数来分配一个 proc_struct 结构体，用于表示子进程。
       2、调用 setup_kstack 函数为子进程分配一个内核栈。
       3、调用 copy_mm 函数根据 clone_flags 来复制或共享父进程的内存管理结构 mm。
       4、调用 copy_thread 函数来设置子进程的 trapframe 和 context，以及设置进程的内核入口点和栈。
       5、将 proc_struct 插入到 proc hash_list 和 proc_list 中。
       6、调用 wakeup_proc 函数将新的子进程设置为可运行状态。
       7、使用子进程的 pid 来设置 ret 值，即返回子进程的 pid 作为函数的返回值。
     */

    // 调用 alloc_proc 函数分配 proc_struct 结构体
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }

    proc->parent = current; // 设置子进程的父进程为当前进程

    // 调用 setup_kstack 函数为子进程分配内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }

    // 调用 copy_mm 函数复制或共享父进程的内存管理结构
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }

    // 调用 copy_thread 函数设置子进程的 trapframe 和 context
    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag); // 禁用中断
    {
        proc->pid = get_pid(); // 获取新子进程的 pid
        hash_proc(proc); // 将 proc_struct 插入到 proc hash_list 中
        list_add(&proc_list, &(proc->list_link)); // 将 proc_struct 插入到 proc_list 中
        nr_process++; // 增加进程数
    }
    local_intr_restore(intr_flag); // 恢复中断状态

    wakeup_proc(proc); // 唤醒新的子进程

    ret = proc->pid; // 设置返回值为子进程的 pid

fork_out:
    return ret; // 返回结果

bad_fork_cleanup_kstack:
    put_kstack(proc); // 清理内核栈
bad_fork_cleanup_proc:
    kfree(proc); // 释放 proc_struct 内存
    goto fork_out;
}
```

ucore通过 get_pid() 函数来给每一个新fork的进程唯一的id。

- 在 get_pid（） 首次被调用时，就会返回last_pid=1;也就是说返回1；

- 第二次被调用时：

由于++last_pid<MAX_PID,不能进入if分支。

由于last_pid<next_safe(MAX_PID),因此，不进入while、不进入if分支，直接返回2；

- 第MAX_PID-1次调用时：

++last_pid=MAX_PID，因此进入if分支，last_pid=1；

进入inside之后，遍历整个proc链表

如果找到last_pid,将last_pid+=1;

如果last_pid>=next_safe：重找！（last_pid>=MAXPID,置1）

如果当前遍历到的proc并不是last_pid，但是比当前的last_pid还要大，当前的pid也在

next_safe的范围,那就将next_safe置为当前的pid,接着下一个找。如此一来，如果有与last_pid相同的pid，那么last_pid+1；如果没有，但是找超过了，那就重新设置next_safe.继续调用，如果调用到next_safe-1时：那就将next_safe=MAX_PID,继续遍历链表，如果有与last_pid相同的，就将last_pid+1，接着遍历;如果没有，就啥都不干接着遍历。

如此一来，就能保障能找到唯一的pid

### 练习3：编写proc_run函数

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
- 禁用中断。你可以使用`/kern/sync/sync.h`中定义好的宏`local_intr_save(x)`和`local_intr_restore(x)`来实现关、开中断。
- 切换当前进程为要运行的进程。
- 切换页表，以便使用新进程的地址空间。`/libs/riscv.h`中提供了`lcr3(unsigned int cr3)`函数，可实现修改CR3寄存器值的功能。
- 实现上下文切换。`/kern/process`中已经预先编写好了`switch.S`，其中定义了`switch_to()`函数。可实现两个进程的context切换。
- 允许中断。

请回答如下问题：

- 在本实验的执行过程中，创建且运行了几个内核线程？

完成代码编写后，编译并运行代码：make qemu

#### 答：

```c
void proc_run(struct proc_struct *proc) {
    // 检查是否需要切换到的进程与当前运行的进程不同
    if (proc != current) {
        // 保存中断标志的状态
        bool intr_flag;

        // 保存指向当前进程的指针
        struct proc_struct *prev = current;

        // 暂时禁用中断
        local_intr_save(intr_flag);
        {
            // 将当前进程设置为要切换到的进程
            current = proc;
            // 加载新进程的页目录
            lcr3(proc->cr3);
            // 切换到新进程的上下文
            switch_to(&(prev->context), &(proc->context));
        }
        // 恢复中断标志的状态
        local_intr_restore(intr_flag);
    }
}
```

在本实验中，创建且运行了2两个内核线程：

①idleproc：第一个内核进程，完成内核中各个子系统的初始化，之后立即调度，执行其他进程。

②initproc：用于完成实验的功能而调度的内核进程。 

### Challenge：

- 说明语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`是如何实现开关中断的？

#### 答：

首先需要定义一个标志位（bool变量）来标识是否可以进行开关中断。在__intr_save函数中，条件语句的判断条件成立时，说明此时的中断信息已经被存储，从而可以执行后续的关中断操作。在__intr_save函数中，需要根据标志位来判断是否开中断。

另外，在定义宏 local_intr_save(x) 时使用了循环语句 do { ... } while (0) ，这种写法是为了确保在宏展开时不会产生意外的行为，并且能够被安全地嵌入到其他复合语句中去。具体而言，宏展开后可能会产生一些意想不到的情况，尤其是在使用 if 、 else 等语句时。使用循环语句的方式可以避

免这些问题，具体原因如下：

- 避免语法错误： 如果宏展开后不使用循环语句，而直接以花括号包裹，当这个宏被用在一个条件语句中时（比如 if 语句），可能会导致意外的语法错误。因为宏展开后的代码片段会与周围的代码上下文结合在一起，可能破坏原有的代码结构。

- 确保语句的完整性： 使用循环语句的方式能够确保宏展开后生成的代码是一个完整的语句，而不会受到外部代码块的干扰。即使在宏展开后，后面会跟着一个分号，也不会导致语法错误或逻辑错误。

- 安全的替代方式： 这种写法是 C 语言中一种常见的编程习惯，被广泛认可为一种安全的宏定义方式，能够避免许多潜在的问题和错误。

```c
// 保存当前的中断使能状态，并将中断禁止
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) //检查终端使能
    {
        intr_disable(); //如果可以，就调用中断不能函数
        return 1;
    }
    return 0;
}
//中断不能函数，就是清除sstatus上的中断使能位
void intr_disable(void)
{
    clear_csr(sstatus, SSTATUS_SIE); 
}
//清楚中断使能位的具体过程
//csrrc使读取、指令 控制和状态寄存器
#define clear_csr(reg, bit) ({ unsigned long __tmp; \
  asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "rK"(bit)); \
  __tmp; })

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

void intr_enable(void) 
{
    set_csr(sstatus, SSTATUS_SIE); 
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
// 这个宏实际上就是将 __intr_restore() 函数封装为一个宏，用于在代码中调用__intr_restore() 函数并传递之前保存的中断状态标志位
#define local_intr_restore(x) __intr_restore(x);
```

## 实验结果

make qemu

<img title="" src="file:///C:/Users/鑫鑫/AppData/Roaming/marktext/images/2023-11-20-13-49-32-image.png" alt="" width="339">

make grade

<img src="file:///C:/Users/鑫鑫/AppData/Roaming/marktext/images/2023-11-20-13-50-27-image.png" title="" alt="" width="567">

### 进程与线程

- 程序：编写的源代码，经过编译器编译就变成了可执行文件，这一类文件叫做程序。

- 进程：当一个程序被用户或操作系统启动，分配资源，装载进内存开始执行后，它就成为了一个进程。进程是一个“正在运行”的实体，而程序只是一个不动的文件。

- 线程：一个进程可以对应一个线程，也可以对应很多线程。这些线程之间往往具有相同的代码，共享一块内存，但是却有不同的CPU执行状态。

相比于线程，进程更多的作为一个资源管理的实体，线程作为可以被调度的最小单元，给了调度器更多的调度可能。

### 实验执行流程

LAB4进行CPU的虚拟化，让ucore实现分时共享CPU，实现多条控制流能够并发执行。

内核线程是一种特殊的进程，内核线程与用户进程的区别：

- 内核线程只运行在内核态而用户进程会在用户态和内核态交替运行；

- 所有内核线程直接使用共同的ucore内核内存空间，不需要为每个内核线程维护单独的内存空间，而用户进程需要维护各自的用户内存空间。

### 
