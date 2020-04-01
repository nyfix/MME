/* ********************************************************** */
/* Includes. */
/* ********************************************************** */
#include "mama/mamaEnvEvent.h"
#include <pthread.h>

/* ********************************************************** */
/* Windows Implementation. */
/* ********************************************************** */
#ifdef WIN32

/* ********************************************************** */
/* Includes. */
/* ********************************************************** */
#include "windows.h"

/* ********************************************************** */
/* Public Functions. */
/* ********************************************************** */

mama_status mamaEnv_createEvent(MamaEvent** synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Create the event. */
        HANDLE localEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        ret = MAMA_STATUS_PLATFORM;
        if (localEvent != NULL) {
            ret = MAMA_STATUS_OK;
        }

        /* Write back the event object. */
        *synchEvent = localEvent;
    }

    return ret;
}

mama_status mamaEnv_destroyEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Destroy the event. */
        BOOL ch = CloseHandle(synchEvent);
        ret = MAMA_STATUS_PLATFORM;
        if (ch) {
            ret = MAMA_STATUS_OK;
        }
    }

    return ret;
}

mama_status mamaEnv_resetEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Acquire the mutex before the event can be signaled. */
        BOOL se = ResetEvent(synchEvent);
        ret = MAMA_STATUS_PLATFORM;
        if (se) {
            ret = MAMA_STATUS_OK;
        }
    }

    return ret;
}

mama_status mamaEnv_setEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Acquire the mutex before the event can be signaled. */
        BOOL se = SetEvent(synchEvent);
        ret = MAMA_STATUS_PLATFORM;
        if (se) {
            ret = MAMA_STATUS_OK;
        }
    }

    return ret;
}

mama_status mamaEnv_timedWaitEvent(long seconds, MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Acquire the mutex before waiting on the event. */
        DWORD wso = WaitForSingleObject(synchEvent, (seconds * 1000));

        /* Check the return code. */
        switch (wso) {
        case WAIT_OBJECT_0:
            ret = MAMA_STATUS_OK;
            break;
        case WAIT_TIMEOUT:
            ret = MAMA_STATUS_TIMEOUT;
            break;
        default:
            ret = MAMA_STATUS_PLATFORM;
            break;
        }
    }

    return ret;
}

mama_status mamaEnv_waitEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Acquire the mutex before waiting on the event. */
        DWORD wso = WaitForSingleObject(synchEvent, INFINITE);
        ret = MAMA_STATUS_PLATFORM;
        if (wso == WAIT_OBJECT_0) {
            ret = MAMA_STATUS_OK;
        }
    }

    return ret;
}

/* ********************************************************** */
/* GCC Implementation. */
/* ********************************************************** */
#else

/* ********************************************************** */
/* Includes. */
/* ********************************************************** */
#include "errno.h"
#include "time.h"

/* ********************************************************** */
/* Public Functions. */
/* ********************************************************** */

mama_status mamaEnv_createEvent(MamaEvent** synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* Allocate a new event structure. */
        MamaEvent* localEvent = (MamaEvent*)calloc(1, sizeof(MamaEvent));
        ret = MAMA_STATUS_NOMEM;
        if (localEvent != NULL) {
            /* Create the mutex. */
            int mi = pthread_mutex_init(&localEvent->m_mutex, NULL);
            ret = MAMA_STATUS_PLATFORM;
            if (mi == 0) {
                /* Create the condition. */
                mi = pthread_cond_init(&localEvent->m_condition, NULL);
                if (mi == 0) {
                    ret = MAMA_STATUS_OK;
                }
            }
        }

        /* If something went wrong destroy the object. */
        if (ret != MAMA_STATUS_OK) {
            mamaEnv_destroyEvent(localEvent);
            localEvent = NULL;
        }

        /* Write back the event object. */
        *synchEvent = localEvent;
    }

    return ret;
}

mama_status mamaEnv_destroyEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    if (synchEvent != NULL) {
        /* The return code will be preserved with the first error. */
        ret = MAMA_STATUS_OK;
        {
            /* Destroy the condition. */
            int cd = pthread_cond_destroy(&synchEvent->m_condition);
            if ((cd != 0) && (ret == MAMA_STATUS_OK)) {
                ret = MAMA_STATUS_PLATFORM;
            }

            /* Destroy the mutex. */
            cd = pthread_mutex_destroy(&synchEvent->m_mutex);
            if ((cd != 0) && (ret == MAMA_STATUS_OK)) {
                ret = MAMA_STATUS_PLATFORM;
            }
        }

        /* Free the structure. */
        free(synchEvent);
    }

    return ret;
}

