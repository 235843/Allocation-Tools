#ifndef HEAP_H
#define HEAP_H
#include <stdio.h>
#include <string.h>
#include "custom_unistd.h"
#include "tested_declarations.h"


struct memory_manager_t
{
    void *memory_start;
    size_t memory_size;
    struct memory_chunk_t *first_memory_chunk;
    int is_init;
}memory_manager;

struct memory_chunk_t
{
    struct memory_chunk_t* prev;
    struct memory_chunk_t* next;
    size_t size;
    size_t max_size;
    int free;
    int control_sum;
};

enum pointer_type_t
{
    pointer_null,
    pointer_heap_corrupted,
    pointer_control_block,
    pointer_inside_fences,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

#define STRUCTSIZE sizeof(struct memory_chunk_t)
#define FENCE 1


void* heap_malloc(size_t size);
void* heap_calloc(size_t number, size_t size);
void* heap_realloc(void* memblock, size_t count);
void  heap_free(void* memblock);
enum pointer_type_t get_pointer_type(const void* const pointer);
size_t   heap_get_largest_used_block_size(void);
int heap_setup(void);
void heap_clean(void);
int heap_validate(void);
int heap_control_sum(struct memory_chunk_t *chunk);
void set_every_control_sum();
void delete_last_free_blocks(void);


#endif
