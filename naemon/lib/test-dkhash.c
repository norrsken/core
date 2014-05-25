#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.c"
#include "t-utils.h"

static struct dkkey keys[] = {
	{ "nisse", "banan" },
	{ "foo", "bar" },
	{ "kalle", "penslar" },
	{ "hello", "world" },
	{ "test", "fnurg" },
	{ "barsk", "nitfol" },
	{ "andreas", "regerar" },
	{ "Nagios", "rules" },
};

static int removed;
static struct test_data {
	int x, i, j;
} del;

unsigned int nae_hash_count_entries(nae_hash_table *table)
{
	unsigned int i, count = 0;

	for (i = 0; i < table->alloc; i++) {
		struct nae_hash_entry *bkt;
		for (bkt = table->entries[i]; bkt; bkt = bkt->next)
			count++;
	}

	return count;
}

static struct test_data *ddup(int x, int i, int j)
{
	struct test_data *d;

	d = malloc(sizeof(*d));
	d->x = x;
	d->i = i;
	d->j = j;
	return d;
}

struct nae_hash_check {
	unsigned int entries, count, max, added, removed;
	int ent_delta, addrm_delta;
};

static int del_matching(void *data)
{
	struct test_data *d = (struct test_data *)data;

	if (!memcmp(d, &del, sizeof(del))) {
		removed++;
		return NAE_HASH_WALK_REMOVE;
	}

	return 0;
}

int main(int argc, char **argv)
{
	nae_hash_table *tx, *t;
	unsigned int x;
	int ret, r2;
	struct test_data s;
	char *p1, *p2;
	char *strs[10];
	char tmp[32];

	t_set_colors(0);
	t_start("nae_hash basic test");
	t = nae_hash_create_string(512);

	p1 = strdup("a not-so secret value");
	nae_hash_insert(t, "nisse", p1);
	ok_int(t->alloc, 512, "Table must be sized properly");
	p2 = nae_hash_get(t, "nisse");
	test(p1 == p2, "get should get what insert set");
	nae_hash_insert(t, "kalle", p1);
	p2 = nae_hash_get(t, "kallebezinga");
	test(p1 != p2, "we should never get the wrong key");
	p2 = nae_hash_remove(t, "kallebezinga");
	test(NULL == p2, "nothing should be removed");
	p2 = nae_hash_remove(t, "kalle");
	test(p1 == p2, "nae_hash_remove() should return removed data");
	p2 = nae_hash_remove(t, "nisse");
	test(p1 == p2, "nae_hash_remove() should return removed data");
	ret = t_end();

	t_reset();
	/* lots of tests below, so we shut up while they're running */
	t_verbose = 0;

	t_start("nae_hash_walk() test");
	memset(&s, 0, sizeof(s));
	/* first we set up the nae_hash-tables */
	tx = nae_hash_create_dk(16);
	for (x = 0; x < ARRAY_SIZE(keys); x++) {
		nae_hash_insert(tx, &keys[x], ddup(x, 0, 0));
		s.x += 1;
	}


	for (x = 0; x < ARRAY_SIZE(keys); x++) {
		del.x = x;
		del.i = del.j = 0;

		s.x -= 1;
		nae_hash_walk(tx, del_matching);
	}

	r2 = t_end();
	ret = r2 ? r2 : ret;

	t_reset();
	for (x = 0; x < 10; x++) {
		sprintf(tmp, "string %d", x);
		strs[x] = strdup(tmp);
	}

	t_start("nae_hash single bucket add remove forward");

	t = nae_hash_create_string(1);
	for (x = 0; x < 10; x++) {
		nae_hash_insert(t, strs[x], strs[x]);
	}
	for (x = 0; x < 10; x++) {
		p1 = strs[x];
		p2 = nae_hash_remove(t, p1);
		test(p1 == p2, "remove should return a value");
	}
	r2 = t_end();
	ret = r2 ? r2 : ret;
	t_reset();

	t_start("nae_hash single bucket add remove backward");

	t = nae_hash_create_string(1);
	for (x = 0; x < 10; x++) {
		nae_hash_insert(t, strs[x], strs[x]);
	}
	for (x = 9; x; x--) {
		p1 = strs[x];
		p2 = nae_hash_remove(t, p1);
		test(p1 == p2, "remove should return a value");
	}

	nae_hash_destroy(t, NULL);

	r2 = t_end();
	return r2 ? r2 : ret;
}
