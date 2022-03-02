#include "open_addr_htab.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

#include "errinj.h"

static const size_t primes[] = {
    59,      71,      89,      107,     131,     163,     197,     239,
    293,     353,     431,     521,     631,     761,     919,     1103,
    1327,    1597,    1931,    2333,    2801,    3371,    4049,    4861,
    5839,    7013,    8419,    10103,   12143,   14591,   17519,   21023,
    25229,   30293,   36353,   43627,   52361,   62851,   75431,   90523,
    108631,  130363,  156437,  187751,  225307,  270371,  324449,  389357,
    467237,  560689,  672827,  807403,  968897,  1162687, 1395263, 1674319,
    2009191, 2411033, 2893249, 3471899, 4166287, 4999559, 5999471, 7199369};
static const size_t primes_cnt = sizeof(primes) / sizeof(primes[0]);
static_assert(7199369 < INT64_MAX,
              "maximum size of hash table must not overflow 64-bit integer");

static const void *ERASED = (void *)1;

static struct open_addr_htab_arr_header *
open_addr_htab_arr_header_ctor(size_t primes_idx);

static void
open_addr_htab_arr_header_dtor(struct open_addr_htab_arr_header *arr_header);

static bool
rehash(struct open_addr_htab *htab);

static void
move_node_to_new_htab(const char *key, void *val, void *state);

static size_t
crc32_hash(const char *key);

static struct open_addr_htab_node *
open_addr_htab_find_node(struct open_addr_htab *htab, const char *key);

static size_t
open_addr_htab_pos_next(const struct open_addr_htab *htab, size_t pos);

static void
open_addr_htab_pos_erase(struct open_addr_htab *htab, size_t pos);

bool
open_addr_htab_ctor(struct open_addr_htab *htab, hash_fn hash) {
  assert(htab != NULL);

  if (hash != NULL) {
    htab->hash = hash;
  } else {
    htab->hash = crc32_hash;
  }
  htab->sz = 0;
  htab->arr_header = open_addr_htab_arr_header_ctor(0);
  return htab->arr_header != NULL;
}

void
open_addr_htab_dtor(struct open_addr_htab *htab) {
  assert(htab != NULL);

  open_addr_htab_arr_header_dtor(htab->arr_header);
}

bool
open_addr_htab_insert(struct open_addr_htab *htab, const char *key, void *val) {
  assert(htab != NULL);
  assert(key != NULL);
  assert(val != NULL);

  if (htab->sz == primes[htab->arr_header->primes_idx] - 1 && !rehash(htab)) {
    return false;
  }

  size_t pos = htab->hash(key) % primes[htab->arr_header->primes_idx];
  while (htab->arr_header->arr[pos].key != NULL &&
         htab->arr_header->arr[pos].key != ERASED) {
    pos = open_addr_htab_pos_next(htab, pos);
  }

  if (htab->arr_header->arr[pos].key != ERASED) {
    ++htab->sz;
  }
  htab->arr_header->arr[pos].key = key;
  htab->arr_header->arr[pos].val = val;

  return true;
}

void *
open_addr_htab_find(struct open_addr_htab *htab, const char *key) {
  assert(htab != NULL);
  assert(key != NULL);

  struct open_addr_htab_node *node = open_addr_htab_find_node(htab, key);
  if (node == NULL) {
    return NULL;
  }

  return node->val;
}

void
open_addr_htab_erase(struct open_addr_htab *htab, const char *key) {
  assert(htab != NULL);
  assert(key != NULL);

  struct open_addr_htab_node *node = open_addr_htab_find_node(htab, key);
  if (node == NULL) {
    return;
  }

  open_addr_htab_pos_erase(htab, node - htab->arr_header->arr);
}

void
open_addr_htab_for_each(struct open_addr_htab *htab, for_each_fn for_each,
                        void *state) {
  assert(htab != NULL);
  assert(for_each != NULL);

  for (size_t i = 0; i < primes[htab->arr_header->primes_idx]; ++i) {
    if (htab->arr_header->arr[i].key != NULL &&
        htab->arr_header->arr[i].key != ERASED) {
      for_each(htab->arr_header->arr[i].key, htab->arr_header->arr[i].val,
               state);
    }
  }
}

