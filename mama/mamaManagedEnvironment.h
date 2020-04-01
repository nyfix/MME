/*
 * This is the main header file for the mama managed environment library that
 * contains all of the exported function prototypes.
 *
 * The environment has 2 principal objects:
 * - connection that corresponds to a mama bridge
 * - session that is a queue plus a dispatcher, (and hence a separate thread).
 *
 * This library allows mama objects such as subscriptions, timers and inboxes
 * to be created and destroyed from any thread. Additionally such objects will
 * automatically be cleaned up by the session and connection whenever they are
 * shut down.
 *
 * This library supports the standard mama types and introduces only negligible
 * latency to the happy path. This latency is introduced solely by the acquisition
 * of a synchronization lock before an object's callback function is invoked.
 *
 */
#ifndef MAMAMANAGEDENVIRONMENT_H
#define MAMAMANAGEDENVIRONMENT_H

#include "mama/subscription.h"
#include "mamaEnvGeneral.h"

//////////////////////////////////////////////////////////////////////////////
// A "connection" corresponds to a bridge/transport.
//////////////////////////////////////////////////////////////////////////////
typedef struct mmeConnection mmeConnection;     // forward-declare actual definition
typedef mmeConnection* mamaEnvConnection;       // opaque pointer to definition

/**
 * This function will create a connection that is required before any sessions or
 * event objects, (such as timers and subscriptions), can be created.
 * The resulting connection object should be destroyed by calling mamaEnv_destroyConnection.
 *
 * @param bridge (in) The mama bridge.
 * @param connection (out) To return the resulting connection.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NO_MEM
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_createConnection(mamaBridge bridge, mamaEnvConnection* connection);

/**
 * This function will destroy a connection created by mamaEnv_createConnection. Note that
 * it must be called from the same thread that invoked mamaEnv_createConnection. Before this
 * function returns it will ensure that all sessions (and their associated event objects, such as
 * subscriptions and timers), are destroyed. Therefore calling this function may result in
 * a slight delay.
 *
 * @param connection (in) The connection to destroy.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_destroyConnection(mamaEnvConnection connection);


//////////////////////////////////////////////////////////////////////////////
// A "session" correpsonds to a queue/dispatcher/thread.
//////////////////////////////////////////////////////////////////////////////
typedef struct mmeSession mmeSession;           // forward-declare actual definition
typedef mmeSession* mamaEnvSession;             // opaque pointer to definition

/**
 * This function will create a session that is required before any event objects, (such as
 * timers and subscriptions), can be created. The session contains a mama queue and dispatcher
 * which will be used to service any objects created.
 * This function can be called by any thread.
 * The resulting session object should be destroyed by calling mamaEnv_destroySession.
 *
 * @param connection (in) The connection object.
 * @param session (out) To return the resulting session.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NO_MEM
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_createSession(mamaEnvConnection connection, mamaEnvSession* session);


typedef mama_status(MAMACALLTYPE* mamaEnv_onSessionEventCallback)(mamaEnvSession session, void* closure);

/**
 * This function will destroy a session created by mamaEnv_createSession. This function
 * can be called by any thread and will result in all of the open mama event objects, (such
 * as timers and subscriptions), being destroyed. Calling this function will not result in
 * any significant time delay.
 *
 * @param connection (in) The connection on which the session was created.
 * @param session (in) The session to destroy.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_destroySession(mamaEnvConnection connection, mamaEnvSession session);

MAMAENV_API mama_status mamaEnv_shutdownSession(mamaEnvConnection connection, mamaEnvSession session);

//////////////////////////////////////////////////////////////////////////////
// Subscriptions
//////////////////////////////////////////////////////////////////////////////

/**
 * This function will add create a basic subscription within the managed environment.
 * This subscription should only be destroyed by calling mamaEnv_destroySubscription,
 * and should not be passed to either mamaSubscription_destroy or mamaSubscription_deallocate.
 * Aside from that it can be used in any other mama subscription call.
 *
 * Note that the subscription runs on top of a session but be created or destroyed from any thread.
 * All of the callback functions will be invoked by the thread pumping the queue for the supplied
 * session.
 *
 * @param callback (in) Subscription callback function pointers.
 * @param closure (in) The closure that will be passed back to the callback functions.
 * @param session (in) The session for which the subscription should be created.
 * @param symbol (in) The symbol to subscribe to.
 * @param transport (in) The mama transport.
 * @param subscription (out) To return the resulting mamaSubscription.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NO_MEM
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_createBasicSubscription(const mamaMsgCallbacks* callback,
    void* closure, mamaEnvSession session, const char* symbol, mamaTransport transport,
    mamaSubscription* subscription);

/**
 * This function will add create a wildcard subscription within the managed environment.
 * This subscription should only be destroyed by calling mamaEnv_destroySubscription,
 * and should not be passed to either mamaSubscription_destroy or mamaSubscription_deallocate.
 * Aside from that it can be used in any other mama subscription call.
 *
 * Note that the subscription runs on top of a session but be created or destroyed from any thread.
 * All of the callback functions will be invoked by the thread pumping the queue for the supplied
 * session.
 *
 * @param callback (in) Subscription callback function pointers.
 * @param closure (in) The closure that will be passed back to the callback functions.
 * @param session (in) The session for which the subscription should be created.
 * @param source (in) The source to subscribe to.
 * @param symbol (in) The symbol to subscribe to.
 * @param transport (in) The mama transport.
 * @param subscription (out) To return the resulting mamaSubscription.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NO_MEM
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_createWildcardSubscription(const mamaWildCardMsgCallbacks* callback,
    void* closure, mamaEnvSession session,
    const char* source, const char* symbol,
    mamaTransport transport, mamaSubscription* subscription);


/**
 * This function will destroy a subscription created by one of the mamaEnv_createXXXSubscription
 * functions. Note that this function can be called from any thread.
 * Calling this function will not result in any significant time delay.
 *
 * @param session (in) The session that the inbox was created on.
 * @param subscription (in) The subscription to be destroyed.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_destroySubscription(mamaEnvSession session, mamaSubscription subscription);

mama_status mamaEnv_shutdownSubscription(mamaEnvSession session, mamaSubscription subscription);


//////////////////////////////////////////////////////////////////////////////
// Inboxes
//////////////////////////////////////////////////////////////////////////////

/**
 * This function will create an inbox within the managed environment.
 * This inbox should only be destroyed by calling mamaEnv_destroyInbox and should not be
 * passed to mamaInbox_destroy.
 * Aside from that it can be used in any other mama inbox call.
 *
 * Note that the inbox runs on top of a session but be created or destroyed from any thread.
 * All of the callback functions will be invoked by the thread pumping the queue for the supplied
 * session.
 *
 * @param closure (in) The closure that will be passed back to the callback functions.
 * @param errorCallback (in) Error callback function pointer.
 * @param msgCallback (in) Message callback function pointer.
 * @param session (in) The session for which the subscription should be created.
 * @param transport (in) The mama transport.
 * @param result (out) To return the resulting mamaInbox.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NO_MEM
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_createInbox(void* closure, mamaInboxErrorCallback errorCallback, mamaInboxMsgCallback msgCallback,
    mamaEnvSession session, mamaTransport transport, mamaInbox* result);


/**
 * This function will destroy an inbox created by mamaEnv_createInbox. Note that
 * this function can be called from any thread.
 * Calling this function will not result in any significant time delay.
 *
 * @param session (in) The session that the inbox was created on.
 * @param inbox (in) The inbox to be destroyed.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_destroyInbox(mamaEnvSession session, mamaInbox inbox);

MAMAENV_API mama_status mamaEnv_shutdownInbox(mamaEnvSession session, mamaInbox inbox);

//////////////////////////////////////////////////////////////////////////////
// Timers
//////////////////////////////////////////////////////////////////////////////

/**
 * This function will create a timer within the managed environment.
 * This timer should only be destroyed by calling mamaEnv_destroyTimer, and should not be passed
 * mamaTimer_destroy.
 * Aside from that it can be used in any other mama timer call.
 *
 * Note that the timer runs on top of a session but be created or destroyed from any thread.
 * All of the callback functions will be invoked by the thread pumping the queue for the supplied
 * session.
 *
 * @param callback (in) Callback function pointer.
 * @param closure (in) The closure that will be passed back to the callback functions.
 * @param interval (in) The timer interval.
 * @param session (in) The session for which the subscription should be created.
 * @param result (out) To return the resulting mamaTimer.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NO_MEM
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_createTimer(mamaTimerCb callback, void* closure, mama_f64_t interval,
    mamaEnvSession session, mamaTimer* result);


/**
 * This function will destroy a timer created by calling mamaEnv_createTimer.
 * Note that this function can be called from any thread.
 * Calling this function will not result in any significant time delay.
 *
 * @param session (in) The session that the timer was created on.
 * @param timer (in) The timer to be destroyed.
 * @return Resulting status of the call which can be
 *      MAMA_STATUS_NULL_ARG
 *      MAMA_STATUS_PLATFORM
 *      MAMA_STATUS_OK
 */
MAMAENV_API mama_status mamaEnv_destroyTimer(mamaEnvSession session, mamaTimer timer);

MAMAENV_API mama_status mamaEnv_shutdownTimer(mamaEnvSession session, mamaTimer timer);

#endif
