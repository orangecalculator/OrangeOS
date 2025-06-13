#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <bitset>
#include <limits>

#ifdef OOK_SANITIZE
#include <assert.h>
#endif /* OOK_SANITIZE */

#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12U
#endif /* PAGE_SHIFT */

#ifndef PAGE_SIZE
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#endif /* PAGE_SIZE */

#ifndef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE - 1UL)
#endif /* PAGE_MASK */

#ifndef MAX_NUM_REGIONS
#define MAX_NUM_REGIONS 4U
#endif /* MAX_NUM_REGIONS */

#ifndef MAX_BLOCK_ORDER
#define MAX_BLOCK_ORDER 16U
#endif /* MAX_BLOCK_ORDER */

#ifndef container_of
#define container_of(ptr, type, member)                                        \
  ((type *)((char *)(ptr)-offsetof(type, member)))
#endif /* container_of */

namespace {

#ifdef OOK_SANITIZE
#define OOKBugOn(value) assert(!(value))
#else
#define OOKBugOn(value)                                                        \
  do {                                                                         \
    if (0) {                                                                   \
      (value);                                                                 \
    }                                                                          \
  } while (0)
#endif

template <typename UnsignedType> inline constexpr unsigned int bitwidth() {
  static_assert(std::is_unsigned<UnsignedType>::value);

  return CHAR_BIT * sizeof(UnsignedType);
}

template <typename UnsignedType>
inline constexpr unsigned int log2floor(const UnsignedType x) {
  static_assert(std::is_unsigned<UnsignedType>::value);

  if (x == 0)
    return 0;

  for (unsigned int nb = 0; nb < CHAR_BIT * sizeof(x); ++nb) {
    if ((x >> nb) == 1)
      return nb;
  }

  /* Never reached. */
  return std::numeric_limits<unsigned int>::max();
}

template <typename UnsignedType>
inline constexpr unsigned int log2ceil(const UnsignedType x) {
  static_assert(std::is_unsigned<UnsignedType>::value);

  if (x <= 1)
    return 0;

  return 1 + log2floor(x - 1);

  /* Never reached. */
  return std::numeric_limits<unsigned int>::max();
}

template <typename UnsignedType>
inline constexpr unsigned int ctz(const UnsignedType x) {
  static_assert(std::is_unsigned<UnsignedType>::value);

  if (x == 0)
    return CHAR_BIT * sizeof(UnsignedType);

  return log2floor(x & -x);
}

template <typename UnsignedType>
inline constexpr unsigned int popcount(const UnsignedType x) {
  static_assert(std::is_unsigned<UnsignedType>::value);

  if (x == 0)
    return 0;

  unsigned int count = 0;
  for (unsigned int nb = 0; nb < CHAR_BIT * sizeof(x); ++nb) {
    if ((x >> nb) & 1)
      count++;
  }

  return count;
}

struct list_head {
  struct list_head *prev = this;
  struct list_head *next = this;

  constexpr list_head() { reset(); }

  void add(struct list_head *e) {
    struct list_head *next = this->next;

    this->next = next->prev = e;
    e->prev = this;
    e->next = next;
  }

  void add_tail(struct list_head *e) { this->prev->add(e); }

  void remove() {
    struct list_head *prev = this->prev;
    struct list_head *next = this->next;

    prev->next = next;
    next->prev = prev;

    reset();
  }

  bool empty() { return (this->next == this); }

  constexpr void reset() { this->prev = this->next = this; }
};

template <size_t NBITS> class bitmap_t {
  using BitmapBasicType = unsigned long;
  static constexpr size_t BITMAP_GRANULE_BITS =
      sizeof(BitmapBasicType) * CHAR_BIT;

  static_assert(NBITS % BITMAP_GRANULE_BITS == 0);

  static constexpr size_t NELEMENTS =
      (NBITS + BITMAP_GRANULE_BITS - 1) / BITMAP_GRANULE_BITS;

public:
  void reset() {
    for (size_t i = 0; i < NELEMENTS; ++i) {
      data[i] = 0;
    }
  }

  bool get_bit(size_t bit) const {
    size_t index = bit / BITMAP_GRANULE_BITS;
    size_t offset = bit % BITMAP_GRANULE_BITS;

    return static_cast<bool>((data[index] >> offset) & 1);
  }

  void set_bit(size_t bit) {
    size_t index = bit / BITMAP_GRANULE_BITS;
    size_t offset = bit % BITMAP_GRANULE_BITS;

    data[index] |= static_cast<BitmapBasicType>(1) << offset;
  }

  void clear_bit(size_t bit) {
    size_t index = bit / BITMAP_GRANULE_BITS;
    size_t offset = bit % BITMAP_GRANULE_BITS;

    data[index] &= ~(static_cast<BitmapBasicType>(1) << offset);
  }

  size_t get_lowest_free_index() const {
    for (size_t index = 0; index < NELEMENTS; ++index) {
      unsigned long value = data[index];

      if (~value) {
        /* The answer exists in this block. */

        return index * BITMAP_GRANULE_BITS + ctz(~value);
      }
    }

    return NBITS;
  }

  size_t get_popcount() const {
    size_t count = 0;
    for (size_t index = 0; index < NELEMENTS; ++index) {
      count += popcount(data[index]);
    }

    return count;
  }

private:
  BitmapBasicType data[NELEMENTS];
};

struct ookpage {
  void *addr;
  size_t size;
  struct list_head list;
};

static_assert((sizeof(struct ookpage) & (sizeof(struct ookpage) - 1)) == 0);
constexpr unsigned int OOKPAGE_SIZE_SHIFT = log2ceil(sizeof(struct ookpage));
static_assert(OOKPAGE_SIZE_SHIFT < bitwidth<unsigned int>() &&
              sizeof(struct ookpage) == (1UL << OOKPAGE_SIZE_SHIFT));

struct OOKRegion {
  enum {
    OOKREGION_STATE_REGISTERED = 1 << 0,
  };

