#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* 在首次适应算法中，分配器维护一个空闲块的列表（称为空闲列表），
   当收到内存请求时，它沿着列表扫描，找到第一个足够大的块来满足请求。
   如果选择的块比请求的大很多，通常会对其进行拆分，剩余部分作为另一个空闲块加入列表中。
   详细内容请参考闫卫民的中文书籍《数据结构——C语言描述》第196~198页，第8.2节。
*/

// 你需要重写以下函数：default_init、default_init_memmap、default_alloc_pages、default_free_pages。
/*
 * FFMA的细节
 * (1) 准备工作：为了实现首次适应内存分配（FFMA），我们应该使用某种列表来管理空闲内存块。
 *              结构体 `free_area_t` 用于管理空闲内存块。首先，你需要熟悉 `list.h` 中的结构体 `list`，
 *              它是一个简单的双向链表实现。你需要知道如何使用：list_init、list_add(list_add_after)、list_add_before、list_del、list_next、list_prev。
 *              还有一种巧妙的方法是将通用的链表结构转换为特定的结构（例如 `struct page`）：
 *              你可以找到一些宏：如 `le2page`（在 `memlayout.h` 中），以及在未来的实验中会使用到的宏：`le2vma`（在 `vmm.h` 中），`le2proc`（在 `proc.h` 中）等。
 * 
 * (2) `default_init`：你可以复用示例中的 `default_init` 函数来初始化 `free_list` 并将 `nr_free` 设置为0。
 *              `free_list` 用于记录空闲内存块，`nr_free` 是空闲内存块的总数。
 * 
 * (3) `default_init_memmap`：调用图：`kern_init` --> `pmm_init` --> `page_init` --> `init_memmap` --> `pmm_manager->init_memmap`
 *              这个函数用于初始化一个空闲块（参数：`addr_base` 和 `page_number`）。
 *              首先，你应该初始化这个空闲块中的每一页（`memlayout.h` 中），包括：
 *                  - `p->flags` 应设置 `PG_property` 位（表示这个页面是有效的。在 `pmm.c` 中的 `pmm_init` 函数中，`PG_reserved` 位已设置）。
 *                  - 如果这个页面是空闲的，并且不是空闲块的第一个页面，`p->property` 应该设置为 0。
 *                  - 如果这个页面是空闲的，并且是空闲块的第一个页面，`p->property` 应该设置为空闲块的总页数。
 *                  - `p->ref` 应为 0，因为此时 `p` 是空闲的，没有引用。
 *                  - 我们可以使用 `p->page_link` 将这个页面链接到 `free_list`（例如：`list_add_before(&free_list, &(p->page_link));`）。
 *              最后，我们应该将空闲内存块的数量加到 `nr_free` 中：`nr_free += n`
 * 
 * (4) `default_alloc_pages`：在空闲列表中搜索第一个空闲块（块大小 >= `n`），调整空闲块大小，返回分配的块的地址。
 *              (4.1) 你应该像下面这样搜索空闲列表：
 *                       `list_entry_t le = &free_list;`
 *                       `while ((le = list_next(le)) != &free_list) {`
 *                       `....`
 *                 (4.1.1) 在 `while` 循环中，获取 `struct page` 并检查 `p->property`（记录空闲块的页数）是否 `>= n`：
 *                       `struct Page *p = le2page(le, page_link);`
 *                       `if (p->property >= n) { ...`
 *                 (4.1.2) 如果找到这个 `p`，这意味着我们找到了一个空闲块（块大小 >= `n`），可以分配前 `n` 页。
 *                     一些标志位需要设置：`PG_reserved = 1`，`PG_property = 0`
 *                     将这些页从 `free_list` 中取消链接。
 *                     (4.1.2.1) 如果 `p->property > n`，我们需要重新计算剩余空闲块的大小：
 *                           （例如：`le2page(le, page_link)->property = p->property - n;`）
 *                 (4.1.3) 重新计算剩余所有空闲块的数量：`nr_free`
 *                 (4.1.4) 返回 `p`
 *              (4.2) 如果没有找到合适的空闲块（块大小 >= `n`），返回 `NULL`
 * 
 * (5) `default_free_pages`：将页面重新链接到空闲列表中，可能需要将小的空闲块合并成大的空闲块。
 *               (5.1) 根据撤销分配块的基地址，搜索空闲列表，找到正确的位置（按地址从低到高），插入这些页（可以使用 `list_next`、`le2page`、`list_add_before`）。
 *               (5.2) 重置页面的字段，例如 `p->ref`，`p->flags`（`PageProperty`）。
 *               (5.3) 尝试合并相邻的低地址块或高地址块。注意：需要正确更改某些页面的 `p->property`。
 */

free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
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
}

static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
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
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
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
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

