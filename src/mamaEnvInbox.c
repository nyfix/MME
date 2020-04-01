#include "mama/mamaEnvInbox.h"


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvInbox_allocate(void* closure, mamaInboxErrorCallback errorCallback, mamaInboxMsgCallback msgCallback, mmeInbox** inbox)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NOMEM;

    /* Allocate a new inbox object. */
    mmeInbox* envInbox = (mmeInbox*)calloc(1, sizeof(mmeInbox));
    if (envInbox != NULL) {
        /* Create the lock object. */
        ret = MAMA_STATUS_PLATFORM;
        envInbox->m_lock = wlock_create();
        if (envInbox->m_lock != NULL) {
            /* Save arguments in member variables. */
            envInbox->m_closure = closure;
            envInbox->m_errorCallback = errorCallback;
            envInbox->m_msgCallback = msgCallback;

            /* Success. */
            ret = MAMA_STATUS_OK;
        }

        /* If something went wrong then destroy the timer. */
        if (ret != MAMA_STATUS_OK) {
            mamaEnvInbox_destroy(envInbox);
            envInbox = NULL;
        }
    }

    /* Write back data. */
    *inbox = envInbox;

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvInbox_shutdown(mmeInbox* inbox)
{
    wlock_lock(inbox->m_lock);
    inbox->m_msgCallback = NULL;
    wlock_unlock(inbox->m_lock);

    return MAMA_STATUS_OK;
}

//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvInbox_destroy(mmeInbox* inbox)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;
    if (inbox != NULL) {
        /* Destroy the mama inbox. */
        if (inbox->m_inbox != NULL) {
            ret = mamaInbox_destroy(inbox->m_inbox);
            inbox->m_inbox = NULL;
        }

        /* Destroy the lock. */
        if (inbox->m_lock != NULL) {
            wlock_unlock(inbox->m_lock);
            int rc = wlock_destroy(inbox->m_lock);
            if (rc != 0) {
                mama_log(MAMA_LOG_LEVEL_SEVERE, "mamaEnvInbox_destroy -- got non-zero rc=%d destoying m_lock", rc);
            }
            inbox->m_lock = NULL;
        }

        /* Free the inbox object. */
        free(inbox);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvInbox_onMessageCallback(mamaMsg msg, void* closure)
{
    /* Cast the closure to the inbox object. */
    mmeInbox* envInbox = (mmeInbox*)closure;
    if (envInbox != NULL) {
        /* Lock the inbox. */
        wlock_lock(envInbox->m_lock);

        /* Invoke the original callback function. */
        if (envInbox->m_msgCallback != NULL) {
            (envInbox->m_msgCallback)(msg, envInbox->m_closure);
        }

        /* Unlock the inbox. */
        wlock_unlock(envInbox->m_lock);
    }
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvInbox_onInboxDestroy(mamaQueue queue, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to an inbox object. */
    mmeInbox* inbox = (mmeInbox*)closure;
    if (inbox != NULL) {
        /* Acquire the lock in case anything else is using the timer at the moment. */
        wlock_lock(inbox->m_lock);

        /* Destroy it. */
        ret = mamaEnvInbox_destroy(inbox);

        /* Note that we do not release the lock as it has now been destroyed. */
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - onInboxDestroy with inbox %p completed with code %X.", inbox, ret);
}

//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvInbox_onErrorCallback(mama_status status, void* closure)
{
    /* Cast the closure to the inbox object. */
    mmeInbox* envInbox = (mmeInbox*)closure;
    if (envInbox != NULL) {
        /* Lock the inbox. */
        wlock_lock(envInbox->m_lock);

        /* Invoke the original callback function. */
        if (envInbox->m_errorCallback != NULL) {
            (envInbox->m_errorCallback)(status, envInbox->m_closure);
        }

        /* Unlock the inbox. */
        wlock_unlock(envInbox->m_lock);
    }
}
