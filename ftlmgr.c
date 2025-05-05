// 주의사항
// 1. hybridmapping.h에 정의되어 있는 상수 변수를 우선적으로 사용해야 함 
// (예를 들면, PAGES_PER_BLOCK의 상수값은 채점 시에 변경할 수 있으므로 반드시 이 상수 변수를 사용해야 함)
// 2. hybridmapping.h에 필요한 상수 변수가 정의되어 있지 않을 경우 본인이 이 파일에서 만들어서 사용하면 됨
// 3. 새로운 data structure가 필요하면 이 파일에서 정의해서 쓰기 바람(hybridmapping.h에 추가하면 안됨)

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "hybridmapping.h"

// fdevicedriver.c의 함수 선언
extern int fdd_read(int ppn, char *pagebuf);
extern int fdd_write(int ppn, char *pagebuf);
extern int fdd_erase(int pbn);

// FTL 함수 선언
void ftl_open(void);
void ftl_write(int lsn, char *sectorbuf);
int ftl_read(int lsn, char *sectorbuf);
void ftl_print(void);

// Free block list node structure
typedef struct free_block_node {
    int pbn;  // Physical block number
    struct free_block_node* next;
} free_block_node;

// Address mapping table entry structure
typedef struct {
    int pbn;         // Physical block number
    int last_offset; // Last written page offset in the block
} mapping_entry;

// Global variables
static mapping_entry* mapping_table;  // Address mapping table
static free_block_node* free_list;    // Free block list
static int total_blocks;              // Total number of blocks

// Function to initialize free block list
static void init_free_list() {
    free_list = NULL;
    // Create nodes in ascending order (0 to BLOCKS_PER_DEVICE-1)
    for (int i = BLOCKS_PER_DEVICE - 1; i >= 0; i--) {
        free_block_node* new_node = (free_block_node*)malloc(sizeof(free_block_node));
        if (!new_node) {
            perror("Failed to allocate free block node");
            exit(1);
        }
        new_node->pbn = i;
        new_node->next = free_list;
        free_list = new_node;
    }
}

// Function to get a free block from the list
static int get_free_block() {
    if (!free_list) return -1;  // No free blocks available
    
    free_block_node* node = free_list;
    int pbn = node->pbn;
    free_list = node->next;
    free(node);
    return pbn;
}

// Function to add a block back to free list
static void add_to_free_list(int pbn) {
    free_block_node* new_node = (free_block_node*)malloc(sizeof(free_block_node));
    if (!new_node) {
        perror("Failed to allocate free block node");
        exit(1);
    }
    new_node->pbn = pbn;
    new_node->next = free_list;
    free_list = new_node;
}

//
// flash memory를 처음 사용할 때 필요한 초기화 작업, 예를 들면 address mapping table에 대한
// 초기화 등의 작업을 수행한다. 따라서, 첫 번째 ftl_write() 또는 ftl_read()가 호출되기 전에
// file system에 의해 반드시 먼저 호출이 되어야 한다.
//
void ftl_open()
{
    // Calculate total number of logical blocks
    total_blocks = DATAPAGES_PER_DEVICE / PAGES_PER_BLOCK;
    
    // Allocate and initialize mapping table
    mapping_table = (mapping_entry*)malloc(total_blocks * sizeof(mapping_entry));
    if (!mapping_table) {
        perror("Failed to allocate mapping table");
        exit(1);
    }
    
    // Initialize all entries to unmapped state
    for (int i = 0; i < total_blocks; i++) {
        mapping_table[i].pbn = -1;
        mapping_table[i].last_offset = -1;
    }
    
    // Initialize free block list
    init_free_list();
}

//
// 이 함수를 호출하는 쪽(file system)에서 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 함
// (즉, 이 함수에서 메모리를 할당 받으면 안됨)
//
int ftl_read(int lsn, char *sectorbuf)
{
    int lbn = lsn / PAGES_PER_BLOCK;
    int offset = lsn % PAGES_PER_BLOCK;
    char pagebuf[PAGE_SIZE];
    int pbn, ppn;
    int latest_offset = -1;
    char latest_data[SECTOR_SIZE];
    
    // Check if logical block is mapped
    if (mapping_table[lbn].pbn == -1) {
        memset(sectorbuf, 0xFF, SECTOR_SIZE);
        return 1;
    }
    
    pbn = mapping_table[lbn].pbn;
    
    // Check all pages in the block to find the latest version
    for (int i = 0; i <= mapping_table[lbn].last_offset; i++) {
        ppn = pbn * PAGES_PER_BLOCK + i;
        if (fdd_read(ppn, pagebuf) == 1) {
            int page_lsn;
            memcpy(&page_lsn, pagebuf + SECTOR_SIZE, sizeof(int));
            
            if (page_lsn == lsn) {
                if (i > latest_offset) {
                    latest_offset = i;
                    memcpy(latest_data, pagebuf, SECTOR_SIZE);
                }
            }
        }
    }
    
    if (latest_offset != -1) {
        memcpy(sectorbuf, latest_data, SECTOR_SIZE);
        return 1;
    }
    
    // If no valid data found, return 0xFF
    memset(sectorbuf, 0xFF, SECTOR_SIZE);
    return 1;
}

