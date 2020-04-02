#include "mama/mamaManagedEnvironment.h"
#include "mama/mamaEnvConnection.h"


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
            // TODO: keep or delete?
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
mama_status mamaEnvConnection_addSessionToList(wList list, mmeSession* session)
{
    if (list == NULL) {
        return MAMA_STATUS_NULL_ARG;
    }

    /* allocate the element, at this point not in the list */
    mmeSession** newElement = (mmeSession**)list_allocate_element(list);

    if (newElement == NULL) {
        return MAMA_STATUS_NOMEM;
    }

    *newElement = session;

    /* Also make a void reference to the list in the session to make it easy
     * to remove the session.
     */
    session->m_listEntry = newElement;

    /* push_back element into list with all values filled in */
    list_push_back(list, (void*)newElement);

    return MAMA_STATUS_OK;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvConnection_deallocate(mmeConnection* connection)
{
    /* Reset the return code, in the calls below all objects will be
     * destroyed but the return code will be preserved so that it
     * indicates the first error.
     */
    mama_status ret = MAMA_STATUS_OK;

    /* Destroy the session timer. */
    if (connection->m_sessionTimer != NULL) {
        mama_status mtd = mamaTimer_destroy(connection->m_sessionTimer);
        if (ret == MAMA_STATUS_OK) {
            ret = mtd;
        }
    }

    /* Destroy the dispatcher. */
    if (connection->m_objectDispatcher != NULL) {
        mama_status mdd = mamaDispatcher_destroy(connection->m_objectDispatcher);
        if (ret == MAMA_STATUS_OK) {
            ret = mdd;
        }
    }

    /* Destroy the object queue. */
    if (connection->m_objectQueue != NULL) {
        mama_status mqd = mamaQueue_destroyWait(connection->m_objectQueue);
        if (ret == MAMA_STATUS_OK) {
            ret = mqd;
        }
    }

    /* Delete the sessions array. */
    if ((connection->m_sessions != NULL) && (connection->m_sessions != INVALID_LIST)) {
        list_destroy(connection->m_sessions, (wListCallback)NULL, NULL);
    }

    /* Delete the destroyed sessions array. */
    if ((connection->m_destroyedSessions != NULL) && (connection->m_destroyedSessions != INVALID_LIST)) {
        list_destroy(connection->m_destroyedSessions, (wListCallback)NULL, NULL);
    }

    /* Destroy the synchronization object. */
    if (connection->m_destroySynch != NULL) {
        mama_status mde = mamaEnv_destroyEvent(connection->m_destroySynch);
        if (ret == MAMA_STATUS_OK) {
            ret = mde;
        }
    }

    /* Free the connection object. */
    free(connection);

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvConnection_enumerateList(wList list, mamaEnv_listCallback cb, void* closure)
{
    /* The following utility structure will hold the closure and original callback. */
    WombatListUtility utility;
    memset(&utility, 0, sizeof(utility));
    utility.m_callback = cb;
    utility.m_closure = closure;

    /* Invoke the original for each. */
    list_for_each(list, (wListCallback)mamaEnvConnection_onWombatListCallback, (void*)&utility);

    /* Return the code from the structure. */
    return utility.m_error;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvConnection_removeSessionFromList(wList list, mmeSession* session)
{
    /* Remove the session from the list. */
    list_remove_element(list, session->m_listEntry);

    /* Free the associated memory. */
    list_free_element(list, session->m_listEntry);

    /* Clear the list entry in the session. */
    session->m_listEntry = NULL;

    return MAMA_STATUS_OK;
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvConnection_onDestroyAllSessions(mamaQueue queue, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the connection object. */
    mmeConnection* connection = (mmeConnection*)closure;
    if (connection != NULL) {
        /* Get the number of sessions to be destroyed. */
        long numberSessions = list_size(connection->m_destroyedSessions);
        ret = MAMA_STATUS_OK;
        while ((ret == MAMA_STATUS_OK) && (numberSessions > 0)) {
            /* Enumerate all of the sessions in the list and destroy each one in turn. */
            ret = mamaEnvConnection_enumerateList(connection->m_destroyedSessions, (mamaEnv_listCallback)mamaEnvConnection_onSessionDestroyListEnumerate, NULL);

            /* Get the number of sessions remaining in the list. */
            numberSessions = list_size(connection->m_destroyedSessions);
            /* If there are still any open sessions then sleep. */
            if (numberSessions > 0) {
                ret = mamaQueue_timedDispatch(queue, 10);
            }
        }

        /* Set the event to release the waiting thread. */
        ret = mamaEnv_setEvent(connection->m_destroySynch);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - onDestroyAllSessions with connection %p completed with code %X.", connection, ret);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvConnection_onSessionCreate(mamaQueue queue, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to a utility object. */
    CreationUtilityStructure* utility = (CreationUtilityStructure*)closure;
    if (utility != NULL) {
        /* Complete creation of the session. */
        utility->m_status = mamaEnvSession_create(utility->m_bridge, utility->m_session);

        /* Signal the synchronization event. */
        ret = mamaEnv_setEvent(utility->m_synch);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINE, "MamaEnv - onSessionCreate completed with code %X.", ret);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvConnection_onSessionDestroyListEnumerate(wList list, void* element, void* closure)
{
    /* Errors. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Get the session itself. */
    mmeSession** session = (mmeSession**)element;
    if (session != NULL) {
        /* Check to see if the session can be destroyed. */
        ret = mamaEnvSession_canDestroy(*session);
        if (ret == MAMA_STATUS_OK) {
            /* Remove the session from the destroyed list. */
            ret = mamaEnvConnection_removeSessionFromList(list, *session);
            if (ret == MAMA_STATUS_OK) {
                /* Destroy the session. */
                ret = mamaEnvSession_destroy(*session);
                if (ret == MAMA_STATUS_OK) {
                    /* Deallocate all associated memory. */
                    ret = mamaEnvSession_deallocate(*session);
                }
            }
        }

        /* Return a success code in this case. */
        else if (ret == MAMA_STATUS_QUEUE_OPEN_OBJECTS) {
            ret = MAMA_STATUS_OK;
        }
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvConnection_onSessionListEnumerate(wList list, void* element, void* closure)
{
    /* Errors. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the connection object. */
    mmeConnection* connection = (mmeConnection*)closure;

    /* Get the session itself. */
    mmeSession** session = (mmeSession**)element;
    if (session != NULL) {
        /* Destroy the session. */
        ret = mamaEnv_destroySession(connection, *session);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvConnection_onSessionTimerTick(mamaTimer timer, void* closure)
{
    /* Errors. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the connection object. */
    mmeConnection* connection = (mmeConnection*)closure;
    if (connection != NULL) {
        /* Enumerate all of the sessions in the destroy list. */
        ret = mamaEnvConnection_enumerateList(connection->m_destroyedSessions, (mamaEnv_listCallback)mamaEnvConnection_onSessionDestroyListEnumerate, NULL);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINEST, "MamaEnv - onSessionTimerTick with connection %p completed with code %X.", connection, ret);
}


//////////////////////////////////////////////////////////////////////////////
void mamaEnvConnection_onWombatListCallback(wList list, void* element, void* closure)
{
    /* Cast the closure to the utility structure. */
    WombatListUtility* utility = (WombatListUtility*)closure;

    /* Invoke the original callback function. */
    utility->m_error = (utility->m_callback)(list, element, utility->m_closure);
}
