// ------------------------SLUB 设计文档

// ------------SLUB 原理概述
// SLUB（Slab Utilization By-pass）是 Linux 内核中的一种内存分配器，专门用于高效地管理内核中的小对象。
// 它是 SLAB 分配器的改进版本，旨在提高性能、简化实现并减少内存碎片。
// SLUB 是现代 Linux 内核中默认的内存分配器，广泛用于分配和回收内核数据结构，如进程描述符、文件描述符等。

// ------------SLUB 核心思想
// 通过预分配固定大小的内存块（称为 slabs）来管理和分配内存。每个 slab 包含多个相同大小的对象，这些对象可以被快速分配和释放。

// ------------SLUB 主要机制
// 缓存（Caches）
// - 缓存：SLUB 为每种大小的内存对象维护一个缓存（cache）。每个缓存对应一种特定大小的对象，管理着这些对象的分配和释放。
// - 对象大小：每个缓存中的对象大小固定，这样可以减少内存碎片并提高分配效率。
// Slab 的管理
// - Slab 结构：一个 slab 是一块连续的内存区域，包含多个相同大小的对象（在本实验中可以以页为单位）。每个 slab 都与一个特定的缓存相关联。
// - 状态管理：每个 slab 可以处于三种状态之一：
//   - 完全空闲（All free）：所有对象都未被分配。
//   - 部分分配（Partial）：部分对象已被分配，部分对象仍然空闲。
//   - 完全分配（Full）：所有对象都已被分配。
// 对象的分配和释放
// - 分配对象：
//   - 当需要分配一个对象时，SLUB 会在对应缓存的 slabs 中寻找第一个有空闲对象的 slab。
//   - 如果找到一个部分分配的 slab，SLUB 会分配一个空闲对象并更新 slab 的状态。
//   - 如果没有找到合适的 slab，SLUB 会创建一个新的 slab 并分配对象。
// - 释放对象：
//   - 当释放一个对象时，SLUB 会将其返回到所属的 slab，并更新 slab 的状态。
//   - 如果一个 slab 中所有对象都被释放，SLUB 可以将该 slab 返回给内存池以供重用。

// ------------设计实现
// 我们在ucore中仿照SLUB的主要思想设计了简易版的实现，步骤如下：
// 1、设计思路
// 我们要实现的是slub算法，实现两层架构的高效内存单元分配，第一层是基于页大小的内存分配，第二层是在第一层基础上实现基于任意大小的内存分配。
// 因此，我们保留页级别的分配策略，比如default（First-Fit）或Best-Fit算法，以当作第一层的分配，
// 期望用户要求大内存分配的时候它们负责。而小内存需求由第二层来管理，这个内存需求应该小于4096KB（页大小），所以我将每个slab的占据单位设定为1页
// slab里存有固定大小的很多对象(可以通过观察检查部分的输出来理解)。关于slab的页面具体内容分配如下：

//          slab_struct_size  ||  obj  ||  bitmap

// 也就是说先存slab结构体大小，再存其中的很多对象，然后再存储表示每个obj位置是否被分配的位图。
// 2、新增数据结构Cache和Slab：
// typedef struct Slab{    //Slab 是一块连续的内存区域
//                         //用于存储多个相同大小的对象
//     list_entry_t list;  //链接自身的
//     size_t free_cnt;    //空闲对象数量
//     void *objs;         //对象寻址指针
//     unsigned char *bitmap;//位图，标记对象的使用与否
// }slab_t;

// typedef struct Cache{
//     list_entry_t slabs;//链接slab
//     size_t obj_size;   //每个对象的大小
//     size_t objs_num;   //一个slab有多少对象
// }cache_t;

// 3、初始化
// 初始化分两层，我们先用默认页级别分配器进行页级别的初始化（后面slab的分配也是基于页的，统一按页初始化就比较方便）然后初始化cache数据结构：
// static cache_t caches[3];//简化实现，3个cache
// static size_t cache_n=0;//cache计数器

