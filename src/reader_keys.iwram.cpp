// The keys read at each VBlank (IWRAM, ARM code: it runs in the interrupt),
// then Butano's own VBlank interrupt.
#include "reader_keys.h"

#include "bn_core.h"

extern "C" {
// libugba's interrupt table (Butano's bn_hw_irq.h): VBlank is entry 0.
using irq_handler = void (*)(void);
extern irq_handler IRQ_VectorTable[];
}

namespace rkey {
namespace {

alignas(4) BN_DATA_EWRAM_BSS KeyLatch key_latch;
irq_handler butano_vblank = nullptr;

unsigned keys_now() { return ~*reinterpret_cast<volatile uint16_t*>(0x04000130) & ALL; }

void vblank()
{
    key_latch.sample(keys_now());
    butano_vblank();
}

}

KeyLatch& latch() { return key_latch; }

void start_latch()
{
    volatile uint16_t& ime = *reinterpret_cast<volatile uint16_t*>(0x04000208);
    const uint16_t was = ime;
    ime = 0;
    key_latch.start(keys_now());
    if(IRQ_VectorTable[0] != vblank) {
        butano_vblank = IRQ_VectorTable[0];
        IRQ_VectorTable[0] = vblank;
    }
    ime = was;
}

}
