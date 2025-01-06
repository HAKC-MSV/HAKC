/* All this code is from virt/kvm/kvm_main.c and included files */

typedef unsigned char bool;
typedef unsigned long gfn_t;
typedef unsigned long gpa_t;
typedef struct {
    int counter;
} atomic_t;

struct kvm_rmap_head;
struct kvm_lpage_info;

#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)

#define PAGE_SHIFT		12
#define PAGE_SIZE		(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

#define __PAGE_OFFSET_BASE_L4	_AC(0xffff888000000000, UL)
#define __PAGE_OFFSET           __PAGE_OFFSET_BASE_L4
#define PAGE_OFFSET		((unsigned long)__PAGE_OFFSET)

#define offset_in_page(p)	((unsigned long)(p) & ~PAGE_MASK)

#define KVM_MEMSLOT_INVALID	(1UL << 16)
#define KVM_HVA_ERR_BAD		(PAGE_OFFSET)
#define KVM_HVA_ERR_RO_BAD	(PAGE_OFFSET + PAGE_SIZE)

#define BITS_PER_LONG 64

#define KVM_MEM_READONLY	(1UL << 1)

#define NULL                (void*)0

#define true                (bool)1

static inline unsigned long array_index_mask_nospec(unsigned long index,
                                                    unsigned long size)
{
    return ~(long)(index | (size - 1UL - index)) >> (BITS_PER_LONG - 1);
}

#define array_index_nospec(index, size)					\
({									\
	typeof(index) _i = (index);					\
	typeof(size) _s = (size);					\
	unsigned long _mask = array_index_mask_nospec(_i, _s);		\
	(typeof(_i)) (_i & _mask);					\
})

struct kvm_arch_memory_slot {
    struct kvm_rmap_head *rmap[5];
    struct kvm_lpage_info *lpage_info[5 - 1];
    unsigned short *gfn_track[5];
};

struct kvm_memory_slot {
    gfn_t base_gfn;
    unsigned long npages;
    unsigned long *dirty_bitmap;
    struct kvm_arch_memory_slot arch;
    unsigned long userspace_addr;
    unsigned int flags;
    short id;
    unsigned short as_id;
};

struct kvm_memslots {
    unsigned long generation;
    /* The mapping table from slot id to the index in memslots[]. */
    short id_to_index[5];
    atomic_t last_used_slot;
    int used_slots;
    struct kvm_memory_slot memslots[];
};

struct gfn_to_hva_cache {
    unsigned long generation;
    gpa_t gpa;
    unsigned long hva;
    unsigned long len;
    struct kvm_memory_slot *memslot;
};

static int
atomic_read(const atomic_t *v)
{
    return v->counter;
}

static void
atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline unsigned long
__gfn_to_hva_memslot(const struct kvm_memory_slot *slot, gfn_t gfn)
{
    /*
     * The index was checked originally in search_memslots.  To avoid
     * that a malicious guest builds a Spectre gadget out of e.g. page
     * table walks, do not let the processor speculate loads outside
     * the guest's registered memslots.
     */
    unsigned long offset = gfn - slot->base_gfn;
    offset = array_index_nospec(offset, slot->npages);
    return slot->userspace_addr + offset * PAGE_SIZE;
}

static bool memslot_is_readonly(struct kvm_memory_slot *slot)
{
    return slot->flags & KVM_MEM_READONLY;
}

static unsigned long __gfn_to_hva_many(struct kvm_memory_slot *slot, gfn_t gfn,
                                       gfn_t *nr_pages, bool write)
{
    if (!slot || slot->flags & KVM_MEMSLOT_INVALID)
        return KVM_HVA_ERR_BAD;

    if (memslot_is_readonly(slot) && write)
        return KVM_HVA_ERR_RO_BAD;

    if (nr_pages)
        *nr_pages = slot->npages - (gfn - slot->base_gfn);

    return __gfn_to_hva_memslot(slot, gfn);
}

static unsigned long gfn_to_hva_many(struct kvm_memory_slot *slot, gfn_t gfn,
                                     gfn_t *nr_pages)
{
    return __gfn_to_hva_many(slot, gfn, nr_pages, true);
}

static inline struct kvm_memory_slot *
try_get_memslot(struct kvm_memslots *slots, int slot_index, gfn_t gfn)
{
    struct kvm_memory_slot *slot;

    if (slot_index < 0 || slot_index >= slots->used_slots)
        return NULL;

    /*
     * slot_index can come from vcpu->last_used_slot which is not kept
     * in sync with userspace-controllable memslot deletion. So use nospec
     * to prevent the CPU from speculating past the end of memslots[].
     */
    slot_index = array_index_nospec(slot_index, slots->used_slots);
    slot = &slots->memslots[slot_index];

    if (gfn >= slot->base_gfn && gfn < slot->base_gfn + slot->npages)
        return slot;
    else
        return NULL;
}

static inline struct kvm_memory_slot *
search_memslots(struct kvm_memslots *slots, gfn_t gfn, int *index)
{
    int start = 0, end = slots->used_slots;
    struct kvm_memory_slot *memslots = slots->memslots;
    struct kvm_memory_slot *slot;

    if (!slots->used_slots)
        return NULL;

    while (start < end) {
        int slot = start + (end - start) / 2;

        if (gfn >= memslots[slot].base_gfn)
            end = slot;
        else
            start = slot + 1;
    }

    slot = try_get_memslot(slots, start, gfn);
    if (slot) {
        *index = start;
        return slot;
    }

    return NULL;
}

static inline struct kvm_memory_slot *
__gfn_to_memslot(struct kvm_memslots *slots, gfn_t gfn)
{
    struct kvm_memory_slot *slot;
    int slot_index = atomic_read(&slots->last_used_slot);

    slot = try_get_memslot(slots, slot_index, gfn);
    if (slot)
        return slot;

    slot = search_memslots(slots, gfn, &slot_index);
    if (slot) {
        atomic_set(&slots->last_used_slot, slot_index);
        return slot;
    }

    return NULL;
}

static inline bool kvm_is_error_hva(unsigned long addr)
{
    return addr >= PAGE_OFFSET;
}

int __kvm_gfn_to_hva_cache_init(struct kvm_memslots *slots,
          struct gfn_to_hva_cache *ghc,
          gpa_t gpa, unsigned long len) {
    int offset = offset_in_page(gpa);
    gfn_t start_gfn = gpa >> PAGE_SHIFT;
    gfn_t end_gfn = (gpa + len - 1) >> PAGE_SHIFT;
    gfn_t nr_pages_needed = end_gfn - start_gfn + 1;
    gfn_t nr_pages_avail;

    /* Update ghc->generation before performing any error checks. */
    ghc->generation = slots->generation;

    if (start_gfn > end_gfn) {
        ghc->hva = -1;
        return -22;
    }

    /*
     * If the requested region crosses two memslots, we still
     * verify that the entire region is valid here.
     */
    for ( ; start_gfn <= end_gfn; start_gfn += nr_pages_avail) {
        ghc->memslot = __gfn_to_memslot(slots, start_gfn);
        ghc->hva = gfn_to_hva_many(ghc->memslot, start_gfn,
                                   &nr_pages_avail);
        if (kvm_is_error_hva(ghc->hva))
            return -14;
    }

    /* Use the slow path for cross page reads and writes. */
    if (nr_pages_needed == 1)
        ghc->hva += offset;
    else
        ghc->memslot = 0;

    ghc->gpa = gpa;
    ghc->len = len;
    return 0;
}
