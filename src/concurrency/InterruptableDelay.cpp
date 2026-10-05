#include "concurrency/InterruptableDelay.h"
#include "configuration.h"

#ifdef SIM_MESH
// sim-mesh's idle (simradio-portduino): ends on a wake, a DIO1 rise or input on the API's sockets.
void rnode_idle(uint32_t max_ms);
void rnode_wake();
#endif

namespace concurrency
{

InterruptableDelay::InterruptableDelay() {}

InterruptableDelay::~InterruptableDelay() {}

/**
 * Returns false if we were interrupted
 */
bool InterruptableDelay::delay(uint32_t msec)
{
#ifdef SIM_MESH
    rnode_idle(msec);
    return false;
#else
    // LOG_DEBUG("delay %u ", msec);

    // sem take will return false if we timed out (i.e. were not interrupted)
    bool r = semaphore.take(msec);

    // LOG_DEBUG("interrupt=%d", r);
    return !r;
#endif
}

void InterruptableDelay::interrupt()
{
#ifdef SIM_MESH
    rnode_wake();
#else
    semaphore.give();
#endif
}

IRAM_ATTR void InterruptableDelay::interruptFromISR(BaseType_t *pxHigherPriorityTaskWoken)
{
#ifdef SIM_MESH
    rnode_wake();
#else
    semaphore.giveFromISR(pxHigherPriorityTaskWoken);
#endif
}

} // namespace concurrency
