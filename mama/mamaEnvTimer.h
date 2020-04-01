#ifndef MAMAENVTIMER_H
#define MAMAENVTIMER_H

#include <wlock.h>
#include "mamaEnvGeneral.h"

/* This structure contains all of the information used to create a timer, it will be
 * passed as a closure to the object queue.
 */
typedef struct mmeTimer
{
    // The mama timer, this MUST appear first in the structure, so that mmeTimer and mamaTimer
    // can be used interchangeably.
    // (see 6.7.2.1-13. A pointer to a structure object, suitably converted, points to its initial member)
    mamaTimer m_timer;

    /* The original callback function, this will be set to NULL whenever the timer
     * destroy is enqueued.
     */
    mamaTimerCb m_callback;

    /* The closure data provided to the create function. */
    void* m_closure;

    /* This is used to control access to the callback function. */
    wLock m_lock;

    /* This is used to control scheduling so destroy can grab m_lock. */
    wLock m_destroyLock;

} mmeTimer;


mama_status mamaEnvTimer_allocate(mamaTimerCb callback, void* closure, mmeTimer** timer);
mama_status mamaEnvTimer_destroy(mmeTimer* timer);
mama_status mamaEnvTimer_shutdown(mmeTimer* timer);

void MAMACALLTYPE mamaEnvTimer_onTimerDestroy(mamaQueue queue, void* closure);
void MAMACALLTYPE mamaEnvTimer_onTimerTick(mamaTimer timer, void* closure);

#endif
