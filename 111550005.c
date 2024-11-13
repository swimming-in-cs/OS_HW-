#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#define MAX_SIZE 20000
#define HEADER_SIZE 32

struct block {
    size_t size;
    int free;
    struct block *prev;
    struct block *next;
};

struct block *head = NULL;

// Align size to the nearest multiple of 32
int align_size(int size) {
    return (size % 32 == 0) ? size : (size / 32 + 1) * 32;
}

// Output the result and unmap memory
void output_res(int res) {
    char output_str[40]; // Adjusted size for potentially larger numbers
    sprintf(output_str, "Max Free Chunk Size = %d\n", res);
    write(STDOUT_FILENO, output_str, sizeof(output_str));
    munmap(head, MAX_SIZE); // Release memory pool
}

// Custom malloc function using Best Fit strategy
void* malloc(size_t size) {
    // Initialize memory pool if not already done
    if (head == NULL) {
        head = (struct block*)mmap(NULL, MAX_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (head == MAP_FAILED) {
            output_res(-1); // Output -1 if mmap fails
            perror("mmap");
            return NULL;
        }
        head->size = MAX_SIZE - HEADER_SIZE;
        head->free = 1;
        head->prev = NULL;
        head->next = NULL;
    }
    
    // End-of-test signal (malloc(0))
    if (size == 0) { 
        size_t max_free = 0;
        struct block *curr = head;

        // Find the largest free block
        while (curr) {
            if (curr->free && curr->size > max_free) {
                max_free = curr->size;
            }
            curr = curr->next;
        }

        output_res(max_free);
        return NULL;
    }
    
    int aligned_size = align_size(size); // Align requested size
    struct block *best_fit = NULL;
    size_t min_size = MAX_SIZE;

    struct block *curr = head;
    // Find the best-fit free block
    while (curr) {
        if (curr->free && curr->size >= aligned_size && curr->size < min_size) {
            best_fit = curr;
            min_size = curr->size;
        }
        curr = curr->next;
    }
    
    if (best_fit == NULL) {
        return NULL; // No suitable block found
    }
    
    // Check if block splitting is necessary
    if (best_fit->size == aligned_size) {
        best_fit->free = 0;
    } else { // Split the block if it's larger than needed
        struct block *new_block = (struct block*)((char*)best_fit + HEADER_SIZE + aligned_size);
        new_block->free = 1;
        new_block->size = best_fit->size - aligned_size - HEADER_SIZE;
        new_block->prev = best_fit;
        new_block->next = best_fit->next;

        if (best_fit->next) {
            best_fit->next->prev = new_block;
        }

        best_fit->free = 0;
        best_fit->size = aligned_size;
        best_fit->next = new_block;
    }

    return (void*)(best_fit + 1); // Return pointer to the data area
}

// Custom free function with coalescing
void free(void *ptr) {
    if (!ptr) return;

    struct block *curr = (struct block*)ptr - 1;
    curr->free = 1;

    // Coalesce with next block if it’s free
    if (curr->next && curr->next->free) {
        curr->size += curr->next->size + HEADER_SIZE;
        curr->next = curr->next->next;
        if (curr->next) {
            curr->next->prev = curr;
        }
    }

    // Coalesce with previous block if it’s free
    if (curr->prev && curr->prev->free) {
        curr->prev->size += curr->size + HEADER_SIZE;
        curr->prev->next = curr->next;
        if (curr->next) {
            curr->next->prev = curr->prev;
        }
    }
}