// static void cache_init(void){
//     cache_n=3;
//     size_t sizes[3]={32,64,128};//对应大小
//     for(int i=0;i<cache_n;i++){
//         caches[i].obj_size=sizes[i];//大小设定
//         caches[i].objs_num=calculate_objs_num(sizes[i]);//计算每个slab能存obj数量
//         list_init(&caches[i].slabs);//初始化slab链表
//     }
// }
// Cache管理很多个Slab，Slab再存很多个obj，同Cache的obj大小都是一样的。
// 这里我们需要根据大小来计算数量，计算过程比较简单，我们设定x为个数，x是应满足如下不等式的最大整数：
//          slab_struct_size+x*size+(x+7)/8<=4096
// 很容易计算的，比如size为32KB的时候（slab结构体40KB），我们可以计算得x最大为126.
// 这样我们就初始化好两层的分配管理器了！
// 4、分配和释放
// 下面我们看分配/释放的方法：
//   对于分配，我们按照如下步骤进行：
//   1. 先根据size寻找最小的大于等于size的大小型号的Cache，找不到返回NULL
//   2. 找到后进入链表项访问其中的Slab
//   3. 如果该Slab已经满了，按链表访问下一个Slab，直到找到不满的Slab进行分配，分配时注意：返回对应obj指针，同时位图设置相应位，空闲数减一等操作。
//   4. 如果都恰好满了，分配新的一页给新的Slab，并进行相应链接。
//   具体实现见代码。
//   对于释放，我们按照如下步骤进行
//   1. 寻找该obj所在的Slab（遍历Cache，Slab，比较慢）
//   2. 找到后直接进行相应释放操作
//   3. 如果释放后整个Slab没有obj了，直接调用页级别的释放页面函数将其释放即可！

// ------------测试代码
// 以上就是我们根据Slub的主要思想设计的相应算法。下面我们给出测试我们机制的一些关键点，具体关键点的测试实现详见代码中的测试函数。
// - 测试基本的分配/释放函数的功能（单个+多个）。
// - 测试分配大小的边界条件。
// - 测试大量分配释放的整体逻辑。
// - 测试一些混合分配/释放流程。





#include <pmm.h>
#include <list.h>
#include <string.h>
#include <slub_pmm.h>
#include <stdio.h>
// 扩展练习2：编写slub 内存分配算法
static free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

#define le2slab(le, member)                 \
    to_struct((le), struct Slab, member)
//SLUB必要的组件

typedef struct Slab{   //Slab 是一块连续的内存区域
                        //用于存储多个相同大小的对象
    list_entry_t list;  //链接自身的
    size_t free_cnt;    //空闲对象数量
    void *objs;         //对象寻址指针
    unsigned char *bitmap;//位图，标记对象的使用与否
}slab_t;

typedef struct Cache{
    list_entry_t slabs;//链接slab
    size_t obj_size;   //每个对象的大小
    size_t objs_num;    //一个slab有多少对象
}cache_t;

static cache_t caches[3];//简化实现，3个cache
static size_t cache_n=0;//cache计数器

static size_t calculate_objs_num(size_t obj_size) {
    // slab_t 结构体大小
    size_t slab_struct_size = sizeof(slab_t);
    // 每个对象需要一位表示分配状态，位图大小
    // 对象数量 n，则位图大小为 ceil(n / 8) 字节
    // 总内存占用 = slab_struct_size + (object_size * n) + ceil(n / 8)
    // 我们需要找到最大的 n，使得：
    // slab_struct_size + (object_size * n) + ceil(n / 8) <= PGSIZE
    // 实际上整数的除法是个很好的操作，能直接实现结果向下取整的效果。
    size_t objects_per_slab = ((PGSIZE - slab_struct_size) / (obj_size + 1.0 / 8.0));
    // 确保至少有一个对象
    if (objects_per_slab == 0) {
        objects_per_slab = 1;
    }
    return objects_per_slab;
}

//为缓存初始化
static void cache_init(void){
    cache_n=3;
    size_t sizes[3]={32,64,128};
    for(int i=0;i<cache_n;i++){
        caches[i].obj_size=sizes[i];
        caches[i].objs_num=calculate_objs_num(sizes[i]);
        list_init(&caches[i].slabs);
    }
}



static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;//没有空闲页
}
static void
slub_init(void) {
    default_init();         // 初始化第一层：页级分配器
    cache_init();           // 初始化第二层：SLUB 缓存
}


static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));//确保都是保留好的
        p->flags = p->property = 0;//都不是空闲块的头部
        set_page_ref(p, 0);
    }
    base->property = n;//这个是头部！
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {//如果表空，就把他直接添加进去
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {//穿进去，按顺序
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {//说明这个base特大，加最后去
                list_add(le, &(base->page_link));
            }
        }
    }
}
static void 
slub_init_memmap(struct Page *base,size_t n){
    default_init_memmap(base,n);
}



static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);//分配的数量肯定要大于0
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {//割掉相应大小，剩余的再串回去
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);//此时flag清了，属性成员并没有清
    }
    return page;
}

static slab_t* create_slab(size_t obj_size,size_t objs_num){
    struct Page* page=default_alloc_pages(1);//分配一页！
    if(!page)return NULL;
    void* kva=KADDR(page2pa(page));
    slab_t *slab=(slab_t*)kva;
    slab->free_cnt=objs_num;
    slab->objs=(void*)slab+sizeof(slab_t);//指向对象存储区域
    slab->bitmap=(unsigned char*)((void*)slab->objs+obj_size*objs_num);//指向位图区域
    memset(slab->bitmap,0,(objs_num+7)/8);
    list_init(&slab->list);
    return slab;
}


