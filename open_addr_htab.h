#ifndef OPEN_ADDR_HTAB_H
#define OPEN_ADDR_HTAB_H

#include <stdbool.h>
#include <stddef.h>

typedef size_t (*hash_fn)(const char *);
typedef void (*for_each_fn)(const char *, void *, void *);

struct open_addr_htab_node {
  const char *key;
  void *val;
};

struct open_addr_htab_arr_header {
  size_t primes_idx;
  struct open_addr_htab_node arr[];
};

struct open_addr_htab {
  hash_fn hash;
  size_t sz;
  struct open_addr_htab_arr_header *arr_header;
};

bool
open_addr_htab_ctor(struct open_addr_htab *htab, hash_fn hash);

void
open_addr_htab_dtor(struct open_addr_htab *htab);

bool
open_addr_htab_insert(struct open_addr_htab *htab, const char *key, void *val);

void *
open_addr_htab_find(struct open_addr_htab *htab, const char *key);

void
open_addr_htab_erase(struct open_addr_htab *htab, const char *key);

void
open_addr_htab_for_each(struct open_addr_htab *htab, for_each_fn for_each,
                        void *state);

#endif /* OPEN_ADDR_HTAB_H */
