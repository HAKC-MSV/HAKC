//
// Created by de29664 on 12/12/24.
//

#include "compartment-transfer.h"
#include <stdlib.h>
#include <string.h>

#ifdef CHERI_TEST

void * __capability get_system_sealer(char id) {
	void * __capability sealcap;
	sealcap = getauxptr(AT_CHERI_SEAL_CAP);
	return sealcap + 0x100 + id;
}

#else
unsigned int OPENSSL_armcap_P;

struct tag_node *insert(struct tag_node *root, unsigned long safe_addr,
			unsigned long len, u8 color)
{
	if (!root) {
		struct tag_node *new_tag_node =
			(struct tag_node *)malloc(sizeof(struct tag_node));
		new_tag_node->safe_addr = safe_addr;
		new_tag_node->len = len;
		new_tag_node->color = color;
		new_tag_node->left = NULL;
		new_tag_node->right = NULL;
		return new_tag_node;
	} else if (safe_addr > root->safe_addr) {
		root->right = insert(root->right, safe_addr, len, color);
	} else if (safe_addr < root->safe_addr) {
		root->left = insert(root->left, safe_addr, len, color);
	} else if (safe_addr == root->safe_addr) {
		if (color != root->color) {
			root->color = color;
		}
		if (len != root->len) {
			root->len = len;
		}
	}
	return root;
}

u8 search(struct tag_node *root, unsigned long safe_addr)
{
	if (root == NULL) {
		return (u8)SILVER_CLIQUE;
	} else if ((root->safe_addr <= safe_addr) &&
			(safe_addr < (root->safe_addr + root->len))) {
		return root->color;
	} else if (root->safe_addr < safe_addr) {
		return search(root->right, safe_addr);
	} else {
		return search(root->left, safe_addr);
	}
}

void hakc_init_tags(void)
{
	if (hakc_initialized) return;
	root = NULL;
	hakc_initialized = true;
}

void hakc_color_address(const void *addr_to_color, clique_color_t color,
			size_t size)
{
	void *ptr;
	void *safe_addr_to_color;
	/* do not re-color if it is going back to kernel */
	if (color == SILVER_CLIQUE) {
		return;
	}
	if (!addr_to_color) {
		return;
	}
	if (!hakc_initialized) {
		return;
	}
	if (!VALID_COLOR(color)) {
		color = SILVER_CLIQUE;
	}
	safe_addr_to_color = (void*)HAKC_GET_SAFE_PTR(addr_to_color);
	ptr = (void *)HAKC_GET_SAFE_PTR(addr_to_color);
	ptr = (void *)round_down((unsigned long)ptr, COLOR_GRANULARITY);
	if (size > COLOR_GRANULARITY) {
		size = round_up(size + (safe_addr_to_color - ptr),
				COLOR_GRANULARITY);
	} else {
		size = COLOR_GRANULARITY;
	}
	if (root == NULL) {
		root = insert(root, (unsigned long)ptr, size, color);
	} else {
		insert(root, (unsigned long)ptr, size, color);
	}
}

clique_color_t get_hakc_address_color(const void *addr)
{
	unsigned long _addr = (unsigned long)addr;
	/*
	 * The kernel will return (unsigned short)-1 for pointer values, so
	 * ignore those, or error pointers
	 */
	/*if (_addr <= 0xffffffff) {
		return SILVER_CLIQUE;
	} else if (is_userspace_addr(addr)) {
		return SILVER_CLIQUE;
	} else*/ if  (!hakc_initialized) {
		return SILVER_CLIQUE;
	}

	_addr = (unsigned long)HAKC_KADDR(addr);

	return search(root, _addr);
}

