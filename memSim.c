#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

// Constants
#define PAGE_SIZE 256   // Size of a page in bytes
#define PAGE_TABLE_SIZE 256 // Number of entries in the page table
#define FRAME_SIZE 256
#define TLB_SIZE 16    // Number of entries in the TLB


typedef enum {
    FIFO,  // First-Come, First-Served
    LRU,  // Least Recently Used
    OPT,  // Optimal Page Replacement
} Algorithm;

typedef struct {
    unsigned int page_number;
    unsigned int frame_number;
    int valid; // 1 if the entry is valid, 0 otherwise
} TLBEntry;

typedef struct {
    unsigned int frame_number;
    int present; // 1 if the entry is present, 0 otherwise
} PageTableEntry;

// Globals
char *input_path;
static int num_frames;
static Algorithm algorithm;
static TLBEntry tlb[TLB_SIZE];
static PageTableEntry page_table[PAGE_TABLE_SIZE]; // page -> frame
static unsigned char *physical_mem = NULL;
static int *frame_to_page; // frame -> page
static int *fifo_queue = NULL;
static int  fifo_head = 0;
static int  fifo_tail = 0;
int num_requests; // total number of page requests, len of page_requests array
static unsigned int *page_requests; // For OPT: Stores order of page requests


int time = 0; // put in last_used to keep track of time when frame accessed, increments each iteration of while loop

// Function prototypes
static void parse_arguments(int argc, char *argv[]);
static void print_output(int addr_cnt, int page_fault_cnt, int tlb_hit_cnt, int tlb_miss_cnt);
int in_tlb(unsigned int page);
int pick_victim(int *last_used);


static void fifo_push(int frame)
{
    fifo_queue[fifo_tail] = frame;
    fifo_tail = (fifo_tail + 1) % num_frames;
}

static int fifo_pop(void)
{
    int head_frame = fifo_queue[fifo_head];
    fifo_head = (fifo_head + 1) % num_frames;
    return head_frame;
}

