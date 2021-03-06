#include "mama/mamaEnvSession.h"


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


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_allocate(mmeSession** session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NOMEM;

    /* Allocate the session structure. */
    mmeSession* localSession = (mmeSession*)calloc(1, sizeof(mmeSession));
    if (localSession != NULL) {
        /* Create the inbox map. */
        localSession->m_inboxes = synchronizedMap_create();
        if (localSession->m_inboxes != NULL) {
            /* Create the timer map. */
            localSession->m_timers = synchronizedMap_create();
            if (localSession->m_timers != NULL) {
                /* Create the subscriptions map. */
                localSession->m_subscriptions = synchronizedMap_create();
                if (localSession->m_subscriptions != NULL) {
                    /* Function success. */
                    ret = MAMA_STATUS_OK;
                }
            }
        }
    }

    /* Write the session back. */
    *session = localSession;

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_canDestroy(mmeSession* session)
{
    /* The session can be destroyed if there are no open objects on the queue. */
    return mamaQueue_canDestroy(session->m_queue);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_create(mamaBridge bridge, mmeSession* session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;

    /* Create the queue. */
    ret = mamaQueue_create(&session->m_queue, bridge);
    if (ret == MAMA_STATUS_OK) {
        /* Format the queue name using the session pointer. */
        char queueName[64] = "";
        sprintf(queueName, "MME Session queue %p", (void*)session);

        /* Set the name of the queue. */
        ret = mamaQueue_setQueueName(session->m_queue, queueName);
        if (ret == MAMA_STATUS_OK) {
            /* Create the dispatcher and start dispatching messages. */
            ret = mamaDispatcher_create(&session->m_dispatcher, session->m_queue);
        }
    }

    /* If something went wrong destroy the session. */
    if (ret != MAMA_STATUS_OK) {
        mamaEnvSession_destroy(session);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_deallocate(mmeSession* session)
{
    /* Delete the inbox map. */
    if (session->m_inboxes != NULL) {
        synchronizedMap_destroy(session->m_inboxes);
        session->m_inboxes = NULL;
    }

    /* Delete the subscriptions map. */
    if (session->m_subscriptions != NULL) {
        synchronizedMap_destroy(session->m_subscriptions);
        session->m_subscriptions = NULL;
    }

    /* Delete the timer map. */
    if (session->m_timers != NULL) {
        synchronizedMap_destroy(session->m_timers);
        session->m_timers = NULL;
    }

    /* Free the session structure. */
    free(session);

    return MAMA_STATUS_OK;
}



//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_destroy(mmeSession* session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;

    /* Destroy the dispatcher which will stop all events. */
    if (session->m_dispatcher != NULL) {
        mama_status mdd = mamaDispatcher_destroy(session->m_dispatcher);
        session->m_dispatcher = NULL;
        if (ret == MAMA_STATUS_OK) {
            ret = mdd;
        }
    }

    /* Destroy the queue. */
    if (session->m_queue != NULL) {
        mama_status mqd = mamaQueue_destroy(session->m_queue);
        session->m_queue = NULL;
        if (ret == MAMA_STATUS_OK) {
            ret = mqd;
        }
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_destroyAllEvents(mmeSession* session)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;

    /* Destroy all the inboxes, note that the return code will be preserved. */
    mama_status ra = synchronizedMap_removeAll((synchronizedMap_Callback)mamaEnvSession_onDestroyAllInboxesCallback, (void*)session, session->m_inboxes);
    if (MAMA_STATUS_OK == ret) {
        ret = ra;
    }

    /* Destroy all the subscriptions. */
    ra = synchronizedMap_removeAll((synchronizedMap_Callback)mamaEnvSession_onDestroyAllSubscriptionsCallback, (void*)session, session->m_subscriptions);
    if (MAMA_STATUS_OK == ret) {
        ret = ra;
    }

    /* Destroy all the timers. */
    ra = synchronizedMap_removeAll((synchronizedMap_Callback)mamaEnvSession_onDestroyAllTimersCallback, (void*)session, session->m_timers);
    if (MAMA_STATUS_OK == ret) {
        ret = ra;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_createSubscription(mmeSubscriptionCallback* callback, void* closure, mmeSession* session, const char* source, const char* symbol, mamaTransport transport, mmeSubscriptionType type, mamaSubscription* result)
{
    /* The mama subscription will be returned. */
    mamaSubscription localMamaSubscription = NULL;

    /* Allocate a new subscription object. */
    mmeSubscription* subscription = NULL;
    mama_status ret = mamaEnvSubscription_allocate(callback, closure, &subscription);
    if (ret == MAMA_STATUS_OK) {
        /* Create a subscription of the appropriate type. */
        ret = mamaEnvSubscription_create(session->m_queue, source, subscription, symbol, transport, type);
        if (MAMA_STATUS_OK == ret) {
            /* Add the subscription to the map. */
            ret = synchronizedMap_insert((void*)subscription, (void*)subscription->m_subscription, session->m_subscriptions);
            if (MAMA_STATUS_OK == ret) {
                /* Extract the mama subscrtiption from the object. */
                localMamaSubscription = subscription->m_subscription;
            }
        }

        /* Write a mama log. */
        mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - createSubscription with session %p and subscription %p completed with code %X.", session, subscription, ret);

        /* If something went wrong then destroy the subscription. */
        if (ret != MAMA_STATUS_OK) {
            mamaEnvSubscription_destroy(subscription);
        }
    }

    /* Return the mama timer. */
    *result = localMamaSubscription;

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_destroyInbox(mmeSession* envSession, mmeInbox* envInbox)
{
    /* Lock the inbox. */
    wlock_lock(envInbox->m_lock);

    /* Clear the callback functions to prevent them being fired. */
    envInbox->m_closure = NULL;
    envInbox->m_errorCallback = NULL;
    envInbox->m_msgCallback = NULL;

    /* Unlock the inbox, this must be done before the event in enqueued. */
    wlock_unlock(envInbox->m_lock);

    mama_log(MAMA_LOG_LEVEL_FINER, "mamaEnvSession_destroyInbox: inbox=%p", envInbox->m_inbox);

    /* Enqueue an event to destroy the inbox on the connection's object queue. */
    return mamaQueue_enqueueEvent(envSession->m_queue, (mamaQueueEventCB)mamaEnvInbox_onInboxDestroy, (void*)envInbox);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_destroySubscription(mmeSession* envSession, mmeSubscription* envSubscription)
{
    /* Lock the subscription. */
    wlock_lock(envSubscription->m_lock);

    /* Clear the callback functions to prevent it being fired. */
    memset(&envSubscription->m_callback, 0, sizeof(mmeSubscriptionCallback));
    envSubscription->m_closure = NULL;

    /* Unlock the subscription, this must be done before the event in enqueued. */
    wlock_unlock(envSubscription->m_lock);

    /* Enqueue an event to destroy the subscription on the connection's object queue. */
    return mamaQueue_enqueueEvent(envSession->m_queue, (mamaQueueEventCB)mamaEnvSubscription_onSubscriptionDestroy, (void*)envSubscription);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_destroyTimer(mmeSession* envSession, mmeTimer* envTimer)
{
    /* Lock the timer. */
    wlock_lock(envTimer->m_destroyLock);
    wlock_lock(envTimer->m_lock);

    /* Clear the callback function to prevent it being fired. */
    envTimer->m_callback = NULL;
    envTimer->m_closure = NULL;

    /* Unlock the timer, this must be done before the event is enqueued. */
    wlock_unlock(envTimer->m_lock);
    wlock_unlock(envTimer->m_destroyLock);

    /* Enqueue an event to destroy the timer on the session queue. */
    return mamaQueue_enqueueEvent(envSession->m_queue, (mamaQueueEventCB)mamaEnvTimer_onTimerDestroy, (void*)envTimer);
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_deactivate(mmeSession* envSession)
{
    /* optional function -- ignore return */
    mamaQueue_deactivate(envSession->m_queue);

    return MAMA_STATUS_OK;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_onDestroyAllInboxesCallback(void* data, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the session. */
    mmeSession* session = (mmeSession*)closure;

    /* Cast the data to an inbox. */
    mmeInbox* inbox = (mmeInbox*)data;
    if ((session != NULL) && (inbox != NULL)) {

        /* Destroy the inbox, note that the session function must be called as the inbox will
		 * have already been removed from the map.
		 */
        ret = mamaEnvSession_destroyInbox(session, inbox);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - onDestroyAllInboxesCallback with session %p and inbox %p completed with code %X.", session, inbox, ret);

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_onDestroyAllSubscriptionsCallback(void* data, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the session. */
    mmeSession* session = (mmeSession*)closure;

    /* Cast the data to a subscription. */
    mmeSubscription* subscription = (mmeSubscription*)data;
    if ((session != NULL) && (subscription != NULL)) {
        /* Destroy the subscription, note that the session function must be called as the subscription will
		 * have already been removed from the map.
		 */
        ret = mamaEnvSession_destroySubscription(session, subscription);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - onDestroyAllSubscriptionsCallback with session %p and subscription %p completed with code %X.", session, subscription, ret);

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSession_onDestroyAllTimersCallback(void* data, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the session. */
    mmeSession* session = (mmeSession*)closure;

    /* Cast the data to a timer. */
    mmeTimer* timer = (mmeTimer*)data;
    if ((session != NULL) && (timer != NULL)) {
        /* Destroy the timer, note that the session function must be called as the timer will
		 * have already been removed from the map.
		 */
        ret = mamaEnvSession_destroyTimer(session, timer);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - onDestroyAllTimersCallback with session %p and timer %p completed with code %X.", session, timer, ret);

    return ret;
}
