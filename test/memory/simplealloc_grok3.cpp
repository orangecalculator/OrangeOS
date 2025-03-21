#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define MAX_ORDER 10   // Example: supports blocks up to 2^10 * PAGE_SIZE
#define SLAB_SIZE 64   // Example size for small objects
#define MAX_PAGES 1024 // Maximum pages we’ll support for simplicity

class Allocator {
public:
  struct Page {
    void *addr;         // Start of the page’s usable memory
    unsigned int order; // Buddy allocator order
    bool free;          // Is the page free?
    Page *next;         // Next page in free list
  };

  struct Slab {
    Slab *next;         // Next slab in the cache
    void *free_list;    // List of free objects
    unsigned int inuse; // Number of allocated objects
  };

  Allocator() : base_address(nullptr), page_count(0), slab_cache(nullptr) {
    for (int i = 0; i <= MAX_ORDER; ++i) {
      free_lists[i] = nullptr;
    }
  }

  void register_region(void *addr, size_t size) {
    if (base_address != nullptr)
      return; // Simplification: one region
    base_address = addr;
    size_t num_pages = size / PAGE_SIZE;
    if (page_count + num_pages > MAX_PAGES)
      return; // Error: too many pages

    for (size_t i = 0; i < num_pages; ++i) {
      Page *page = &page_array[page_count + i];
      page->addr = (void *)((uintptr_t)addr + i * PAGE_SIZE);
      page->order = 0;
      page->free = true;
      page->next = free_lists[0];
      free_lists[0] = page;
    }
    page_count += num_pages;
    coalesce_free_pages();
  }

  void *ookmalloc(size_t size) {
    if (size == 0)
      return nullptr;
    if (size <= SLAB_SIZE) {
      return allocate_slab_object();
    } else {
      size_t order = calculate_order(size);
      Page *page = buddy_alloc(order);
      return page ? page->addr : nullptr;
    }
  }

  void ookfree(void *p) {
    if (!p)
      return;
    uintptr_t addr = (uintptr_t)p;

    // Check if it’s a slab object
    for (Slab *slab = slab_cache; slab; slab = slab->next) {
      uintptr_t slab_addr = (uintptr_t)slab;
      size_t index = (slab_addr - (uintptr_t)base_address) / PAGE_SIZE;
      if (index >= page_count)
        continue;
      Page *slab_page = &page_array[index];
      if (slab_addr != (uintptr_t)slab_page->addr)
        continue;

      size_t header_size = (sizeof(Slab) + SLAB_SIZE - 1) & ~(SLAB_SIZE - 1);
      uintptr_t object_start = slab_addr + header_size;
      size_t max_objects = (PAGE_SIZE - header_size) / SLAB_SIZE;
      uintptr_t object_end = object_start + max_objects * SLAB_SIZE;

      if (addr >= object_start && addr < object_end &&
          (addr - object_start) % SLAB_SIZE == 0) {
        *reinterpret_cast<void **>(p) = slab->free_list;
        slab->free_list = p;
        slab->inuse--;
        if (slab->inuse == 0) {
          free_slab(slab, slab_page);
        }
        return;
      }
    }

    // Assume buddy allocation
    size_t index = (addr - (uintptr_t)base_address) / PAGE_SIZE;
    if (index < page_count) {
      Page *page = &page_array[index];
      if ((uintptr_t)page->addr == addr) {
        buddy_free(page);
      }
    }
  }

private:
  void *base_address;              // Base address of managed memory
  size_t page_count;               // Number of pages registered
  Page page_array[MAX_PAGES];      // Metadata for pages
  Page *free_lists[MAX_ORDER + 1]; // Free lists for each order
  Slab *slab_cache;                // Cache of slabs

  size_t calculate_order(size_t size) {
    size_t order = 0;
    size_t block_size = PAGE_SIZE;
    while (block_size < size && order < MAX_ORDER) {
      block_size <<= 1;
      order++;
    }
    return order;
  }

