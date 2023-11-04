# lab3缺页异常和页面置换

## 实验目的

了解虚拟内存的Page Fault异常处理实现

了解页替换算法在操作系统中的实现

学会如何使用多级页表，处理缺页异常（Page Fault），实现页面置换算法。

## 实验内容

借助于页表机制和中断异常处理机制，完成Page Fault异常处理和部分页面替换算法的实现，结合磁盘提供的缓存空间，从而能够支持虚存管理，提供一个比实际物理内存空间“更大”的虚拟内存空间给系统使用。

### 练习1: 理解基于FIFO的页面替换算法（思考题）

描述FIFO页面置换算法下，一个页面从被换入到被换出的过程中，会经过代码里哪些函数/宏的处理（或者说，需要调用哪些函数/宏），并用简单的一两句话描述每个函数在过程中做了什么？

1.pgfault_handler:当程序触发页异常的时候，会进入对应的处理程序 pgfault_handler函数，在此函数中会调用 print_pgfault打印一些错误信息，以及将这些错误信息给 do_pgfault函数处理。

2.do_pgfault:是处理函数的主体，在此函数中进行页面的换入换出等操作。

```c
int do_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr)
```

在 do_pgfault函数调用过程中首先会调用 find_vma函数。

3.find_vma: 函数会在 vma结构体链表中找到一个满足 vma->vm_start<=addr && addr < vma->vm_end条件的vma结构体。检测地址是否合法。在调用 find_vma检测地址合法后会调用 get_pte函数。

```c
find_vma(struct mm_struct *mm, uintptr_t addr) {
    struct vma_struct *vma = NULL;
    if (mm != NULL) {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) {
                bool found = 0;
                list_entry_t *list = &(mm->mmap_list), *le = list;
                while ((le = list_next(le)) != list) {
                    vma = le2vma(le, list_link);
                    if (vma->vm_start<=addr && addr < vma->vm_end) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    vma = NULL;
                }
        }
        if (vma != NULL) {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}
```

4.get_pte:

```c
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create) {
pde_t *pdep1 = &pgdir[PDX1(la)]; // 获取二级页表目录
if (!(*pdep1 & PTE_V)) { // 检查是否合法
  struct Page *page;
  if (!create || (page = alloc_page()) == NULL) {
      return NULL;
  }
  set_page_ref(page, 1);
  uintptr_t pa = page2pa(page);
  memset(KADDR(pa), 0, PGSIZE);
  *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
}
pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)]; // 找到一级页表目录项
if (!(*pdep0 & PTE_V)) {
  struct Page *page;
  if (!create || (page = alloc_page()) == NULL) {
      return NULL;
  }
  set_page_ref(page, 1);
  uintptr_t pa = page2pa(page);
  memset(KADDR(pa), 0, PGSIZE);
  *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
}
return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)]; // 返回一级页表页表项
}
```

get_pte 函数会根据得到的虚拟地址，在三级页表中进行查找。在查找页表项的时候，如果页表项无效的话会给页表项分配一个全是0的页并建立映射。最后返回虚拟地址对应的一级页表的页表项。获取了 `pte` 以后，会检测此页表项是否有对应的页面。如果页表项全零，这个时候就会调用 `pgdir_alloc_page` 。

5.alloc_pages:pgdir_alloc_page首先会调用 alloc_page函数。alloc_page函数则是调了 alloc_pages(1) ,此函数原型如下所示：

```c
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    while (1) {
        local_intr_save(intr_flag);
        { page = pmm_manager->alloc_pages(n); }
        local_intr_restore(intr_flag);
        if (page != NULL || n > 1 || swap_init_ok == 0) break;
        extern struct mm_struct *check_mm_struct;
        swap_out(check_mm_struct, n, 0);
    }
    return page;
}
```

在 `alloc_pages` 函数中，首先是根据物理页面分配算法给自身发呢配一个物理页面，然后会调用 `swap_out` 函数。

6.swap_out:函数则是会根据页面置换算法选择出一个应该换出的页面并写入到磁盘中，并将此页面释放。swap_out函数找到应该换出的页面则是通过 swap_out_victim实现的。

7.swap_out_victim:

```c
static int _fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
  list_entry_t *head=(list_entry_t*) mm->sm_priv;
      assert(head != NULL);
  assert(in_tick==0);
 list_entry_t* entry = list_prev(head);
 if (entry != head) {
     list_del(entry);
     *ptr_page = le2page(entry, pra_page_link);
 } else {
     *ptr_page = NULL;
 }
 return 0;
}
```

