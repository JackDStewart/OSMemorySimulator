#include <stdlib.h>
#include <stdio.h>

// Constants
#define PAGE_SIZE 256   // Size of a page in bytes
#define PAGE_TABLE_SIZE 256 // Number of entries in the page table
#define TLB_SIZE 16     // Number of entries in the TLB


typedef enum {
    FIFO,  // First-Come, First-Served
    LRU,  // Least Recently Used
    OPT,  // Optimal Page Replacement
} Algorithm;

typedef struct {
    int page_number;
    int frame_number;
    int valid; // 1 if the entry is valid, 0 otherwise
} TLBEntry;

typedef struct {
    int frame_number;
    int present; // 1 if the entry is present, 0 otherwise
} PageTableEntry;

// Globals
char *input_file;
static int num_frames;
static Algorithm algorithm;
static TLBEntry tlb[TLB_SIZE];
static PageTableEntry page_table[PAGE_TABLE_SIZE];
static unsigned char **physical_mem  = NULL;


// Function prototypes
void parse_arguments(int argc, char *argv[]);
void print_output();





// parse arguments and set global variables
void parse_arguments(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <FRAMES> <PRA>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    input_file = argv[1];
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
void print_output(){
    return;
}



// usage: ./memSim <reference-sequence-file.txt> <FRAMES> <PRA>
// reference-sequence-file.txt: a text file containing list of logical memory addresses (integers)
// FRAMES: number of frames in the memory FRAMES (integer <= 256 and > 0)
// PRA: “FIFO” or “LRU” or “OPT”
int main(int argc, char *argv[]) {

    parse_arguments(argc, argv);

    // allocate physical memory
    

    print_output();

    return 0;
}