  void *addr = nullptr;
  size_t size = 0;
  struct ookpage *memmap = nullptr;
  int state = 0;

  static size_t calc_memmap_size(size_t size) {
    const size_t npages = size >> PAGE_SHIFT;
    const size_t memmap_size = npages << OOKPAGE_SIZE_SHIFT;
    const size_t memmap_region_size =
        (memmap_size + PAGE_SIZE - 1) & ~PAGE_MASK;

    return memmap_region_size;
  }

  /**
   * @warning size must be PAGE_SIZE aligned, and should be 2 pages or more.
   */
  int register_region(void *addr, size_t size) {
    if (this->state & OOKREGION_STATE_REGISTERED)
      return -EFAULT;

    if (size & PAGE_MASK)
      return -EINVAL;

    const size_t memmap_size = calc_memmap_size(size);

    if (memmap_size >= size)
      return -ERANGE;

    this->addr = addr;
    this->size = size;
    this->memmap = reinterpret_cast<struct ookpage *>(
        reinterpret_cast<uintptr_t>(addr) + (size - memmap_size));
    this->state |= OOKREGION_STATE_REGISTERED;

    return 0;
  }

  bool registered() const {
    return static_cast<bool>(this->state & OOKREGION_STATE_REGISTERED);
  }

  bool inrange(void *p) const { return inrange(p, 1); }

  bool inrange(void *p, size_t size) const {
    const auto [allocatable_addr, allocatable_size] = allocatable_region();
    return (allocatable_addr <= p &&
            reinterpret_cast<uintptr_t>(p) + size <=
                reinterpret_cast<uintptr_t>(allocatable_addr) +
                    allocatable_region().size);
  }

  struct mem_region_t {
    void *addr;
    size_t size;
  };
  struct mem_region_t allocatable_region() const {
    return mem_region_t{
        .addr = addr,
        .size = static_cast<size_t>(reinterpret_cast<uintptr_t>(memmap) -
                                    reinterpret_cast<uintptr_t>(addr))};
  }

  struct ookpage *get_memmap_entry(void *p) const {
    if (!inrange(p))
      return nullptr;

    return &this->memmap[(reinterpret_cast<uintptr_t>(p) -
                          reinterpret_cast<uintptr_t>(this->addr)) >>
                         PAGE_SHIFT];
  }
};

class PageAllocator {
public:
  int register_region(void *addr, size_t size) {
    int ret;

    for (unsigned int k = 0; k < std::size(registered_regions); ++k) {
      OOKRegion *region = &registered_regions[k];
      if (!region->registered()) {
        OOKRegion new_region;
        ret = new_region.register_region(addr, size);
        if (ret)
          return ret;

        ret = insert_region(&new_region);
        if (ret)
          return ret;

        *region = std::move(new_region);
        return 0;
      }
    }

    return -ENOMEM;
  }

  std::pair<int, struct ookpage *> allocate(size_t size) {
    if (size == 0)
      return {0, nullptr};

    if (size & PAGE_MASK)
      return {-EINVAL, nullptr};

    if (size > (static_cast<size_t>(1) << (MAX_BLOCK_ORDER + PAGE_SHIFT)))
      return {-ENOMEM, nullptr};

    unsigned int order = log2ceil(size >> PAGE_SHIFT);

    auto [ret, alloc_result] = allocate_pow2(order);

    OOKBugOn(!ret &&
             (alloc_result.region == nullptr || alloc_result.addr == nullptr));

    if (ret)
      return {ret, nullptr};

    struct OOKRegion *region = alloc_result.region;
    void *addr = alloc_result.addr;

    size_t rem_size = (static_cast<size_t>(1) << (order + PAGE_SHIFT)) - size;

    deallocate_impl(
        mem_location_t{
            .region = region,
            .addr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(addr) +
                                             size),
        },
        rem_size);

    struct ookpage *ookpage =
        region->get_memmap_entry(reinterpret_cast<void *>(addr));

    ookpage->addr = addr;
    ookpage->size = size;

    return {0, ookpage};
  }

  void deallocate(struct ookpage *ookpage) {
    if (ookpage == nullptr)
      return;

    void *addr = ookpage->addr;
    size_t size = ookpage->size;

    OOKBugOn(size == 0);

    ookpage->addr = nullptr;
    ookpage->size = 0;

    auto [ret, region_id] = find_region(addr);
    OOKBugOn(ret);

    struct OOKRegion *region = &registered_regions[region_id];

    deallocate_impl(
        mem_location_t{
            .region = region,
            .addr = addr,
        },
        size);
  }

  struct ookpage *page_to_desc(void *addr) {
    if (addr == nullptr)
      return nullptr;

    auto [ret, region_id] = find_region(addr);
    OOKBugOn(ret);

    struct OOKRegion *region = &registered_regions[region_id];

    struct ookpage *ookpage =
        region->get_memmap_entry(reinterpret_cast<void *>(addr));

    OOKBugOn(ookpage == nullptr);