struct open_addr_htab_arr_header *
open_addr_htab_arr_header_ctor(size_t primes_idx) {
  struct errinj *errinj =
      errinj(ERRINJ_OPEN_ADDR_HTAB_ARR_HEADER_CTOR_PRIME_OVERFLOW);
  if (errinj != NULL && errinj->enabled) {
    primes_idx = primes_cnt;
  }

  if (primes_idx >= primes_cnt) {
    return NULL;
  }

  assert(primes_idx < primes_cnt);
  size_t arr_alloc_sz = primes[primes_idx] * sizeof(struct open_addr_htab_node);

  struct open_addr_htab_arr_header *arr_header =
      malloc(sizeof(*arr_header) + arr_alloc_sz);

  errinj = errinj(ERRINJ_OPEN_ADDR_HTAB_ARR_HEADER_CTOR_ALLOC);
  if (errinj != NULL && errinj->enabled) {
    if (arr_header != NULL) {
      free(arr_header);
    }
    arr_header = NULL;
  }

  if (arr_header == NULL) {
    return NULL;
  }

  arr_header->primes_idx = primes_idx;
  memset(arr_header->arr, 0, arr_alloc_sz);
  return arr_header;
}

void
open_addr_htab_arr_header_dtor(struct open_addr_htab_arr_header *arr_header) {
  free(arr_header);
}

bool
rehash(struct open_addr_htab *htab) {
  assert(htab != NULL);

  struct open_addr_htab_arr_header *new_arr_header =
      open_addr_htab_arr_header_ctor(htab->arr_header->primes_idx + 1);
  if (new_arr_header == NULL) {
    return false;
  }

  struct open_addr_htab new_htab = {
      .hash = htab->hash,
      .sz = 0,
      .arr_header = new_arr_header,
  };
  open_addr_htab_for_each(htab, move_node_to_new_htab, &new_htab);
  open_addr_htab_arr_header_dtor(htab->arr_header);
  htab->arr_header = new_arr_header;

  return true;
}

static void
move_node_to_new_htab(const char *key, void *val, void *state) {
  assert(key != NULL);
  assert(val != NULL);
  assert(state != NULL);

  struct open_addr_htab *htab = state;
  open_addr_htab_insert(htab, key, val);
}

size_t
crc32_hash(const char *key) {
  assert(key != NULL);

  ssize_t len = (ssize_t)strlen(key);
  size_t hash = 0;
  ssize_t i = 0;
  for (; i < len - (ssize_t)sizeof(long long); i += sizeof(long long)) {
    hash += __crc32q(hash, *(long long *)(key + i));
  }

  for (; i < len - (ssize_t)sizeof(int); i += sizeof(int)) {
    hash += __crc32d(hash, *(const int *)(key + i));
  }

  for (; i < len - (ssize_t)sizeof(short); i += sizeof(short)) {
    hash += __crc32w(hash, *(const short *)(key + i));
  }

  for (; i < len; i += sizeof(char)) {
    hash += __crc32w(hash, *(const char *)(key + i));
  }

  return hash;
}

static struct open_addr_htab_node *
open_addr_htab_find_node(struct open_addr_htab *htab, const char *key) {
  size_t hash_pos = htab->hash(key) % primes[htab->arr_header->primes_idx];
  size_t pos = hash_pos;
  ssize_t first_erased_pos = -1;
  while (htab->arr_header->arr[pos].key != NULL) {
    if (htab->arr_header->arr[pos].key == ERASED && first_erased_pos == -1) {
      first_erased_pos = (ssize_t)pos;
    }
    if (htab->arr_header->arr[pos].key != ERASED &&
        strcmp(key, htab->arr_header->arr[pos].key) == 0) {
      break;
    }

    pos = open_addr_htab_pos_next(htab, pos);
  }

  if (htab->arr_header->arr[pos].key == NULL) {
    return NULL;
  }

  if (pos != hash_pos && first_erased_pos != -1) {
    htab->arr_header->arr[first_erased_pos] = htab->arr_header->arr[pos];
    open_addr_htab_pos_erase(htab, pos);
    pos = first_erased_pos;
  }

  return &htab->arr_header->arr[pos];
}

static size_t
open_addr_htab_pos_next(const struct open_addr_htab *htab, size_t pos) {
  return (pos + 1) % primes[htab->arr_header->primes_idx];
}

static void
open_addr_htab_pos_erase(struct open_addr_htab *htab, size_t pos) {
  if (htab->arr_header->arr[open_addr_htab_pos_next(htab, pos)].key == NULL) {
    htab->arr_header->arr[pos].key = NULL;
    assert(htab->sz > 0);
    --htab->sz;
  } else {
    htab->arr_header->arr[pos].key = ERASED;
  }
}
