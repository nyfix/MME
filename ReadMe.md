# MAMA Managed Environment (MME)
The MME (MAMA Managed Environment) library is designed to address several deficiences of MAMA for dynamic multi-threaded programs:

- Event objects (subscriptions, inboxes and timers) can be destroyed from any thread, not just the thread that is dispatching the objects' queue.
- Shutdown is handled automatically by the library, such that:
  - Dispatchers, events, queues, and transports are automically shut down in the correct order.
  - The shutdown process is thread-safe (i.e., there are no data races).
  - Event objects cannot leak -- any existing event objects are automatically destroyed when their parent session is destroyed.


## Architecture
MME defines two new abstractions that are used to manage the native MAMA objects.

### Connection
A connection (`mamaEnvConnection`) represents a specific instance of a MAMA transport.  Internally it encapsulates the following MAMA objects:

- `mamaBridge`
- `mamaTransport`

### Session
A session (`mamaEnvSession`) essentially represents a callback thread that is associated with a specific connection.  Internally it encapsulates the following MAMA objects:

- `mamaQueue`
- `mamaDispatcher`


### Event Objects
Every event object (subscription, inbox or timer) must be associated with a session object, which is responsible for managing its lifetime.  When a session is destroyed, it first destroys all its child event objects in a thread-safe manner.

## Event shutdown
MME adds an important feature that is completely lacking in MAMA, which is the ability to coordinate the shutdown of an event object with a callback function that may be running in a different thread.[^rv]

[^rv]: This was inspired by similar functionality (`tibrvEvent_DestroyEx`) in Tibco Rendezvous.

When a call to `mamaEnv_shutdown...` returns, the caller is guaranteed that:

1. There are no callback functions running against the object in any thread.
2. No subsequent callback functions will be invoked against the object.

## Example code
To create a connection, first initilize MAMA and then create a `mamaEnvConnection ` object:

```
mamaBridge bridge;
mama_loadBridge(&bridge, ...);
mama_open();
mamaTransport transport;
mamaTransport_create(..., bridge);

mamaEnvConnection connection;
mamaEnv_createConnection(bridge, &connection);
```

Once you've done that, you can create a session, and then use it to create managed event objects:

```
mamaEnvSession session;
mamaEnv_createSession(connection, &session);

mamaTimer timer;
mamaEnv_createTimer(... session, &timer);
```

Note that in the example above, we define the result of the `mamaEnv_createTimer` call as a `mamaTimer `.  In actuality, `mamaEnv_createTimer` creates a struct (`mamaEnvTimer`) in which the `mamaTimer` is the first member.  (See [Tricky Bit](#Tricky-Bit), above).

To stop events from firing, call the appropriate `mamaEnv_shutdown...` function:

```
mamaEnv_shutdownTimer(session, timer);
```

This leaves the object allocated, but "inactive" -- no further callbacks will be invoked on the object.

To completely destroy the object, call the appropriate `mamaEnv_destroy...` function:

```
mamaEnv_destroyTimer(session, timer);
```

This deallocates all the memory associated with the object. 

> Note that the destruction is NOT done immediately -- the destroy is queued on the session's event queue.  

When the application is ready to terminate, it should first shutdown and then destroy the session and connection objects:

```
mamaEnv_shutdownSession(connection, session);
mamaEnv_destroySession(connection, session);
```

This call will destroy all sessions, and their associated event objects:

```
mamaEnv_destroyConnection(connection);
```

## Tricky Bit
The original design uses a clever trick to make MME event objects interchangeable with the native MAMA objects which they manage.  The MME objects are defined as `struct`s where the first member is the native MAMA object.   

The C standard (§ 6.7.2.1) mandates that the address of a struct, and the address of its first member, be interchangeable, so e.g. a `mamaEnvTimer` can be used anywhere a `mamaTimer` can be. 


## History
MME was originally developed by Graeme Clarke of NYSE Technologies, back when NYFIX was also part of NYSE.  It was eventually supposed to become part of MAMA proper, but that never happened, and when NYSE divested NYFIX and NYSE Technologies, MME was transferred back to NYFIX.

MME has been used in the NYFIX Marketplace since 2010, processing up to 50 million messages per day.