    return ookpage;
  }

private:
  struct mem_location_t {
    struct OOKRegion *region;
    void *addr;
  };

  int insert_region(struct OOKRegion *region) {
    OOKBugOn(region == nullptr);

    if (!region->registered())
      return -EFAULT;

    const auto [addr, size] = region->allocatable_region();

    uintptr_t uaddr = reinterpret_cast<uintptr_t>(addr);
    uintptr_t uaddr_end = uaddr + size;

    return deallocate_impl(
        mem_location_t{
            .region = region,
            .addr = addr,
        },
        size);
  }

  int deallocate_impl(struct mem_location_t mem, size_t size) {
    OOKBugOn(mem.region == nullptr);
    OOKBugOn(mem.addr == nullptr);

    struct OOKRegion *region = mem.region;
    void *addr = mem.addr;

    const uintptr_t uaddr = reinterpret_cast<uintptr_t>(addr);
    const uintptr_t uaddr_end = uaddr + size;

    if (uaddr & PAGE_MASK)
      return -EINVAL;
    if (uaddr_end & PAGE_MASK)
      return -EINVAL;

    uintptr_t p;
    for (p = uaddr; p < uaddr_end;) {
      const unsigned int address_align_log2 = ctz(p);
      OOKBugOn(address_align_log2 < PAGE_SHIFT);

      const size_t remain_size = uaddr_end - p;
      const unsigned int remainder_align_log2 = log2floor(remain_size);
      OOKBugOn(remainder_align_log2 < PAGE_SHIFT);

      const unsigned int block_size_log2 = std::min(
          std::min(address_align_log2, remainder_align_log2) - PAGE_SHIFT,
          MAX_BLOCK_ORDER);

      OOKBugOn(!(block_size_log2 <= MAX_BLOCK_ORDER));

      struct ookpage *block =
          region->get_memmap_entry(reinterpret_cast<void *>(p));

      block->addr = reinterpret_cast<void *>(p);
      block->size = static_cast<size_t>(1) << (block_size_log2 + PAGE_SHIFT);
      block->list.reset();

      for (size_t i = 1; i < (1UL << block_size_log2); i++) {
        struct ookpage *block_next = &block[i];
        block_next->addr = nullptr;
        block_next->size =
            0; /* Only the leading page entry will hold the actual size. */
        block_next->list.reset();
      }

      deallocate_pow2(
          mem_location_t{
              .region = region,
              .addr = block->addr,
          },
          block_size_log2);

      p += static_cast<uintptr_t>(1) << (block_size_log2 + PAGE_SHIFT);
    }
    OOKBugOn(p != uaddr_end);

    return 0;
  }

  std::pair<int, unsigned int> find_region(void *addr) {
    if (addr == nullptr)
      return {-EINVAL, 0};

    for (unsigned int k = 0; k < std::size(registered_regions); ++k) {
      OOKRegion *region = &registered_regions[k];
      if (region->registered() && region->inrange(addr)) {
        return {0, k};
      }
    }

    return {-ENOENT, 0};
  }

  std::pair<int, struct mem_location_t> allocate_pow2(unsigned int order) {
    if (order > MAX_BLOCK_ORDER)
      return {-ENOMEM, {}};

    struct ookpage *ookpage = nullptr;
    unsigned int found_order = 0;

    for (unsigned int alloc_order = order; alloc_order <= MAX_BLOCK_ORDER;
         alloc_order++) {
      struct list_head *block = &blocks[alloc_order];
      if (!block->empty()) {
        struct list_head *alloc_chunk = block->next;
        alloc_chunk->remove();

        ookpage = container_of(alloc_chunk, struct ookpage, list);
        ookpage->list.remove();
        found_order = alloc_order;
        goto found_chunk;
      }
    }

    return {-ENOMEM, {}};

  found_chunk:
    void *addr = ookpage->addr;
    uintptr_t uaddr = reinterpret_cast<uintptr_t>(addr);
    unsigned int region_id;

    if (auto [ret, region_id_] = find_region(addr); ret)
      return {ret, {}};
    else
      region_id = region_id_;

    struct OOKRegion *region = &registered_regions[region_id];

    for (unsigned int i = found_order; i > order;) {
      --i;

      size_t half_size = 1UL << (i + PAGE_SHIFT);
      uintptr_t pair_uaddr = uaddr ^ (1UL << (i + PAGE_SHIFT));
      void *pair_addr = reinterpret_cast<void *>(pair_uaddr);
      struct ookpage *pair_ookpage = region->get_memmap_entry(pair_addr);

      pair_ookpage->addr = pair_addr;
      pair_ookpage->size = half_size;
      pair_ookpage->list.reset();

      blocks[i].add(&pair_ookpage->list);
    }

    ookpage->addr = nullptr; /* Not owned regions will be zeroed out. */
    ookpage->size = 0;
    ookpage->list.reset();

    return {0, mem_location_t{
                   .region = region,
                   .addr = addr,
               }};
  }

