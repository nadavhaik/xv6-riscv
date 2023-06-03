#include "types.h"

enum pagelocation { UNINITIALIZED, PHYSICAL, VIRTUAL };

struct page {
  enum pagelocation pagelocation;
  struct {
    uint64 memaddress;
    uint64 fileoffset;
  } address;
  uint64 size;
};