//
// 이 함수를 호출하는 쪽(file system)에서 이미 sectorbuf가 가리키는 곳에 512B의 메모리가 할당되어 있어야 함
// (즉, 이 함수에서 메모리를 할당 받으면 안됨)
//
void ftl_write(int lsn, char *sectorbuf)
{
    int lbn = lsn / PAGES_PER_BLOCK;
    int offset = lsn % PAGES_PER_BLOCK;
    char pagebuf[PAGE_SIZE];
    int pbn, ppn;
    
    // Check if logical block is mapped
    if (mapping_table[lbn].pbn == -1) {
        // Block not mapped, allocate a new block
        pbn = get_free_block();
        if (pbn == -1) {
            perror("No free blocks available");
            exit(1);
        }
        mapping_table[lbn].pbn = pbn;
        mapping_table[lbn].last_offset = -1;
    } else {
        pbn = mapping_table[lbn].pbn;
    }
    
    // Check if we need block replacement
    if (mapping_table[lbn].last_offset >= PAGES_PER_BLOCK - 1) {
        // Need block replacement
        int new_pbn = get_free_block();
        if (new_pbn == -1) {
            perror("No free blocks available for replacement");
            exit(1);
        }
        
        // 각 LSN의 최신 offset을 저장할 배열
        int* latest_offsets = (int*)malloc(PAGES_PER_BLOCK * sizeof(int));
        if (!latest_offsets) {
            perror("Failed to allocate latest_offsets array");
            exit(1);
        }
        // 초기화: -1은 해당 LSN이 아직 발견되지 않음을 의미
        for (int i = 0; i < PAGES_PER_BLOCK; i++) {
            latest_offsets[i] = -1;
        }
        
        // 첫 번째 패스: 각 LSN의 최신 offset 찾기 (역순으로 검사)
        for (int i = mapping_table[lbn].last_offset; i >= 0; i--) {
            int old_ppn = pbn * PAGES_PER_BLOCK + i;
            if (fdd_read(old_ppn, pagebuf) == 1) {
                int page_lsn;
                memcpy(&page_lsn, pagebuf + SECTOR_SIZE, sizeof(int));
                int offset = page_lsn % PAGES_PER_BLOCK;
                
                // 해당 LBN의 LSN이고 아직 발견되지 않은 경우에만 업데이트
                if (page_lsn / PAGES_PER_BLOCK == lbn && latest_offsets[offset] == -1) {
                    latest_offsets[offset] = i;
                }
            }
        }
        
        // 두 번째 패스: 최신 데이터만 복사
        int new_offset = 0;
        for (int i = 0; i < PAGES_PER_BLOCK; i++) {
            if (latest_offsets[i] != -1) {
                int old_ppn = pbn * PAGES_PER_BLOCK + latest_offsets[i];
                if (fdd_read(old_ppn, pagebuf) == 1) {
                    int new_ppn = new_pbn * PAGES_PER_BLOCK + new_offset;
                    
                    if (fdd_write(new_ppn, pagebuf) != 1) {
                        free(latest_offsets);
                        add_to_free_list(new_pbn);
                        perror("Failed to write during block replacement");
                        exit(1);
                    }
                    new_offset++;
                }
            }
        }
        
        free(latest_offsets);
        
        // Erase old block and add to free list
        fdd_erase(pbn);
        add_to_free_list(pbn);
        
        // Update mapping
        pbn = new_pbn;
        mapping_table[lbn].pbn = pbn;
        mapping_table[lbn].last_offset = new_offset - 1;
    }
    
    // Prepare page buffer
    memset(pagebuf, 0xFF, PAGE_SIZE);  // Initialize with 0xFF
    memcpy(pagebuf, sectorbuf, SECTOR_SIZE);  // Copy sector data
    memcpy(pagebuf + SECTOR_SIZE, &lsn, sizeof(int));  // Store LSN in spare area
    
    // Write to next available page
    ppn = pbn * PAGES_PER_BLOCK + (mapping_table[lbn].last_offset + 1);
    
    if (fdd_write(ppn, pagebuf) != 1) {
        perror("Failed to write page");
        exit(1);
    }
    
    // Update last_offset
    mapping_table[lbn].last_offset++;
}

// 
// Address mapping table 등을 출력하는 함수이며, 출력 포맷은 과제 설명서 참조
// 출력 포맷을 반드시 지켜야 하며, 그렇지 않는 경우 채점시 불이익을 받을 수 있음
//
void ftl_print()
{
    printf("lbn pbn last_offset\n");
    for (int i = 0; i < total_blocks; i++) {
        printf("%d %d %d\n", i, mapping_table[i].pbn, mapping_table[i].last_offset);
    }
}