  void deallocate_pow2(struct mem_location_t mem, unsigned int order) {
    OOKBugOn(mem.region == nullptr);
    OOKBugOn(mem.addr == nullptr);
    OOKBugOn(order > MAX_BLOCK_ORDER);

    struct OOKRegion *region = mem.region;

    uintptr_t uaddr = reinterpret_cast<uintptr_t>(mem.addr);
    size_t size = 1UL << (order + PAGE_SHIFT);

    struct ookpage *ookpage = region->get_memmap_entry(mem.addr);

    OOKBugOn(uaddr & (size - 1));

    OOKBugOn(!ookpage->list.empty());
    ookpage->addr = nullptr;
    ookpage->size = 0;

    while (order < MAX_BLOCK_ORDER) {
      uintptr_t pair_uaddr = uaddr ^ size;

      if (!region->inrange(reinterpret_cast<void *>(pair_uaddr), size))
        break;

      struct ookpage *pair_ookpage =
          region->get_memmap_entry(reinterpret_cast<void *>(pair_uaddr));

      OOKBugOn(pair_ookpage->size > (1UL << (order + PAGE_SHIFT)));

      if (pair_ookpage->size != (1UL << (order + PAGE_SHIFT)))
        break;

      if (pair_ookpage->list.empty())
        break;

      pair_ookpage->list.remove();
      pair_ookpage->addr = nullptr;
      pair_ookpage->size = 0;

      if (uaddr > pair_uaddr) {
        std::swap(uaddr, pair_uaddr);
        std::swap(ookpage, pair_ookpage);
      }

      order++;
      size <<= 1;
    }

    ookpage->addr = reinterpret_cast<void *>(uaddr);
    ookpage->size = size;
    blocks[order].add(&ookpage->list);
  }

private:
  /**
   * Page allocation itself requires retrieving the region of memory,
   * so it is crucial that the regions are owned by the page allocator.
   */
  OOKRegion registered_regions[MAX_NUM_REGIONS];

  struct list_head blocks[1 + MAX_BLOCK_ORDER];
};

class OOKAllocator {
private:
  static constexpr unsigned int MAX_ALLOC_COUNT = 128;
  static constexpr unsigned int MIN_ALLOC_ORDER = 5;
  static constexpr unsigned int MAX_ALLOC_ORDER = PAGE_SHIFT - 1;
  static constexpr unsigned int VALID_ORDER_COUNT =
      MAX_ALLOC_ORDER - MIN_ALLOC_ORDER + 1;

  struct alloc_config_t {
    unsigned int block_order;
  };

  class alloc_helper_t {
  protected:
    static constexpr struct alloc_config_t ALLOC_CONFIG[VALID_ORDER_COUNT] = {
        {0}, {1}, {1}, {2}, {3}, {3}, {3},
    };

    constexpr alloc_helper_t(unsigned int alloc_order_,
                             const alloc_config_t &config_)
        : alloc_order(alloc_order_), config(config_) {}

  public:
    static constexpr const alloc_helper_t getByOrder(unsigned int alloc_order) {
      return alloc_helper_t(alloc_order,
                            ALLOC_CONFIG[alloc_order - MIN_ALLOC_ORDER]);
    }

    constexpr unsigned int get_alloc_order() { return alloc_order; }

    constexpr size_t chunk_size() const { return 1UL << alloc_order; }

    constexpr unsigned int block_order() const { return config.block_order; }

    constexpr unsigned int max_alloc_shift() const {
      return PAGE_SHIFT + config.block_order - alloc_order;
    }

    constexpr size_t max_alloc_count() const {
      return 1UL << max_alloc_shift();
    }

  private:
    const unsigned int alloc_order;
    const alloc_config_t config;
  };

  using alloc_state_t = int;
  enum : alloc_state_t {
    ALLOC_STATE_SATURATED = 1 << 0,
  };

  struct alloc_header_t {
    bitmap_t<MAX_ALLOC_COUNT> alloc_map;
    alloc_state_t state;

    void reset() {
      alloc_map.reset();
      state = 0;
    }
  };
  static_assert(MAX_ALLOC_COUNT % (sizeof(unsigned long) * CHAR_BIT) == 0);
  static_assert(sizeof(struct alloc_header_t) <= (1UL << MIN_ALLOC_ORDER));

public:
  int register_region(void *addr, size_t size) {
    return page_allocator.register_region(addr, size);
  }

  void *allocate(size_t size) {
    if (size == 0)
      return nullptr;

    if (size <= alloc_helper_t::getByOrder(MAX_ALLOC_ORDER).chunk_size()) {
      unsigned int order = log2ceil(size);
      if (order < MIN_ALLOC_ORDER)
        order = MIN_ALLOC_ORDER;
      return allocate_small(order);
    } else {
      size_t size_page_aligned = (size + (PAGE_SIZE - 1)) & ~PAGE_MASK;
      auto [ret, page] = page_allocator.allocate(size_page_aligned);
      if (ret)
        return nullptr;

      return page->addr;
    }
  }