这个函数是基于FIFO的页面替换算法的核心。根据此算法的思想，在页面置换中，我们需要换出的是最先使用的页面，也就是最先加入到链表的节点对应的页面。在链表中，最先加入页面对应的节点就是头节点 `head` 的上一个，调用 `list_prev` 即可。

将页面内容写入磁盘则是通过磁盘的写函数实现的。在 `kern/fs/swapfs.c` 中封装了磁盘的读入写出函数。

```c
int // 此函数封装了磁盘的读操作。
swapfs_read(swap_entry_t entry, struct Page *page) {
 return ide_read_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}
```

```c
int // 此函数封装了磁盘的读操作。
swapfs_write(swap_entry_t entry, struct Page *page) {
 return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}
```

在 pgdir_alloc_page调用alloc_page获得分配的页面后会调用 page_insert函数。

8.page_insert:函数则是将虚拟地址和页面之间建立映射关系。在此函数中首先会用 get_pte获取页表项。然后会判断页表项对应的页面和要建立映射的页面是否相同。不同的话会调用 page_remove_pte函数将此页表项失效。接着会调用 pte_create 函数建立新的页表项比那个将其赋值给 get_pte找到的页表项的地址。

9.page_remove_pte:执行时会找到 pte对应的页面，减少其引用，并将页面释放。

```c
static inline pte_t pte_create(uintptr_t ppn, int type) {
 return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
}
```

10.pte_create:直接根据物理页号进行偏移并对标志位进行设置完成。

