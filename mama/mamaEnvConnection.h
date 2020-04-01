#ifndef MAMAENVCONNECTION_H
#define MAMAENVCONNECTION_H

#include <list.h>
#include "mamaEnvEvent.h"
#include "mamaEnvSession.h"

/* The interval at which the session timer ticks. */
#define MEVC_SESSION_TIMER_INTERVAL 1

/* The amount of time to wait on all sessions being destroyed before an error is
 * returned is set to 10 seconds.
 */
#define MEVC_DESTROY_WAIT_TIME 10


/* This structure holds all the information required for a connection. */
typedef struct mmeConnection
{
    /* The bridge. */
    mamaBridge m_bridge;

    /* Used when destroying the connection. */
    MamaEvent* m_destroySynch;

    /* The dispatcher used to pump the object queue. */
    mamaDispatcher m_objectDispatcher;

    /* This queue is used to create and destroy all objects within all session in the connection. */
    mamaQueue m_objectQueue;

    /* The list of active sessions. */
    wList m_sessions;

    /* The list of destroyed sessions. */
    wList m_destroyedSessions;

    /* The session timer is used to destroy sessions. */
    mamaTimer m_sessionTimer;

} mmeConnection;


/* This structure is used to whole utility data when creating sessions. */
typedef struct CreationUtilityStructure
{
    /* The bridge needed to create the session queue. */
    mamaBridge m_bridge;

    /* The session itself. */
    mmeSession* m_session;

    /* The returned status code. */
    mama_status m_status;

    /* Used to synchronize creating the session. */
    MamaEvent* m_synch;

} CreationUtilityStructure;



/* Defines a callback function used when enumerating wombat lists. */
typedef mama_status (*mamaEnv_listCallback)(wList list, void* element, void* closure);

/* This utility structure is used when enumerating wombat lists. */
typedef struct WombatListUtility
{
    /* The callback function to invoke for each entry in the list. */
    mamaEnv_listCallback m_callback;

    /* The closure. */
    void* m_closure;

    /* The error code. */
    mama_status m_error;

} WombatListUtility;


mama_status mamaEnvConnection_addSessionToList(wList list, mmeSession* session);
mama_status mamaEnvConnection_enumerateList(wList list, mamaEnv_listCallback cb, void* closure);
mama_status mamaEnvConnection_deallocate(mmeConnection* connection);
mama_status mamaEnvConnection_removeSessionFromList(wList list, mmeSession* session);

void MAMACALLTYPE mamaEnvConnection_onDestroyAllSessions(mamaQueue queue, void* closure);
void MAMACALLTYPE mamaEnvConnection_onSessionCreate(mamaQueue queue, void* closure);
mama_status mamaEnvConnection_onSessionDestroyListEnumerate(wList list, void* element, void* closure);
mama_status mamaEnvConnection_onSessionListEnumerate(wList list, void* element, void* closure);
void mamaEnvConnection_onWombatListCallback(wList list, void* element, void* closure);

void MAMACALLTYPE mamaEnvConnection_onSessionTimerTick(mamaTimer timer, void* closure);



#endif