// ------------------------------------------------Buddy System设计文档------------------------------------------------
/*
一些基础函数：
- IS_POWER_OF_2：判断是不是2的幂
- Get_Order_Of_2：返回一个数对应的幂指数
- Find_The_Small_2：找到该数对应的最接近的2的幂，比该数小
- Find_The_Big_2：找到该数对应的最接近的2的幂，比该数大
- show_buddy_array：显示指定范围内的空闲链表数组的内容，包括页数量和地址信息
- buddy_system_init：初始化buddy system
- buddy_system_init_memmap：初始化链表内容
- get_buddy：获取给定内存块地址的伙伴块地址
- buddy_system_nr_free_pages：输出当前的空闲页的数量

分块设计：设计了buddy_system_split函数用于处理空闲页表的分块操作。
首先我们输入一个对应的n值，也就是数组的下标位置。首先进行一些合法性判断，然后定义了两个Page指针，用于定义分裂的两个小块。
然后获取到我们对应的数组位置的链表的第一个块，也就是需要被分割的块。然后我们将第一个分裂后的块地址不变，第二个块的地址紧
跟在第一块的后面，也就是加上2的n-1次的地址。然后完成两个块的数组位置的下移，以及更新对应的属性，最后完成原始链表的删除和
新链表的链接即可。

分配设计：传入一个参数为需要分配的页数大小。如果小于0或者大于上限，直接返回null或者中断程序。然后，我们根据请求的页数大小，
找到比他大的第一个2的幂次数，也就是我们需要给他分配的页数大小。然后从其对应的数组位置开始遍历链表，找到第一个不为空的链表，
即可进行块的分配。我们在此做了一个判断，如果刚好不需要拆分，也就是下标刚好对上，那就不用调用split函数；如果对应下标的链表
为空的话，我们就往后寻找不为空的链表，然后使用循环进行递归拆分，直到链表不为空，停止。最后，我们更新当前空闲页面的数量，返
回对应的分配页面即可。

释放设计：首先我们计算出需要释放的页面的大小，也就是比他大的第一个2的幂指数。首先将left_block初始化为传入的要释放的块的起
始地址，将当前块先插入我们对应的数组链表中。然后通过同等大小的页表块是否相邻来进行合并的操作。在合并的同时，我们需要同步更
新页表对应的大小和位置，具体操作为：将合并后的地址进行交换，保持大小一致的顺序；删除原先两个小块在链表中的链接，更新其数组
位置，然后链接在大块位置的数组上。最后，更新对应的空闲页表的数量。

*/

// --------------------------------------------------------------------------------------------------------------------

//begin to code

#include <pmm.h>
#include <list.h>
#include <string.h>
#include <stdio.h>
#include <buddy_system_pmm.h>

buddy_system_t buddy_system;
#define buddy_array (buddy_system.free_array)
#define max_order (buddy_system.max_order)
#define nr_free (buddy_system.nr_free)


static int IS_POWER_OF_2(size_t n){//判断是不是2的幂

    if(n&(n-1)){
        return 0;
    }
    else{
        return 1;
    }

}

static unsigned int Get_Order_Of_2(size_t n){//返回一个数对应的幂指数
    unsigned int count = 0;
    while (n >> 1)
    {
        n >>= 1;
        count++;
    }
    return count;
}

static size_t Find_The_Small_2(size_t n){//找到该数对应的最接近的2的幂，比该数小
    size_t temp=1;
    
    if(IS_POWER_OF_2(n)){
        return n;
    }
    else{
        while(n!=0){
            if(n%2==1){
                n=(n-1)/2;
            }
            else{
                n=n/2;
            }
            temp=temp*2;
        }
        temp=temp/2;
        return temp;
    }
}

static size_t Find_The_Big_2(size_t n){//找到该数对应的最接近的2的幂，比该数大
    size_t temp=1;
    if(IS_POWER_OF_2(n)){
        return n;
    }
    else{
        while(n!=0){
            if(n%2==1){
                n=(n-1)/2;
            }
            else{
                n=n/2;
            }
            temp=temp*2;
        }
        return temp; 
    }
}


