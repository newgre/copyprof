#include <cstdint>

// Simple test class with copy c'tor in other TU.
struct TestClass {
  TestClass();
  ~TestClass();
  TestClass(const TestClass &other);
  TestClass &operator=(const TestClass &other);
  int x;
  int y;
  int z;
};

using VecType = uint64_t;

class VectorLike {
public:
  VectorLike();
  ~VectorLike();
  VectorLike(const VectorLike &other);
  void Add(VecType x);
  VecType operator[](int index) const;
  VecType &operator[](int index);

private:
  int size_;
  int capacity_;
  VecType *data_;
};
