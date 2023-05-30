#include "types.h"

enum pagelocation { UNITIALIZED, PHYSICAL, VIRTUAL };

struct page {
  enum pagelocation pagelocation;
  union {
    uint64 memaddress;
    uint64 fileoffset;
  } address;
  uint64 size;
};
