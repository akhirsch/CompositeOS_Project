#ifdef LINUX
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#define printd printf
#else
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#define printd printc
#endif

#include <heap.h>

//#define HEAP_TRACE_DEBUG
#ifdef HEAP_TRACE_DEBUG
#define debug(format, ...) printd(format, ## __VA_ARGS__)
#else 
#define debug(format, ...)
#endif

static inline void swap_entries(void *arr[], int a, int b, update_fn_t u)
{
	void *t;

	t = arr[a];
	arr[a] = arr[b];
	arr[b] = t;

	u(arr[a], a);
	u(arr[b], b);
}

/* 
 * a: array
 * c: current index
 * e: end index
 */
static inline int swap_down(struct heap *h, int c)
{
	int l; 			/* last entry */
	assert(c != 0);
	assert(c <= h->e);

	l = h->e-1;
	while (c <= l/2) { /* not a leaf? */
		int n; 	   /* next index */
		int left, right;

		left = 2*c;
		right = 2*c+1;

		if (right > l) {
			n = left;
		} else if (h->c(h->data[left], h->data[right])) {
			n = left;
		} else {
			n = right;
		}
		assert(n < h->e);
		if (h->c(h->data[c], h->data[n])) break; /* done? */

		swap_entries(h->data, n, c, h->u);
		c = n;
	}

	return c;
}

static inline int swap_up(struct heap *h, int c)
{
	assert(c <= h->e);
	assert(c > 0);

	while (c > 1) {
		int p; 		/* parent index */
		
		p = c/2;
		assert(p != 0);
		if (h->c(h->data[p], h->data[c])) break; /* done? */

		swap_entries(h->data, p, c, h->u);
		c = p;
	}
	assert(c != 0);

	return c;
}

/* return c's final index */
static inline int heapify(struct heap *h, int c)
{
	c = swap_up(h, c);
	return swap_down(h, c);
}

#ifdef LINUX_TEST
struct hentry {
	int index, value;
};
#endif

//#define HEAP_VERIFY
#ifdef HEAP_VERIFY
static inline int __heap_verify(struct heap *h, int c)
{
	int left, right;

	left = c*2;
	right = c*2+1;
	if (left < h->e) {
		assert(((struct hentry*)h->data[left])->index == left);
		if (!h->c(h->data[c], h->data[left]) || __heap_verify(h, left)) {
			printd("Left data %d @ %d < %d @ %d\n", 
			       ((struct hentry*)h->data[c])->value, c, 
			       ((struct hentry*)h->data[left])->value, left);
			return 1;
		}
	}
	if (right < h->e) {
		assert(((struct hentry*)h->data[right])->index == right);
		if (!h->c(h->data[c], h->data[right]) || __heap_verify(h, right)) {
			printd("Right data %d @ %d < %d @ %d\n", 
			       ((struct hentry*)h->data[c])->value, c, 
			       ((struct hentry*)h->data[left])->value, left);
			return 1;
		}
	}
	return 0;
}

static int heap_verify(struct heap *h)
{
	return __heap_verify(h, 1);
}
#else
#define heap_verify(h) 0
#endif

/* public functions */

struct heap *heap_alloc(int max_sz, cmp_fn_t c, update_fn_t u)
{
	struct heap *h;

	h = malloc(sizeof(struct heap) + (max_sz * sizeof(void*)) + 1);
	if (NULL == h) return NULL;

	h->max_sz = max_sz+1;
	h->e = 1;
	h->c = c;
	h->u = u;
	h->data = (void *)&h[1];
	assert(!heap_verify(h));

	return h;
}

void heap_destroy(struct heap *h)
{
	assert(h && h->data);
	free(h);
}

int heap_add(struct heap *h, void *new)
{
	int c;

	if (h->max_sz == h->e) return -1;

	debug("heap_add(%p,%d) %p", h, h->e, new);

	assert(!heap_verify(h));
	c = h->e;
	h->data[c] = new;
	h->u(new, c);
	h->e++;
	heapify(h, c);
	assert(!heap_verify(h));

	return 0;
}

void *heap_highest(struct heap *h)
{
	void *r;

	if (h->e == 1) return NULL;
	assert(!heap_verify(h));
	r = h->data[1];
	debug("heap_highest(%p,%d) %p\n", h, h->e, r);
	h->e--;
	h->data[1] = h->data[h->e];
	h->u(h->data[1], 1);
	swap_down(h, 1);
	assert(!heap_verify(h));
	h->u(r, 0);

	return r;
}

void *heap_peek(struct heap *h)
{
	if (h->e == 1) return NULL;
	assert(!heap_verify(h));
	return h->data[1];
}

void heap_adjust(struct heap *h, int c)
{
	assert(c < h->e);
	assert(c > 0);
	debug("heap_adjust(%p,%d) %p@%d\n", h, h->e, h->data[c], c);
	heapify(h, c);
	assert(!heap_verify(h));
}

void *heap_remove(struct heap *h, int c)
{
	void *r;

	assert(c < h->e);
	assert(c >= 1);
	if (h->e == 1) return NULL;
	assert(!heap_verify(h));
	r = h->data[c];
	debug("heap_remove(%p,%d) %p@%d\n", h, h->e, h->data[c], c);
	h->e--;
	h->u(r, 0);
	if (c == h->e) {
		assert(!heap_verify(h));
		return r;
	}
	h->data[c] = h->data[h->e];
	h->u(h->data[c], c);
	heap_adjust(h, c);
	assert(!heap_verify(h));

	return r;
}

int heap_size(struct heap *h)
{
	return h->e-1;
}

#ifdef LINUX_TEST

int c(void *a, void *b) { return ((struct hentry*)a)->value >= ((struct hentry*)b)->value; }
void u(void *e, int pos) { ((struct hentry*)e)->index = pos; }

static void entries_validate(struct heap *h, struct hentry *es, int amnt)
{
	int i;

	for (i = 0 ; i < amnt ; i++) {
		assert(h->data[es[i].index] == &es[i]);
	}
}

static void test_driver(int amnt)
{
	int i;
	struct hentry *prev, *es;
	struct heap *h;

	h = heap_alloc(amnt, c, u);
	es = malloc(sizeof(struct hentry) * amnt);
	assert(es);

	for (i = 0 ; i < amnt ; i++) {
		es[i].value = rand();
		assert(!heap_add(h, &es[i]));
	}
	entries_validate(h, es, amnt);
	for (i = 0 ; i < amnt ; i++) {
		es[i].value = rand();
		heap_adjust(h, es[i].index);
	}
	entries_validate(h, es, amnt);
	prev = h->data[1];
	for (i = 0 ; i < amnt ; i++) {
		struct hentry *curr = heap_highest(h);
		if (!c((struct hentry*)prev, (struct hentry*)curr)) assert(0);
		prev = curr;
	}
	assert(!heap_highest(h));
	assert(heap_size(h) == 0);
	for (i = 0 ; i < amnt ; i++) {
		es[i].value = rand();
		assert(!heap_add(h, &es[i]));
	}
	entries_validate(h, es, amnt);
	for (i = amnt ; i > 0 ; i--) {
		int idx;
		idx = (rand() % i) + 1;
		struct hentry *curr = heap_remove(h, idx);
		assert(curr);
		assert(h->e == i);
	}
	assert(!heap_highest(h));
	assert(heap_size(h) == 0);

	heap_destroy(h);
	free(es);
}

#define ITER 50
#define BOUND 4096

int main(void)
{
	int i;

	srand(time(NULL));

	for (i = 0 ; i < ITER ; i++) {
		test_driver(rand() % BOUND);
	}

	return 0;
}

#endif
