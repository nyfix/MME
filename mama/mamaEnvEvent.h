#ifndef MAMAENVEVENT_H
#define MAMAENVEVENT_H

#include <mama/mama.h>

#ifdef WIN32
/* ********************************************************** */
/* Windows Implementation. */
/* ********************************************************** */

/* The event is basically a windows handle. */
typedef void* MamaEvent;

#else
/* ********************************************************** */
/* GCC Implementation. */
/* ********************************************************** */

/* This structure contains all of the objects required to create an event in gcc.
 */
typedef struct MamaEvent
{
    /* The condition is used to send the signal amongst threads. */
    pthread_cond_t m_condition;

    /* The mutex protects access to the condition. */
    pthread_mutex_t m_mutex;

    /* This variable reflects the state of the event, if the event is signaled
     * before the waiting thread calls mamaEnv_waitEvent then the condition
     * will not be set.
     * This state will be set to 1 if the event is signaled.
     */
    int m_state;

} MamaEvent;

#endif

/* ********************************************************** */
/* Functions Prototypes. */
/* ********************************************************** */

mama_status mamaEnv_createEvent(MamaEvent** synchEvent);
mama_status mamaEnv_destroyEvent(MamaEvent* synchEvent);
mama_status mamaEnv_resetEvent(MamaEvent* synchEvent);
mama_status mamaEnv_setEvent(MamaEvent* synchEvent);
mama_status mamaEnv_timedWaitEvent(long seconds, MamaEvent* synchEvent);
mama_status mamaEnv_waitEvent(MamaEvent* synchEvent);

#endif