  void deallocate(void *addr) {
    if (addr == nullptr)
      return;

    struct ookpage *addr_ookpage = page_allocator.page_to_desc(addr);
    OOKBugOn(addr_ookpage == nullptr);

    size_t size = addr_ookpage->size;
    if (size <= alloc_helper_t::getByOrder(MAX_ALLOC_ORDER).chunk_size()) {
      unsigned int order = log2ceil(size);

      OOKBugOn(order < MIN_ALLOC_ORDER);
      OOKBugOn(size != alloc_helper_t::getByOrder(order).chunk_size());

      deallocate_small(addr, addr_ookpage);
    } else {
      OOKBugOn(size & PAGE_MASK);
      OOKBugOn(addr_ookpage->addr != addr);

      page_allocator.deallocate(addr_ookpage);
    }
  }

private:
  void *allocate_small(unsigned int order) {
    OOKBugOn(order < MIN_ALLOC_ORDER);

    const alloc_helper_t config = alloc_helper_t::getByOrder(order);
    const size_t chunk_size = config.chunk_size();

    struct ookpage *ookpage = nullptr;
    void *alloc_block = nullptr;
    if (get_free_blocks(order).empty()) {

      auto [ret, new_block_ookpage] =
          page_allocator.allocate(1UL << (PAGE_SHIFT + config.block_order()));
      if (ret)
        return nullptr;

      ookpage = new_block_ookpage;
      void *addr = ookpage->addr;

      for (size_t i = 0; i < (1UL << config.block_order()); ++i) {
        ookpage[i].addr = addr;
        ookpage[i].size = chunk_size;
      }

      get_free_blocks(order).add(&ookpage->list);
      alloc_block = addr;

      struct alloc_header_t *header =
          static_cast<struct alloc_header_t *>(addr);
      header->reset();

      /* Set bit 0 for metadata usage. */
      header->alloc_map.set_bit(0);
    } else {
      struct ookpage *block =
          (struct ookpage *)((char *)get_free_blocks(order).next -
                             offsetof(struct ookpage, list));

      alloc_block = block->addr;
      ookpage = page_allocator.page_to_desc(alloc_block);
    }

    struct alloc_header_t *header =
        static_cast<struct alloc_header_t *>(alloc_block);

    size_t alloc_index = header->alloc_map.get_lowest_free_index();
    OOKBugOn(alloc_index >= config.max_alloc_count());

    header->alloc_map.set_bit(alloc_index);

    if (header->alloc_map.get_lowest_free_index() >= config.max_alloc_count()) {
      header->state |= ALLOC_STATE_SATURATED;
      ookpage->list.remove();
    }

    return (void *)((char *)alloc_block + (alloc_index << order));
  }

  void deallocate_small(void *addr, struct ookpage *addr_ookpage) {
    unsigned int order = log2ceil(addr_ookpage->size);
    const alloc_helper_t config = alloc_helper_t::getByOrder(order);

    void *addr_block = addr_ookpage->addr;
    struct ookpage *ookpage = page_allocator.page_to_desc(addr_block);

    struct alloc_header_t *header =
        static_cast<struct alloc_header_t *>(addr_block);

    if (header->state & ALLOC_STATE_SATURATED) {
      header->state &= ~ALLOC_STATE_SATURATED;

      get_free_blocks(order).add(&ookpage->list);
    }

    size_t dealloc_index = (reinterpret_cast<uintptr_t>(addr) -
                            reinterpret_cast<uintptr_t>(addr_block)) >>
                           order;
    OOKBugOn((reinterpret_cast<uintptr_t>(addr) -
              reinterpret_cast<uintptr_t>(addr_block)) &
             ((1UL << order) - 1));
    OOKBugOn(dealloc_index >= config.max_alloc_count());

    header->alloc_map.clear_bit(dealloc_index);

    if (header->alloc_map.get_bit(0) && header->alloc_map.get_popcount() == 1) {
      header->alloc_map.clear_bit(0);

      ookpage->list.remove();

      for (size_t i = 1; i < (1UL << config.block_order()); ++i) {
        ookpage[i].addr = nullptr;
        ookpage[i].size = 0;
      }

      /* ookpage->addr should not change. */
      ookpage->size = static_cast<size_t>(1)
                      << (PAGE_SHIFT + config.block_order());

      page_allocator.deallocate(ookpage);
    }
  }

private:
  struct list_head &get_free_blocks(unsigned int order) {
    return free_blocks[order - MIN_ALLOC_ORDER];
  }

  PageAllocator page_allocator;

  struct list_head free_blocks[VALID_ORDER_COUNT];
};
} // namespace

#ifdef OOK_STANDALONE
// clang++ --std=c++17 -o simplealloc simplealloc.cpp -ggdb3 -DOOK_STANDALONE -DOOK_SANITIZE && ./simplealloc
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <random>
#include <stdexcept>

void testLinkedList() {
  struct tag_list_t {
    int tag;
    struct list_head list;
  };

  std::mt19937 rng(0x117);

  constexpr unsigned int MAX_NUM_TAGGED_LIST_ELEMENTS = 0x100;
  std::vector<tag_list_t> tagged_list_elements(MAX_NUM_TAGGED_LIST_ELEMENTS);
  for (int i = 0; i < MAX_NUM_TAGGED_LIST_ELEMENTS; ++i) {
    struct tag_list_t *e = &tagged_list_elements[i];

    e->tag = i;
    e->list.reset();
  }

  std::vector<unsigned int> retrieve_order;
  retrieve_order.reserve(MAX_NUM_TAGGED_LIST_ELEMENTS);
  for (size_t num_elements = 1; num_elements <= MAX_NUM_TAGGED_LIST_ELEMENTS;
       ++num_elements) {
    struct list_head head;
    head.reset();

    for (int i = 0; i < MAX_NUM_TAGGED_LIST_ELEMENTS; ++i) {
      struct tag_list_t *e = &tagged_list_elements[i];

      e->tag = i;
      e->list.reset();

      if (i & 1)
        head.add(&e->list);
      else
        head.add_tail(&e->list);
    }

    retrieve_order.resize(num_elements);
    std::iota(retrieve_order.begin(), retrieve_order.end(), 0);

    std::shuffle(retrieve_order.begin(), retrieve_order.end(), rng);

    for (unsigned int t : retrieve_order) {
      tagged_list_elements[t].list.remove();
    }
  }
}

