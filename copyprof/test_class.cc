#include "test_class.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdio.h>

TestClass::TestClass() : x(0), y(0), z(0) {}

TestClass::TestClass(const TestClass &other) {
  puts("TestClass: copy c'tor");
  x = other.x;
  y = other.y;
  z = other.z;
}

TestClass &TestClass::operator=(const TestClass &other) {
  puts("TestClass: copy assignment operator");
  x = other.x;
  y = other.y;
  z = other.z;
  return *this;
}

TestClass::~TestClass() { puts("d'tor"); }

VectorLike::VectorLike() : size_(0), capacity_(0), data_(nullptr) {}

VectorLike::~VectorLike() {
  size_ = capacity_ = 0;
  delete[] data_;
}

VectorLike::VectorLike(const VectorLike &other) {
  data_ = new VecType[other.size_];
  memcpy(data_, other.data_, other.size_ * sizeof(VecType));
  size_ = other.size_;
  capacity_ = other.capacity_;
}

void VectorLike::Add(VecType x) {
  if (capacity_ == 0) {
    int alloc_size = std::max(2 * size_, 1);
    VecType *data = new VecType[alloc_size];
    memcpy(data, data_, size_ * sizeof(VecType));
    delete[] data_;
    data_ = data;
    capacity_ = alloc_size - size_;
  }
  data_[size_] = x;
  ++size_;
  --capacity_;
}

VecType VectorLike::operator[](int index) const { return data_[index]; }

VecType &VectorLike::operator[](int index) { return data_[index]; }