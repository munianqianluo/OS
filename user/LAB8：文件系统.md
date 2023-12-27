# LAB8：文件系统

## 实验目的

了解文件系统抽象层-VFS的设计与实现

了解基于索引节点组织方式的Simple FS文件系统与操作的设计与实现

了解“一切皆为文件”思想的设备文件设计

了解简单系统终端的实现

## 实验内容

### 练习0：填写已有实验

本实验依赖实验2/3/4/5/6/7。请把实验2/3/4/5/6/7的代码填入本实验中代码中有“LAB2”/“LAB3”/“LAB4”/“LAB5”/“LAB6” /“LAB7”的注释相应部分。并确保编译通过。注意：为了能够正确执行lab8的测试应用程序，可能需对已完成的实验2/3/4/5/6/7的代码进行进一步改进。

proc.c 中，在 proc_struct 结构中加入了一个 file_struct，需要在 alloc_proc 时加上对它的初始化，就是加上一行：

```c
proc->filesp = NULL;
```

### 练习1: 完成读文件操作的实现（需要编码）

首先了解打开文件的处理流程，然后参考本实验后续的文件读写操作的过程分析，填写在 kern/fs/sfs/sfs_inode.c中 的sfs_io_nolock()函数，实现读文件中数据的代码。

#### 打开文件处理大致流程

文件系统，会将磁盘上的文件（程序）读取到内存里面来，在用户空间里面变成进程去进—步执行或其他操作。通过—系列系统调用完成这个过程。

① 首先是应用程序发出请求，请求硬盘中写数据或读数据，应用程序通过 FS syscall 接口执行系统调用，获得 ucore操作系统关于文件的—些服务；
② 之后，—旦操作系统内系统调用得到了请求，就会到达 VFS层面（虚拟文件系统），包含很多部分比如文件接口、目录接口等，是—个抽象层面，它屏蔽底层具体的文件系统；
③ VFS 如果得到了处理，那么VFS 会将这个 iNode 传递给 SimpleFS，此时，VFS 中的iNode 还是—个抽象的结构，在 SimpleFS中会转化为—个具体的 iNode；
④ 通过该 iNode 经过 IO 接口对磁盘进行读写。
在本实验中，第三个磁盘（即disk0，前两个磁盘分别是 ucore.img 和 swap.img）用于存放一个SFS文件系统（Simple Filesystem）。通常文件系统中，磁盘的使用是以扇区（Sector）为单位的，但是为了实现简便，SFS 中以 block （4K，与内存 page 大小相等）为基本单位。

#### 代码

```c
// (1) If offset isn't aligned with the first block
    blkoff = offset % SFS_BLKSIZE;//计算未对齐部分的大小
    if (blkoff != 0) {
        size_t first_block_size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
        //如果还有剩余块（nblks != 0），则第一个块的大小是块大小减去未对齐的部分。
        //如果没有剩余块，即这是最后一个块，则第一个块的大小是结束位置（endpos）减去偏移量。
        ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino);
        //通过块映射加载块号为blkno的块对应的磁盘块号（ino），并将其存储在ino中。
        if (ret != 0) {
            goto out;
        }
        ret = sfs_buf_op(sfs, buf, first_block_size, ino, blkoff);
        //执行文件系统缓冲区操作，将磁盘块的数据加载到缓冲区中或将缓冲区的数据写入磁盘块。
        //buf是指向缓冲区的指针，first_block_size是操作的大小，ino是磁盘块号，blkoff是未对齐的偏移量。
        if (ret != 0) {
            goto out;
        }
        alen += first_block_size;//累积已处理的数据长度
        buf += first_block_size;//将缓冲区指针移动到下一个要处理的位置
        if (nblks == 0) {
            goto out;
        }
        blkno++;
        nblks--;//减少剩余块的计数
    }
    // (2) Rd/Wr aligned blocks
    while (nblks > 0) {//处理剩余的所有块
        ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino);
        //通过块映射加载块号为blkno的块对应的磁盘块号（ino），并将其存储在ino中。
        if (ret != 0) {
            goto out;
        }
        ret = sfs_block_op(sfs, buf, ino, 1);
        if (ret != 0) {
            goto out;
        }
        alen += SFS_BLKSIZE;
        buf += SFS_BLKSIZE;
        nblks--;
        blkno++;
    }
    // (3) If the end position isn't aligned with the last block
    if (endpos % SFS_BLKSIZE != 0) {
        //检查endpos是否对齐到块的边界。如果不对齐，说明最后一个块被部分写入。
        ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino);
        if (ret != 0) {
            goto out;
        }
        ret = sfs_buf_op(sfs, buf + alen, endpos % SFS_BLKSIZE, ino, 0);
        if (ret != 0) {
            goto out;
        }
        alen += endpos % SFS_BLKSIZE;
    }  
```

