#ifndef MAMAENVINBOX_H
#define MAMAENVINBOX_H

#include <wlock.h>
#include "mamaEnvGeneral.h"

/* This structure contains all of the information used to create an inbox, it will be
 * passed as a closure to the object queue.
 */
typedef struct mmeInbox
{
    // The mama inbox, this MUST appear first in the structure, so that mmeInbox and mamaInbox
    // can be used interchangeably.
    // (see 6.7.2.1-13. A pointer to a structure object, suitably converted, points to its initial member)
    mamaInbox m_inbox;

    /* The closure data provided to the create function. */
    void* m_closure;

    /* The original error callback function, this will be set to NULL whenever the
     * inbox destroy is enqueued.
     */
    mamaInboxErrorCallback m_errorCallback;

    /* This is used to control access to the callback function. */
    wLock m_lock;

    /* The original message callback function, this will be set to NULL whenever the
     * inbox destroy is enqueued.
     */
    mamaInboxMsgCallback m_msgCallback;

} mmeInbox;


mama_status mamaEnvInbox_allocate(void* closure, mamaInboxErrorCallback errorCallback, mamaInboxMsgCallback msgCallback, mmeInbox** inbox);
mama_status mamaEnvInbox_destroy(mmeInbox* inbox);
mama_status mamaEnvInbox_shutdown(mmeInbox* inbox);


void MAMACALLTYPE mamaEnvInbox_onErrorCallback(mama_status status, void* closure);
void MAMACALLTYPE mamaEnvInbox_onInboxDestroy(mamaQueue queue, void* closure);
void MAMACALLTYPE mamaEnvInbox_onMessageCallback(mamaMsg msg, void* closure);

#endif
