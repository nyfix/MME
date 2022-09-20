/* ********************************************************** */
/* Includes. */
/* ********************************************************** */
#include "mama/mamaSynchronizedMap.h"
#include <assert.h>

/* ********************************************************** */
/* Private Function Prototypes. */
/* ********************************************************** */
int synchronizedMap_compare(RedBlackTreeEntry* lhs, RedBlackTreeEntry* rhs);

/* Generate the tree functionality as a static functions. */
RB_GENERATE_STATIC(mamaEnvSynchMap_, RedBlackTreeEntry, m_entry, synchronizedMap_compare)

/* ********************************************************** */
/* Private Functions. */
/* ********************************************************** */

int synchronizedMap_compare(RedBlackTreeEntry* lhs, RedBlackTreeEntry* rhs)
{
    /* Returns. */
    int ret = 1;

    /* Compare the key object pointers in the structure. */
    if (lhs->m_key < rhs->m_key) {
        ret = -1;
    }

    else if (lhs->m_key == rhs->m_key) {
        ret = 0;
    }

    return ret;
}

/* ********************************************************** */
/* Public Functions. */
/* ********************************************************** */

SynchronizedMap* synchronizedMap_create(void)
{
    /* Allocate the structure. */
    SynchronizedMap* ret = (SynchronizedMap*)calloc(1, sizeof(SynchronizedMap));
    if (ret != NULL) {
        /* The following flag will be set on success. */
        int success = 0;

        /* Create the lock. */
        ret->m_lock = wlock_create();
        if (ret->m_lock != NULL) {
            /* Initialise the root of the red black tree to NULL. */
            RB_INIT(&ret->m_tree);

            success = 1;
        }

        /* If something went wrong then destroy the map. */
        if (success != 1) {
            synchronizedMap_destroy(ret);
            ret = NULL;
        }
    }

    return ret;
}

void synchronizedMap_destroy(SynchronizedMap* map)
{
    if (map != NULL) {
        /* Destroy the tree by clearing the root pointer. */
        RB_INIT(&map->m_tree);

        /* Destroy the lock. */
        if (map->m_lock != NULL) {
            //wlock_unlock(map->m_lock); wrong thread!
            int rc = wlock_destroy(map->m_lock);
            if (rc != 0) {
                mama_log(MAMA_LOG_LEVEL_SEVERE, "synchronizedMap_destroy -- got non-zero rc=%d destoying m_lock", rc);
            }
            assert(rc == 0);
        }

        /* Free the map object. */
        free(map);
    }
}

mama_status synchronizedMap_foreach(synchronizedMap_Callback callback, void* closure, int ignoreErrors, SynchronizedMap* map)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;

    /* Acquire the lock first. */
    wlock_lock(map->m_lock);
    {
        /* Enumerate all entries in the red and black tree. */
        RedBlackTreeEntry* nextEntry = NULL;
        RB_FOREACH(nextEntry, mamaEnvSynchMap_, &map->m_tree)
        {
            /* Invoke the callback function for this entry. */
            mama_status cbret = (*callback)(nextEntry->m_data, closure);

            /* Save the return code. */
            if (MAMA_STATUS_OK == ret) {
                ret = cbret;
            }

            /* Quit out if we are not ignoring errors. */
            if ((0 == ignoreErrors) && (MAMA_STATUS_OK != ret)) {
                break;
            }
        }
    }

    /* Release the lock. */
    wlock_unlock(map->m_lock);

    return ret;
}

mama_status synchronizedMap_insert(void* data, void* key, SynchronizedMap* map)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_NOMEM;

    /* Allocate a new red black tree node. */
    RedBlackTreeEntry* treeEntry = (RedBlackTreeEntry*)calloc(1, sizeof(RedBlackTreeEntry));
    if (treeEntry != NULL) {
        /* Save the data in the entry. */
        treeEntry->m_data = data;
        treeEntry->m_key = key;

        /* Acquire the lock first. */
        wlock_lock(map->m_lock);

        /* Add the data object to the tree. */
        RB_INSERT(mamaEnvSynchMap_, &map->m_tree, treeEntry);

        /* Increment the number of entries. */
        map->m_numberEntries++; // NOLINT

        /* Release the lock. */
        wlock_unlock(map->m_lock);

        /* Function success. */
        ret = MAMA_STATUS_OK;
    }

    return ret;
}

