//
// This file contains the implementations for the public API functions defined in mamaManagedEnvironment.h
//

// API definition
#include "mama/mamaManagedEnvironment.h"

// internal definitions
#include "mama/mamaEnvConnection.h"
#include "mama/mamaEnvSession.h"
#include "mama/mamaEnvSubscription.h"
#include "mama/mamaEnvInbox.h"
#include "mama/mamaEnvTimer.h"

//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_createConnection(mamaBridge bridge, mamaEnvConnection* connection)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((bridge != NULL) && (connection != NULL)) {
        /* Create the connection structure. */
        mmeConnection* localConnection = (mamaEnvConnection)calloc(1, sizeof(mmeConnection));
        ret = MAMA_STATUS_NOMEM;
        if (localConnection != NULL) {
            /* Create the synch object used when shutting down. */
            ret = mamaEnv_createEvent(&localConnection->m_destroySynch);
            if (ret == MAMA_STATUS_OK) {
                /* Create the object queue. */
                ret = mamaQueue_create(&localConnection->m_objectQueue, bridge);
                if (ret == MAMA_STATUS_OK) {
                    /* Set the queue name. */
                    ret = mamaQueue_setQueueName(localConnection->m_objectQueue, "Object queue");
                    if (ret == MAMA_STATUS_OK) {
                        /* Create the dispatcher. */
                        ret = mamaDispatcher_create(&localConnection->m_objectDispatcher, localConnection->m_objectQueue);
                        if (ret == MAMA_STATUS_OK) {
                            /* Create the sessions array. */
                            ret = MAMA_STATUS_NOMEM;
                            localConnection->m_sessions = list_create(sizeof(mmeSession*));
                            if (localConnection->m_sessions != INVALID_LIST) {
                                /* Create the destroyed sessions array. */
                                localConnection->m_destroyedSessions = list_create(sizeof(mmeSession*));
                                if (localConnection->m_destroyedSessions != NULL) {
                                    /* Create the session timer. */
                                    ret = mamaTimer_create(&localConnection->m_sessionTimer, localConnection->m_objectQueue, (mamaTimerCb)mamaEnvConnection_onSessionTimerTick, MEVC_SESSION_TIMER_INTERVAL, (void*)localConnection);
                                    if (ret == MAMA_STATUS_OK) {
                                        /* Save arguments in the structure. */
                                        localConnection->m_bridge = bridge;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /* Write a mama log. */
            mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - createConnection with connection %p completed with code %X.", localConnection, ret);

            /* If something went wrong the delete all allocated memory. */
            if (ret != MAMA_STATUS_OK) {
                mamaEnvConnection_deallocate(localConnection);
                localConnection = NULL;
            }
        }

        /* Return the connection object. */
        *connection = (mamaEnvConnection)localConnection;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_destroyConnection(mamaEnvConnection connection)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (connection != NULL) {
        /* Cast the connection object. */
        mmeConnection* envConnection = (mmeConnection*)connection;

        /* Destroy the session timer first to prevent it ticking. */
        ret = mamaTimer_destroy(envConnection->m_sessionTimer);
        if (ret == MAMA_STATUS_OK) {
            /* Clear the timer to prevent it being destroyed again. */
            envConnection->m_sessionTimer = NULL;

            /* Enumerate all the open sessions and destroy each one. */
            ret = mamaEnvConnection_enumerateList(envConnection->m_sessions, (mamaEnv_listCallback)mamaEnvConnection_onSessionListEnumerate, (void*)envConnection);
            if (ret == MAMA_STATUS_OK) {
                /* Now the timer has stopped enqueue an event on the object queue that
                 * actually performs the destruction of the session.
                 * This will be the last event that goes onto the queue.
                 */
                ret = mamaQueue_enqueueEvent(envConnection->m_objectQueue, (mamaQueueEventCB)mamaEnvConnection_onDestroyAllSessions, (void*)envConnection);
                if (ret == MAMA_STATUS_OK) {
                    /* Wait until the final message has been processed. */
                    ret = mamaEnv_waitEvent(envConnection->m_destroySynch);
                    if (ret == MAMA_STATUS_OK) {
                        /* Free the connection object. */
                        ret = mamaEnvConnection_deallocate(envConnection);
                    }
                }
            }
        }

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - destroyConnection with connection %p completed with code %X.", envConnection, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_createSession(mamaEnvConnection connection, mamaEnvSession* session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((connection != NULL) && (session != NULL)) {
        /* Cast the connection object. */
        mmeConnection* envConnection = (mmeConnection*)connection;

        /* Allocate a new session object. */
        mmeSession* localSession = NULL;
        ret = mamaEnvSession_allocate(&localSession);
        if (ret == MAMA_STATUS_OK) {
            /* Use a utility structure to pass the data to the callback. */
            CreationUtilityStructure utility;
            memset(&utility, 0, sizeof(utility));
            utility.m_bridge = envConnection->m_bridge;
            utility.m_session = localSession;
            utility.m_status = MAMA_STATUS_OK;

            /* Create a synchronization object. */
            ret = mamaEnv_createEvent(&utility.m_synch);
            if (ret == MAMA_STATUS_OK) {
                /* Enqueue an event on the object queue to complete creation of the session. */
                ret = mamaQueue_enqueueEvent(envConnection->m_objectQueue, (mamaQueueEventCB)mamaEnvConnection_onSessionCreate, &utility);
                if (ret == MAMA_STATUS_OK) {
                    /* Wait on the session being created. */
                    ret = mamaEnv_waitEvent(utility.m_synch);
                    if (ret == MAMA_STATUS_OK) {
                        /* Extract the status code from the utility structure. */
                        ret = utility.m_status;
                        if (ret == MAMA_STATUS_OK) {
                            /* Add the session to the sessions array. */
                            ret = mamaEnvConnection_addSessionToList(envConnection->m_sessions, localSession);
                        }
                    }
                }

                /* Destroy the synchronization object. */
                mamaEnv_destroyEvent(utility.m_synch);
            }

            /* Write a mama log. */
            mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - createSession with connection %p and session %p completed with code %X.", envConnection, localSession, ret);

            /* If something went wrong then delete the session. */
            if (ret != MAMA_STATUS_OK) {
                mamaEnvSession_deallocate(localSession);
                localSession = NULL;
            }
        }

        /* Write the session object back. */
        *session = (mamaEnvSession)localSession;
    }

    return ret;
}



//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_shutdownSession(mamaEnvConnection connection, mamaEnvSession session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((connection != NULL) && (session != NULL)) {
        /* Cast the pointers. */
        mmeConnection* envConnection = (mmeConnection*)connection;
        mmeSession* envSession = (mmeSession*)session;

        /* Stop dispatching events */
        ret = mamaEnvSession_deactivate(envSession);

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - shutdownSession with connection %p and session %p completed with code %X.", envConnection, envSession, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_destroySession(mamaEnvConnection connection, mamaEnvSession session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((connection != NULL) && (session != NULL)) {
        /* Cast the pointers. */
        mmeConnection* envConnection = (mmeConnection*)connection;
        mmeSession* envSession = (mmeSession*)session;

        /* Remove the session from the session list. */
        ret = mamaEnvConnection_removeSessionFromList(envConnection->m_sessions, envSession);
        if (ret == MAMA_STATUS_OK) {
            /* Stop dispatching events */
            //mamaEnvSession_deactivate(envSession);

            /* Destroy all of the outstanding events in the session. */
            ret = mamaEnvSession_destroyAllEvents(envSession);
            if (ret == MAMA_STATUS_OK) {
                /* Add the session to the destroy list. */
                ret = mamaEnvConnection_addSessionToList(envConnection->m_destroyedSessions, envSession);
            }
        }

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - destroySession with connection %p and session %p completed with code %X.", envConnection, envSession, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_createBasicSubscription(const mamaMsgCallbacks* callback, void* closure, mamaEnvSession session, const char* symbol, mamaTransport transport, mamaSubscription* subscription)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((callback != NULL) && (session != NULL) && (symbol != NULL) && (transport != NULL) && (subscription != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* Format a callback structure to hold all the function pointers. */
        mmeSubscriptionCallback localCallback;
        memset(&localCallback, 0, sizeof(mmeSubscriptionCallback));
        localCallback.m_onCreate = callback->onCreate;
        localCallback.m_onError = callback->onError;
        localCallback.m_onMsgBasic = callback->onMsg;

        /* Create the subscription. */
        ret = mamaEnvSession_createSubscription(&localCallback, closure, envSession, NULL, symbol, transport, Basic, subscription);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_createWildcardSubscription(const mamaWildCardMsgCallbacks* callback, void* closure, mamaEnvSession session, const char* source, const char* symbol, mamaTransport transport, mamaSubscription* subscription)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((callback != NULL) && (session != NULL) && (symbol != NULL) && (transport != NULL) && (subscription != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* Format a callback structure to hold all the function pointers. */
        mmeSubscriptionCallback localCallback;
        memset(&localCallback, 0, sizeof(mmeSubscriptionCallback));
        localCallback.m_onCreate = callback->onCreate;
        localCallback.m_onError = callback->onError;
        localCallback.m_onMsgWildcard = callback->onMsg;

        /* Create the subscription. */
        ret = mamaEnvSession_createSubscription(&localCallback, closure, envSession, source, symbol, transport, Wildcard, subscription);
    }

    return ret;
}

mama_status mamaEnv_shutdownSubscriptionCallback(void* data, void* closure)
{
    mmeSubscription* subscription = (mmeSubscription*)data;
    return mamaEnvSubscription_shutdown(subscription);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_shutdownSubscription(mamaEnvSession session, mamaSubscription subscription)
{
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (subscription != NULL)) {
        // use _for here to ensure that callback is done under control of map's mutex
        // this avoids a race when an event object is being shutdown at the same time it is being deleted from its parent session
        ret = synchronizedMap_for(mamaEnv_shutdownSubscriptionCallback, subscription, session->m_subscriptions, NULL);

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - shutdownSubscription with session %p and subscription %p completed with code %X.", session, subscription, ret);
    }

    return ret;
}

//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_destroySubscription(mamaEnvSession session, mamaSubscription subscription)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (subscription != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* Remove the subscription from the map. */
        mmeSubscription* envSubscription = NULL;
        ret = synchronizedMap_remove((void*)subscription, envSession->m_subscriptions, (void**)&envSubscription);

        /* If the subscription wasn't in the map then it has already been removed, still return OK. */
        if (MAMA_STATUS_INVALID_ARG == ret) {
            ret = MAMA_STATUS_OK;
        }

        else if ((MAMA_STATUS_OK == ret) && (envSubscription != NULL)) {
            /* Call the session level function to clear the subscription callbacks and enqueue the
          * mama object destruction.
          */
            ret = mamaEnvSession_destroySubscription(envSession, envSubscription);
        }

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - destroySubscription with session %p and subscription %p completed with code %X.", envSession, envSubscription, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_createInbox(void* closure, mamaInboxErrorCallback errorCallback, mamaInboxMsgCallback msgCallback, mamaEnvSession session, mamaTransport transport, mamaInbox* result)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (result != NULL) && (transport != NULL) && (msgCallback != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* The mama inbox will be returned. */
        mamaInbox localMamaInbox = NULL;

        /* Allocate a new inbox object. */
        mmeInbox* inbox = NULL;
        ret = mamaEnvInbox_allocate(closure, errorCallback, msgCallback, &inbox);
        if (ret == MAMA_STATUS_OK) {
            /* Create the mama inbox, saving it in the environment structure, note that the
             * inbox structure will be passed as the closure data.
             */
            ret = mamaInbox_create(
                &inbox->m_inbox,
                transport,
                envSession->m_queue,
                (mamaInboxMsgCallback)mamaEnvInbox_onMessageCallback,
                (mamaInboxErrorCallback)mamaEnvInbox_onErrorCallback,
                (void*)inbox);
            if (ret == MAMA_STATUS_OK) {
                /* Add the inbox to the map. */
                ret = synchronizedMap_insert((void*)inbox, (void*)inbox->m_inbox, envSession->m_inboxes);
                if (ret == MAMA_STATUS_OK) {
                    /* Extract the mama inbox from the inbox object. */
                    localMamaInbox = inbox->m_inbox;
                }
            }

            /* Write a mama log. */
            mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - createInbox with session %p and inbox %p completed with code %X.", envSession, inbox, ret);

            /* If something went wrong then destroy the inbox. */
            if (ret != MAMA_STATUS_OK) {
                mamaEnvInbox_destroy(inbox);
            }
        }

        /* Return the mama inbox. */
        *result = localMamaInbox;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_shutdownInboxCallback(void* data, void* closure)
{
    mmeInbox* inbox = (mmeInbox*)data;
    return mamaEnvInbox_shutdown(inbox);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_shutdownInbox(mamaEnvSession session, mamaInbox inbox)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (inbox != NULL)) {
        // use _for here to ensure that callback is done under control of map's mutex
        // this avoids a race when an event object is being shutdown at the same time it is being deleted from its parent session
        ret = synchronizedMap_for(mamaEnv_shutdownInboxCallback, inbox, session->m_inboxes, NULL);

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - shutdownInbox with session %p and inbox %p completed with code %X.", session, inbox, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_destroyInbox(mamaEnvSession session, mamaInbox inbox)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (inbox != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* Remove the inbox from the map. */
        mmeInbox* envInbox = NULL;
        ret = synchronizedMap_remove((void*)inbox, envSession->m_inboxes, (void**)&envInbox);

        /* If the inbox wasn't in the map then it has already been removed, still return OK. */
        if (MAMA_STATUS_INVALID_ARG == ret) {
            ret = MAMA_STATUS_OK;
        }

        else if ((MAMA_STATUS_OK == ret) && (envInbox != NULL)) {
            /* Call the session level function to clear the inbox callbacks and enqueue the
          * mama object destruction.
          */
            ret = mamaEnvSession_destroyInbox(envSession, envInbox);
        }

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - destroyInbox with session %p and inbox %p completed with code %X.", envSession, envInbox, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_createTimer(mamaTimerCb callback, void* closure, mama_f64_t interval, mamaEnvSession session, mamaTimer* result)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (result != NULL) && (callback != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* The mama timer will be returned. */
        mamaTimer localMamaTimer = NULL;

        /* Allocate a new timer object. */
        mmeTimer* timer = NULL;
        ret = mamaEnvTimer_allocate(callback, closure, &timer);
        if (ret == MAMA_STATUS_OK) {
            /* Create the mama timer. */
            ret = mamaTimer_create(&timer->m_timer, envSession->m_queue, (mamaTimerCb)mamaEnvTimer_onTimerTick, interval, (void*)timer);
            if (ret == MAMA_STATUS_OK) {
                /* Add the timer to the map. */
                ret = synchronizedMap_insert((void*)timer, (void*)timer->m_timer, envSession->m_timers);
                if (ret == MAMA_STATUS_OK) {
                    /* Extract the mama timer from the session timer object. */
                    localMamaTimer = timer->m_timer;
                }
            }

            /* Write a mama log. */
            mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - createTimer with session %p and timer %p completed with code %X.", envSession, timer, ret);

            /* If something went wrong then destroy the timer. */
            if (ret != MAMA_STATUS_OK) {
                mamaEnvTimer_destroy(timer);
            }
        }

        /* Return the mama timer. */
        *result = localMamaTimer;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_shutdownTimerCallback(void* data, void* closure)
{
    mmeTimer* timer = (mmeTimer*)data;
    return mamaEnvTimer_shutdown(timer);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_shutdownTimer(mamaEnvSession session, mamaTimer timer)
{
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (timer != NULL)) {
        // use _for here to ensure that callback is done under control of map's mutex
        // this avoids a race when an event object is being shutdown at the same time it is being deleted from its parent session
        ret = synchronizedMap_for(mamaEnv_shutdownTimerCallback, timer, session->m_timers, NULL);

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - shutdownTimer with session %p and timer %p completed with code %X.", session, timer, ret);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnv_destroyTimer(mamaEnvSession session, mamaTimer timer)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if ((session != NULL) && (timer != NULL)) {
        /* Cast the session. */
        mmeSession* envSession = (mmeSession*)session;

        /* Remove the timer from the map. */
        mmeTimer* envTimer = NULL;
        ret = synchronizedMap_remove((void*)timer, envSession->m_timers, (void**)&envTimer);

        /* If the timer wasn't in the map then it has already been removed, still return OK. */
        if (MAMA_STATUS_INVALID_ARG == ret) {
            ret = MAMA_STATUS_OK;
        }

        else if ((MAMA_STATUS_OK == ret) && (envTimer != NULL)) {
            /* Call the session level function to clear the timer callbacks and enqueue the
          * mama object destruction.
          */
            ret = mamaEnvSession_destroyTimer(envSession, envTimer);
        }

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - destroyTimer with session %p and timer %p completed with code %X.", envSession, envTimer, ret);
    }

    return ret;
}

