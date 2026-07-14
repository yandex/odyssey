/* copy-pasted from heap-inl.h of libuv */

#include <machinarium/machinarium.h>
#include <machinarium/ds/heap.h>

void mm_heap_init(mm_heap_t *h, mm_heap_cmp_fn cmp)
{
	h->min = NULL;
	h->cmp = cmp;
	h->size = 0;
}

mm_heap_node_t *mm_heap_min(mm_heap_t *h)
{
	return h->min;
}

size_t mm_heap_size(mm_heap_t *h)
{
	return h->size;
}

static void heap_swap(mm_heap_t *h, mm_heap_node_t *parent,
		      mm_heap_node_t *child)
{
	mm_heap_node_t *sibling = NULL;
	mm_heap_node_t t;

	t = *parent;
	*parent = *child;
	*child = t;

	parent->parent = child;

	if (child->left == child) {
		child->left = parent;
		sibling = child->right;
	} else {
		child->right = parent;
		sibling = child->left;
	}

	if (sibling != NULL) {
		sibling->parent = child;
	}

	if (parent->left != NULL) {
		parent->left->parent = parent;
	}

	if (parent->right != NULL) {
		parent->right->parent = parent;
	}

	if (child->parent == NULL) {
		h->min = child;
	} else if (child->parent->left == parent) {
		child->parent->left = child;
	} else {
		child->parent->right = child;
	}
}

void mm_heap_push(mm_heap_t *h, mm_heap_node_t *node)
{
	size_t path;
	size_t n;
	size_t depth;
	mm_heap_node_t **parent;
	mm_heap_node_t **child;

	node->left = NULL;
	node->right = NULL;
	node->parent = NULL;

	path = 0;
	for (depth = 0, n = 1 + h->size; n >= 2; ++depth, n /= 2) {
		path = (path << 1) | (n & 1);
	}

	child = &h->min;
	parent = child;
	while (depth > 0) {
		parent = child;
		if (path & 1) {
			child = &(*child)->right;
		} else {
			child = &(*child)->left;
		}
		path >>= 1;
		--depth;
	}

	node->parent = *parent;
	*child = node;
	++h->size;

	while (node->parent != NULL && h->cmp(node, node->parent)) {
		heap_swap(h, node->parent, node);
	}
}

void mm_heap_remove(mm_heap_t *h, mm_heap_node_t *node)
{
	size_t path;
	size_t depth;
	size_t n;

	mm_heap_node_t *smallest;
	mm_heap_node_t **max;
	mm_heap_node_t *child;

	if (h->size == 0) {
		return;
	}

	path = 0;
	for (depth = 0, n = h->size; n >= 2; ++depth, n /= 2) {
		path = (path << 1) | (n & 1);
	}

	max = &h->min;
	while (depth > 0) {
		if (path & 1) {
			max = &(*max)->right;
		} else {
			max = &(*max)->left;
		}

		path >>= 1;
		--depth;
	}

	--h->size;

	child = *max;
	*max = NULL;

	if (child == node) {
		if (child == h->min) {
			h->min = NULL;
		}
		return;
	}

	child->left = node->left;
	child->right = node->right;
	child->parent = node->parent;

	if (child->left != NULL) {
		child->left->parent = child;
	}

	if (child->right != NULL) {
		child->right->parent = child;
	}

	if (node->parent == NULL) {
		h->min = child;
	} else if (node->parent->left == node) {
		node->parent->left = child;
	} else {
		node->parent->right = child;
	}

	while (1) {
		smallest = child;
		if (child->left != NULL && h->cmp(child->left, smallest)) {
			smallest = child->left;
		}

		if (child->right != NULL && h->cmp(child->right, smallest)) {
			smallest = child->right;
		}

		if (smallest == child) {
			break;
		}

		heap_swap(h, child, smallest);
	}

	while (child->parent != NULL && h->cmp(child, child->parent)) {
		heap_swap(h, child->parent, child);
	}
}

mm_heap_node_t *mm_heap_pop(mm_heap_t *h)
{
	mm_heap_node_t *n = h->min;

	mm_heap_remove(h, n);

	return n;
}

static int validate_node(mm_heap_t *h, mm_heap_node_t *node,
			 mm_heap_node_t *expected_parent)
{
	if (node == NULL) {
		return 1;
	}

	if (node->parent != expected_parent) {
		return 0;
	}

	if (node->left != NULL && h->cmp(node->left, node)) {
		return 0;
	}
	if (node->right != NULL && h->cmp(node->right, node)) {
		return 0;
	}

	return validate_node(h, node->left, node) &&
	       validate_node(h, node->right, node);
}

int mm_heap_validate(mm_heap_t *h)
{
	return validate_node(h, h->min, NULL);
}