class TestRegionManager {
private:
  static constexpr unsigned int kTestRegionBaseShift = 40U;
  static constexpr size_t kTestRegionBaseSize = 1UL << kTestRegionBaseShift;

  static constexpr std::pair<uintptr_t, size_t> kTestRegions[] = {
      {0x5d00900000, 0x100000},
      {0x5d30050000, 0x80000},
      {0x5d74500000, 0x570000},
      {0x5d97900000, 0x530000},
  };

  static constexpr std::pair<uint8_t, size_t> kTestSpareRegion = {0x5dc0000000,
                                                                  0x20000000};

public:
  void setup() {
    void *p = mmap(nullptr, 2 * kTestRegionBaseSize, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
      throw std::runtime_error("Failed to reserve region for test.");
    }

    uintptr_t pu = reinterpret_cast<uintptr_t>(p);
    uintptr_t baseu =
        (pu + (kTestRegionBaseSize - 1)) & ~(kTestRegionBaseSize - 1);

    if (pu < baseu) {
      if (int ret = munmap(p, baseu - pu); ret) {
        fprintf(stderr,
                "Failed to return spare region in head. Ignoring. [ret = %d]",
                ret);
      }
    }

    if (baseu + kTestRegionBaseSize < pu + 2 * kTestRegionBaseSize) {
      if (int ret = munmap(
              reinterpret_cast<void *>(baseu + kTestRegionBaseSize),
              (pu + 2 * kTestRegionBaseSize) - (baseu + kTestRegionBaseSize));
          ret) {
        fprintf(stderr,
                "Failed to return spare region in tail. Ignoring. [ret = %d]",
                ret);
      }
    }

    base = baseu;

    for (int regionno = 0; regionno < std::size(kTestRegions); ++regionno) {
      const auto [addr, size] = getTestRegion(regionno);

      int ret = mprotect(reinterpret_cast<void *>(addr), size,
                         PROT_READ | PROT_WRITE);
      if (ret) {
        fprintf(stderr,
                "Failed to activate region. [region no = %d, addr = 0x%zx]",
                regionno, static_cast<size_t>(addr));
        abort();
      }
      printf(
          "Successfully activated region. [region no = %d, addr = 0x%zx, size = 0x%zx]\n",
          regionno, static_cast<size_t>(addr), size);
    }

    {
      const auto [addr, size] = getTestSpareRegion();

      int ret = mprotect(reinterpret_cast<void *>(addr), size,
                         PROT_READ | PROT_WRITE);
      if (ret) {
        fprintf(stderr, "Failed to activate spare region. [addr = 0x%zx]",
                static_cast<size_t>(addr));
        abort();
      }
      printf(
          "Successfully activated spare region. [addr = 0x%zx, size = 0x%zx]\n",
          static_cast<size_t>(addr), size);
    }
  }

  void clear() {
    if (base == 0)
      return;

    for (int regionno = 0; regionno < std::size(kTestRegions); ++regionno) {
      const auto [addr, size] = getTestRegion(regionno);

      memset(reinterpret_cast<void *>(addr), 0, size);
    }
  }

  size_t getTestRegionCount() const { return std::size(kTestRegions); }

  std::pair<uintptr_t, size_t> getTestRegion(int regionno) const {
    const auto [addr_offset, size] = kTestRegions[regionno];
    return {base | addr_offset, size};
  }

  std::pair<uintptr_t, size_t> getTestSpareRegion() const {
    const auto [addr_offset, size] = kTestSpareRegion;
    return {base | addr_offset, size};
  }

private:
  uintptr_t base = 0;
};

struct allocatable_status_t {
  size_t total_size;
  std::array<unsigned long, 1 + MAX_BLOCK_ORDER> counts;
};

struct allocatable_status_t getAllocatablePageSize(OOKAllocator &allocator) {
  struct allocatable_status_t status = {};

  std::vector<void *> allocs;

  for (int order = MAX_BLOCK_ORDER; order >= 0; --order) {
    const size_t size = 1UL << (PAGE_SHIFT + order);
    while (true) {
      void *p = allocator.allocate(size);
      if (p == nullptr)
        break;

      allocs.emplace_back(p);

      status.total_size += size;
      status.counts[order]++;
    }
  }

  for (const auto p : allocs)
    allocator.deallocate(p);

  return status;
}

void testRegionRegistration(const TestRegionManager &test_region_manager) {
  {
    OOKAllocator allocator;

    for (int regionno = 0; regionno < test_region_manager.getTestRegionCount();
         ++regionno) {
      const auto [addr, size] = test_region_manager.getTestRegion(regionno);

      int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
      assert(!ret);
    }

    {
      const auto [addr, size] = test_region_manager.getTestSpareRegion();

      int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
      assert(ret == -ENOMEM);
    }
  }

  std::mt19937 rng(0x5ac);
  for (int mask = 1; mask < (1 << test_region_manager.getTestRegionCount());
       ++mask) {
    OOKAllocator allocator;
    size_t allocatable_size = 0;

    for (int regionno = 0; regionno < test_region_manager.getTestRegionCount();
         ++regionno) {
      if (!(mask & (1 << regionno)))
        continue;

      const auto [addr, size] = test_region_manager.getTestRegion(regionno);

      int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
      assert(!ret);

      allocatable_size += size - OOKRegion::calc_memmap_size(size);
    }

    std::vector<void *> allocs;

    for (long allocno = 0;; allocno++) {
      void *p = allocator.allocate(PAGE_SIZE);

      if (p == nullptr)
        break;

      std::independent_bits_engine<decltype(rng), CHAR_BIT, char> randbits(rng);
      std::generate(reinterpret_cast<char *>(p),
                    reinterpret_cast<char *>(p) + PAGE_SIZE, randbits);

      allocs.push_back(p);
    }

    assert(allocs.size() << PAGE_SHIFT == allocatable_size);

    std::shuffle(allocs.begin(), allocs.end(), rng);

    for (void *p : allocs)
      allocator.deallocate(p);
  }
}

