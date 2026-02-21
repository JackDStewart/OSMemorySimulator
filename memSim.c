#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

// Constants
#define PAGE_SIZE 256   // Size of a page in bytes
#define PAGE_TABLE_SIZE 256 // Number of entries in the page table
#define FRAME_SIZE 256
#define TLB_SIZE 16     // Number of entries in the TLB


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

// Function prototypes
void parse_arguments(int argc, char *argv[]);
void print_output(int addr_cnt, int page_fault_cnt, int tlb_hit_cnt, int tlb_miss_cnt);
int in_tlb(unsigned int page);


// parse arguments and set global variables
void parse_arguments(int argc, char *argv[]) {
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
void print_output(int addr_cnt, int page_fault_cnt, int tlb_hit_cnt, int tlb_miss_cnt) {
    double tlb_hit_rate = 0.0;
    if ((tlb_hit_cnt + tlb_miss_cnt) > 0) {
        tlb_hit_rate = (double)tlb_hit_cnt / (tlb_hit_cnt + tlb_miss_cnt);
    }
    printf("Number of Translated Addresses = %x\n", addr_cnt);
    printf("Page Faults = %d\n", page_fault_cnt);
    printf("Page Fault Rate = %.2f%%\n", (double)page_fault_cnt / addr_cnt);
    printf("TLB Hits = %d\n", tlb_hit_cnt);
    printf("TLB Misses = %d\n", tlb_miss_cnt);
    printf("TLB Hit Rate = %.2f%%\n", tlb_hit_rate * 100);
}

int in_tlb(unsigned int page){ // returns idx in tlb, -1 if not present
    for (int i = 0; i < TLB_SIZE; i++){
        if (tlb[i].valid && tlb[i].page_number == page)
            return i;
    }
    return -1;
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
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < num_frames; i++)
        frame_to_page[i] = -1;

    
    FILE *input_file = fopen(input_path, "r");
    if (!input_file) {
        perror("Error opening input file");
        exit(EXIT_FAILURE);
    }

    FILE *backing_store = fopen("BACKING_STORE.bin", "r");
    if (!backing_store) {
        perror("Error opening backing store");
        exit(EXIT_FAILURE);
    }

    int empty_frames = num_frames; // start at all empty

    // vars for tracking
    int addr_cnt = 0;
    int tlb_hit_cnt = 0;
    int tlb_miss_cnt = 0;
    int page_fault_cnt = 0;

    int tlb_idx = 0; // tlb FIFO index, increment when used, then %= 16

    char *line = NULL;
    const size_t line_len = 4; // 32 bits -> 4 bytes

    while(fgets(line, line_len, input_file)) {
        addr_cnt++;
        // extract address
        int virtual_addr = atoi(line); // convert ascii to int, ignore upper 16 bits
        int page = (virtual_addr >> 8) & 0xFF; // upper 8
        int offset = virtual_addr & 0xFF; // lower 8

        //check if in TLB
        unsigned int frame;
        int tlb_entry = in_tlb(page);

        if (tlb_entry > 0) { // TLB hit
            frame = tlb[tlb_entry].frame_number;
            tlb_hit_cnt++;
        }

        else { // TLB miss
            tlb_miss_cnt++;
            //check if in page table
            PageTableEntry page_entry = page_table[page];
            if (page_entry.present) { //page hit
                frame = page_entry.frame_number;
            } 
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
                        exit(1);
                    }
                    empty_frames--;
                }
                else { //replacement time
                    switch (algorithm)
                    {
                    case FIFO:
                        /* code */
                        break;

                    case LRU:

                    case OPT:
                    default:
                        break;
                    }
                }
                page_entry.present = 1;
                page_entry.frame_number = frame;
            }

            tlb[tlb_idx].page_number = page; // replace in TLB using FIFO
            tlb[tlb_idx].valid = 1;
            tlb[tlb_idx].frame_number = frame;

            tlb_idx = (tlb_idx + 1) % 16; // increment and rotate back when full
        }
        
        //compute physical addr
        int phys_addr = frame * FRAME_SIZE;
        unsigned char frame_data[FRAME_SIZE];
        memcpy(&frame_data, &physical_mem[phys_addr], FRAME_SIZE); //get whole frame
        int value = frame_data[offset]; //get specified byte
        // print virtual address, value of byte, frame number
        printf("%s, %x, %u, ", line, value, frame);
        // print entire frame
        for (int i = 0; i < FRAME_SIZE; i++) {
            printf("%02x", frame_data[i]);
        }        
        printf("\n");

    }

    print_output(addr_cnt, page_fault_cnt, tlb_hit_cnt, tlb_miss_cnt);

    free(physical_mem);
    free(frame_to_page);
    return 0;
}
