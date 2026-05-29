

#ifndef __GCC_MTCS_TYPES_H__
#define __GCC_MTCS_TYPES_H__

//原型 vector_types.h /usr/local/cuda-12.2/targets/x86_64-linux/include

#include "nlib.h"
#include "aetparser.h"

typedef struct _MtcsTypes MtcsTypes;
/* --- structures --- */
struct _MtcsTypes
{
    AetParser *parser;
    nboolean init;
};

MtcsTypes *mtcs_types_get();
void       mtcs_types_init(MtcsTypes *self);
nboolean   mtcs_types_is_init(MtcsTypes *self);
tree       mtcs_types_get_record(MtcsTypes *self,char *name);

#endif