void testAllocatorLargeAllocationFailure(
    const TestRegionManager &test_region_manager) {
  OOKAllocator allocator;
  size_t allocatable_size = 0;

  {
    const auto [addr, size] = test_region_manager.getTestSpareRegion();

    int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
    assert(!ret);

    allocatable_size += size - OOKRegion::calc_memmap_size(size);
  }

  {
    void *p = allocator.allocate(1UL << (PAGE_SHIFT + MAX_BLOCK_ORDER));
    assert(p != nullptr);
    allocator.deallocate(p);
  }

  {
    void *p = allocator.allocate((1UL << (PAGE_SHIFT + MAX_BLOCK_ORDER)) + 1);
    assert(p == nullptr);
    allocator.deallocate(p);
  }
}

void testAllocatorResilience(const TestRegionManager &test_region_manager) {

  std::mt19937 rng(0x5bc);
  for (int mask = 1; mask < (1 << test_region_manager.getTestRegionCount());
       ++mask) {
    OOKAllocator allocator;
    size_t allocatable_size = 0;

    for (int regionno = 0; regionno < test_region_manager.getTestRegionCount();
         ++regionno) {
      if (!(mask & (1 << regionno)))
        continue;

      const auto [addr, size] = test_region_manager.getTestRegion(regionno);

      int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
      assert(!ret);

      allocatable_size += size - OOKRegion::calc_memmap_size(size);
    }

    using memchunk_t = std::pair<void *, size_t>;
    std::vector<memchunk_t> allocs;
    std::vector<unsigned long> original_counts(1 + MAX_BLOCK_ORDER, 0);

    for (int order = MAX_BLOCK_ORDER; order >= 0; --order) {
      const size_t size = 1UL << (PAGE_SHIFT + order);
      while (true) {
        void *p = allocator.allocate(size);
        if (p == nullptr)
          break;

        std::independent_bits_engine<decltype(rng), CHAR_BIT, char> randbits(
            rng);
        std::generate(reinterpret_cast<char *>(p),
                      reinterpret_cast<char *>(p) + size, randbits);

        allocs.emplace_back(p, size);
        original_counts[order]++;
      }
    }

    assert(std::accumulate(allocs.begin(), allocs.end(),
                           static_cast<size_t>(0UL),
                           [](size_t acc, const memchunk_t &x) -> size_t {
                             return acc + x.second;
                           }) == allocatable_size);

    std::shuffle(allocs.begin(), allocs.end(), rng);

    for (const auto [p, size] : allocs)
      allocator.deallocate(p);
    allocs.clear();

    {
      std::vector<void *> alloc_pages;

      while (true) {
        void *p = allocator.allocate(PAGE_SIZE);
        if (p == nullptr)
          break;

        std::independent_bits_engine<decltype(rng), CHAR_BIT, char> randbits(
            rng);
        std::generate(reinterpret_cast<char *>(p),
                      reinterpret_cast<char *>(p) + PAGE_SIZE, randbits);

        alloc_pages.emplace_back(p);
      }

      std::shuffle(allocs.begin(), allocs.end(), rng);

      for (const auto p : alloc_pages)
        allocator.deallocate(p);
      alloc_pages.clear();
    }

    std::vector<unsigned long> new_counts(1 + MAX_BLOCK_ORDER, 0);
    for (int order = MAX_BLOCK_ORDER; order >= 0; --order) {
      const size_t size = 1UL << (PAGE_SHIFT + order);
      while (true) {
        void *p = allocator.allocate(size);
        if (p == nullptr)
          break;

        allocs.emplace_back(p, size);
        new_counts[order]++;
      }
    }

    assert(std::accumulate(allocs.begin(), allocs.end(),
                           static_cast<size_t>(0UL),
                           [](size_t acc, const memchunk_t &x) -> size_t {
                             return acc + x.second;
                           }) == allocatable_size);
    assert(original_counts == new_counts);

    for (const auto [p, size] : allocs)
      allocator.deallocate(p);
    allocs.clear();
  }
}

void testUniformSmallAllocation(const TestRegionManager &test_region_manager) {

  std::mt19937 rng(0x51c);

  OOKAllocator allocator;
  size_t allocatable_size = 0;

  for (int regionno = 0; regionno < test_region_manager.getTestRegionCount();
       ++regionno) {
    const auto [addr, size] = test_region_manager.getTestRegion(regionno);

    int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
    assert(!ret);

    allocatable_size += size - OOKRegion::calc_memmap_size(size);
  }

  const auto original_status = getAllocatablePageSize(allocator);
  assert(allocatable_size == original_status.total_size);

  for (unsigned int order = 0; order < PAGE_SHIFT; ++order) {
    std::vector<void *> allocs;
    while (true) {
      void *p = allocator.allocate(1UL << order);
      if (p == nullptr)
        break;

      allocs.push_back(p);
    }

    const auto exhausted_status = getAllocatablePageSize(allocator);
    for (unsigned int order = 3; order < MAX_BLOCK_ORDER; ++order)
      assert(exhausted_status.counts[order] == 0);

    std::shuffle(allocs.begin(), allocs.end(), rng);

    for (const auto p : allocs)
      allocator.deallocate(p);
    allocs.clear();

    const auto new_status = getAllocatablePageSize(allocator);

    assert(original_status.total_size == new_status.total_size);
    assert(original_status.counts == new_status.counts);
  }
}