mama_status mamaEnv_resetEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    int ml = 0;

    if (synchEvent != NULL) {
        /* Acquire the mutex before the event can be signaled. */
        ret = MAMA_STATUS_PLATFORM;
        ml = pthread_mutex_lock(&synchEvent->m_mutex);
        if (0 == ml) {
            /* Clear the state to indicate that the event has been reset. */
            synchEvent->m_state = 0;

            /* Function success. */
            ret = MAMA_STATUS_OK;

            /* Unlock the mutex. */
            ml = pthread_mutex_unlock(&synchEvent->m_mutex);
            if ((MAMA_STATUS_OK == ret) && (ml != 0)) {
                ret = MAMA_STATUS_PLATFORM;
            }
        }
    }

    return ret;
}

mama_status mamaEnv_setEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    int ml = 0;

    if (synchEvent != NULL) {
        /* Acquire the mutex before the event can be signaled. */
        ret = MAMA_STATUS_PLATFORM;
        ml = pthread_mutex_lock(&synchEvent->m_mutex);
        if (0 == ml) {
            /* Wake all threads waiting on the event. */
            ml = pthread_cond_broadcast(&synchEvent->m_condition);
            if (ml == 0) {
                /* Set the state to indicate that the event is signaled. */
                synchEvent->m_state = 1;

                /* Function success. */
                ret = MAMA_STATUS_OK;
            }

            /* Unlock the mutex. */
            ml = pthread_mutex_unlock(&synchEvent->m_mutex);
            if ((MAMA_STATUS_OK == ret) && (ml != 0)) {
                ret = MAMA_STATUS_PLATFORM;
            }
        }
    }

    return ret;
}

mama_status mamaEnv_timedWaitEvent(long seconds, MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    int ml = 0;

    if (synchEvent != NULL) {
        /* Acquire the mutex before waiting on the event. */
        ret = MAMA_STATUS_PLATFORM;
        ml = pthread_mutex_lock(&synchEvent->m_mutex);
        if (0 == ml) {
            /* Only wait if the event is actually non-signaled. */
            ret = MAMA_STATUS_OK;
            if (0 == synchEvent->m_state) {
                /* Get the current time. */
                struct timespec timeSpec;
                memset(&timeSpec, 0, sizeof(timeSpec));
                ret = MAMA_STATUS_PLATFORM;
                ml = clock_gettime(CLOCK_REALTIME, &timeSpec);
                if (0 == ml) {
                    /* Add the wait time to the time spec. */
                    timeSpec.tv_sec += seconds;

                    /* Wait on the condition, note that this will atomically release
                     * the mutex.
                     */
                    ml = pthread_cond_timedwait(&synchEvent->m_condition, &synchEvent->m_mutex, &timeSpec);
                    if (0 == ml) {
                        ret = MAMA_STATUS_OK;
                    }

                    else if ((-1 == ml) && (errno == ETIMEDOUT)) {
                        ret = MAMA_STATUS_TIMEOUT;
                    }
                }
            }

            /* Unlock the mutex. */
            ml = pthread_mutex_unlock(&synchEvent->m_mutex);
            if ((MAMA_STATUS_OK == ret) && (ml != 0)) {
                ret = MAMA_STATUS_PLATFORM;
            }
        }
    }

    return ret;
}

mama_status mamaEnv_waitEvent(MamaEvent* synchEvent)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NULL_ARG;
    int ml = 0;

    if (synchEvent != NULL) {
        /* Acquire the mutex before waiting on the event. */
        ret = MAMA_STATUS_PLATFORM;
        ml = pthread_mutex_lock(&synchEvent->m_mutex);
        if (0 == ml) {
            /* Only wait if the event is actually non-signaled. */
            ret = MAMA_STATUS_OK;
            if (0 == synchEvent->m_state) {
                /* Wait on the condition, note that this will atomically release
                 * the mutex.
                 */
                ret = MAMA_STATUS_PLATFORM;
                ml = pthread_cond_wait(&synchEvent->m_condition, &synchEvent->m_mutex);
                if (0 == ml) {
                    ret = MAMA_STATUS_OK;
                }
            }

            /* Unlock the mutex. */
            ml = pthread_mutex_unlock(&synchEvent->m_mutex);
            if ((MAMA_STATUS_OK == ret) && (ml != 0)) {
                ret = MAMA_STATUS_PLATFORM;
            }
        }
    }

    return ret;
}

#endif
