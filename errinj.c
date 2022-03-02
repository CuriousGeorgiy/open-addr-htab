#include "errinj.h"

#define ERRINJ_MEMBER(i, b) [i] = { .enabled = (b) },

struct errinj errinjs[errinj_id_MAX] = {
	ERRINJ_LIST(ERRINJ_MEMBER)
};
