#ifndef ERRINJ_H
#define ERRINJ_H

#include <stdbool.h>

#include "enum.h"

#define ERRINJ_LIST(_) \
  _(ERRINJ_OPEN_ADDR_HTAB_ARR_HEADER_CTOR_ALLOC, false) \
  _(ERRINJ_OPEN_ADDR_HTAB_ARR_HEADER_CTOR_PRIME_OVERFLOW, false)

ENUM(errinj_id, ERRINJ_LIST);

struct errinj {
  bool enabled;
};

extern struct errinj errinjs[];

#ifdef NDEBUG
#define errinj(id) (NULL)
#else
#define errinj(id)                             \
  ({                                           \
    assert((id) >= 0 && (id) < errinj_id_MAX); \
    &errinjs[(id)];                            \
  })
#endif /* NDEBUG */

#endif /* ERRINJ_H */