mama_status synchronizedMap_remove(void* key, SynchronizedMap* map, void** data)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_INVALID_ARG;

    /* Create a temporary node structure to look up the map. */
    RedBlackTreeEntry tempNode;
    tempNode.m_key = key;

    /* Acquire the lock first. */
    wlock_lock(map->m_lock);
    {
        /* Find the item in the tree. */
        RedBlackTreeEntry* treeEntry = RB_FIND(mamaEnvSynchMap_, &map->m_tree, &tempNode);
        if (treeEntry != NULL) {
            /* Remove the item from the tree. */
            RB_REMOVE(mamaEnvSynchMap_, &map->m_tree, treeEntry);

            /* Release the lock. */
            wlock_unlock(map->m_lock);

            /* Write back the data pointer. */
            *data = treeEntry->m_data;

            /* Free the tree entry. */
            free(treeEntry);

            /* Decrement the number of entries. */
            map->m_numberEntries--;

            /* Return success as the entry was found. */
            ret = MAMA_STATUS_OK;
        }

        else {
            /* Release the lock. */
            wlock_unlock(map->m_lock);
        }
    }

    return ret;
}

mama_status synchronizedMap_for(synchronizedMap_Callback callback, void* key, SynchronizedMap* map, void* closure)
{
    if (callback == NULL || key == NULL || map == NULL) {
        return MAMA_STATUS_NULL_ARG;
    }

    wlock_lock(map->m_lock);
    mama_status ret = MAMA_STATUS_NOT_FOUND;
    {
        /* Create a temporary node structure to look up the map. */
        RedBlackTreeEntry tempNode;
        tempNode.m_key = key;

        /* Find the item in the tree. */
        RedBlackTreeEntry* treeEntry = RB_FIND(mamaEnvSynchMap_, &map->m_tree, &tempNode);
        if (treeEntry != NULL) {
            ret = (*callback)(treeEntry->m_data, closure);
        }
    }
    wlock_unlock(map->m_lock);

    return ret;
}


mama_status synchronizedMap_removeAll(synchronizedMap_Callback callback, void* closure, SynchronizedMap* map)
{
    /* Returns. */
    mama_status ret = MAMA_STATUS_OK;

    /* A copy of the tree must be made under the lock, once this has been done the lock can be released to prevent
	 * race conditions while the items are removed from the tree.
	 */
    /* Declare a local tree. */
    mamaEnvSynchMap localTree;

    /* Acquire the lock first. */
    wlock_lock(map->m_lock);
    {
        /* Make a locak copy of the number of entries. */
        long entriesLeft = map->m_numberEntries;

        /* Make a copy of the member tree into the local one. */
        RB_COPY(&map->m_tree, &localTree);

        /* Re-initialise the tree to clear all of the entries. */
        RB_INIT(&map->m_tree);

        /* Reset the number of entries. */
        map->m_numberEntries = 0;

        /* Release the lock now that the copy has been made. */
        wlock_unlock(map->m_lock);
        {
            /* Enumerate all the entries in the map. */
            mama_status cbret = MAMA_STATUS_OK;
            while (entriesLeft > 0) {
                /* Get the root entry. */
                RedBlackTreeEntry* rootEntry = RB_ROOT(&localTree);

                /* If this is NULL quit out as the tree is empty even through the count maybe wrong. */
                if (NULL == rootEntry) {
                    break;
                }

                /* Remove the root entry. */
                RB_REMOVE(mamaEnvSynchMap_, &localTree, rootEntry);     // NOLINT

                /* Invoke the callback function for this entry. */
                if (NULL != callback) {
                    cbret = (*callback)(rootEntry->m_data, closure);
                }

                /* Save the return code. */
                if (MAMA_STATUS_OK == ret) {
                    ret = cbret;
                }

                /* Free the memory associated with the tree node. */
                free(rootEntry);

                /* Decrement the count for the next iteration. */
                entriesLeft--;
            }
        }
    }

    assert(map->m_numberEntries == 0);

    return ret;
}
