#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "hash.h"

struct nae_hash_entry {
	const void *key;
	void *data;
	struct nae_hash_entry *next;
};

struct nae_hash_table {
	unsigned long alloc;
	unsigned long (*hash)(const void *);
	long (*compare)(const void *, const void *);
	struct nae_hash_entry **entries;
};

#define PRIME 509
unsigned long nae_hash_string(const void *key)
{
	const char *k = key;
	register unsigned int h = 0x123; /* magic */

	while (*k)
		h = *k++ + PRIME * h;

	return h;
}

unsigned long nae_hash_dk(const void *key_)
{
	const struct dkkey *key = key_;
	return (nae_hash_string(key->k1) ^ nae_hash_string(key->k2));
}

unsigned long nae_hash_long(const void *key)
{
	return (unsigned long)(size_t)key;
}

long nae_cmp_dk(const void *k1_, const void *k2_)
{
	const struct dkkey *k1 = k1_, *k2 = k2_;
	int res;
	if (k1 == NULL && k2 == NULL)
		return 0;
	else if (k1 == NULL)
		return 1;
	else if (k2 == NULL)
		return -1;

	res = strcmp(k1->k1, k2->k1);
	if (res)
		return res;
	return strcmp(k1->k2, k2->k2);
}
long nae_cmp_string(const void *k1, const void *k2)
{
	return strcmp(k1, k2);
}
long nae_cmp_long(const void *k1, const void *k2)
{
	return (unsigned long)k1 - (unsigned long)k2;
}

nae_hash_table *nae_hash_create_long(unsigned long size)
{
	return nae_hash_create(size, nae_hash_long, nae_cmp_long);
}
nae_hash_table *nae_hash_create_string(unsigned long size)
{
	return nae_hash_create(size, nae_hash_string, nae_cmp_string);
}
nae_hash_table *nae_hash_create_dk(unsigned long size)
{
	return nae_hash_create(size, nae_hash_dk, nae_cmp_dk);
}


nae_hash_table *nae_hash_create(unsigned long size, unsigned long (*hash)(const void *), signed long (*compare)(const void *, const void *))
{
	nae_hash_table *t = calloc(1, sizeof(*t));
	if (!t)
		return NULL;
	t->entries = calloc(size, sizeof(struct nae_hash_entry *));
	if (!t->entries) {
		free(t);
		return NULL;
	}
	t->alloc = size;
	t->hash = hash;
	t->compare = compare;
	return t;
}

void nae_hash_destroy(nae_hash_table *t, void (*destructor)(void *))
{
	unsigned long i;
	struct nae_hash_entry **entries, *next;

	if (!t || !t->entries || !t->alloc)
		return;

	/* protect against destructors calling nae_hash_remove() */
	entries = t->entries;
	t->entries = NULL;

	for (i = 0; i < t->alloc; i++) {
		struct nae_hash_entry *entry;

		for (entry = entries[i]; entry; entry = next) {
			void *data = entry->data;
			next = entry->next;
			free(entry);
			if (destructor) {
				destructor(data);
			}
		}
	}
	free(entries);
	free(t);
}

int nae_hash_insert(nae_hash_table *t, const void *key, void *data)
{
	struct nae_hash_entry *entry;
	unsigned long hash = t->hash(key);
	if (!t || !t->entries || !t->alloc || !key)
		return NAE_HASH_EINVAL;

	entry = calloc(1, sizeof(*entry));
	if (!entry)
		return NAE_HASH_ENOMEM;

	if (nae_hash_get(t, key)) {
		return NAE_HASH_EDUPE;
	}

	entry->key = key;
	entry->data = data;
	entry->next = t->entries[hash % t->alloc];
	t->entries[hash % t->alloc] = entry;

	return NAE_HASH_OK;
}

void *nae_hash_get(nae_hash_table *t, const void *key)
{
	struct nae_hash_entry *entry;
	unsigned long slot, hash = t->hash(key);

	if (!t || !t->entries || !t->alloc || !key)
		return NULL;

	slot = hash % t->alloc;
	for (entry = t->entries[slot]; entry; entry = entry->next) {
		if (!t->compare(entry->key, key))
			return entry->data;
	}

	return NULL;
}

void *nae_hash_remove(nae_hash_table *t, const void *key)
{
	struct nae_hash_entry *entry, *next, *prev = NULL;
	unsigned long slot, hash = t->hash(key);

	if (!t || !t->entries || !t->alloc)
		return NULL;

	slot = hash % t->alloc;
	for (entry = t->entries[slot]; entry; prev = entry, entry = next) {
		next = entry->next;
		if (!t->compare(entry->key, key)) {
			void *data = entry->data;
			if (prev) {
				prev->next = entry->next;
			} else {
				t->entries[slot] = entry->next;
			}
			free(entry);
			return data;
		}
	}
	return NULL;
}

int nae_hash_walk(nae_hash_table *t, int (*walker)(void *))
{
	struct nae_hash_entry *entry, *prev;
	unsigned int i;

	if (!t->entries)
		return 0;

	for (i = 0; i < t->alloc; i++) {
		int depth = 0;
		struct nae_hash_entry *next;

		prev = t->entries[i];
		for (entry = t->entries[i]; entry; entry = next) {
			next = entry->next;

			switch (walker(entry->data)) {
				case NAE_HASH_WALK_REMOVE:
					if (depth) {
						prev->next = next;
					} else {
						t->entries[i] = next;
					}
					free(entry);
					break;
				case NAE_HASH_WALK_STOP:
					return NAE_HASH_WALK_STOP;
				default:
					prev = entry;
					depth++;
					break;
			}
		}
	}

	return 0;
}