void testRandomSizeSmallAllocation(
    const TestRegionManager &test_region_manager) {

  constexpr unsigned int SMALL_ALLOCATION_TEST_ORDER_BASE = 5;
  constexpr unsigned long SMALL_ALLOCATION_TEST_COUNT[] = {
      0x2000,
      0x1000,
      0x0800,
      0x0400,
  };

  std::mt19937 rng(0x58c);
  std::uniform_int_distribution<size_t> US(0, PAGE_SIZE);

  OOKAllocator allocator;
  size_t allocatable_size = 0;

  for (int regionno = 0; regionno < test_region_manager.getTestRegionCount();
       ++regionno) {
    const auto [addr, size] = test_region_manager.getTestRegion(regionno);

    int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
    assert(!ret);

    allocatable_size += size - OOKRegion::calc_memmap_size(size);
  }

  const auto original_status = getAllocatablePageSize(allocator);
  assert(allocatable_size == original_status.total_size);

  using memchunk_t = std::pair<void *, size_t>;
  std::vector<memchunk_t> allocs;
  for (unsigned int order_offset = 0;
       order_offset < std::size(SMALL_ALLOCATION_TEST_COUNT); ++order_offset) {
    const unsigned int order = SMALL_ALLOCATION_TEST_ORDER_BASE + order_offset;
    const size_t random_size = US(rng) & ((1UL << order) - 1);

    allocs.emplace_back(nullptr, random_size);
  }

  std::shuffle(allocs.begin(), allocs.end(), rng);

  for (auto &[p, size] : allocs) {
    p = allocator.allocate(size);
    assert(p != nullptr);
  }

  std::shuffle(allocs.begin(), allocs.end(), rng);

  for (const auto [p, size] : allocs)
    allocator.deallocate(p);
  allocs.clear();

  const auto new_status = getAllocatablePageSize(allocator);

  assert(original_status.total_size == new_status.total_size);
  assert(original_status.counts == new_status.counts);
}

void testRandomSizeAllocation(const TestRegionManager &test_region_manager) {

  constexpr unsigned int SMALL_ALLOCATION_TEST_ORDER_BASE = 5;
  constexpr unsigned long SMALL_ALLOCATION_TEST_COUNT[] = {
      0x2000,
      0x1000,
      0x0800,
      0x0400,
  };

  constexpr unsigned int LARGE_ALLOCATION_TEST_ORDER_BASE = 12;
  constexpr unsigned long LARGE_ALLOCATION_TEST_COUNT[] = {
      0x20,
      0x10,
      0x08,
      0x04,
  };

  std::mt19937 rng(0x58c);
  std::uniform_int_distribution<size_t> US(0, PAGE_SIZE);

  OOKAllocator allocator;
  size_t allocatable_size = 0;

  for (int regionno = 0; regionno < test_region_manager.getTestRegionCount();
       ++regionno) {
    const auto [addr, size] = test_region_manager.getTestRegion(regionno);

    int ret = allocator.register_region(reinterpret_cast<void *>(addr), size);
    assert(!ret);

    allocatable_size += size - OOKRegion::calc_memmap_size(size);
  }

  const auto original_status = getAllocatablePageSize(allocator);
  assert(allocatable_size == original_status.total_size);

  using memchunk_t = std::pair<void *, size_t>;
  std::vector<memchunk_t> allocs;
  for (unsigned int order_offset = 0;
       order_offset < std::size(SMALL_ALLOCATION_TEST_COUNT); ++order_offset) {
    const unsigned int order = SMALL_ALLOCATION_TEST_ORDER_BASE + order_offset;
    const size_t random_size = US(rng) & ((1UL << order) - 1);

    allocs.emplace_back(nullptr, random_size);
  }
  for (unsigned int order_offset = 0;
       order_offset < std::size(LARGE_ALLOCATION_TEST_COUNT); ++order_offset) {
    const unsigned int order = LARGE_ALLOCATION_TEST_ORDER_BASE + order_offset;
    const size_t random_size = US(rng) & ((1UL << order) - 1);

    allocs.emplace_back(nullptr, random_size);
  }

  std::shuffle(allocs.begin(), allocs.end(), rng);

  for (auto &[p, size] : allocs) {
    p = allocator.allocate(size);
    assert(p != nullptr);
  }

  std::shuffle(allocs.begin(), allocs.end(), rng);

  for (const auto [p, size] : allocs)
    allocator.deallocate(p);
  allocs.clear();

  const auto new_status = getAllocatablePageSize(allocator);

  assert(original_status.total_size == new_status.total_size);
  assert(original_status.counts == new_status.counts);
}

int main() {
  testLinkedList();

  TestRegionManager test_region_manager;
  test_region_manager.setup();

  for (auto test_cb : {
           testRegionRegistration,
           testAllocatorLargeAllocationFailure,
           testAllocatorResilience,
           testUniformSmallAllocation,
           testRandomSizeSmallAllocation,
           testRandomSizeAllocation,
       }) {
    test_region_manager.clear();
    test_cb(test_region_manager);
  }
}
#endif /* OOK_STANDALONE */