static inline void *neon_sha256_hash_address_with_modifier(const void *address,
							pac_salt_t modifier)
{
	uint32_t H[8];
	unsigned char buffer[64];
	uint64_t h0;
	void *result;

	memcpy(H, H_0, 8*4);
	memset(buffer, 0, 64);

	*((pac_salt_t *)&buffer[0]) = modifier;
	*((uintptr_t *)&buffer[sizeof(pac_salt_t)]) =
		(uintptr_t)((uintptr_t)address & 0xff00ffffffffffffL);
	/*
	 * NEON state not saved/not properly saved on context switch
	 * multi-threading issues have been encountered in practice
	 * with v5.10.24 kernel
	 */
	sha256_block_neon(H, buffer, 1);

	h0 = (uint64_t)H[0];
	/*
	 * if chosen byte from hash ends up being ff
	 * the signed pointer will start with ffff
	 * indistinguishable from an unsigned kernel pointer
	 * instead of another round of hashing, use fe instead
	 */
	if ((h0 & 0xff000000) == 0xff000000) {
		h0 = (uint64_t)0xfe000000 << 24;
	} else {
		h0 = (h0 & 0xff000000) << 24;
	}
	/* put the byte from the hash into the pointer */
	result = (void*)(((uintptr_t)address & 0xff00ffffffffffffL) | h0);
	return result;
}

void *sign_data(const void *address, pac_salt_t modifier)
{
	void *result;

	result = neon_sha256_hash_address_with_modifier(address, modifier);

	return result;
}

void *sign_code(const void *address, pac_salt_t modifier)
{
	void *result;

	result = neon_sha256_hash_address_with_modifier(address, modifier);

	return result;
}

static void *compute_pac(const void *addr, clique_color_t color,
				hakc_compartment_id_t compartment,
				void *(sign_func)(const void *, pac_salt_t))
{
	pac_salt_t modifier = PAC_MODIFIER(compartment, HAKC_MASK_COLOR(color));
	u64 ctx_addr = HAKC_CONTEXT_ADDR(addr);
	void *signed_ptr;

	signed_ptr = sign_func((const void *)ctx_addr, modifier);

	return (void *)((u64)signed_ptr | HAKC_COMPARTMENT_ADDR(addr));
}

static u64 compute_data_pac(const void *addr, clique_color_t color,
				hakc_compartment_id_t compartment)
{
	u64 result;
	result = (u64)compute_pac(addr, color, compartment, sign_data);
	return result;
}

static uintptr_t compute_code_pac(const void *addr, clique_color_t color,
					hakc_compartment_id_t compartment)
{
	u64 result;
	result = (u64)compute_pac(addr, color, compartment, sign_code);
	return result;
}

void *hakc_sign_pointer(void *addr, hakc_compartment_id_t compartment,
			clique_color_t color, bool is_code)
{
	if (compartment == 0 || !hakc_initialized) {
		return HAKC_GET_SAFE_PTR(addr);
	}

	if (VALID_COMPARTMENT(compartment)) {
		addr = HAKC_GET_SAFE_PTR(addr);
		if (is_code) {
			addr = (void *)compute_code_pac((void *)addr, color,
							compartment);
		} else {
			addr = (void *)compute_data_pac((void *)addr, color,
							compartment);
		}
	}
	return (void *)addr;
}

static void *color_and_sign(void *data_to_transfer, size_t size,
				hakc_compartment_id_t compartment,
				clique_color_t color, bool is_code)
{
	if (compartment == 0 || !hakc_initialized) {
		return HAKC_GET_SAFE_PTR(data_to_transfer);
	}
	if (/*!is_userspace_addr(data_to_transfer) &&*/ size > 0) {
		unsigned long addr = (unsigned long)data_to_transfer;

		if (addr_is_signed(data_to_transfer)) {
			addr = HAKC_GET_SAFE_PTR(addr);
		}

		if (hakc_initialized && !is_code /*&& !is_readonly(addr)*/) {
			hakc_color_address((void *)addr, color, size);
		} else if (hakc_initialized) {
			color = get_hakc_address_color(data_to_transfer);
		}

		return hakc_sign_pointer((void *)addr, compartment, color,
					is_code);
	} else {
		return data_to_transfer;
	}
}

void *hakc_transfer_to_clique(void *data_to_transfer, size_t size,
				hakc_compartment_id_t compartment,
				clique_color_t color, bool is_code)
{
	void *res;
	if (compartment == 0 || !hakc_initialized) {
		return HAKC_GET_SAFE_PTR(data_to_transfer);
	}
	if (!data_to_transfer) {
		return data_to_transfer;
	}
	res = color_and_sign(data_to_transfer, size, compartment, color,
				is_code);
	return res;
}
static inline pac_salt_t create_pac_context(hakc_compartment_id_t compartment,
						u64 masked_color)
{
	return PAC_MODIFIER(compartment, masked_color);
}