static void buddy_system_split(size_t n){//分块操作

    assert(n>0&&n<=max_order);//判断n在不在数组的范围内
    assert(!list_empty(&(buddy_array[n])));//判断该列表是不是空的，只有不是空的情况下才能分裂

    struct Page *page1;//定义两个指向Page结构体的指针，用于分别表示即将分裂出的两个小块
    struct Page *page2;

    list_entry_t* le=list_next(&(buddy_array[n]));//获取对应块阶数为n的链表中的第一个元素的指针，list_next表示跳过链表头
    page1=le2page(le,page_link);//将链表元素指针转换为Page结构体指针
    size_t temp = 1;
    for(int i=0;i<n-1;i++){//计算分裂后的buddy块大小，应该是原来的一半
        temp=temp*2;
    }
    page2=page1+temp;//page2相当于是page1的buddy块，用page1的地址加上temp就变成了page2的地址

    page1->property=n-1;
    page2->property=n-1;//拆开之后，将两个分开的页表项的阶数属性都减一，因为大小减小了一半
    SetPageProperty(page1);
    SetPageProperty(page2);//更新完之后，更新两个新分裂后的页表的属性，包括flag等

    list_del(le);//操作完之后，把原来的大块删了
    list_add(&(buddy_array[n-1]),&page1->page_link);
    list_add(&(page1->page_link),&(page2->page_link));//在n-1的链表中，往后依次添加page1和page2的链表项

    return;
}


static void
show_buddy_array(int left, int right) //显示指定范围内的空闲链表数组的内容，包括页数量和地址信息
{
    bool empty = 1; //初始化empty变量为1，后续用于判定是否为空
    assert(left >= 0 && left <= max_order && right >= 0 && right <= max_order);//判断合法性
    cprintf("------------------当前空闲的链表数组:------------------\n");
    for (int i = left; i <= right; i++)//循环遍历指定范围内的每个块阶数，逐个检查每个块阶数对应的链表
    {
        list_entry_t *le = &buddy_array[i];//定义一个链表元素指针le，并初始化为指向当前块阶数对应的链表的头指针
        if (list_next(le) != &buddy_array[i])//判定是否为空链表
        {
            empty = 0;//标记为0，表示非空

            //如果链表不为空，进入这个循环，遍历链表中的每个元素。在每次循环中，将le更新为链表中的下一个元素，直到le再次指向链表头为止
            while ((le = list_next(le)) != &buddy_array[i])
            {
                cprintf("No.%d的空闲链表有", i);//输出当前块阶数的信息，表示该块阶数的空闲链表有内容
                struct Page *p = le2page(le, page_link);
                cprintf("%d页 ", 1 << (p->property));//输出当前链表元素对应的页数量
                cprintf("【地址为%p】\n", p);//输出当前链表元素对应的页的地址
            }
            if (i != right)
            {
                cprintf("\n");
            }
        }
    }
    if (empty)
    {
        cprintf("目前无空闲块！！！\n");
    }
    cprintf("------------------显示完成!------------------\n\n\n");
    return;

}

static void
buddy_system_init(void)//初始化buddy system
{

    for (int i = 0; i < MAX_BUDDY_ORDER + 1; i++)
    {
        list_init(buddy_array + i);//对每个块阶数对应的链表进行初始化
    }
    max_order = 0;
    nr_free = 0;//全部初始化为0
    return;
}


static void
buddy_system_init_memmap(struct Page *base, size_t n) //初始化链表内容
{
    assert(n > 0);
    size_t p_number;//页数量
    unsigned int order;//块的阶数
    p_number = Find_The_Small_2(n);      
    order = Get_Order_Of_2(p_number); 
    struct Page *p = base;//base是首页

    for (; p != base + p_number; p++)//遍历每一页
    {
        assert(PageReserved(p));
        p->flags = 0;      // 清除所有flag标记
        p->property = -1;   // 全部初始化为非头页
        set_page_ref(p, 0);  //将当前页的引用计数设置为0
    }
    max_order = order;
    nr_free = p_number;
    list_add(&(buddy_array[max_order]), &(base->page_link)); //将第一个页（内存起始地址base对应的页）插入到对应块阶数为max_order的链表中

    base->property = max_order; //将base页面的属性设置为max_order阶数
    SetPageProperty(base);      //设置对应的属性
    return;
    
}

