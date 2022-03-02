#include <assert.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>

#include "errinj.h"
#include "open_addr_htab.h"

#define ENTRIES_CNT (1 << 16)

static const size_t htab_ini_sz = 59;

static size_t
dummy_hash(const char *key);

static void
sum_htab(__attribute__((unused)) const char *, void *val, void *state);

int
main() {
  struct open_addr_htab htab;
  assert(open_addr_htab_ctor(&htab, dummy_hash));
  open_addr_htab_dtor(&htab);

  assert(open_addr_htab_ctor(&htab, NULL));
  alignas(16) const char test[] = "xxxxxxxxxxxxxxxx";
  assert(open_addr_htab_find(&htab, test) == NULL);
  open_addr_htab_erase(&htab, test);

  int val = 1;
  assert(open_addr_htab_insert(&htab, test, &val));
  assert(open_addr_htab_find(&htab, test) == &val);
  open_addr_htab_erase(&htab, test);
  assert(open_addr_htab_find(&htab, test) == NULL);

  int vals[ENTRIES_CNT];
  alignas(16) char strs[ENTRIES_CNT][16];
  for (size_t i = 0; i < ENTRIES_CNT; ++i) {
    vals[i] = 1;
    snprintf(strs[i], 16, "%d", rand() + 1);
    assert(open_addr_htab_insert(&htab, strs[i], &vals[i]));
  }

  int sum = 0;
  open_addr_htab_for_each(&htab, sum_htab, &sum);
  assert(sum == ENTRIES_CNT);

  for (size_t i = 0; i < ENTRIES_CNT / 2; ++i) {
    open_addr_htab_erase(&htab, strs[i]);
  }
  for (size_t i = ENTRIES_CNT / 2; i < ENTRIES_CNT; ++i) {
    assert(open_addr_htab_find(&htab, strs[i]) == &vals[i]);
  }
  for (size_t i = ENTRIES_CNT / 2; i < ENTRIES_CNT * 3 / 4; ++i) {
    assert(open_addr_htab_find(&htab, strs[i]) == &vals[i]);
  }
  for (size_t i = ENTRIES_CNT * 3 / 4; i < ENTRIES_CNT; ++i) {
    assert(open_addr_htab_find(&htab, strs[i]) == &vals[i]);
  }

  open_addr_htab_dtor(&htab);

  assert(open_addr_htab_ctor(&htab, NULL));
  for (size_t i = 0; i < htab_ini_sz; ++i) {
    assert(open_addr_htab_insert(&htab, strs[i], &vals[i]));
    open_addr_htab_erase(&htab, strs[i]);
  }
  assert(open_addr_htab_insert(&htab, test, &val));
  assert(open_addr_htab_find(&htab, test) == &val);

  open_addr_htab_dtor(&htab);

  struct errinj *errinj = errinj(ERRINJ_OPEN_ADDR_HTAB_ARR_HEADER_CTOR_ALLOC);
  if (errinj == NULL) {
    return EXIT_SUCCESS;
  }

  errinj->enabled = true;
  assert(!open_addr_htab_ctor(&htab, NULL));
  errinj->enabled = false;
  assert(open_addr_htab_ctor(&htab, NULL));
  errinj->enabled = true;
  bool insert_failed = false;
  for (size_t i = 0; i < ENTRIES_CNT; ++i) {
    if (!open_addr_htab_insert(&htab, strs[i], &vals[i])) {
      insert_failed = true;
      break;
    }
  }
  assert(insert_failed);
  errinj->enabled = false;

  open_addr_htab_dtor(&htab);

  errinj = errinj(ERRINJ_OPEN_ADDR_HTAB_ARR_HEADER_CTOR_PRIME_OVERFLOW);
  if (errinj == NULL) {
    return EXIT_SUCCESS;
  }

  errinj->enabled = true;
  assert(!open_addr_htab_ctor(&htab, NULL));
  errinj->enabled = false;

  open_addr_htab_dtor(&htab);
}

static size_t
dummy_hash(__attribute__((__unused__)) const char *key) {
  return 0;
}

static void
sum_htab(__attribute__((__unused__)) const char *key, void *val, void *state) {
  assert(val != NULL);
  assert(state != NULL);

  int *sum = state;
  *sum += *(int *)val;
}

#undef ENTRIES_CNT
