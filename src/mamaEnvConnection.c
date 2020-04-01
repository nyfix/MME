#include "mama/mamaManagedEnvironment.h"
#include "mama/mamaEnvConnection.h"


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