static struct Page *
buddy_system_alloc_pages(size_t requested_pages)//用于完成内存块的分配
{
    assert(requested_pages > 0);//参数合法性检查

    if (requested_pages > nr_free)//如果请求的页大于最大页数，直接返回空值
    {
        return NULL;
    }

    struct Page *allocated_page = NULL;//初始化分配的页，为空
    size_t adjusted_pages = Find_The_Big_2(requested_pages); //首先，根据请求的页数，找到我们对应需要去获取到页的数量，往上找
    size_t order_of_2 = Get_Order_Of_2(adjusted_pages);   //找到我们页数所在的数组位置，也就是求出对应的幂指数

    bool found = 0;//初始化bool变量帮我们确定是否找到，然后进入while循环，直到找到为止
    while (!found)
    {
        if (!list_empty(&(buddy_array[order_of_2])))//查对应块阶数为order_of_2的链表是否为空
        {
            allocated_page = le2page(list_next(&(buddy_array[order_of_2])), page_link);//如果不空的话，就从对应的链表中取出第一个块，存储到allocated_page中
            list_del(list_next(&(buddy_array[order_of_2]))); //存完之后，就可以把原先的那块空闲内存删了

            ClearPageProperty(allocated_page); //完成该页的属性更新
            found = 1;//同时将found置为1，代表已经找到了
        }
        else//如果为空的话，说明对应的那一个数组，没有对应的空闲块了，我们就要往后找
        {
            int i;
            for (i = order_of_2 + 1; i <= max_order; ++i)//该循环，找到从order_of_2之后的第一个不为空的数组链表
            {
                if (!list_empty(&(buddy_array[i])))//只要不为空，就直接执行拆分操作
                {

                    buddy_system_split(i);//执行拆分操作
                    break;
                }
            }

            if (i > max_order)
            {
                break;
            }
        }
    }

    if (allocated_page != NULL)//完成剩余的nr_free的数量的更新
    {
        nr_free = nr_free - adjusted_pages;
    }

    return allocated_page;
}

struct Page *get_buddy(struct Page *block_addr, unsigned int block_size)//获取给定内存块地址的伙伴块地址
{
    unsigned int temp = 1;
    for(int i = 0; i < block_size; i++){
        temp = temp * 2;
    }
    size_t real_block_size = temp;     //存储我们当前内存块的大小
            
    size_t relative_block_addr = (size_t)block_addr - 0xffffffffc020f318; //计算伙伴块的相对偏移量

    size_t sizeOfPage = real_block_size * 0x28;       //计算块的大小，单位是字节           
    size_t buddy_relative_addr = (size_t)relative_block_addr ^ sizeOfPage;      //计算出字节偏移量
    struct Page *buddy_page = (struct Page *)(buddy_relative_addr + 0xffffffffc020f318); //计算伙伴块的真实地址
    return buddy_page;

}

static void
buddy_system_free_pages(struct Page *base, size_t n)//完成内存的释放和内存块合并功能
{
    assert(n > 0);
    unsigned int p_number = 1 << (base->property); //计算出要释放的页的大小
    assert(Find_The_Big_2(n) == p_number);//确保要释放的页数量与块的大小匹配
    cprintf("Buddy System算法将释放第NO.%d页开始的共%d页\n", page2ppn(base), p_number);
    struct Page *left_block = base; //初始化为传入的要释放的块的起始地址
    struct Page *buddy = NULL;
    struct Page *tmp = NULL;

    list_add(&(buddy_array[left_block->property]), &(left_block->page_link)); // 将当前块先插入我们对应的数组链表中

    buddy = get_buddy(left_block, left_block->property);//计算当前块的伙伴块地址，调用函数并输出伙伴块的地址情况
    while (PageProperty(buddy) && left_block->property < max_order)//while循环来完成伙伴块的递归合并操作
    {
        if (left_block > buddy)//如果当前块的地址大于伙伴块的地址，进入if分支，完成合并操作
        {                              
            left_block->property = -1; 
            SetPageProperty(base);     

            tmp = left_block;
            left_block = buddy;
            buddy = tmp;//完成了buddy和left_block的交换操作，确保较小地址的块始终被标记为left_block
        }

        list_del(&(left_block->page_link));
        list_del(&(buddy->page_link));//删除两个小块在链表中的位置
        left_block->property = left_block->property + 1; // 左快头页设置幂次加一，然后将合并之后的数组位置再加一，完成合并操作

        list_add(&(buddy_array[left_block->property]), &(left_block->page_link)); // 头插入相应链表

        buddy = get_buddy(left_block, left_block->property);//再给出现在的页表地址情况
    }
    SetPageProperty(left_block); //更新对应页表的属性
    nr_free += p_number;//操作完后，更新我们的空闲页表总数


    return;
}

