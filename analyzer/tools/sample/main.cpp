#include <iostream>

#include "Utils/Path.h"

using namespace std;

int main() {
  cout << "left stripping 2 components from path a/b/c/d: " << stripname("a/b/c/d", 2) << endl;
  return 0;
}
