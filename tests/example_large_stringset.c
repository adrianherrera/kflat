/**
 * @file example_large_stringset.c
 * @author Samsung R&D Poland - Mobile Security Group
 * 
 */

#include "common.h"

#ifdef __KERNEL__
#include <linux/rbtree.h>
#include <linux/random.h>
#else
#include <stdbool.h>
#include "../lib/include_priv/rbtree.h"
#endif

struct string_node_atomic {
	struct rb_node node;
	char *s;
};

#ifdef __KERNEL__

static struct rb_root stringset_root = RB_ROOT;

static inline void* ptr_remove_color(const void* ptr) {
	return (void*)((uintptr_t)ptr & ~3);
}

static inline struct flatten_pointer *fptr_add_color(struct flatten_pointer *fptr, const struct flatten_base *ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DECLARE_FLATTEN_STRUCT(string_node_atomic);

FUNCTION_DEFINE_FLATTEN_STRUCT(string_node_atomic,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_EMBEDDED_POINTER(string_node_atomic, node.__rb_parent_color, ptr_remove_color, fptr_add_color);
	AGGREGATE_FLATTEN_STRUCT(string_node_atomic, node.rb_right);
	AGGREGATE_FLATTEN_STRUCT(string_node_atomic, node.rb_left);
	AGGREGATE_FLATTEN_STRING(s);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_SPECIALIZE(atomic_stringset,rb_root,
	AGGREGATE_FLATTEN_STRUCT(string_node_atomic,rb_node);
);

static int stringset_insert(struct kflat* kflat, const char *s) {
	struct string_node_atomic *data = kflat_zalloc(kflat,sizeof(struct string_node_atomic),1);
	struct rb_node **new, *parent = 0;
	data->s = kflat_zalloc(kflat,strlen(s) + 1,1);
	strcpy(data->s, s);
	new = &(stringset_root.rb_node);

	/* Figure out where to put new node */
	while (*new) {
		struct string_node_atomic *this = container_of(*new, struct string_node_atomic, node);

		parent = *new;
		if (strcmp(data->s, this->s) < 0)
			new = &((*new)->rb_left);
		else if (strcmp(data->s, this->s) > 0)
			new = &((*new)->rb_right);
		else {
			return 0;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&data->node, parent, new);
	rb_insert_color(&data->node, &stringset_root);

	return 1;
}

static void stringset_destroy(struct rb_root *root) {
	struct rb_node *p = rb_first(root);
	while (p) {
		struct rb_node *q = rb_next(p);
		rb_erase(p, root);
		p = q;
	}
}

static int kflat_large_stringset_test(struct kflat *kflat) {
	
	unsigned i, j;
	static const char chars[] = "ABCDEFGHIJKLMNOPQRST";
	unsigned char r;

	for (j = 0; j < 2000; ++j) {
		char *s = kflat_zalloc(kflat,11,1);
		for (i = 0; i < 10; ++i) {
			get_random_bytes(&r, 1);
			s[i] = chars[r%20];
		}
		stringset_insert(kflat,s);
	}

	FOR_ROOT_POINTER(&stringset_root,
		FLATTEN_STRUCT_SPECIALIZE(atomic_stringset, rb_root, &stringset_root);
	);

	stringset_destroy(&stringset_root);
	stringset_root.rb_node = 0;
	return 0;
}

#else

static size_t stringset_count(const struct rb_root *root) {
	struct rb_node *p = rb_first(root);
	size_t count = 0;
	while (p) {
		count++;
		p = rb_next(p);
	}
	return count;
}

static int kflat_large_stringset_validate(void *memory, size_t size, CFlatten flatten) {
	const struct rb_root *root = (struct rb_root *)memory;
	ASSERT(stringset_count(root) == 2000);

	PRINT("Stringset size: %ld", stringset_count(root));

	return KFLAT_TEST_SUCCESS;
}

#endif

KFLAT_REGISTER_TEST_FLAGS("LARGE_STRINGSET", kflat_large_stringset_test, kflat_large_stringset_validate,KFLAT_TEST_ATOMIC);