### 练习2: 完成基于文件系统的执行程序机制的实现（需要编码）

改写proc.c中的load_icode函数和其他相关函数，实现基于文件系统的执行程序机制。执行：make qemu。如果能看看到sh用户程序的执行界面，则基本成功了。如果在sh用户界面上可以执行”ls”,”hello”等其他放置在sfs文件系统中的其他执行程序，则可以认为本实验基本成功。

在实验5的基础上，需要对load_icode进行更改，需要使用文件句柄来加载ELF格式的可执行文件。按照ELF文件的程序头表来加载各个段，包括TEXT、DATA、BSS等。

需要完成以下操作：

- 创建新的内存管理结构（`struct mm_struct`）和页目录表。
- 读取 ELF 文件头，检查 ELF 文件的有效性。
- 针对 ELF 文件中的每个可加载的段（ELF_PT_LOAD）：
  - 创建虚拟内存区域，设置权限和属性。
  - 分配物理页，将文件中的数据拷贝到新分配的页中。
  - 处理文件大小不等于内存大小的情况，填充多余的空间。
  - 在页表中建立映射关系。
- 关闭 ELF 文件。
- 创建用户栈区域，分配几页用于用户栈，并设置权限。
- 更新内存管理结构的引用计数，设置当前进程的内存管理结构和 CR3 寄存器，更新页目录地址寄存器。
- 在用户栈中设置 argc 和 argv。
- 设置陷阱帧，包括用户栈的栈顶、入口地址、状态寄存器等。

