#include "sink.h"
#include <stdio.h>
#include <vector>

int main(int argc, const char *argv[]) {
  std::vector<int> v;
  for (int i = 0; i < argc; ++i) {
    v.push_back(i + 23);
  }
  for (int i = 0; i < v.size(); ++i) {
    printf("%d\n", v[i]);
  }
  std::vector<int> vv = v;
  // vv[0] = 666;
  Sink(vv);
  return 0;
}