// parse arguments and set global variables
static void parse_arguments(int argc, char *argv[]) {
    if ( argc != 2 && argc != 4) { // can be default 256 frames and FIFO 
        fprintf(stderr, "Usage: %s <input_file> <FRAMES> <PRA>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    input_path = argv[1];

    if (argc == 2) {
        num_frames = 256;
        algorithm = FIFO;
    }

    else if (argc == 4) {
        num_frames = atoi(argv[2]);
        char *alg_str = argv[3];

        if (num_frames <= 0 || num_frames > 256) {
            fprintf(stderr, "Error: FRAMES must be an integer between 1 and 256.\n");
            exit(EXIT_FAILURE);
        }

        if (strcmp(alg_str, "FIFO") == 0) {
            algorithm = FIFO;
        } else if (strcmp(alg_str, "LRU") == 0) {
            algorithm = LRU;
        } else if (strcmp(alg_str, "OPT") == 0) {
            algorithm = OPT;
        } else {
            fprintf(stderr, "Error: PRA must be 'FIFO', 'LRU', or 'OPT'.\n");
            exit(EXIT_FAILURE);
        }
    }

}

/*
For every address in the given addresses file, print one line of comma-separated fields, consisting of
    The full address (from the reference file)
    The value of the byte referenced (1 signed integer)
    The physical memory frame number (one positive integer)
    The content of the entire frame (256 bytes in hex ASCII characters, no spaces in between)
    new line character
Total number of page faults and a % page fault rate
Total number of TLB hits, misses and % TLB hit rate
*/
static void print_output(int addr_cnt, int page_fault_cnt, int tlb_hit_cnt, int tlb_miss_cnt) {
    double tlb_hit_rate = 0.0;
    if ((tlb_hit_cnt + tlb_miss_cnt) > 0) {
        tlb_hit_rate = (double)tlb_hit_cnt / (tlb_hit_cnt + tlb_miss_cnt);
    }
    printf("Number of Translated Addresses = %d\n", addr_cnt);
    printf("Page Faults = %d\n", page_fault_cnt);
    printf("Page Fault Rate = %.3f\n", (double)page_fault_cnt / addr_cnt);
    printf("TLB Hits = %d\n", tlb_hit_cnt);
    printf("TLB Misses = %d\n", tlb_miss_cnt);
    printf("TLB Hit Rate = %.3f\n", tlb_hit_rate);
}

int in_tlb(unsigned int page){ // returns idx in tlb, -1 if not present
    for (int i = 0; i < TLB_SIZE; i++){
        if (tlb[i].valid && tlb[i].page_number == page)
            return i;
    }
    return -1;
}

// Choose a victim frame based on algorithm, return index of frame to replace
int pick_victim(int *last_used) {
    switch (algorithm)
    {
    case FIFO: {
        int victim_frame = fifo_pop();
        return victim_frame;
    }

    case LRU: {
        int lru_frame = 0;
        int lru_time = INT_MAX; // initialize to max int so any real time will be smaller
        for (int i = 0; i < num_frames; i++) {
            if (last_used[i] < lru_time) {
                lru_time = last_used[i];
                lru_frame = i;
            }
        }
        return lru_frame;
    }

    case OPT:;
        int victim_frame = 0;
        int victim_idx = -1; // anything else will be larger
        for (int i = 0; i < num_frames; i++) { // for each current frame
            int p = frame_to_page[i];
            int j = time;
            int found = 0;
            // scan all future frames, find which of current frames is farthest 
            while (j < num_requests) { 
                if ((int)page_requests[j] == p) {
                    if (j > victim_idx) { // if difference is bigger
                        victim_idx = j;
                        victim_frame = i;
                    }
                    found = 1;
                    break;
                }
                j++;
            }
            if (!found) return i;
        }
        return victim_frame;
        break;
    }
    return 0;
}
// usage: ./memSim <reference-sequence-file.txt> <FRAMES> <PRA>
// reference-sequence-file.txt: a text file containing list of logical memory addresses (integers)
// FRAMES: number of frames in the memory FRAMES (integer <= 256 and > 0)
// PRA: “FIFO” or “LRU” or “OPT”
int main(int argc, char *argv[]) {
    parse_arguments(argc, argv);

    // allocate physical memory
    physical_mem = malloc(num_frames * FRAME_SIZE);
    if (!physical_mem) {
        perror("Error allocating physical memory");
        exit(EXIT_FAILURE);
    }
    
    // allocate frame to page
    frame_to_page = malloc(num_frames * sizeof(int));
    if (!frame_to_page) {
        perror("Error allocating frame to page");
        free(physical_mem);
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_frames; i++)
        frame_to_page[i] = -1;

    fifo_queue = malloc(num_frames * FRAME_SIZE);
    if (!fifo_queue) {
        perror("Error allocating FIFO queue");
        free(physical_mem);
        free(frame_to_page);
        exit(EXIT_FAILURE);
    }
    fifo_head = fifo_tail = 0;

    FILE *input_file = fopen(input_path, "r");
    if (!input_file) {
        perror("Error opening input file");
        free(physical_mem);
        free(frame_to_page);
        free(fifo_queue);
        exit(EXIT_FAILURE);
    }

    FILE *backing_store = fopen("BACKING_STORE.bin", "rb");
    if (!backing_store) {
        perror("Error opening backing store");
        free(physical_mem);
        free(frame_to_page);
        free(fifo_queue);
        exit(EXIT_FAILURE);
    }

    int empty_frames = num_frames; // start at all empty

    // vars for tracking
    int addr_cnt = 0;
    int tlb_hit_cnt = 0;
    int tlb_miss_cnt = 0;
    int page_fault_cnt = 0;

    int tlb_idx = 0; // tlb FIFO index, increment when used, then %= 16

    const size_t line_len = 32; // 32 bytes
    char line[line_len]; // buffer to hold line
    num_requests = 0; // total numvber of page requests, used for OPT

    int last_used[num_frames];
    for (int i = 0; i < num_frames; i++){
        last_used[i] = 0;
    }

    if (algorithm == OPT) { // get sequence of requests only once so we can limit reads
        //get len of input file to malloc page_requests
        int cap = 4;
        page_requests = malloc(cap * sizeof(unsigned int));
        if (!page_requests) {
            perror("Error allocating page request array for OPT");
            free(physical_mem);
            free(frame_to_page);
            free(fifo_queue);
            exit(EXIT_FAILURE);
        }
        while (fgets(line, line_len, input_file)){
            if (num_requests == cap) {
                cap *=2;
                unsigned int *tmp = realloc(page_requests, cap * sizeof(*page_requests));
                if (!tmp){
                    perror("Error reallocating page request array for OPT");
                    free(page_requests);
                    free(physical_mem);
                    free(frame_to_page);
                    free(fifo_queue);
                    exit(EXIT_FAILURE);
                }
                page_requests = tmp;
            }
            page_requests[num_requests] = (atoi(line) >> 8) & 0xFF;
            num_requests++;
        }
        // try to shrink to i only
        unsigned int *tmp = realloc(page_requests, num_requests * sizeof(*page_requests));
        if (tmp) page_requests = tmp;
        rewind(input_file); // go back to beginning
    }

    while(fgets(line, line_len, input_file)) {
        line[strcspn(line, "\n")] = '\0';
        addr_cnt++;
        // extract address
        int virtual_addr = atoi(line); // convert ascii to int, ignore upper 16 bits
        int page = (virtual_addr >> 8) & 0xFF; // upper 8
        int offset = virtual_addr & 0xFF; // lower 8

        //check if in TLB
        unsigned int frame;
        int tlb_entry = in_tlb(page);

        if (tlb_entry >= 0) { // TLB hit
            frame = tlb[tlb_entry].frame_number;
            tlb_hit_cnt++;
        }

        else { // TLB miss
            tlb_miss_cnt++;
            //check if in page table
            PageTableEntry *page_entry = &page_table[page];

            if (page_entry->present) //page hit
                frame = page_entry->frame_number;

            else { // page fault, load from backing store
                page_fault_cnt++;

                if (empty_frames > 0) { // once all are full, can never have empty frames
                    frame = num_frames - empty_frames;
                    long file_offset = (long)page * 256;
                    long mem_offset  = (long)frame * 256;
                    fseek(backing_store, file_offset, SEEK_SET);
                    size_t n = fread(physical_mem + mem_offset, 1, 256, backing_store);
                    if (n != FRAME_SIZE) {
                        perror("Error reading from backing store");
                        free(physical_mem);
                        free(frame_to_page);
                        free(page_requests);
                        free(fifo_queue);
                        exit(EXIT_FAILURE);
                    }
                    frame_to_page[frame] = page; // used so we can invalidate the page if we evict its frame
                    empty_frames--;
                }

                else { //replacement time
                    // TODO: mark old page as not present, replace frame, and update frame_to_page
                    int victim_frame = pick_victim(last_used);
                    int victim_page = frame_to_page[victim_frame];
                    page_table[victim_page].present = 0; // invalidate old page
                    tlb_entry = in_tlb(victim_page);
                    tlb[tlb_entry].valid = 0;

                    long file_offset = (long)page * 256;
                    long mem_offset  = (long)victim_frame * 256;
                    fseek(backing_store, file_offset, SEEK_SET);
                    size_t n = fread(physical_mem + mem_offset, 1, 256, backing_store);
                    if (n != FRAME_SIZE) {
                        perror("Error reading from backing store");
                        free(physical_mem);
                        free(frame_to_page);
                        free(page_requests);
                        free(fifo_queue);
                        exit(EXIT_FAILURE);
                    }

                    frame_to_page[victim_frame] = page; // update frame to page mapping
                    frame = victim_frame;
                }
                page_entry->present = 1;
                page_entry->frame_number = frame;
                if (algorithm == FIFO) 
                    fifo_push(frame); // add new frame to FIFO queue if page fault occurs
            }

            
            tlb[tlb_idx].page_number = page; // replace in TLB using FIFO
            tlb[tlb_idx].valid = 1;
            tlb[tlb_idx].frame_number = frame;
            
            tlb_idx = (tlb_idx + 1) % 16; // increment and rotate back when full
        }
        // update last used for LRU
        last_used[frame] = time; 
        
        //compute physical addr
        int phys_addr = frame * FRAME_SIZE;
        unsigned char frame_data[FRAME_SIZE];
        memcpy(&frame_data, &physical_mem[phys_addr], FRAME_SIZE); //get whole frame
        int value = (signed char)frame_data[offset]; //get specified byte
        // print virtual address, value of byte, frame number
        printf("%s, %d, %u, ", line, value, frame);
        // print entire frame
        for (int i = 0; i < FRAME_SIZE; i++) {
            printf("%02X", frame_data[i]);
        }        
        printf("\n");

        time++;
    }

    print_output(addr_cnt, page_fault_cnt, tlb_hit_cnt, tlb_miss_cnt);

    free(physical_mem);
    free(frame_to_page);
    free(page_requests);
    free(fifo_queue);
    return 0;
}
