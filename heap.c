#include "heap.h"
#include "tested_declarations.h"
#include "rdebug.h"
#include "tested_declarations.h"
#include "rdebug.h"

int heap_validate(void)
{
    if(!memory_manager.is_init)
        return  2;

    struct memory_chunk_t* chunk = memory_manager.first_memory_chunk;
    while  (chunk)
    {

        int temp=heap_control_sum(chunk);
        if(chunk->control_sum!= temp)
            return 3;
        if(!chunk->free){
            for (unsigned int i = 0; i < FENCE; i++) {
                if(*((char*)chunk+STRUCTSIZE+i)!=0)
                    return 1;
                if(*((char *)chunk+STRUCTSIZE+i+FENCE+chunk->size)!=0)
                    return 1;
            }
        }
        chunk = chunk->next;
    }

    return 0;
}


int heap_setup(void)
{
    memory_manager.memory_start = custom_sbrk(0);
    if(memory_manager.memory_start == (void*)-1)
        return -1;

    memory_manager.is_init=2137;
    memory_manager.memory_size=0;
    memory_manager.first_memory_chunk=NULL;

    return 0;
}

void *heap_malloc(size_t s) {
    if (!s || !memory_manager.memory_start)
        return NULL;

    if (!memory_manager.first_memory_chunk) {
        struct memory_chunk_t *chunk= (struct memory_chunk_t*)custom_sbrk((intptr_t)(STRUCTSIZE + FENCE*2 + s));
        if((void*)chunk == (void*)-1)
            return NULL;
        chunk->max_size=s;
        chunk->size = s;
        chunk->prev = NULL;
        chunk->next = NULL;
        chunk->free = 0;


        memset((void*)((char*)chunk+STRUCTSIZE),0,FENCE);
        memset((void*)((char*)chunk+STRUCTSIZE+FENCE+s),0,FENCE);

        memory_manager.first_memory_chunk = chunk;
        memory_manager.first_memory_chunk->control_sum= heap_control_sum(chunk);
        return (void*)((char*)chunk+sizeof(struct memory_chunk_t)+FENCE);
    }

    if(heap_validate())
        return NULL;

    struct memory_chunk_t *chunk = (struct memory_chunk_t*)memory_manager.first_memory_chunk;
    while (1)
    {
        if(chunk->free && chunk->max_size>=s)
        {
            chunk->free=0;
            chunk->size=s;

            memset((void*)((char*)chunk+STRUCTSIZE),0,FENCE);
            memset((void*)((char*)chunk+STRUCTSIZE+FENCE+s),0,FENCE);
            chunk->control_sum= heap_control_sum(chunk);
            return (void*)((char*)chunk+sizeof (struct  memory_chunk_t)+FENCE);
        }


        if(!chunk->next)
            break;
        chunk = chunk->next;
    }

    struct memory_chunk_t* new = (struct memory_chunk_t*)custom_sbrk((intptr_t)(STRUCTSIZE + FENCE*2 + s));
    if((void*)new == (void*)-1)
        return NULL;

    new->free=0;
    new->size=s;
    new->max_size=s;
    new->next=NULL;

    new->prev = chunk;
    chunk->next=new;
    chunk->control_sum = heap_control_sum(chunk);
    memset((void*)((char*)new+STRUCTSIZE+FENCE+s),0,FENCE);
    memset((void*)((char*)new+STRUCTSIZE),0,FENCE);
    new->control_sum = heap_control_sum(new);
    return (void*)((char*)new+sizeof (struct  memory_chunk_t)+FENCE);
}

void* heap_calloc(size_t number, size_t size)
{
    void* ptr = heap_malloc(number*size);
    if(!ptr)
        return NULL;
    memset((char*)ptr, 0, number*size);
    return ptr;
}

struct memory_chunk_t* get_last_block(void)
{
    struct memory_chunk_t* chunk = memory_manager.first_memory_chunk;
    while (chunk->next)
        chunk=chunk->next;

    return chunk;
}