然后 pgdir_alloc_page会调用 swap_map_swappable`函数。

11.swap_map_swappable:将页面加入相应的链表，设置页面可交换。如果 do_pgfault 函数获取 addr函数对应的 pte不为空的话，则首先会调用 swap_in函数。

12：swap_in：作用是分配一个页面并从磁盘中将相应的值写入到此页面上。然后会调用 page_insert函数进行页面的映射以及调用 swap_map_swappable则是将页面加入相应的链表，设置页面可交换。

### 练习2:深入理解不同分页模式的工作原理（思考题）

get_pte()函数（位于`kern/mm/pmm.c`）用于在页表中查找或创建页表项，从而实现对指定线性地址对应的物理页的访问和映射操作。这在操作系统中的分页机制下，是实现虚拟内存与物理内存之间映射关系非常重要的内容。

- get_pte()函数中有两段形式类似的代码， 结合sv32，sv39，sv48的异同，解释这两段代码为什么如此相像。
- 目前get_pte()函数将页表项的查找和页表项的分配合并在一个函数里，你认为这种写法好吗？有没有必要把两个功能拆开？

get_pte()函数的逻辑是类似于一个递归向下寻找页表项的过程，根据PDX1和三级页表PDE找到二级页表PDE，再结合PDX0，找到一级页表PDE，根据PTX找到PTE。

- **如果页表项有效**：说明已经存在一个合适的页表，函数返回该页表项的地址。
- **如果页表项无效**：
  - 如果当前处理的是最底层的页表级别，表示已经到达页表的叶子级别，需要创建一个新的页表项，并将其添加到当前页表中。
  - 如果当前处理的不是最底层的页表级别，表示需要继续往下一级的页表查找或创建。函数会继续向下寻找，在下一级的页表上执行相同的操作

1.get_pte 函数中的两段类似代码都用于获取页表项（PTE）的指针，一个是针对32位的sv32模式，另一个是针对64位的sv39和sv48模式。它们相似的原因是因为它们都执行相似的操作，只是在不同的页表层次结构和寻址方式下稍有不同。

在RISC-V中，不同的地址位数模式（sv32、sv39、sv48）在页表层次结构上有所不同，但它们都采用相同的思想，就是将线性地址映射到物理地址。因此，获取页表项的逻辑相似。

**相似性**：

1. 计算页表项的索引：不论是sv32、sv39还是sv48，都需要计算线性地址的偏移，并根据页表的结构来确定哪个部分用于页表项的索引。
2. 检查页表是否存在：无论哪种模式，都要检查父级页表项中的标志位，通常是“存在”（PTE_P）位来判断子级页表是否存在。

**不同之处**：

1. 地址位数：sv32使用32位线性地址，因此在计算索引时，需要不同的位偏移。而sv39和sv48使用39位和48位线性地址，偏移量不同。
2. 页表层次：sv32有两级页表，sv39有三级页表，sv48有四级页表。这意味着在获取页表项时，需要访问不同级别的页表。

2.`get_pte` 函数将页表项的查找和页表项的分配合并在一个函数,这种写法有一些优点，但也存在一些潜在的问题。

**优点**：

1. 简化代码结构：合并查找和分配页表项的操作可以减少代码的复杂性，使代码更易于理解和维护。
2. 减少函数调用：将两个操作合并到一个函数中可以减少函数调用的开销，提高执行效率。在操作系统内核中，性能通常是一个关键问题。
3. 一致性：合并操作可以保持代码的一致性。通过一个函数来处理所有页表项的相关操作，可以减少错误和不一致性的可能性。

**潜在问题**：

1. 功能过于复杂：如果一个函数尝试做太多事情，它可能变得非常复杂，难以维护和测试。如果一个函数既负责查找页表项又负责分配新页表项，它可能会变得非常庞大。
2. 可扩展性：将查找和分配页表项的操作分开可以更容易地扩展功能。例如，如果以后需要在查找操作中添加更多的逻辑，可以更容易地修改和扩展单独的查找函数。

是否将这两个功能拆分开来通常取决于代码的复杂性和维护需求。在某些情况下，将它们合并在一起是合理的，但在其他情况下，拆分它们可能更有利于代码的可维护性和可扩展性。维护者通常需要综合考虑代码的具体情况，以确定哪种方式更适合。如果代码的逻辑变得非常复杂，拆分功能通常是一个不错的选择，以保持代码的清晰度和可维护性。

### 练习3: 给未被映射的地址映射上物理页（需要编程）

补充完成do_pgfault（mm/vmm.c）函数，给未被映射的地址映射上物理页。设置访问权限 的时候需要参考页面所在 VMA 的权限，同时需要注意映射物理页时需要操作内存控制 结构所指定的页表，而不是内核的页表。

- 描述页目录项（Page Directory Entry）和页表项（Page Table Entry）中组成部分对ucore实现页替换算法的潜在用处。

- - 数据结构Page的全局变量的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是什么？

PTE_A和PTE_D分别代表了内存页是否被访问过和内存也是否被修改过，借助这两个标志位，可以实现进阶时钟页替换算法。当出现页访问异常时，硬件首先会将错误的相关信息保存在相应寄存器中，并且将执行流程转交给中断处理程序。page数组中的每一项都对应了一个页面，page的vaddr属性存储了页面虚拟地址，通过虚拟地址可以获得页目录项和页表项。

### 练习4:补充完成Clock页替换算法（需要编程）

在给出的框架上，填写代码，实现 Clock页替换算法（mm/swap_clock.c）。

- 设计实现过程

- 比较Clock页替换算法和FIFO算法的不同。 

clock页替换算法实现过程

1.将clock页替换算法所需数据结构进行初始化。

```c
_clock_init_mm(struct mm_struct *mm)
{
    list_init(&pra_list_head);
    curr_ptr = &pra_list_head;
    mm->sm_priv = &pra_list_head;
    return 0;
}
```

2.将页面`page`插入到页面链表`pra_list_head`的末尾并将页面的visited标志置为1，表示该页面已被访问。

```c
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry = &(page->pra_page_link);
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(entry != NULL && curr_ptr != NULL);
    list_add(head, entry);
    page->visited = 1;
    curr_ptr = entry;
    cprintf("curr_ptr %p\n", curr_ptr);
    return 0;
}
```

3.通过`tmp`指针遍历页面链表，找到一个visited为0即未被访问的页面。若找到了此页面，将该页面从链表中删除，并将其地址存储在ptr_page作为换出页面。若当前页面已被访问，则将visited标志置为0，表示该页面已被重新访问。

```c
_clock_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    list_entry_t *tmp = head;
    while (1)
    {
        list_entry_t *entry = list_prev(tmp);
        struct Page *p = le2page(entry, pra_page_link);
        if (p->visited == 0)
        {
            list_del(entry);
            *ptr_page = p;
            cprintf("curr_ptr %p\n", curr_ptr);
            break;
        }
        else
            p->visited = 0;
        tmp = entry;
    }
    return 0;
}
```

比较Clock页替换算法和FIFO算法的不同

- **FIFO页替换算法：** 根据页面进入内存的先后顺序进行替换，即最早进入内存的页面先被替换。使用队列数据结构来维护页面的进入顺序，不考虑页面的访问频率，且存在Belady现象。
- **clock页替换算法：** 时钟页替换算法把各个页面组织成环形链表的形式，每个页面都有一个标识位，当页面被访问时，标识位被设置为1，当需要替换页面时，从当前位置开始查找标识位为0的页面进行替换。其在使用过程中需要维护一个循环链表，并且需要更新页面的标识位，实现起来相对复杂。但其考虑页面的访问频率，且不存在Belady现象，性能更加优秀。

### 练习5: 阅读代码和实现手册，理解页表映射方式相关知识（思考题）

如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

**优势和好处：**

减少页表项数量： "一个大页"的方式将多个小页面映射到一个较大的页面中，从而减少了需要管理的页表项数量。这降低了内存开销和访问页表的时间开销，因为每个页表项需要一定的内存和处理器时间来维护。

提高性能：减少页表项的数量通常会提高访问内存的性能。CPU在访问页表时需要查找页表项，较少的页表项意味着更少的查找操作，从而减少了访存延迟。

减少TLB的负担：较大的页面可以更好地填充TLB，这意味着更多的内存访问可以直接从TLB中获取翻译，而不必查找页表。这提高了缓存命中率，减少了内存访问的时间延迟。

**坏处和风险：**

内部碎片：使用较大的页面可能导致内部碎片，因为如果一个页面没有被完全填充，将会浪费一些内存。这可能导致内存的浪费，特别是在存储大量小文件或数据时。

不适合所有情况："一个大页"方式适合某些工作负载，但不适合所有情况。例如，对于大多数操作系统代码和小型应用程序，小页面通常更为适用，因为它们更加灵活，可以更好地利用内存。

映射冲突：使用较大的页面时，如果多个进程需要映射相同的页面，可能会导致映射冲突。这种情况需要更复杂的页共享机制来解决。

难以管理： 较大的页面可能更难管理，尤其是在操作系统需要频繁地进行页面置换或内存分配时。细粒度的页表可以提供更好的控制和管理。

## 重要知识点

### 虚拟内存管理

#### 基本原理：

**虚拟内存**：是指程序员或CPU看到的内存。虚拟内存单元不一定有实际的物理内存单元对应，如果虚拟内存单元对应有实际的物理内存单元，那二者的地址一般不相等。通过操作系统实现的某种内存映射可建立虚拟内存与物理内存的对应关系，使得程序员或CPU访问的虚拟内存地址会自动转换为一个物理内存地址。

**按需分页技术**：软件在没有访问某虚拟内存地址时不分配具体的物理内存，只有在实际访问某虚拟内存地址时，操作系统再动态地分配物理内存，建立虚拟内存到物理内存的页映射关系，这种技术称为按需分页。

**页换入换出技术**：把不经常访问的数据所占的内存空间临时写到硬盘上，腾出更多的空闲内存空间给经常访问的数据；当CPU访问到不经常访问的数据时，再把这些数据从硬盘读入到内存中，这种技术称为页换入换出。

**内存地址虚拟化的意义**：

有了内存地址虚拟化，就可以通过设置页表项来限定软件运行时的访问空间，确保软件运行不越界，完成内存访问保护的功能。

内存地址虚拟化是按需分页技术和页换入换出技术的基础，这种内存管理技术给了程序员更大的内存“空间”，从而可以让更多的程序在内存中并发运行。

#### 实验执行流程：

<img src="file:///C:/Users/鑫鑫/AppData/Roaming/marktext/images/2023-10-24-18-52-37-image.png" title="" alt="" width="383">

#### 关键数据结构和函数：

实际上在QEMU里并没有真正模拟“硬盘”。为了实现“页面置换”的效果，采取的措施是，从内核的静态存储(static)区里面分出一块内存， 声称这块存储区域是”硬盘“，然后包裹一下给出”硬盘IO“的接口。这里所谓的“硬盘IO”，只是在内存里用`memcpy`把数据复制来复制去。同时为了逼真地模仿磁盘，只允许以磁盘扇区为数据传输的基本单位，也就是一次传输的数据必须是512字节的倍数，并且必须对齐。

ide在这里是Integrated Drive Electronics的意思，表示的是一种标准的硬盘接口。

fs全称为file system，这个模块称作`fs`只是说明它是“硬盘”和内核之间的接口。

#### 处理缺页异常：

缺页异常是指CPU访问的虚拟地址时， MMU没有办法找到对应的物理地址映射关系，或者与该物理页的访问权不一致而发生的异常。

CPU通过地址总线可以访问连接在地址总线上的所有外设，包括物理内存、IO设备等等，但从CPU发出的访问地址并非是这些外设在地址总线上的物理地址，而是一个虚拟地址，由MMU将虚拟地址转换成物理地址再从地址总线上发出，MMU上的这种虚拟地址和物理地址的转换关系是需要创建的，并且还需要设置这个物理页的访问权限。

当我们引入了虚拟内存，就意味着虚拟内存的空间可以远远大于物理内存，也意味着程序可以访问"不对应物理内存页帧的虚拟内存地址"，这时CPU应当抛出`Page Fault`这个异常。

### 页面置换思路：

由于操作系统给用户态的应用程序提供了一个虚拟的“大容量”内存空间，而实际的物理内存空间没有那么大。所以操作系统会只把应用程序中“常用”的数据和代码放在物理内存中，而不常用的数据和代码放在了硬盘这样的存储介质上。如果应用程序访问的是“常用”的数据和代码，那么操作系统已经放置在内存中了，不会出现什么问题。但当应用程序访问它认为应该在内存中的的数据或代码时，如果这些数据或代码不在内存中，会产生页访问异常。这时，操作系统必须能够尽快把应用程序当前需要的数据或代码放到内存中来，然后重新执行应用程序产生异常的访存指令。如果在把硬盘中对应的数据或代码调入内存前，操作系统发现物理内存已经没有空闲空间了，这时操作系统必须把它认为“不常用”的页换出到磁盘上去，以腾出内存空闲空间给应用程序所需的数据或代码。一个好的页替换算法会导致页访问异常次数少，也就意味着访问硬盘的次数也少，从而使得应用程序执行的效率就高。

- 先进先出(First In First Out, FIFO)页替换算法：总是淘汰最先进入内存的页，只需把一个应用程序在执行过程中已调入内存的页按先后次序链接成一个队列，队列头指向内存中驻留时间最久的页，队列尾指向最近被调入内存的页。FIFO 算法只是在应用程序按线性顺序访问地址空间时效果才好，否则效率不高。FIFO 算法的另一个缺点是，它有Belady 现象，即在增加放置页的物理页帧的情况下，反而使页访问异常次数增多。
- 最久未使用(least recently used, LRU)算法：利用局部性，通过过去的访问情况预测未来的访问情况，认为最近被访问过的页面将来被访问的可能性大，而很久没访问过的页面将来不太可能被访问。于是我们比较当前内存里的页面最近一次被访问的时间，把上一次访问时间离现在最久的页面置换出去。
- 时钟（Clock）页替换算法：是 LRU 算法的一种近似实现。时钟页替换算法把各个页面组织成环形链表的形式，类似于一个钟的表面。然后把一个指针指向最老的那个页面，即最先进来的那个页面。另外，时钟算法需要在页表项（PTE）中设置了一位访问位来表示此页表项对应的页当前是否被访问过。当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当需要淘汰页时，对当前指针指向的页所对应的页表项进行查询，如果访问位为“0”，则淘汰该页，如果该页被写过，则还要把它换出到硬盘上；如果访问位为“1”，则将该页表项的此位置“0”，继续访问下一个页。
- 改进的时钟（Enhanced Clock）页替换算法：改进的时钟置换算法除了考虑页面的访问情况，还需考虑页面的修改情况。这需要为每一页的对应页表项内容中增加一位引用位和一位修改位。当该页被访问时，CPU 中的 MMU 硬件将把访问位置“1”。当该页被“写”时，CPU 中的 MMU 硬件将把修改位置“1”。这样这两位就存在四种可能的组合情况：（0，0）表示最近未被引用也未被修改，首先选择此页淘汰；（0，1）最近未被使用，但被修改，其次选择；（1，0）最近使用而未修改，再次选择；（1，1）最近使用且修改，最后选择。该算法与时钟算法相比，可进一步减少磁盘的 I/O 操作次数，但为了查找到一个尽可能适合淘汰的页面，可能需要经过多次扫描，增加了算法本身的执行开销。