static size_t
buddy_system_nr_free_pages(void)//输出当前的空闲页的数量
{
    return nr_free;
}


static void
buddy_system_check_easy_alloc_and_free_condition(void)
{
    cprintf("CHECK OUR EASY ALLOC CONDITION:\n");
    cprintf("当前总的空闲块的数量为：%d\n", nr_free);
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    cprintf("首先,p0请求10页\n");
    p0 = alloc_pages(10);
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("然后,p1请求10页\n");
    p1 = alloc_pages(10);
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("最后,p2请求10页\n");
    p2 = alloc_pages(10);
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("p0的虚拟地址为:0x%016lx.\n", p0);

    cprintf("p1的虚拟地址为:0x%016lx.\n", p1);

    cprintf("p2的虚拟地址为:0x%016lx.\n", p2);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    cprintf("CHECK OUR EASY FREE CONDITION:\n");
    cprintf("释放p0...\n");
    free_pages(p0, 10);
    cprintf("释放p0后,总空闲块数目为:%d\n", nr_free); 
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("释放p1...\n");
    free_pages(p1, 10);
    cprintf("释放p1后,总空闲块数目为:%d\n", nr_free); 
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("释放p2...\n");
    free_pages(p2, 10);
    cprintf("释放p2后,总空闲块数目为:%d\n", nr_free); 
    show_buddy_array(0, MAX_BUDDY_ORDER);

}

static void
buddy_system_check_difficult_alloc_and_free_condition(void)
{
    cprintf("CHECK OUR DIFFICULT ALLOC CONDITION:\n");
    cprintf("当前总的空闲块的数量为：%d\n", nr_free);
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;

    cprintf("首先,p0请求10页\n");
    p0 = alloc_pages(10);
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("然后,p1请求50页\n");
    p1 = alloc_pages(50);
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("最后,p2请求100页\n");
    p2 = alloc_pages(100);
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("p0的虚拟地址为:0x%016lx.\n", p0);

    cprintf("p1的虚拟地址为:0x%016lx.\n", p1);

    cprintf("p2的虚拟地址为:0x%016lx.\n", p2);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    cprintf("CHECK OUR EASY DIFFICULT CONDITION:\n");
    cprintf("释放p0...\n");
    free_pages(p0, 10);
    cprintf("释放p0后,总空闲块数目为:%d\n", nr_free); 
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("释放p1...\n");
    free_pages(p1, 50);
    cprintf("释放p1后,总空闲块数目为:%d\n", nr_free); 
    show_buddy_array(0, MAX_BUDDY_ORDER);

    cprintf("释放p2...\n");
    free_pages(p2, 100);
    cprintf("释放p2后,总空闲块数目为:%d\n", nr_free); 
    show_buddy_array(0, MAX_BUDDY_ORDER);

}


static void buddy_system_check_min_alloc_and_free_condition(void){
    struct Page *p3 = alloc_pages(1);
    cprintf("分配p3之后(1页)\n");
    show_buddy_array(0, MAX_BUDDY_ORDER);

    // 全部回收
    free_pages(p3, 1);
    show_buddy_array(0, MAX_BUDDY_ORDER);
}


static void buddy_system_check_max_alloc_and_free_condition(void){
    struct Page *p3 = alloc_pages(16384);
    cprintf("分配p3之后(16384页)\n");
    show_buddy_array(0, MAX_BUDDY_ORDER);

    // 全部回收
    free_pages(p3, 16384);
    show_buddy_array(0, MAX_BUDDY_ORDER);
}


static void
buddy_system_check(void){//我们的最终检测函数！！
    cprintf("BEGIN TO TEST OUR BUDDY SYSTEM!\n");
    buddy_system_check_easy_alloc_and_free_condition();
    buddy_system_check_min_alloc_and_free_condition();
    buddy_system_check_max_alloc_and_free_condition();
    buddy_system_check_difficult_alloc_and_free_condition();
    
}




//我们的结构体，便于pmm.c中的内存分配方式的调用
const struct pmm_manager buddy_system_pmm_manager = {
    .name = "buddy_system_pmm_manager",
    .init = buddy_system_init,
    .init_memmap = buddy_system_init_memmap,
    .alloc_pages = buddy_system_alloc_pages,
    .free_pages = buddy_system_free_pages,
    .nr_free_pages = buddy_system_nr_free_pages,
    .check = buddy_system_check,
};
