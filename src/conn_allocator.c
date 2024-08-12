#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>


typedef struct Pool_Free_Node Pool_Free_Node;  

//pointer could be NULL if there is no next free element
struct Pool_Free_Node {
    Pool_Free_Node *next;
};

typedef struct Pool Pool;

struct Pool {
    //backing buffer.
    unsigned char *buf;
    size_t buf_len;
    size_t chunk_size;

    //pointer to the head of the free list.
    Pool_Free_Node *head;
};


void pool_free_all(Pool *pool);

bool is_power_of_two(uintptr_t x) {
    return (x &(x - 1)) == 0;
}
uintptr_t align_forward(uintptr_t ptr, size_t align) {
    uintptr_t p, a, modulo;

    assert(is_power_of_two(align));

    p = ptr;
    a = (uintptr_t)align;
    //same as p % a buf faster as p is a power of 2
    modulo = p & (a - 1);

    if (modulo != 0) {
        //if p address is not aligned, push the address to the
        //next value which is aligned
        p += a - modulo;
    }
    return p;
}

size_t align_forward_size(size_t size, size_t align) {
    size_t modulo = size & (align - 1);
    if (modulo != 0) {
        size += align - modulo;
    }
    return size;
}

void pool_init(Pool *pool, void *backing_buffer, size_t backing_buffer_length, size_t chunk_size, size_t chunk_alignment) {
    uintptr_t initial_start = (uintptr_t)backing_buffer;
    uintptr_t start = align_forward(initial_start, (uintptr_t)chunk_alignment);
    backing_buffer_length -= (size_t)(start - initial_start);

    chunk_size = align_forward_size(chunk_size, chunk_alignment);

    assert(chunk_size >= sizeof(Pool_Free_Node) && "chunk size is too small");
    assert(backing_buffer_length >= chunk_size && "backing buffer length is smaller than the chunk size");

    pool -> buf = (unsigned char *)backing_buffer;
    pool -> buf_len = backing_buffer_length;
    pool -> chunk_size = chunk_size;
    pool -> head = NULL;

    pool_free_all(pool);
}


void *pool_alloc(Pool *pool) {
    //get latest free node
    Pool_Free_Node *node = pool -> head;
    if (node == NULL) {
        assert(0 && "Pool allocator has no free memory");
        return NULL;
    }

    pool -> head = pool -> head -> next;
    return memset(node, 0, pool -> chunk_size);
}

void pool_free(Pool *pool, void *ptr) {
    Pool_Free_Node *node;

    void *start = pool -> buf;
    void *end =  &pool -> buf[pool -> buf_len];

    if (ptr == NULL) {
        //ignore NULL pointers
        return;
    }

    if (!(start <= ptr && ptr < end)) {
        assert(0 && "memory is out of bounds of the buffer of this pool");
        return;
    }

    node = (Pool_Free_Node *)ptr;
    node -> next = pool -> head;
    pool -> head = node;

}


void pool_free_all(Pool *pool) {
    size_t chunk_count = pool -> buf_len / pool -> chunk_size;
    size_t i;

    //set all chunks to be free
    for (i = 0; i < chunk_count; i++) {
        void *ptr = &pool -> buf[i * pool -> chunk_size];
        Pool_Free_Node *node = (Pool_Free_Node *)ptr;
        //push free Node onto the next free list
        node  -> next = pool -> head;
        pool -> head = node;
    }
}
