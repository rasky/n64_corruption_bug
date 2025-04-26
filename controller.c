#include <libdragon.h>
#include "controller.h"

typedef struct SI_regs_s {
   /** @brief Uncached address in RAM where data should be found */
   volatile void * DRAM_addr;
   /** @brief Address to read when copying from PIF RAM */
   volatile void * PIF_addr_read;
   /** @brief Reserved word */
   uint32_t reserved1;
   /** @brief Reserved word */
   uint32_t reserved2;
   /** @brief Address to write when copying to PIF RAM */
   volatile void * PIF_addr_write;
   /** @brief Reserved word */
   uint32_t reserved3;
   /** @brief SI status, including DMA busy and IO busy */
   uint32_t status;
} SI_regs_t;

static volatile struct SI_regs_s * const SI_regs = (struct SI_regs_s *)0xa4800000;
static void * const PIF_RAM = (void *)0x1fc007c0;
#define SI_STATUS_DMA_BUSY  ( 1 << 0 )
#define SI_STATUS_IO_BUSY   ( 1 << 1 )

__attribute__((aligned(16))) uint64_t si_send_data[8] = {
    0xff01040100000000,
    0xfe00000000000000,
    0, 0, 0, 0, 0, 1
};
__attribute__((aligned(16))) uint64_t si_recv_data[16];

uint16_t poll_controller() {
    while (SI_regs->status & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY));
    SI_regs->status = 0;
    MEMORY_BARRIER();
    SI_regs->DRAM_addr = si_send_data;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_write = PIF_RAM;
    while(!((*MI_INTERRUPT & *MI_MASK) & MI_INTERRUPT_SI));
    SI_regs->status = 0;
    MEMORY_BARRIER();
    SI_regs->DRAM_addr = si_recv_data;
    MEMORY_BARRIER();
    SI_regs->PIF_addr_read = PIF_RAM;
    while(!((*MI_INTERRUPT & *MI_MASK) & MI_INTERRUPT_SI));
    SI_regs->status = 0;
    data_cache_hit_invalidate(si_recv_data, 64);
    return ((uint16_t*)si_recv_data)[2];
}
