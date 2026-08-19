// RUN: %clang_cc1 -std=c++11 -triple x86_64-none-linux-gnu -emit-llvm -o - %s | FileCheck %s

namespace std {
  typedef decltype(sizeof(int)) size_t;

  template <class E>
  class initializer_list {
    const E *__begin_;
    size_t __size_;

    initializer_list(const E *b, size_t s) : __begin_(b), __size_(s) {}

  public:
    initializer_list() : __begin_(nullptr), __size_(0) {}
    size_t size() const { return __size_; }
    const E *begin() const { return __begin_; }
    const E *end() const { return __begin_ + __size_; }
  };
}

void sink(std::initializer_list<int>);

// A backing array whose elements are all constants is emitted once, with static
// storage duration, instead of being copied onto the stack at each evaluation.
//
// CHECK-LABEL: define{{.*}} void @_Z8constantv(
// CHECK-NOT:   alloca [5 x i32]
// CHECK-NOT:   llvm.memcpy
// CHECK:       store ptr @.init_list_backing, ptr %{{.*}}, align 8
// CHECK:       store i64 5, ptr %{{.*}}, align 8
void constant() {
  sink({1, 2, 3, 4, 5});
}

// The same list evaluated repeatedly refers to the same array; no copy is made
// inside the loop.
//
// CHECK-LABEL: define{{.*}} void @_Z4loopv(
// CHECK-NOT:   llvm.memcpy
// CHECK:       ret void
void loop() {
  for (int i = 0; i < 10; ++i)
    sink({1, 2, 3, 4, 5});
}

// A run-time value forces the array back onto the stack.
//
// CHECK-LABEL: define{{.*}} void @_Z11nonconstanti(
// CHECK:       alloca [3 x i32]
void nonconstant(int x) {
  sink({1, x, 3});
}

namespace nontrivial_dtor {
  struct S {
    int x;
    constexpr S(int x) : x(x) {}
    ~S();
  };
  void sink(std::initializer_list<S>);

  // A non-trivially-destructible element type still needs a scoped cleanup, so
  // the array stays on the stack.
  //
  // CHECK-LABEL: define{{.*}} void @_ZN15nontrivial_dtor1fEv(
  // CHECK:       alloca [3 x %{{.*}}]
  void f() { sink({S(1), S(2), S(3)}); }
}

namespace mutable_member {
  struct S {
    mutable int x;
  };
  void sink(std::initializer_list<S>);

  // A mutable subobject would be observably shared between evaluations.
  //
  // CHECK-LABEL: define{{.*}} void @_ZN14mutable_member1fEv(
  // CHECK:       alloca [2 x %{{.*}}]
  void f() { sink({S{1}, S{2}}); }
}
