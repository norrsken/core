#ifndef LIBNAEMON_hash_h__
#define LIBNAEMON_hash_h__
#include "lnae-utils.h"
#include <errno.h>

#define NAE_HASH_OK     0         /**< Success */
#define NAE_HASH_EDUPE  (-EPERM)  /**< duplicate insert attempted */
#define NAE_HASH_EINVAL (-EINVAL) /**< Invalid parameters passed */
#define NAE_HASH_ENOMEM (-ENOMEM) /**< Memory allocation failed */

#define NAE_HASH_WALK_REMOVE 1
#define NAE_HASH_WALK_STOP   2

/**
 * @file hash.h
 * @brief Simple hash table implementation
 *
 * Fanouts are useful to hold short-lived integer-indexed data where
 * the keyspan between smallest and largest key can be too large and
 * change too often for it to be practical to maintain a growing array.
 * If you think of it as a hash-table optimized for unsigned longs you've
 * got the right idea.
 *
 * @{
 */

NAGIOS_BEGIN_DECL

/** Primary (opaque) type for this api */
typedef struct nae_hash_table nae_hash_table;

struct dkkey {
	const char *k1;
	const char *k2;
};

nae_hash_table *nae_hash_create_long(unsigned long size);
nae_hash_table *nae_hash_create_string(unsigned long size);
nae_hash_table *nae_hash_create_dk(unsigned long size);
/**
 * Create a hash table
 * @param[in] size The size of the table. Preferrably a power of 2
 * @return Pointer to a newly created table
 */
extern nae_hash_table *nae_hash_create(unsigned long size, unsigned long (*hash)(const void *), long (*compare)(const void *, const void *));

/**
 * Destroy a hash table, with optional destructor.
 * This function will iterate over all the entries in the hash
 * table and remove them, one by one. If 'destructor' is not NULL,
 * it will be called on each and every object in the table. Note that
 * 'free' is a valid destructor.
 *
 * @param[in] t The hash table to destroy
 * @param[in] destructor Function to call on data pointers in table
 */
extern void nae_hash_destroy(nae_hash_table *t, void (*destructor)(void *));

/**
 * Return a pointer from the hash table t
 *
 * @param[in] t table to fetch from
 * @param[in] key key to fetch
 * @return NULL on errors; Pointer to data on success
 */
extern void *nae_hash_get(nae_hash_table *t, const void *key);

/**
 * Add an entry to the hash table.
 * Note that we don't check if the key is unique. If it isn't,
 * nae_hash_remove() will remove the latest added first.
 *
 * @param[in] t hash table to add to
 * @param[in] key Key for this entry
 * @param[in] data Data to add. Must not be NULL
 * @return 0 on success, -1 on errors
 */
extern int nae_hash_insert(nae_hash_table *t, const void *key, void *data);

/**
 * Remove an entry from the hash table and return its data.
 *
 * @param[in] t hash table to look in
 * @param[key] The key whose data we should locate
 * @return Pointer to the data stored on success; NULL on errors
 */
extern void *nae_hash_remove(nae_hash_table *t, const void *key);

int nae_hash_walk(nae_hash_table *t, int (*walker)(void *));

NAGIOS_END_DECL
/** @} */
#endif