static inline pac_salt_t obtain_modifier_cert(clique_color_t address_color,
						hakc_compartment_id_t compartment)
{
	pac_salt_t result;

	result = create_pac_context(compartment,
					HAKC_MASK_COLOR(address_color));
	return result;
}

static void *check_hakc_access(const void *address,
				hakc_compartment_id_t compartment,
				const clique_access_tok_t access_tok,
				void *(*auth_func)(const void *, pac_salt_t))
{
	pac_salt_t salt;
	unsigned long result;
	clique_color_t addr_color;
	void *safe_addr;

	if (!hakc_initialized) {
		return (void*)HAKC_GET_SAFE_PTR(address);
	} else if (is_userspace_addr(address)) {
		return (void *)address;
	}

	safe_addr = (void*)HAKC_GET_SAFE_PTR(address);

	addr_color = get_hakc_address_color(safe_addr);

	/*
	 * the address used to be masked with 0xFF000000_00000000
	 * this was causing the destruction of the PAC signature in
	 * the upper bits, and signed pointers would no longer authenticate
	 *
	 * this change fixes ARM v9 support and most importantly
	 * this change does not break ARM v8 or x86-64 support
	 */
	salt = obtain_modifier_cert(addr_color, compartment) & access_tok;
	result = (unsigned long)auth_func(
			(const void *)HAKC_CONTEXT_ADDR(address), salt);
	result |= (0x0000FFFFFFFFFFFF & (unsigned long)address);

    #ifdef HAKC_ALLOW
		result |= 0xFFFF000000000000;
    #endif

	return (void *)result;
}

void *hakc_auth_data_ptr(const void *address, pac_salt_t modifier)
{
	void *result;
	void *hashed_result;

	hashed_result = neon_sha256_hash_address_with_modifier(address,
								modifier);

	if (hashed_result == address) {
		result = (void *)((uintptr_t)address & ~(0xffff000000000000L));
	} else {
		result = (void*)hashed_result;
	}

	return result;
}

void *check_hakc_data_access(const void *address,
				hakc_compartment_id_t compartment,
				const clique_access_tok_t access_tok)
{
	if (!hakc_initialized) {
		return (void *)HAKC_GET_SAFE_PTR(address);
	}

	return check_hakc_access(address, compartment, access_tok,
					hakc_auth_data_ptr);
}

#endif

void init_hakc() {
    #ifdef CHERI_TEST
    compartment_1_sealcap = get_system_sealer(1);
    compartment_2_sealcap = get_system_sealer(2);
    #else
	hakc_init_tags();
    #endif
}

void untrusted_3rd_party_func(void * data) {
    print_info(data);
}

struct test * get_secret() {
     struct test* result = (struct test*)malloc(sizeof(struct test));
     memset(result, 0, sizeof(struct test));
     strncpy(result->str, "Shh! This is a secret!", sizeof(result->str));
     result->func = untrusted_3rd_party_func;
     return result;
 }

 void print_compartment_1(struct test *test) {
     struct test* validated = validate_ptr(test, compartment_1_validator);
     printf("[%s] str = %s i = %d\n", __FUNCTION__, validated->str, validated->i);
     validated->i += 1;
 }

 void compartment_1_func(struct test *test) {
	 compartment_transfer_count++;
     struct test *protected_ptr = protect_ptr(test, compartment_1_protector);
     print_compartment_1(protected_ptr);
 }

void print_compartment_2(struct test *test) {
	struct test* validated = validate_ptr(test, compartment_2_validator);
	printf("[%s] str = %s i = %d\n", __FUNCTION__, validated->str, validated->i);
	validated->i += 1;
}

void compartment_2_func(struct test *test) {
	compartment_transfer_count++;
	struct test *protected_ptr = protect_ptr(test, compartment_2_protector);
	print_compartment_2(protected_ptr);
    compartment_1_func(test);
}

int main(int argc, char** argv) {
    int i, iterations;
    if(argc != 2) {
        fprintf(stderr, "usage: %s <number of iterations>\n", argv[0]);
        exit(1);
    }

    iterations = atoi(argv[1]);

    init_hakc();

     struct test *test = get_secret();
     print_info(test);

     for(i = 0; i < iterations; i++) {
         compartment_2_func(test);
     }

     return 0;
}