void* heap_realloc(void* memblock, size_t count)
{
    if(!memblock || !memory_manager.first_memory_chunk)
        return heap_malloc(count);
    if(heap_validate())
        return NULL;
    if(!count)
    {
        heap_free(memblock);
        return NULL;
    }

    struct memory_chunk_t* chunk = memory_manager.first_memory_chunk;

    while (chunk)
    {
        if(chunk==(struct memory_chunk_t*)((char*)memblock-FENCE-STRUCTSIZE))
            break;
        chunk=chunk->next;
    }
    if(!chunk)
        return NULL;

    if(count==chunk->max_size)
        return memblock;

    if(count<chunk->max_size)
    {
        chunk->size=count;
        for(int i=0;i<FENCE;i++)
            *((char*)chunk+STRUCTSIZE+FENCE+count+i)=0;
        chunk->control_sum= heap_control_sum(chunk);

        return memblock;
    }

    if(!chunk->next)
    {
        void *new = custom_sbrk((intptr_t)(count-chunk->max_size));
        if(new==(void*)-1)
            return NULL;

        memcpy((char*)chunk+STRUCTSIZE+2*FENCE+chunk->max_size,new, count-chunk->max_size);

        for(int i=0;i<FENCE;i++)
        {
            *((char*)chunk+count+FENCE+STRUCTSIZE+i)=0;
        }
        chunk->size=count;
        chunk->max_size=count;

        set_every_control_sum();
        return memblock;
    }

    int temp = (int)(count- chunk->max_size);
    if(chunk->next->free && temp<=(int)chunk->next->max_size)
    {
        char temp_ptr[100];
        memcpy(temp_ptr,chunk->next,STRUCTSIZE+FENCE);
        memcpy((char*)chunk+STRUCTSIZE+2*FENCE+chunk->max_size,chunk->next,temp);
        memcpy((char*)chunk->next+temp,temp_ptr, STRUCTSIZE+FENCE);

        chunk->max_size=count;
        chunk->size=count;
        chunk->next->max_size-=temp;
        chunk->next->size-=temp;

        memset((void*)((char*)chunk+STRUCTSIZE+FENCE+chunk->max_size),0,FENCE);

        set_every_control_sum();

        return memblock;
    }
    if(chunk->next->free && temp<=(int)(STRUCTSIZE+2*FENCE+chunk->max_size))
    {
        memcpy((char*)chunk+STRUCTSIZE+2*FENCE+chunk->max_size,chunk->next,STRUCTSIZE+2*FENCE+chunk->next->max_size);
        chunk->max_size+=chunk->next->max_size+STRUCTSIZE+2*FENCE;
        chunk->size=count;
        memset((void*)((char*)chunk+STRUCTSIZE+FENCE+chunk->size),0,FENCE);
        chunk->next= chunk->next->next;
        if(chunk->next)
            chunk->next->prev=chunk;

        set_every_control_sum();

        return memblock;
    }

    void* newmem = heap_malloc(count);
    if(!newmem)
        return NULL;

    memcpy(newmem,memblock, chunk->size);
    heap_free(memblock);
    memblock=newmem;
    set_every_control_sum();

    return memblock;
}

void heap_free(void *memblock)
{
    if(!memblock || heap_validate() || memory_manager.memory_start==NULL || memory_manager.first_memory_chunk==NULL )
        return;

    struct memory_chunk_t *chunk = (struct memory_chunk_t*)memory_manager.first_memory_chunk;

    while (chunk)
    {
        if(chunk==(struct memory_chunk_t*)((char*)memblock-sizeof (struct memory_chunk_t)-FENCE))
            break;
        chunk = chunk->next;
    }
    if(!chunk || chunk->free)
    {
        set_every_control_sum();
        return;
    }

    chunk->free=1;
    chunk->size=chunk->max_size;

    if(chunk->next)
    {
        if (chunk->next->free)
        {
            chunk->max_size += chunk->next->max_size + STRUCTSIZE+2*FENCE;
            chunk->next = chunk->next->next;
            if(chunk->next)
                chunk->next->prev = chunk;
        }
    }
    if(chunk->prev)
    {
        if(chunk->prev->free)
        {
            chunk->prev->max_size +=chunk->max_size +STRUCTSIZE+2*FENCE;
            chunk->prev->next=chunk->next;
            if(chunk->next)
                chunk->next->prev = chunk->prev;
        }
    }

    set_every_control_sum();

    //delete_last_free_blocks();


//    int flg=1;
//    struct memory_chunk_t* temp = memory_manager.first_memory_chunk;
//    while (temp)
//    {
//        if(!temp->free)
//        {
//            flg=0;
//            break;
//        }
//        temp = temp->next;
//    }
//
//    if(flg)
//        memory_manager.first_memory_chunk=NULL;


}

