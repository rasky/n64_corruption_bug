#include <libdragon.h>
#include "prime.h"

#define PI_STATUS_DMA_BUSY ( 1 << 0 )
#define PI_STATUS_IO_BUSY  ( 1 << 1 )

#define PI_BUSY_FLAGS (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)
#define SP_BUSY_FLAGS (SP_STATUS_DMA_BUSY | SP_STATUS_IO_BUSY)
#define SP_DEVICE_TO_ADDR ((device == PRIME_DEVICE_DMEM) ? SP_DMEM : SP_IMEM)

//                         ROM start    64 MiB      4 KiB
#define FLASHCART_IO_ADDR (0x10000000 + 0x4000000 - 0x1000)

static __attribute__((aligned(16))) uint8_t prime_ram[MAX_PRIME_SIZE];

void prime_init(uint8_t device, uint8_t dir, uint16_t size_bytes, uint32_t pattern) {
    volatile uint32_t* write_addr;
    if(dir == PRIME_DIR_RDRAM2RCP){
        write_addr = (uint32_t*)prime_ram;
    }else if(device == PRIME_DEVICE_PI){
        while(*PI_STATUS & PI_BUSY_FLAGS);
        write_addr = (uint32_t*)(FLASHCART_IO_ADDR | 0xA0000000);
    }else{
        while(*SP_STATUS & SP_BUSY_FLAGS);
        write_addr = SP_DEVICE_TO_ADDR;
    }
    size_bytes &= ~4;
    while(size_bytes > 0){
        *write_addr = pattern;
        ++write_addr;
        size_bytes -= 4;
    }
    if(dir == PRIME_DIR_RDRAM2RCP){
        data_cache_writeback_invalidate_all();
    }
}

void prime_go(uint8_t device, uint8_t dir, uint32_t size_bytes, uint32_t count) {
    volatile uint32_t* status_reg;
    uint32_t busy_flags;
    volatile uint32_t* rdram_addr_reg;
    volatile uint32_t* other_addr_reg;
    uint32_t other_addr;
    volatile uint32_t* sizem1_trigger_reg;
    if(device == PRIME_DEVICE_PI){
        status_reg = PI_STATUS;
        busy_flags = PI_BUSY_FLAGS;
        rdram_addr_reg = PI_DRAM_ADDR;
        other_addr_reg = PI_CART_ADDR;
        other_addr = FLASHCART_IO_ADDR;
        sizem1_trigger_reg = (dir == PRIME_DIR_RDRAM2RCP) ? PI_WR_LEN : PI_RD_LEN;
    }else{
        status_reg = SP_STATUS;
        busy_flags = SP_BUSY_FLAGS;
        rdram_addr_reg = SP_DMA_RAMADDR;
        other_addr_reg = SP_DMA_SPADDR;
        other_addr = (uint32_t)(SP_DEVICE_TO_ADDR);
        sizem1_trigger_reg = (dir == PRIME_DIR_RDRAM2RCP) ? SP_DMA_RDLEN : SP_DMA_WRLEN;
    }
    
    while(*status_reg & busy_flags);
    
    while(count--){
        
        *rdram_addr_reg = (uint32_t)prime_ram;
        *other_addr_reg = other_addr;
        MEMORY_BARRIER();
        *sizem1_trigger_reg = size_bytes - 1;
        MEMORY_BARRIER();
        
        while(*status_reg & busy_flags);
    }
}