  Page *buddy_alloc(size_t order) {
    if (order > MAX_ORDER)
      return nullptr;

    Page *page = free_lists[order];
    if (page) {
      free_lists[order] = page->next;
      page->free = false;
      page->next = nullptr;
      return page;
    }

    Page *larger = buddy_alloc(order + 1);
    if (!larger)
      return nullptr;

    // Split the larger block
    size_t block_size = PAGE_SIZE << order;
    Page *buddy =
        &page_array[((uintptr_t)larger->addr - (uintptr_t)base_address) /
                        PAGE_SIZE +
                    (block_size / PAGE_SIZE)];
    buddy->addr = (void *)((uintptr_t)larger->addr + block_size);
    buddy->order = order;
    buddy->free = true;
    buddy->next = free_lists[order];
    free_lists[order] = buddy;

    larger->order = order;
    larger->free = false;
    return larger;
  }

  void buddy_free(Page *page) {
    page->free = true;
    uintptr_t addr = (uintptr_t)page->addr;
    size_t order = page->order;

    while (order < MAX_ORDER) {
      uintptr_t buddy_addr = addr ^ (PAGE_SIZE << order);
      size_t buddy_index = (buddy_addr - (uintptr_t)base_address) / PAGE_SIZE;
      if (buddy_index >= page_count)
        break;
      Page *buddy = &page_array[buddy_index];
      if ((uintptr_t)buddy->addr != buddy_addr || !buddy->free ||
          buddy->order != order)
        break;

      // Remove buddy from its free list
      Page *prev = nullptr;
      Page *curr = free_lists[order];
      while (curr && curr != buddy) {
        prev = curr;
        curr = curr->next;
      }
      if (curr == buddy) {
        if (prev) {
          prev->next = buddy->next;
        } else {
          free_lists[order] = buddy->next;
        }
      }

      // Merge: use the lower address
      if (addr > buddy_addr) {
        page = buddy;
        addr = buddy_addr;
      }
      order++;
      page->order = order;
    }

    page->next = free_lists[order];
    free_lists[order] = page;
  }

  void coalesce_free_pages() {
    // Simplified: assume initial pages are order 0 and coalesce as needed
    // In practice, this would merge adjacent free pages up to MAX_ORDER
  }

  void *allocate_slab_object() {
    if (!slab_cache || !slab_cache->free_list) {
      Page *page = buddy_alloc(0);
      if (!page)
        return nullptr;
      Slab *slab = reinterpret_cast<Slab *>(page->addr);
      slab->next = slab_cache;
      slab->inuse = 0;

      size_t header_size = (sizeof(Slab) + SLAB_SIZE - 1) & ~(SLAB_SIZE - 1);
      char *base = reinterpret_cast<char *>(slab) + header_size;
      size_t max_objects = (PAGE_SIZE - header_size) / SLAB_SIZE;

      for (size_t i = 0; i < max_objects - 1; ++i) {
        *reinterpret_cast<void **>(base + i * SLAB_SIZE) =
            base + (i + 1) * SLAB_SIZE;
      }
      *reinterpret_cast<void **>(base + (max_objects - 1) * SLAB_SIZE) =
          nullptr;
      slab->free_list = base;
      slab_cache = slab;
    }

    void *ptr = slab_cache->free_list;
    slab_cache->free_list = *reinterpret_cast<void **>(ptr);
    slab_cache->inuse++;
    return ptr;
  }

  void free_slab(Slab *slab, Page *slab_page) {
    if (slab == slab_cache) {
      slab_cache = slab->next;
    } else {
      Slab *prev = slab_cache;
      while (prev->next != slab)
        prev = prev->next;
      prev->next = slab->next;
    }
    buddy_free(slab_page);
  }
};

// Global instance to maintain the required interface
static Allocator global_allocator;

void *ookmalloc(size_t size) { return global_allocator.ookmalloc(size); }

void ookfree(void *p) { global_allocator.ookfree(p); }

void register_region(void *addr, size_t size) {
  global_allocator.register_region(addr, size);
}