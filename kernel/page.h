struct pageondisk {
  uint64 va;
  uint64 pa;
  uint64 fileoffset;
  int flags;
};