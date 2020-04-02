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


