#ifndef MAMAENVSUBSCRIPTION_H
#define MAMAENVSUBSCRIPTION_H

#include <wlock.h>
#include "mamaEnvGeneral.h"

/* Indicates the type of subscription. */
typedef enum mmeSubscriptionType
{
    Basic = 1,
    Wildcard = 2
} mmeSubscriptionType;


/* This structure contains the callback function pointers. */
typedef struct mmeSubscriptionCallback
{
    /* onCreate. */
    wombat_subscriptionCreateCB m_onCreate;

    /* onError. */
    wombat_subscriptionErrorCB m_onError;

    /* onMsg for a basic subscription. */
    wombat_subscriptionOnMsgCB m_onMsgBasic;

    /* onMsg for a wildcard subscription. */
    wombat_subscriptionWildCardOnMsgCB m_onMsgWildcard;

} mmeSubscriptionCallback;


/* This structure contains all of the information used to create a subscription, it will be
 * passed as a closure to the object queue.
 */
typedef struct mmeSubscription
{
    // The mama subscription, this MUST appear first in the structure, so that mmeSubscription and mamaSubscription
    // can be used interchangeably.
    // (see 6.7.2.1-13. A pointer to a structure object, suitably converted, points to its initial member)
    mamaSubscription m_subscription;

    /* This union contains the callback function pointers. */
    mmeSubscriptionCallback m_callback;

    /* The closure data provided to the create function. */
    void* m_closure;

    /* This is used to control access to the callback function. */
    wLock m_lock;

} mmeSubscription;

mama_status mamaEnvSubscription_allocate(mmeSubscriptionCallback* callback, void* closure, mmeSubscription** subscription);
mama_status mamaEnvSubscription_deallocate(mmeSubscription* subscription);
mama_status mamaEnvSubscription_create(mamaQueue queue, const char* source, mmeSubscription* subscription, const char* symbol, mamaTransport transport, mmeSubscriptionType type);
mama_status mamaEnvSubscription_destroy(mmeSubscription* subscription);
mama_status mamaEnvSubscription_shutdown(mmeSubscription* subscription);

void MAMACALLTYPE mamaEnvSubscription_onSubscriptionDestroy(mamaQueue queue, void* closure);

void MAMACALLTYPE mamaEnvSubscription_onCreateBasic(mamaSubscription subscription, void* closure);
void MAMACALLTYPE mamaEnvSubscription_onDestroy(mamaSubscription subscription, void* closure);
void MAMACALLTYPE mamaEnvSubscription_onErrorBasic(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure);
void MAMACALLTYPE mamaEnvSubscription_onMsgBasic(mamaSubscription subscription, mamaMsg message, void* closure, void* itemClosure);
void MAMACALLTYPE mamaEnvSubscription_onMsgWildcard(mamaSubscription subscription, mamaMsg message, const char* topic, void* closure, void* itemClosure);

#endif