enum pointer_type_t get_pointer_type(const void* const pointer)
{
    if(!pointer)
        return pointer_null;

    if(!memory_manager.first_memory_chunk)
        return pointer_unallocated;
    if(heap_validate())
        return pointer_heap_corrupted;

    struct memory_chunk_t *chunk = memory_manager.first_memory_chunk;
    while(1)
    {
        if((char*)pointer>=(char*)chunk && (char*)pointer<(char*)chunk+STRUCTSIZE )
            return pointer_control_block;

        if((char*)pointer>=(char*)chunk+STRUCTSIZE && (char*)pointer<(char*)chunk+STRUCTSIZE +FENCE)
        {
            if(chunk->free)
                return pointer_unallocated;
            return pointer_inside_fences;
        }
        if((char*)pointer==(char*)chunk+STRUCTSIZE +FENCE)
        {
            if(chunk->free)
                return pointer_unallocated;
            return pointer_valid;
        }
        if((char*)pointer>(char*)chunk+STRUCTSIZE+FENCE && (char*)pointer<(char*)chunk+STRUCTSIZE +FENCE+chunk->size)
        {
            if(!chunk->free)
                return pointer_inside_data_block;
            return pointer_unallocated;
        }

        if((char*)pointer>=(char*)chunk+STRUCTSIZE+FENCE+chunk->size && (char*)pointer<(char*)chunk+STRUCTSIZE +FENCE+chunk->size+FENCE)
        {
            if(chunk->free)
                return pointer_unallocated;
            return pointer_inside_fences;
        }
        if(!chunk->next)
            return pointer_unallocated;
        chunk=chunk->next;
    }

}

void heap_clean(void)
{
    struct memory_chunk_t *chunk;
    if(memory_manager.first_memory_chunk)
    {
        chunk=memory_manager.first_memory_chunk;
        while (chunk->next)
            chunk=chunk->next;

        size_t bytes = ((char*)chunk+chunk->max_size+2*FENCE+STRUCTSIZE-(char*)memory_manager.first_memory_chunk);
        custom_sbrk((intptr_t)(-1*bytes));
    }

    memory_manager.memory_start = NULL;
    memory_manager.first_memory_chunk=NULL;
    memory_manager.memory_size=0;
    memory_manager.is_init=0;
}

size_t heap_get_largest_used_block_size(void)
{
    if(!memory_manager.is_init || !memory_manager.first_memory_chunk || heap_validate())
        return 0;

    struct memory_chunk_t temp, *chunk = memory_manager.first_memory_chunk;
    temp = *chunk;
    if(temp.free)
        temp.size=0;
    while(chunk)
    {
        if(!chunk->free && temp.size<chunk->size)
            temp=*chunk;
        chunk=chunk->next;
    }

    return temp.size;
}

int heap_control_sum(struct memory_chunk_t* chunk)
{
    int i=0,control_sum=0;
    while (i<(int)STRUCTSIZE-4)
    {
        control_sum+=*(int*)((char*)chunk+i);
        i+=4;
    }

    return control_sum;
}

void set_every_control_sum(void)
{
    struct memory_chunk_t *chunk = memory_manager.first_memory_chunk;
    while(chunk)
    {
        chunk->control_sum = heap_control_sum(chunk);
        chunk=chunk->next;
    }
}

void delete_last_free_blocks(void)
{
    struct memory_chunk_t *temp = memory_manager.first_memory_chunk;

    while (temp->next)
        temp = temp->next;

    while (temp->free)
    {
        if(!temp->prev)
            break;
        temp = temp->prev;
        temp->next=NULL;
    }

}



