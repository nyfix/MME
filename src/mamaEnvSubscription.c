#include "mama/mamaEnvSubscription.h"


/* This static struture holds all of the basic callback function pointers. */
static mamaMsgCallbacks sg_basicCallbacks =
    {
        (wombat_subscriptionCreateCB)mamaEnvSubscription_onCreateBasic,
        (wombat_subscriptionErrorCB)mamaEnvSubscription_onErrorBasic,
        (wombat_subscriptionOnMsgCB)mamaEnvSubscription_onMsgBasic,
        NULL,
        NULL,
        NULL,
        (wombat_subscriptionDestroyCB)mamaEnvSubscription_onDestroy
};


/* This static struture holds all of the wildcard function pointers. */
static mamaWildCardMsgCallbacks sg_wildcardCallbacks =
    {
        (wombat_subscriptionCreateCB)mamaEnvSubscription_onCreateBasic,
        (wombat_subscriptionErrorCB)mamaEnvSubscription_onErrorBasic,
        (wombat_subscriptionWildCardOnMsgCB)mamaEnvSubscription_onMsgWildcard,
        (wombat_subscriptionDestroyCB)mamaEnvSubscription_onDestroy
};


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSubscription_allocate(mmeSubscriptionCallback* callback, void* closure, mmeSubscription** subscription)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NOMEM;

    /* Allocate a new subscription object. */
    mmeSubscription* localSubscription = (mmeSubscription*)calloc(1, sizeof(mmeSubscription));
    if (localSubscription != NULL) {
        /* Create the lock object. */
        ret = MAMA_STATUS_PLATFORM;
        localSubscription->m_lock = wlock_create();
        if (localSubscription->m_lock != NULL) {
            /* Allocate the mama subscription. */
            ret = mamaSubscription_allocate(&localSubscription->m_subscription);
            if (ret == MAMA_STATUS_OK) {
                /* Save arguments in member variables. */
                localSubscription->m_closure = closure;
                memcpy(&localSubscription->m_callback, callback, sizeof(mmeSubscriptionCallback));
            }
        }

        /* If something went wrong then deallocate the subscription. */
        if (ret != MAMA_STATUS_OK) {
            mamaEnvSubscription_deallocate(localSubscription);
            localSubscription = NULL;
        }
    }

    /* Write back data. */
    *subscription = localSubscription;

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSubscription_deallocate(mmeSubscription* subscription)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;
    if (subscription != NULL) {
        /* Deallocate the mama subscription. */
        if (subscription->m_subscription != NULL) {
            mama_status msd = mamaSubscription_deallocate(subscription->m_subscription);
            if (ret == MAMA_STATUS_OK) {
                ret = msd;
            }

            /* Clear the member variable. */
            subscription->m_subscription = NULL;
        }

        /* Destroy the lock. */
        if (subscription->m_lock != NULL) {
            wlock_unlock(subscription->m_lock);
            int rc = wlock_destroy(subscription->m_lock);
            if (rc != 0) {
                mama_log(MAMA_LOG_LEVEL_SEVERE, "mamaEnvSubscription_deallocate -- got non-zero rc=%d destoying m_lock", rc);
            }
            subscription->m_lock = NULL;
        }

        /* Free the object. */
        free(subscription);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSubscription_create(mamaQueue queue, const char* source, mmeSubscription* subscription, const char* symbol, mamaTransport transport, mmeSubscriptionType type)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_INVALID_ARG;

    /* Create a subscription of the appropriate type. */
    switch (type) {
    case Basic:

        ret = mamaSubscription_createBasic(
            subscription->m_subscription,
            transport,
            queue,
            &sg_basicCallbacks,
            symbol,
            (void*)subscription);
        break;

    case Wildcard:
        ret = mamaSubscription_createBasicWildCard(
            subscription->m_subscription,
            transport,
            queue,
            &sg_wildcardCallbacks,
            source,
            symbol,
            (void*)subscription);
        break;
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSubscription_shutdown(mmeSubscription* subscription)
{
    wlock_lock(subscription->m_lock);
    subscription->m_callback.m_onMsgBasic = NULL;
    subscription->m_callback.m_onMsgWildcard = NULL;
    wlock_unlock(subscription->m_lock);

    return MAMA_STATUS_OK;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvSubscription_destroy(mmeSubscription* subscription)
{
    /* Destroy the mama subscription, it will be deallocated later on when the subscription
     * destroy callback fires.
     */
    return mamaSubscription_destroy(subscription->m_subscription);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvSubscription_onSubscriptionDestroy(mamaQueue queue, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to a subscription object. */
    mmeSubscription* subscription = (mmeSubscription*)closure;
    if (subscription != NULL) {
        /* Acquire the lock in case anything else is using the subscription at the moment. */
        wlock_lock(subscription->m_lock);

        /* Destroy it. */
        ret = mamaEnvSubscription_destroy(subscription);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - onSubscriptionDestroy with subscription %p completed with code %X.", subscription, ret);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvSubscription_onCreateBasic(mamaSubscription subscription, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the environment subscription object. */
    mmeSubscription* envSubscription = (mmeSubscription*)closure;
    if (envSubscription != NULL) {
        /* Lock the subscription. */
        wlock_lock(envSubscription->m_lock);

        /* Invoke the original callback function. */
        if (envSubscription->m_callback.m_onCreate != NULL) {
            (envSubscription->m_callback.m_onCreate)(subscription, envSubscription->m_closure);
        }

        /* Unlock the subscription. */
        wlock_unlock(envSubscription->m_lock);

        /* Function succeeded. */
        ret = MAMA_STATUS_OK;
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - Subscription_onCreateBasic with subscription %p completed with code %X.", envSubscription, ret);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvSubscription_onDestroy(mamaSubscription subscription, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the environment subscription object. */
    mmeSubscription* envSubscription = (mmeSubscription*)closure;
    if (envSubscription != NULL) {
        /* Deallocate the object now that the subscription has gone. */
        ret = mamaEnvSubscription_deallocate(envSubscription);
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - Subscription_onDestroy with subscription %p completed with code %X.", envSubscription, ret);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvSubscription_onErrorBasic(mamaSubscription subscription, mama_status status, void* platformError, const char* subject, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to the environment subscription object. */
    mmeSubscription* envSubscription = (mmeSubscription*)closure;
    if (envSubscription != NULL) {
        /* Lock the subscription. */
        wlock_lock(envSubscription->m_lock);

        /* Invoke the original callback function. */
        if (envSubscription->m_callback.m_onError != NULL) {
            (envSubscription->m_callback.m_onError)(subscription, status, platformError, subject, envSubscription->m_closure);
        }

        /* Unlock the subscription. */
        wlock_unlock(envSubscription->m_lock);

        /* Function succeeded. */
        ret = MAMA_STATUS_OK;
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - Subscription_onErrorBasic with subscription %p completed with code %X and error %X.", envSubscription, ret, status);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvSubscription_onMsgBasic(mamaSubscription subscription, mamaMsg message, void* closure, void* itemClosure)
{
    /* Cast the closure to the environment subscription object. */
    mmeSubscription* envSubscription = (mmeSubscription*)closure;
    if (envSubscription != NULL) {
        /* Lock the subscription. */
        wlock_lock(envSubscription->m_lock);

        /* Invoke the original callback function. */
        if (envSubscription->m_callback.m_onMsgBasic != NULL) {
            (envSubscription->m_callback.m_onMsgBasic)(subscription, message, envSubscription->m_closure, itemClosure);
        }

        /* Unlock the subscription. */
        wlock_unlock(envSubscription->m_lock);
    }
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvSubscription_onMsgWildcard(mamaSubscription subscription, mamaMsg message, const char* topic, void* closure, void* itemClosure)
{
    /* Cast the closure to the environment subscription object. */
    mmeSubscription* envSubscription = (mmeSubscription*)closure;
    if (envSubscription != NULL) {
        /* Lock the subscription. */
        wlock_lock(envSubscription->m_lock);

        /* Invoke the original callback function. */
        if (envSubscription->m_callback.m_onMsgWildcard != NULL) {
            (envSubscription->m_callback.m_onMsgWildcard)(subscription, message, topic, envSubscription->m_closure, itemClosure);
        }

        /* Unlock the subscription. */
        wlock_unlock(envSubscription->m_lock);
    }
}
