#ifndef MAMASYNCHRONIZEDMAP_H
#define MAMASYNCHRONIZEDMAP_H

#include <wlock.h>
#include "mamaEnvGeneral.h"
#include "systree.h"

/* Instances of this structure will be inserted into the tree.
 */
typedef struct RedBlackTreeEntry
{
    /* This void pointer is the actual data that will be stored in the tree. */
    void* m_data;

    /* This void pointer is the key used to look up the data. */
    void* m_key;

    /* This structure will be stored in the session's synchronized map. This member
	 * is used to generate the red and black tree members.
	 */
    RB_ENTRY(RedBlackTreeEntry) m_entry;

} RedBlackTreeEntry;

/* This structure defines the map and basically contains a red black tree
 * and a lock.
 */
typedef struct SynchronizedMap
{
    /* The lock to protect the map. */
    wLock m_lock;

    /* The number of entries in the tree. */
    long m_numberEntries;

    /* Define the structure of the red black tree. */
    RB_HEAD(mamaEnvSynchMap_, RedBlackTreeEntry) m_tree;

} SynchronizedMap;

typedef struct mamaEnvSynchMap_ mamaEnvSynchMap;

typedef mama_status (*synchronizedMap_Callback)(void* data, void* closure);

MAMAENV_API SynchronizedMap* synchronizedMap_create(void);
MAMAENV_API void synchronizedMap_destroy(SynchronizedMap* map);
MAMAENV_API mama_status synchronizedMap_foreach(synchronizedMap_Callback callback, void* closure, int ignoreErrors, SynchronizedMap* map);
MAMAENV_API mama_status synchronizedMap_insert(void* data, void* key, SynchronizedMap* map);
MAMAENV_API mama_status synchronizedMap_remove(void* key, SynchronizedMap* map, void** data);
MAMAENV_API mama_status synchronizedMap_removeAll(synchronizedMap_Callback callback, void* closure, SynchronizedMap* map);

//MAMAENV_API mama_status synchronizedMap_find(void* key, SynchronizedMap* map, void** data);
MAMAENV_API mama_status synchronizedMap_for(synchronizedMap_Callback callback, void* key, SynchronizedMap* map, void* closure);

#endif
