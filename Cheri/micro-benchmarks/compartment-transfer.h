//
// Created by de29664 on 12/12/24.
//

#ifndef COMPARTMENT_TRANSFER_H
#define COMPARTMENT_TRANSFER_H

#ifndef BUFFER_SIZE
#error "Buffer size must be greater than 0"
#endif

#include <stdio.h>

struct test {
    int i;
    void (*func)(void *);
    char str[BUFFER_SIZE];
};

void init_hakc();

unsigned long long compartment_transfer_count = 0;

#ifdef CHERI_TEST
#include <cheriintrin.h>
#include <sys/auxv.h>

void * __capability compartment_1_sealcap;
void * __capability compartment_2_sealcap;

#define compartment_1_validator compartment_1_sealcap
#define compartment_2_validator compartment_2_sealcap
#define compartment_1_protector compartment_1_sealcap
#define compartment_2_protector compartment_2_sealcap

#define print_info(data) printf("[%s] %s: %#p, sealed %d, valid: %d\n", __FUNCTION__, #data, data, \
                                     cheri_is_sealed(data), cheri_is_valid(data))

#define protect_ptr(data, sealer) cheri_seal(data, sealer)

#define validate_ptr(data, sealer) cheri_unseal(data, sealer);

#else

typedef unsigned char u8;
typedef unsigned long long u64;
typedef unsigned char bool;
typedef unsigned int u32;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef u64 uintptr_t;

const uint32_t H_0[8] = {
    0x6a09e667,
    0xbb67ae85,
    0x3c6ef372,
    0xa54ff53a,
    0x510e527f,
    0x9b05688c,
    0x1f83d9ab,
    0x5be0cd19,
};

typedef u32 hakc_compartment_id_t;
typedef u64 pac_salt_t;
typedef u64 clique_access_tok_t;

#define true 1
#define false 0

struct tag_node {
    struct tag_node *left;
    struct tag_node *right;
    unsigned long safe_addr;
    unsigned long len;
    u8 color;
};

#define HAKC_KADDR(ADDR) (void *)(~(0xFFFF000000000000) & (u64)(ADDR))

#define HAKC_COLOR_BIT_COUNT 4

#define COLOR_GRANULARITY (1 << HAKC_COLOR_BIT_COUNT)

typedef enum {
    SILVER_CLIQUE,
    GREEN_CLIQUE,
    RED_CLIQUE,
    ORANGE_CLIQUE,
    YELLOW_CLIQUE,
    PURPLE_CLIQUE,
    BLUE_CLIQUE,
    GREY_CLIQUE,
    PINK_CLIQUE,
    BROWN_CLIQUE,
    WHITE_CLIQUE,
    BLACK_CLIQUE,
    TEAL_CLIQUE,
    VIOLET_CLIQUE,
    CRIMSON_CLIQUE,
    GOLD_CLIQUE,
    START_CLIQUE = SILVER_CLIQUE,
    END_CLIQUE = START_CLIQUE + COLOR_GRANULARITY,
    INVALID_CLIQUE = END_CLIQUE
} clique_color_t;

#define HAKC_UPPER_BIT_MASK (0x0000000000000000)

#define HAKC_ADDRESS_BITS 48

#define __round_mask(x, y) ((__typeof__(x))((y)-1))

#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)

#define round_down(x, y) ((x) & ~__round_mask(x, y))

#define VALID_COLOR(color) ((color) >= START_CLIQUE && (color) < END_CLIQUE)

#define HAKC_COLOR_BIT_COUNT 4
#define CLAQUE_ID_BIT_COUNT (64 - HAKC_COLOR_BIT_COUNT)

#define HAKC_MASK_COLOR(COLOR) (1 << (COLOR - START_CLIQUE))

#define HAKC_CONTEXT(CLAQUE_ID, COLOR_MASK, TYPE)                               \
(((TYPE)(CLAQUE_ID) << COLOR_GRANULARITY) | (COLOR_MASK))

#define PAC_MODIFIER(CLAQUE_ID, COLOR_MASK)                                    \
HAKC_CONTEXT(CLAQUE_ID, COLOR_MASK, pac_salt_t)

#define VALID_COMPARTMENT(compartment)                                       \
((compartment) > 0 && (compartment) < ((1ul << CLAQUE_ID_BIT_COUNT) - 1))

#define KERN_CLAQUE_BIT_MASK (0xFFFFFFFFFFF00000)
#define HAKC_CONTEXT_ADDR(ADDR) ((u64)(ADDR)&KERN_CLAQUE_BIT_MASK)
#define HAKC_COMPARTMENT_ADDR(ADDR) ((u64)(ADDR) & ~KERN_CLAQUE_BIT_MASK)

static inline void *hakc_safe_ptr(unsigned long addr)
{
    if (!addr) {
        return (void *)addr;
    }
    return (void *)((unsigned long)HAKC_KADDR(addr) | HAKC_UPPER_BIT_MASK);
}

static inline bool addr_is_signed(const void *ptr)
{
    unsigned long p = (unsigned long)ptr;
    unsigned int upper_bits = (p >> HAKC_ADDRESS_BITS);
    return (upper_bits > 0 && upper_bits != 0xFFFF);
}

static inline bool is_userspace_addr(const void *addr)
{
    /* Bits 48:63 are one for kernel addresses */
    return ((unsigned long)1 << HAKC_ADDRESS_BITS) > (unsigned long)addr;
}

#define HAKC_GET_SAFE_PTR(ptr) ((typeof(ptr))hakc_safe_ptr((unsigned long)(ptr)))

static struct tag_node *root;

bool hakc_initialized;

clique_color_t get_hakc_address_color(const void *addr);

void *check_hakc_data_access(const void *address,
                hakc_compartment_id_t compartment,
                const clique_access_tok_t access_tok);

void *hakc_transfer_to_clique(void *data_to_transfer, size_t size,
                hakc_compartment_id_t compartment,
                clique_color_t color, bool is_code);

#define print_info(data) printf("[%s] %s: %p, color %d\n", __FUNCTION__, #data, data, get_hakc_address_color(data))

#define compartment_validator(id) id, (id << COLOR_GRANULARITY) | (1 << id)
#define compartment_protector(id) id, (SILVER_CLIQUE + id)

#define compartment_1_validator compartment_validator(1)
#define compartment_2_validator compartment_validator(2)
#define compartment_1_protector compartment_protector(1)
#define compartment_2_protector compartment_protector(2)

#define validate_ptr(ptr, validator) (typeof(ptr))check_hakc_data_access(ptr, validator)
#define protect_ptr(ptr, protector) (typeof(ptr))hakc_transfer_to_clique(ptr, sizeof(*ptr), protector, false)

void sha256_block_neon(u32 *digest, const void *data, unsigned int num_blks);

#endif


#endif //COMPARTMENT_TRANSFER_H