```c
assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);
// 如果当前进程已经有内存管理结构（mm），表示进程已经有了内存空间，触发 panic。
if (current->mm != NULL) {
    panic("load_icode: current->mm must be empty.\n");
}
int ret = -E_NO_MEM;
struct mm_struct *mm;
// 创建新的内存管理结构
if ((mm = mm_create()) == NULL) {
    goto bad_mm;
}
// 设置新的页目录表
if (setup_pgdir(mm) != 0) {
    goto bad_pgdir_cleanup_mm;
}

struct Page *page;
// 读取 ELF 文件头
struct elfhdr __elf, *elf = &__elf;
if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
    goto bad_elf_cleanup_pgdir;
}
// 检查 ELF 文件的有效性
if (elf->e_magic != ELF_MAGIC) {
    ret = -E_INVAL_ELF;
    goto bad_elf_cleanup_pgdir;
}

struct proghdr __ph, *ph = &__ph;
uint32_t vm_flags, perm, phnum;
// 循环处理 ELF 文件中的每一个程序头
for (phnum = 0; phnum < elf->e_phnum; phnum++) {
    off_t phoff = elf->e_phoff + sizeof(struct proghdr) * phnum;
    // 读取程序头
    if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
        goto bad_cleanup_mmap;
    }
    // 如果不是可加载的段，跳过
    if (ph->p_type != ELF_PT_LOAD) {
        continue;
    }
    // 检查文件大小是否合法
    if (ph->p_filesz > ph->p_memsz) {
        ret = -E_INVAL_ELF;
        goto bad_cleanup_mmap;
    }
    // 根据标志设置虚拟内存区域的权限和属性
    vm_flags = 0, perm = PTE_U | PTE_V;
    if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
    if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
    if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
    // 修改权限位，适用于 RISC-V
    if (vm_flags & VM_READ) perm |= PTE_R;
    if (vm_flags & VM_WRITE) perm |= (PTE_W | PTE_R);
    if (vm_flags & VM_EXEC) perm |= PTE_X;
    // 建立虚拟内存区域
    if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    off_t offset = ph->p_offset;
    size_t off, size;
    uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

    ret = -E_NO_MEM;

    end = ph->p_va + ph->p_filesz;
    // 循环处理每一页
    while (start < end) {
        if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
            ret = -E_NO_MEM;
            goto bad_cleanup_mmap;
        }
        off = start - la, size = PGSIZE - off, la += PGSIZE;
        if (end < la) {
            size -= la - end;
        }
        // 从文件中读取数据，拷贝到新分配的页中
        if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0) {
            goto bad_cleanup_mmap;
        }
        start += size, offset += size;
    }
    end = ph->p_va + ph->p_memsz;

    if (start < la) {
        // 处理文件大小等于内存大小的情况
        if (start == end) {
            continue;
        }
        off = start + PGSIZE - la, size = PGSIZE - off;
        if (end < la) {
            size -= la - end;
        }
        // 在新页中填充 0
        memset(page2kva(page) + off, 0, size);
        start += size;
        assert((end < la && start == end) || (end >= la && start == la));
    }
    // 处理剩余的页面
    while (start < end) {
        if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL) {
            ret = -E_NO_MEM;
            goto bad_cleanup_mmap;
        }
        off = start - la, size = PGSIZE - off, la += PGSIZE;
        if (end < la) {
            size -= la - end;
        }
        // 在新页中填充 0
        memset(page2kva(page) + off, 0, size);
        start += size;
    }
}
// 关闭文件
sysfile_close(fd);

// 创建用户栈区域
vm_flags = VM_READ | VM_WRITE | VM_STACK;
if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
    goto bad_cleanup_mmap;
}
// 分配用户栈的几页，并设置权限
assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER) != NULL);
assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER) != NULL);
assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER) != NULL);

// 增加内存管理结构的引用计数
mm_count_inc(mm);
// 设置当前进程的内存管理结构、cr3 寄存器，更新页目录地址寄存器
current->mm = mm;
current->cr3 = PADDR(mm->pgdir);
lcr3(PADDR(mm->pgdir));

// 设置 argc 和 argv 在用户栈中的位置
uint32_t argv_size = 0, i;
for (i = 0; i < argc; i++) {
    argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
}
uintptr_t stacktop = USTACKTOP - (argv_size / sizeof(long) + 1) * sizeof(long);
char **uargv = (char **)(stacktop - argc * sizeof(char *));
argv_size = 0;
for (i = 0; i < argc; i++) {
    uargv[i] = strcpy((char *)(stacktop + argv_size), kargv[i]);
    argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
}
// 设置用户栈的栈顶
stacktop = (uintptr_t)uargv - sizeof(int);
*(int *)stacktop = argc;

// 设置陷阱帧，包括用户栈的栈顶、入口地址、状态寄存器等
struct trapframe *tf = current->tf;
// 保留 sstatus 寄存器的值
uintptr_t sstatus = tf->status;
memset(tf, 0, sizeof(struct trapframe));
tf->gpr.sp = stacktop;
tf->epc = elf->e_entry;
tf->status = sstatus & ~(SSTATUS_SPP | SSTATUS_SPIE);
ret = 0;
// 返回成功
out:
    return ret;

// 错误处理，需要清理已经分配的资源
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
```

### 扩展练习 Challenge1：完成基于“UNIX的PIPE机制”的设计方案

