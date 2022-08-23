#include "hshg.h"

#include <assert.h>

int error_handler(int e, int c) {
  (void) c;
  if(e == EINTR || e == 0) return 0;
  return -1;
}

int main() {
  struct hshg hshg = {0};

  

  return 0;
}