static void* slub_alloc_obj(size_t size){

    if(size<=0) return NULL;

    //先找合适大小的缓存！
    cache_t *cache=NULL;
    for(int i=0;i<cache_n;i++){
        if(caches[i].obj_size>=size){
            cache =&caches[i];
            break;
        }
    }
    if(cache==NULL) return NULL;

    list_entry_t *le=&cache->slabs;
    while ((le = list_next(le)) != &cache->slabs) {
        slab_t *slab = le2slab(le, list);
        if (slab->free_cnt > 0) {
            for (size_t i = 0; i < cache->objs_num; i++) {
                size_t byte = i / 8;
                size_t bit = i % 8;
                if (!(slab->bitmap[byte] & (1 << bit))) {
                    slab->bitmap[byte] |= (1 << bit); // 标记为已分配
                    slab->free_cnt--;
                    return (void*)slab->objs + i * cache->obj_size;//返回地址指针
                }
            }
        }
    }
    slab_t *new_slab=create_slab(cache->obj_size,cache->objs_num);
    if(!new_slab) return NULL;

    list_add(&cache->slabs,&new_slab->list);
    new_slab->bitmap[0] |= 1;
    new_slab->free_cnt--;
    return new_slab->objs;
}

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {//重置了
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {//和初始函数一样，在链接到正确位置
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {//p是前面的，可以和base合并
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {//p是后面的，可以和base合并
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

static void
slub_free_obj(void* obj){
    for (size_t i = 0; i < cache_n; i++) {
        cache_t *cache = &caches[i];
        list_entry_t *le = &cache->slabs;
        while ((le = list_next(le)) != &cache->slabs) {
            slab_t *slab = le2slab(le, list);
            if (obj >= slab->objs && obj < (slab->objs + cache->obj_size * cache->objs_num)) {
                size_t offset = (char*)obj - (char*)slab->objs;
                size_t index = offset / cache->obj_size;
                size_t byte = index / 8;
                size_t bit = index % 8;
                if (slab->bitmap[byte] & (1 << bit)) {
                    slab->bitmap[byte] &= ~(1 << bit); // 标记为未分配
                    slab->free_cnt++;
                    memset(obj, 0, cache->obj_size);//释放清零
                    // 如果整个slab都是空闲的，可以将其释放
                    if (slab->free_cnt == cache->objs_num) {
                        list_del(&slab->list);
                        default_free_pages(pa2page(PADDR(slab)), 1);
                    }
                }
                return;
            }
        }
    }
}


static size_t
slub_nr_free_pages(void) {
    return nr_free;
}

static void
slub_check(void) {
    cprintf("Starting SLUB allocator tests...\n\n");
    cprintf("The slab struct size is %d\n",sizeof(slab_t));
    cprintf("----------------------START-------------------------\n");
    //检查初始化后的大小
    //对于32字节的slab，应该有126obj/slab
    //64字节：63obj/slab
    //128字节，31obj/slab
    size_t nums[3]={126,63,31};
    for(int i=0;i<cache_n;i++){
        assert(caches[i].objs_num==nums[i]);
    }
    size_t nr_1=nr_free;
    //——————————边界检查
    {
        void* obj=slub_alloc_obj(0);
        assert(obj==NULL);
        obj=slub_alloc_obj(256);
        assert(obj==NULL);
        cprintf("Boundary check passed. \n");
    }
    //——————————分配释放功能检查
    {
        void* obj1=slub_alloc_obj(32);
        assert(obj1!=NULL);
        cprintf("Allocated 32-byte object at %p\n", obj1);
        memset(obj1,0x66,32);
        for(int i=0;i<32;i++){
            assert(((unsigned char*)obj1)[i]==0x66);
        }
        cprintf("Memory alloc verification passed. \n");
        slub_free_obj(obj1);

        void* obj2=slub_alloc_obj(32);
        cprintf("Allocated 32-byte object at %p\n", obj2);
        for(int i = 0; i < 32; i++) {
            assert(((unsigned char*)obj2)[i] == 0x00);
        }
        slub_free_obj(obj2);
        cprintf("Memory free verification passed. \n");
    }
    //——————————多个分配释放功能检查
    {
        const int NUM_TEST_OBJS = 10;
        void* test_objs[NUM_TEST_OBJS];
        cprintf("Allocating %d objects of size 64 bytes.\n", NUM_TEST_OBJS);
        for(int i = 0; i < NUM_TEST_OBJS; i++) {
            test_objs[i] = slub_alloc_obj(64);
            assert(test_objs[i] != NULL);
            // 赋值
            memset(test_objs[i], i, 64);
        }
    
        for(int i = 0; i < NUM_TEST_OBJS; i++) {
            for(int j = 0; j < 64; j++) {
                assert(((unsigned char*)test_objs[i])[j] == (unsigned char)i);
            }
        }
        cprintf("Memory verification for 64-byte objects passed.\n");
        // 释放对象
        for(int i = 0; i < NUM_TEST_OBJS; i++) {
            slub_free_obj(test_objs[i]);
            cprintf("Freed 64-byte object at %p\n", test_objs[i]);
            // 验证内存是否被清零
            for(int j = 0; j < 64; j++) {
                assert(((unsigned char*)test_objs[i])[j] == 0x00);
            }
        }
        cprintf("Memory free verification for 64-byte objects passed.\n");
    }
    //——————————大量分配释放检查
    {
        size_t nr_2,nr_3,nr_4;
        cprintf("Bulk allocation release check start.\n");
        assert(nr_1==nr_free);

        void *objs_bulk[50000];
        for(int i=1;i<=10000;i++){
            objs_bulk[i-1]=slub_alloc_obj(25);
            assert(nr_free==nr_1-(i+125)/126);
        }
        nr_2=nr_free;
        
        for(int i=1;i<=10000;i++){
            objs_bulk[i+9999]=slub_alloc_obj(62);
            assert(nr_free==nr_2-(i+62)/63);
        }
        nr_3=nr_free;
    
        for(int i=1;i<=10000;i++){
            objs_bulk[i+19999]=slub_alloc_obj(124);
            assert(nr_free==nr_3-(i+30)/31);
        }
        nr_4=nr_free;

        for(int i=1;i<=10000;i++){
            objs_bulk[i+29999]=slub_alloc_obj(129+i%666);
            assert(nr_free==nr_4);
        }

        for(int i=0;i<40000;i++){
            if(i<30000){
                assert(objs_bulk[i]!=NULL);
                slub_free_obj(objs_bulk[i]);
            }
            else{
                assert(objs_bulk[i]==NULL);
            }
        }
        assert(nr_free==nr_1);
        cprintf("Bulk allocation release check passed.\n");
    }
    //——————————复杂流程检查
    {
        cprintf("Mixed check start.\n");

        void* obj1=slub_alloc_obj(32);
        assert(obj1!=NULL);
        cprintf("Allocated 32-byte object at %p\n", obj1);
        assert(nr_free==nr_1-1);

        void* obj2=slub_alloc_obj(64);
        assert(obj2!=NULL);
        cprintf("Allocated 64-byte object at %p\n", obj2);
        assert(nr_free==nr_1-2);


        void* obj3=slub_alloc_obj(128);
        assert(obj3!=NULL);
        cprintf("Allocated 128-byte object at %p\n", obj3);
        assert(nr_free==nr_1-3);


        void* obj4=slub_alloc_obj(32);
        assert(obj4!=NULL);
        cprintf("Allocated second 32-byte object at %p\n", obj4);
        assert(nr_free==nr_1-3);


        void* objs[100];
        for(int i=1;i<=29;i++){
            objs[i]=slub_alloc_obj(128);
        }

        void* obj5=slub_alloc_obj(128);
        assert(obj5!=NULL);
        cprintf("Allocated 31th 128-byte object at %p\n", obj5);
        assert(nr_free==nr_1-3);

        void* obj6=slub_alloc_obj(128);
        assert(obj6!=NULL);
        cprintf("Allocated 32th(new slam) 128-byte object at %p\n", obj6);
        assert(nr_free==nr_1-4);

        for(int i=1;i<=29;i++){
            slub_free_obj(objs[i]);
        }
        assert(nr_free==nr_1-4);

        slub_free_obj(obj1);
        assert(nr_free==nr_1-4);

        slub_free_obj(obj2);
        assert(nr_free==nr_1-3);

        slub_free_obj(obj3);
        assert(nr_free==nr_1-3);

        slub_free_obj(obj4);
        assert(nr_free==nr_1-2);

        slub_free_obj(obj5);
        assert(nr_free==nr_1-1);

        slub_free_obj(obj6);
        assert(nr_free==nr_1);
    
        cprintf("Mixed check passed.\n");
    }
    cprintf("----------------------END-------------------------\n");
}
const struct pmm_manager slub_pmm_manager = {//配置那些函数
    .name = "slub_pmm_manager",
    .init = slub_init,
    .init_memmap = slub_init_memmap,
    .alloc_pages = slub_alloc_obj,
    .free_pages = slub_free_obj,
    .nr_free_pages = slub_nr_free_pages,
    .check = slub_check,
};