如果要在ucore里加入UNIX的管道（Pipe)机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个(或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的PIPE机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）

**解答**：

数据结构定义：

1. **管道结构（`struct pipe`）：**
   
   ```c
   struct pipe {
     struct spinlock lock;      // 用于管道操作的自旋锁
     struct semaphore rsem;     // 读信号量
     struct semaphore wsem;     // 写信号量
     struct pipe_buffer *buffer; // 管道缓冲区
   };
   ```

2. **管道缓冲区结构（`struct pipe_buffer`）：**
   
   ```c
   struct pipe_buffer {
     char data[PIPE_BUF_SIZE];  // 管道数据缓冲区
     size_t size;                // 当前缓冲区中的数据大小
   };
   ```

接口设计：

1. **初始化管道（`pipe_init`）：**
   
   ```c
   void pipe_init(struct pipe *p) {
       spinlock_init(&p->lock);            //初始化自旋锁
       sem_init(&p->rsem, 0);              // 初始化为 0，表示初始时没有可读数据
       sem_init(&p->wsem, PIPE_BUF_SIZE);  // 初始化为缓冲区大小，表示初始时有整个缓冲区可写
       p->buffer = create_pipe_buffer();   // 创建管道缓冲区
   }
   ```

2. **读取数据（`pipe_read`）：**
   
   ```c
   ssize_t pipe_read(struct pipe *p, char *buf, size_t count) {
       sem_wait(&p->rsem); // 等待可读信号
       spinlock_acquire(&p->lock);//获取管道的自旋锁，确保在读取数据时不会有其他线程或进程同时修改管道数据
   
       size_t read_size = min(count, p->buffer->size);//计算实际需要读取的数据大小
       memcpy(buf, p->buffer->data, read_size);//将管道缓冲区中的数据复制到目标缓冲区 
       p->buffer->size -= read_size;//更新管道缓冲区中的数据大小，减去已经被读取的数据大小
   
       spinlock_release(&p->lock);//释放管道的自旋锁，允许其他线程或进程继续对管道进行操作
       sem_signal(&p->wsem); // 释放可写信号
   
       return read_size;//返回实际读取的数据大小
   }
   ```

3. **写入数据（`pipe_write`）：**
   
   ```c
   ssize_t pipe_write(struct pipe *p, const char *buf, size_t count) {
       sem_wait(&p->wsem); // 等待可写信号
       spinlock_acquire(&p->lock);//获取管道的自旋锁
   
       size_t write_size = min(count, PIPE_BUF_SIZE - p->buffer->size);//计算实际需要写入的数据大小,取决于请求的数据大小 count 和管道缓冲区中当前可写的空间大小
       memcpy(p->buffer->data + p->buffer->size, buf, write_size);//将数据从源缓冲区 buf 复制到管道缓冲区的空白位置，以供读取
       p->buffer->size += write_size;//更新管道缓冲区中的数据大小，增加已经被写入的数据大小
   
       spinlock_release(&p->lock);
       sem_signal(&p->rsem); // 释放可读信号
   
       return write_size;
   }
   ```

同步互斥问题的处理：

- **自旋锁：** 使用 `struct spinlock` 来保护对管道缓冲区的读写操作，确保在多线程或多进程环境下的原子性。

- **信号量：** 使用 `struct semaphore` 来实现读端和写端的同步。`rsem` 信号量表示可读数据的数量，`wsem` 信号量表示可写空间的数量。通过信号量的等待和释放操作，确保在读写过程中的正确同步。

- **管道缓冲区大小限制：** 在写入数据时，需要检查管道缓冲区的剩余空间，避免数据溢出。同样，读取数据时需要检查缓冲区中是否有足够的数据可读。

### 扩展练习 Challenge2：完成基于“UNIX的软连接和硬连接机制”的设计方案

如果要在ucore里加入UNIX的软连接和硬连接机制，至少需要定义哪些数据结构和接口？（接口给出语义即可，不必具体实现。数据结构的设计应当给出一个(或多个）具体的C语言struct定义。在网络上查找相关的Linux资料和实现，请在实验报告中给出设计实现”UNIX的软连接和硬连接机制“的概要设方案，你的设计应当体现出对可能出现的同步互斥问题的处理。）

**解答**

**硬链接**：当创建一个文件或是目录的硬链接时就是在目录里面创建一个新的目录项，目录项的名字和原来被连接的对象名字不同，但是inode结点的值是一样的。硬链接的文件内容与创建时指向的源文件一模一样，把源文件删除，不会影响硬链接文件。

**软链接**：创建的新的目录项的名字和inode值和原来的对象都不一样。软链接其本身就是一个文件，它存放着另外一个路径的文件，如果把源文件删除，那么软链接就找不到源文件了，也就访问不了了。

数据结构：

1. **`inode` 结构体扩展：**
   
   ```c
   struct inode {
     // 其他字段...
     uint32_t i_flags;  // inode 标志，用于表示连接类型等
     union {
         uint32_t i_links_count;  // 硬连接的链接计数
         char *i_symlink_target;  // 软连接的目标路径
     };
     struct spinlock i_lock;  // 自旋锁，用于同步对 inode 结构体的访问
   };
   ```

2. **`dentry` 结构体扩展：**
   
   ```c
   struct dentry {
     // 其他字段...
     char *d_iname;  // 软连接的目标路径
     struct spinlock d_lock;  // 自旋锁，用于同步对 dentry 结构体的访问
   };
   ```

接口：

1. **创建硬连接：**
   
   ```c
   int hard_link(const char *oldpath, const char *newpath);
   ```
   
   - 检查 oldpath 是否有效，并获取其对应的 inode。
   - 在新路径 newpath 处创建一个目录项，共享相同的 inode。
   - 增加 inode 的链接计数。

2. **创建软连接：**
   
   ```c
   int symlink(const char *target, const char *linkpath);
   ```
   
   - 在 linkpath 处创建一个目录项，该目录项指向一个新的 inode。
   - 为新 inode 分配空间以存储软连接目标路径。
   - 将软连接目标路径写入新 inode。

3. **读取软连接目标路径：**
   
   ```c
   ssize_t readlink(const char *path, char *buf, size_t bufsiz);
   ```
   
   - 读取给定路径的 inode。
   - 如果 inode 是软连接，则读取其中的目标路径到缓冲区 buf。

4. **删除链接：**
   
   ```c
   int unlink(const char *pathname);
   ```
   
   - 检查路径是否有效，并获取其对应的 inode。
   - 如果是硬链接，则减少 inode 的链接计数。
   - 如果是软链接，则释放相关的 inode 和目录项。

5. **同步互斥处理：**
   
   - 使用锁保护对 `inode` 结构体和相关数据结构的访问，确保对链接计数和目标路径的操作是原子的。
   
   - 在增加或减少链接计数时使用自旋锁以防止竞态条件。
   
   - 在写入软连接目标路径时使用锁以确保正确的写入和防止多个进程同时修改。
     
     ```c
     // 初始化锁
     void lock_init(struct spinlock *lock) {
     // ... 初始化锁的相关操作
     }
     
     // 获取锁
     void lock_acquire(struct spinlock *lock) {
     // ... 获取锁的相关操作
     }
     
     // 释放锁
     void lock_release(struct spinlock *lock) {
     // ... 释放锁的相关操作
     }
     ```

设计思路：

1. `inode` 结构体中的 `i_flags` 可以用于标志连接类型，例如硬连接或软连接。
2. `dentry` 结构体中的 `d_iname` 字段存储软连接的目标路径。
3. 创建硬链接时，需要检查链接计数并增加它。
4. 创建软连接时，需要为新的 inode 分配空间以存储软连接目标路径，并写入目标路径。
5. 在删除链接时，需要减少链接计数，并在硬链接的计数为零时释放相关资源。
6. 为了处理同步互斥问题，需要使用锁来保护对相关数据结构的访问。增加和减少链接计数时使用自旋锁，以确保原子性。在写入软连接目标路径时使用锁，以确保正确的写入和防止多个进程同时修改。

## 重要知识点

### 文件系统：

某些数据存储在硬盘上从某个地址开始的多么长的位置，以及硬盘上哪些位置是没有被占用的。编程的时候，如果用到/修改硬盘上的数据，交给计算机来保存和自动维护。这就是文件系统

### 文件

 把一段逻辑上相关联的数据看作一个整体，叫做文件。

### 虚拟文件系统（virtual filesystem, VFS）

作为操作系统和更具体文件系统之间的接口。所谓“具体文件系统”，更接近具体设备和文件系统的内部实现，而“虚拟文件系统”更接近用户使用的接口。

### ucore文件系统

ucore里用虚拟文件系统管理三类设备：

- 硬盘，我们管理硬盘的具体文件系统是Simple File System
- 标准输出（控制台输出），只能写不能读
- 标准输入（键盘输入），只能读不能写、

lab8的Makefile和之前不同，分三段构建内核镜像。

- sfs.img: 一块符合SFS文件系统的硬盘，里面存储编译好的用户程序
- swap.img: 一段初始化为0的硬盘交换区
- kernel objects: ucore内核代码的目标文件

这三部分共同组成ucore.img, 加载到QEMU里运行。ucore代码中，通过链接时添加的首尾符号，把`swap.img`和`sfs.img`两段“硬盘”（实际上对应两段内存空间）找出来，然后作为“硬盘”进行管理。

ucore 的文件系统架构主要由四部分组成：

- 通用文件系统访问接口层：该层提供了一个从用户空间到文件系统的标准访问接口。这一层访问接口让应用程序能够通过一个简单的接口获得 ucore 内核的文件系统服务。
- 文件系统抽象层：向上提供一个一致的接口给内核其他部分（文件系统相关的系统调用实现模块和其他内核功能模块）访问。向下提供一个同样的抽象函数指针列表和数据结构屏蔽不同文件系统的实现细节。
- Simple FS 文件系统层：一个基于索引方式的简单文件系统实例。向上通过各种具体函数实现以对应文件系统抽象层提出的抽象函数。向下访问外设接口
- 外设接口层：向上提供 device 访问接口屏蔽不同硬件细节。向下实现访问各种具体设备驱动的接口，比如 disk 设备接口/串口设备接口/键盘设备接口等。

假如应用程序操作文件（打开/创建/删除/读写），首先需要通过文件系统的通用文件系统访问接口层给用户空间提供的访问接口进入文件系统内部，接着由文件系统抽象层把访问请求转发给某一具体文件系统（比如 SFS 文件系统），具体文件系统（Simple FS 文件系统层）把应用程序的访问请求转化为对磁盘上的 block 的处理请求，并通过外设接口层交给磁盘驱动例程来完成具体的磁盘操作。

### ucore文件系统总体结构

- 超级块（SuperBlock），它主要从文件系统的**全局角度**描述特定文件系统的全局信息。它的作用范围是整个 OS 空间。
- 索引节点（inode）：它主要从文件系统的**单个文件**的角度它描述了文件的各种属性和数据所在位置。它的作用范围是整个 OS 空间。
- 目录项（dentry）：它主要从文件系统的**文件路径**的角度描述了文件路径中的一个特定的目录项（注：一系列目录项形成目录/文件路径）。它的作用范围是整个 OS 空间。对于 SFS 而言，inode(具体为 struct sfs_disk_inode)对应于物理磁盘上的具体对象，dentry（具体为 struct sfs_disk_entry）是一个内存实体，其中的 ino 成员指向对应的 inode number，另外一个成员是 file name(文件名).
- 文件（file），它主要从进程的角度描述了一个进程在访问文件时需要了解的文件标识，文件读写的位置，文件引用情况等信息。它的作用范围是某一具体进程。

### 文件系统抽象层VFS

#### file & dir 接口

file&dir 接口层定义了进程在内核中直接访问的文件相关信息，这定义在 file 数据结构中。

#### inode 接口

index node 是位于内存的索引节点，它是 VFS 结构中的重要数据结构，因为它实际负责把不同文件系统的特定索引节点信息（甚至不能算是一个索引节点）统一封装起来，避免了进程直接访问具体文件系统。

#### inode_ops

 是对常规文件、目录、设备文件所有操作的一个抽象函数表示。对于某一具体的文件系统中的文件或目录，只需实现相关的函数，就可以被用户进程访问具体的文件了，且用户进程无需了解具体文件系统的实现细节。

### sfs

| superblock | root-dir inode | freemap | inode、File Data、Dir Data Blocks |
| ---------- | -------------- | ------- | ------------------------------- |
| 超级块        | 根目录索引节点        | 空闲块映射   | 目录和文件的数据和索引节点                   |

第 0 个块（4K）是超级块（superblock），它包含了关于文件系统的所有关键参数，当计算机被启动或文件系统被首次接触时，超级块的内容就会被装入内存。其定义如下：

```c
struct sfs_super {
    uint32_t magic;                                 /* magic number, should be SFS_MAGIC */
    uint32_t blocks;                                /* # of blocks in fs */
    uint32_t unused_blocks;                         /* # of unused blocks in fs */
    char info[SFS_MAX_INFO_LEN + 1];                /* infomation for sfs  */
};
```

魔数 magic，内核通过它来检查磁盘镜像是否是合法的 SFS img；

blocks 记录了 SFS 中所有 block 的数量，即 img 的大小；

unused_block 记录了 SFS 中还没有被使用的 block 的数量；

info 包含了字符串"simple file system"。

第 1 个块放了一个 root-dir 的 inode，用来记录根目录的相关信息。 root-dir 是 SFS 文件系统的根结点，通过这个 root-dir 的 inode 信息就可以定位并查找到根目录下所有文件信息。

从第 2 个块开始，根据 SFS 中所有块的数量，用 1 个 bit 来表示一个块的占用和未被占用的情况。这个区域称为 SFS 的 freemap 区域，这将占用若干个块空间。为了更好地记录和管理 freemap 区域，专门提供了两个文件 kern/fs/sfs/bitmap.[ch]来完成根据一个块号查找或设置对应的 bit 位的值。

最后在剩余的磁盘空间中，存放了所有其他目录和文件的 inode 信息和内容数据信息。需要注意的是虽然 inode 的大小小于一个块的大小（4096B），但为了实现简单，每个 inode 都占用一个完整的 block。

在 sfs_fs.c 文件中的 sfs_do_mount 函数中，完成了加载位于硬盘上的 SFS 文件系统的超级块 superblock 和 freemap 的工作。这样，在内存中就有了 SFS 文件系统的全局信息。

#### 索引节点

在 SFS 文件系统中，需要记录文件内容的存储位置以及文件名与文件内容的对应关系。sfs_disk_inode 记录了文件或目录的内容存储的索引信息，该数据结构在硬盘里储存，需要时读入内存（从磁盘读进来的是一段连续的字节，我们将这段连续的字节强制转换成sfs_disk_inode结构体；同样，写入的时候换一个方向强制转换）。sfs_disk_entry 表示一个目录中的一个文件或目录，包含该项所对应 inode 的位置和文件名，同样也在硬盘里储存，需要时读入内存。

操作系统中，每个文件系统下的 inode 都应该分配唯一的 inode 编号。SFS 下，为了实现的简便，每个 inode 直接用他所在的磁盘 block 的编号作为 inode 编号。比如，root block 的 inode 编号为 1；每个 sfs_disk_entry 数据结构中，name 表示目录下文件或文件夹的名称，ino 表示磁盘 block 编号，通过读取该 block 的数据，能够得到相应的文件或文件夹的 inode。ino 为 0 时，表示一个无效的 entry。

可以看到 SFS 中的内存 inode 包含了 SFS 的硬盘 inode 信息，而且还增加了其他一些信息，这属于是便于进行是判断否改写、互斥操作、回收和快速地定位等作用。需要注意，一个内存 inode 是在打开一个文件后才创建的，如果关机则相关信息都会消失。而硬盘 inode 的内容是保存在硬盘中的，只是在进程需要时才被读入到内存中，用于访问文件或目录的具体内容数据。

### 设备即文件

在本实验中，为了统一地访问设备(device)，我们可以把一个设备看成一个文件，通过访问文件的接口来访问设备。目前实现了 stdin 设备文件文件、stdout 设备文件、disk0 设备。stdin 设备就是键盘，stdout 设备就是控制台终端的文本显示，而 disk0 设备是承载 SFS 文件系统的磁盘设备。

一个具体设备，只要实现了`d_open()`打开设备， `d_close()`关闭设备，`d_io()`(读写该设备，write参数是true/false决定是读还是写)，`d_ioctl()`(input/output control)四个函数接口，就可以被文件系统使用了。

但这个设备描述没有与文件系统以及表示一个文件的 inode 数据结构建立关系，为此，还需要另外一个数据结构把 device 和 inode 联通起来，这就是 vfs_dev_t 数据结构。利用 vfs_dev_t 数据结构，就可以让文件系统通过一个链接 vfs_dev_t 结构的双向链表找到 device 对应的 inode 数据结构，一个 inode 节点的成员变量 in_type 的值是 0x1234，则此 inode 的成员变量 in_info 将成为一个 device 结构。这样 inode 就和一个设备建立了联系，这个 inode 就是一个设备文件。ucore 虚拟文件系统为了把这些设备链接在一起，还定义了一个设备链表，即双向链表 vdev_list，这样通过访问此链表，可以找到 ucore 能够访问的所有设备文件。

### open系统调用的执行过程

#### 通用文件访问接口层的处理流程

首先，经过syscall.c的处理之后，进入**内核态**，执行sysfile_open()函数，把位于用户空间的字符串__path拷贝到内核空间中的字符串path中，然后调用了file_open， file_open调用了vfs_open, 使用了VFS的接口,进入到文件系统抽象层的处理流程完成进一步的打开文件操作中。

#### 文件系统抽象层的处理流程

1、分配一个空闲的file数据结构变量file在文件系统抽象层的处理中，首先调用的是file_open函数，它要给这个即将打开的文件分配一个file数据结构的变量，这个变量其实是当前进程的打开文件数组current->fs_struct->filemap[]中的一个空闲元素，而这个元素的索引值就是最终要返回到用户进程并赋值给变量fd。到了这一步还仅仅是给当前用户进程分配了一个file数据结构的变量，还没有找到对应的文件索引节点。

2、为此需要进一步调用vfs_open函数来找到path指出的文件所对应的基于inode数据结构的VFS索引节点node。vfs_open函数需要完成两件事情：通过vfs_lookup找到path对应文件的inode；调用vop_open函数打开文件。

3、vfs_lookup函数是一个针对目录的操作函数，它会调用vop_lookup函数来找到SFS文件系统中的目录下的文件。为此，vfs_lookup函数首先调用get_device函数，并进一步调用vfs_get_bootfs函数来找到根目录“/”对应的inode。这个inode就是位于vfs.c中的inode变量bootfs_node。这个变量在init_main函数执行时获得了赋值。通过调用vop_lookup函数来查找到根目录“/”下对应文件sfs_filetest1的索引节点，，如果找到就返回此索引节点。

#### SFS文件系统层的处理流程

这里需要分析文件系统抽象层中没有彻底分析的vop_lookup函数。sfs_lookup有三个参数：node，path，node_store。其中node是根目录“/”所对应的inode节点；path是文件sfs_filetest1的绝对路径/sfs_filetest1，而node_store是经过查找获得的sfs_filetest1所对应的inode节点。

sfs_lookup函数以“/”为分割符，从左至右逐一分解path获得各个子目录和最终文件对应的inode节点。在本例中是调用sfs_lookup_once查找以根目录下的文件sfs_filetest1所对应的inode节点。当无法分解path后，就意味着找到了sfs_filetest1对应的inode节点，就可顺利返回了。sfs_lookup_once将调用sfs_dirent_search_nolock函数来查找与路径名匹配的目录项，如果找到目录项，则根据目录项中记录的inode所处的数据块索引值找到路径名对应的SFS磁盘inode，并读入SFS磁盘inode对的内容，创建SFS内存inode。

### Read系统调用过程

#### 通用文件访问接口层的处理流程

先进入通用文件访问接口层的处理流程，即进一步调用如下用户态函数：read->sys_read->syscall，从而引起系统调用**进入到内核态**。

到了内核态以后，通过中断处理例程，会调用到sys_read内核函数，并进一步调用sysfile_read内核函数，**进入到文件系统抽象层**处理流程完成进一步读文件的操作。

#### 文件系统抽象层的处理流程

1) 检查错误，即检查读取长度是否为0和文件是否可读。

2) 分配buffer空间，即调用kmalloc函数分配4096字节的buffer空间。

