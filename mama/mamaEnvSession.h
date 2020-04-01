#ifndef MAMAENVSESSION_H
#define MAMAENVSESSION_H

/* ********************************************************** */
/* Includes. */
/* ********************************************************** */
#include "mamaManagedEnvironment.h"
#include "mamaEnvSubscription.h"
#include "mamaEnvInbox.h"
#include "mamaEnvTimer.h"
#include "mamaSynchronizedMap.h"


/* ********************************************************** */
/* Structures. */
/* ********************************************************** */

/* This structure defines a session. */
typedef struct mmeSession
{
    /* The dispatcher used to pump the session queue. */
    mamaDispatcher m_dispatcher;

    /* The queue that all events will be processed on. */
    mamaQueue m_queue;

    /* This map contains all inbox objects. */
    SynchronizedMap* m_inboxes;

    /* This map contains all subscription objects. */
    SynchronizedMap* m_subscriptions;

    /* This map contains all timer objects. */
    SynchronizedMap* m_timers;

    /* cache the position of this session in whichever session list it exists (active/destroyed) */
    void* m_listEntry;

} mmeSession;

mama_status mamaEnvSession_allocate(mmeSession** session);
mama_status mamaEnvSession_canDestroy(mmeSession* session);
mama_status mamaEnvSession_create(mamaBridge bridge, mmeSession* session);
mama_status mamaEnvSession_deallocate(mmeSession* session);
mama_status mamaEnvSession_destroy(mmeSession* session);
mama_status mamaEnvSession_destroyAllEvents(mmeSession* session);
mama_status mamaEnvSession_deactivate(mmeSession* session);


mama_status mamaEnvSession_createSubscription(mmeSubscriptionCallback* callback, void* closure, mmeSession* session, const char* source, const char* symbol, mamaTransport transport, mmeSubscriptionType type, mamaSubscription* result);
mama_status mamaEnvSession_destroySubscription(mmeSession* envSession, mmeSubscription* envSubscription);

mama_status mamaEnvSession_destroyInbox(mmeSession* envSession, mmeInbox* envInbox);
mama_status mamaEnvSession_destroyTimer(mmeSession* envSession, mmeTimer* envTimer);

mama_status mamaEnvSession_onDestroyAllInboxesCallback(void* data, void* closure);
mama_status mamaEnvSession_onDestroyAllSubscriptionsCallback(void* data, void* closure);
mama_status mamaEnvSession_onDestroyAllTimersCallback(void* data, void* closure);

#endif
