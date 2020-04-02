#include "mama/mamaEnvTimer.h"


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvTimer_allocate(mamaTimerCb callback, void* closure, mmeTimer** timer)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NOMEM;

    /* Allocate a new timer object. */
    mmeTimer* sessionTimer = (mmeTimer*)calloc(1, sizeof(mmeTimer));
    if (sessionTimer != NULL) {
        /* Create the lock objects. */
        ret = MAMA_STATUS_PLATFORM;
        sessionTimer->m_lock = wlock_create();
        if (sessionTimer->m_lock != NULL) {
            /* Save arguments in member variables. */
            sessionTimer->m_callback = callback;
            sessionTimer->m_closure = closure;

            /* Success. */
            ret = MAMA_STATUS_OK;
        }
        sessionTimer->m_destroyLock = NULL;
        if (ret == MAMA_STATUS_OK) {
            sessionTimer->m_destroyLock = wlock_create();
            if (sessionTimer->m_destroyLock != NULL) {
                /* Success. */
                ret = MAMA_STATUS_OK;
            }
        }

        /* If something went wrong then destroy the timer. */
        if (ret != MAMA_STATUS_OK) {
            mamaEnvTimer_destroy(sessionTimer);
            sessionTimer = NULL;
        }
    }

    /* Write back data. */
    *timer = sessionTimer;

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvTimer_destroy(mmeTimer* timer)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;
    if (timer != NULL) {
        /* Destroy the mama timer. */
        if (timer->m_timer != NULL) {
            ret = mamaTimer_destroy(timer->m_timer);
            timer->m_timer = NULL;
        }

        /* Destroy the locks. */
        if (timer->m_lock != NULL) {
            wlock_unlock(timer->m_lock);
            int rc = wlock_destroy(timer->m_lock);
            if (rc != 0) {
                mama_log(MAMA_LOG_LEVEL_SEVERE, "mamaEnvTimer_destroy -- got non-zero rc=%d destoying m_lock", rc);
            }
            timer->m_lock = NULL;
        }
        if (timer->m_destroyLock != NULL) {
            wlock_unlock(timer->m_destroyLock);
            int rc = wlock_destroy(timer->m_destroyLock);
            if (rc != 0) {
                mama_log(MAMA_LOG_LEVEL_SEVERE, "mamaEnvTimer_destroy -- got non-zero rc=%d destoying m_destroyLock", rc);
            }
            timer->m_destroyLock = NULL;
        }

        /* Free the session timer object. */
        free(timer);
    }

    return ret;
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvTimer_onTimerDestroy(mamaQueue queue, void* closure)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;

    /* Cast the closure to a timer object. */
    mmeTimer* timer = (mmeTimer*)closure;
    if (timer != NULL) {
        /* Acquire the lock in case anything else is using the timer at the moment. */
        wlock_lock(timer->m_destroyLock);
        wlock_lock(timer->m_lock);

        /* Destroy it. */
        ret = mamaEnvTimer_destroy(timer);

        /* Note that we do not release the lock as it has now been destroyed. */
    }

    /* Write a mama log. */
    mama_log(MAMA_LOG_LEVEL_FINER, "MamaEnv - onTimerDestroy with timer %p completed with code %X.", timer, ret);
}


//////////////////////////////////////////////////////////////////////////////
void MAMACALLTYPE mamaEnvTimer_onTimerTick(mamaTimer timer, void* closure)
{
    /* Cast the closure to the session timer object. */
    mmeTimer* sessionTimer = (mmeTimer*)closure;
    if (sessionTimer != NULL) {
        /* Lock the timer. */
        wlock_lock(sessionTimer->m_lock);

        /* Invoke the original callback function. */
        if (sessionTimer->m_callback != NULL) {
            (sessionTimer->m_callback)(timer, sessionTimer->m_closure);
        }

        /* Unlock the timer. */
        wlock_unlock(sessionTimer->m_lock);

        /* Allow destroy function to grab the main lock */
        wlock_lock(sessionTimer->m_destroyLock);
        wlock_unlock(sessionTimer->m_destroyLock);
    }
}


//////////////////////////////////////////////////////////////////////////////
mama_status mamaEnvTimer_shutdown(mmeTimer* timer)
{
    wlock_lock(timer->m_lock);
    timer->m_callback = NULL;
    wlock_unlock(timer->m_lock);

    return MAMA_STATUS_OK;
}