3) 读文件过程

[1] 实际读文件

循环读取文件，每次读取buffer大小。每次循环中，先检查剩余部分大小，若其小于4096字节，则只读取剩余部分的大小。然后调用file_read函数将文件内容读取到buffer中，alen为实际大小。调用copy_to_user函数将读到的内容拷贝到用户的内存空间中，调整各变量以进行下一次循环读取，直至指定长度读取完成。最后函数调用层层返回至用户程序，用户程序收到了读到的文件内容。

[2] file_read函数

函数有4个参数，fd是文件描述符，base是缓存的基地址，len是要读取的长度，copied_store存放实际读取的长度。函数首先调用fd2file函数找到对应的file结构，并检查是否可读。调用filemap_acquire函数使打开这个文件的计数加1。**调用vop_read**函数将文件内容读到iob中。调整文件指针偏移量pos的值，使其向后移动实际读到的字节数iobuf_used(iob)。最后调用filemap_release函数使打开这个文件的计数减1，若打开计数为0，则释放file。

#### SFS文件系统层的处理流程

vop_read函数实际上是对sfs_read的包装。sfs_read函数调用sfs_io函数。它有三个参数，node是对应文件的inode，iob是缓存，write表示是读还是写的布尔值（0表示读，1表示写），这里是0。函数先找到inode对应sfs和sin，然后调用**sfs_io_nolock函数进行读取文件**操作，最后**调用iobuf_skip函数调整iobuf的指针**。

##### sfs_io_nolock函数

1. 先计算一些辅助变量，并处理一些特殊情况（比如越界），然后有sfs_buf_op = sfs_rbuf,sfs_block_op = sfs_rblock，设置读取的函数操作。
2. 接着进行实际操作，先处理起始的没有对齐到块的部分，再以块为单位循环处理中间的部分，最后处理末尾剩余的部分。
3. 每部分中都调用sfs_bmap_load_nolock函数得到blkno对应的inode编号，并调用sfs_rbuf或sfs_rblock函数读取数据（中间部分调用sfs_rblock，起始和末尾部分调用sfs_rbuf），调整相关变量。
4. 完成后如果offset + alen > din->fileinfo.size（写文件时会出现这种情况，读文件时不会出现这种情况，alen为实际读写的长度），则调整文件大小为offset + alen并设置dirty变量。
    
