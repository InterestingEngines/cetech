#include <stdint.h>
#include "celib/types.h"
#include "celib/memory.h"
#include "celib/errors.h"
#include "celib/allocator.h"

#define LOG_WHERE "allocator"

//void* allocator_allocate(struct cel_allocator* allocator, uint32_t size, uint32_t align) {
//    return allocator->allocate(allocator, size, align);
//}
//
//void allocator_deallocate(struct cel_allocator* allocator, void* p) {
//    allocator->deallocate(allocator, p);
//}
//
//uint32_t allocator_total_allocated(struct cel_allocator* allocator){
//    return allocator->total_allocated(allocator);
//}
//
//uint32_t allocator_allocated_size(struct cel_allocator* allocator, void* p){
//    return allocator->allocated_size(p);
//}

void *cel_malloc(size_t size) {
    void *mem = malloc(size);
    if (mem == NULL) {
        log_error("malloc", "Malloc return NULL");
        return NULL;
    }

    return mem;
}

void cel_free(void *ptr) {
    free(ptr);
}


void allocator_trace_pointer(struct cel_allocator_trace_entry *entries,
                             uint64_t max_entries,
                             void *p) {
    char *stacktrace_str = cel_stacktrace(3);

    for (int i = 0; i < max_entries; ++i) {
        if (!entries[i].used) {
            entries[i].used = 1;
            entries[i].ptr = p;
            entries[i].stacktrace = stacktrace_str;
            break;
        }
    }
}

void allocator_stop_trace_pointer(struct cel_allocator_trace_entry *entries,
                                  uint64_t max_entries,
                                  void *p) {
    for (int i = 0; i < max_entries; ++i) {
        if (entries[i].ptr != p) {
            continue;
        }

        entries[i].used = 0;

        cel_stacktrace_free(entries[i].stacktrace);
        entries[i].stacktrace = NULL;
    }
}

void allocator_check_trace(struct cel_allocator_trace_entry *entries,
                           uint64_t max_entries) {
    for (int i = 0; i < max_entries; ++i) {
        if (!entries[i].used) {
            continue;
        }

        log_error(LOG_WHERE, "memory_leak: %p\n  stacktrace:\n%s\n",
                  entries[i].ptr, entries[i].stacktrace);

        //allocator_free(allocator, entries[i].ptr); // TODO: need this?

        cel_stacktrace_free(entries[i].stacktrace);
    }
}