#pragma once

#include "parallel.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <vector>

namespace ParallelTools {

template <class F> class Reducer {

#ifdef __cpp_lib_hardware_interference_size
  using std::hardware_constructive_interference_size;
  using std::hardware_destructive_interference_size;
#else
  // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned
  // │
  // ...
  static constexpr std::size_t hardware_constructive_interference_size = 64;
  static constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

  struct aligned_f {
    alignas(hardware_destructive_interference_size) F f;
  };
  std::vector<aligned_f> data;

public:
  Reducer() { data.resize(ParallelTools::getWorkers()); }
  void update(F new_values) {
    int worker_num = getWorkerNum();
    data[worker_num].f.update(new_values);
  }
  F get() const {
    F output;
    for (const auto &d : data) {
      output.update(d.f);
    }
    return output;
  }
};

template <class T> class Reducer_sum {
  static_assert(std::is_integral<T>::value, "Integral required.");
  struct F {
    T value;
    void update(const F new_value) { value += new_value.value; }
    F(T t) : value(t) {}
    F() : value(0) {}
  };
  Reducer<F> reducer;

public:
  Reducer_sum(T initial_value = {}) { add(initial_value); }
  void add(T new_value) { reducer.update(new_value); }
  void inc() { reducer.update(1); }
  T get() const { return reducer.get().value; }
};

template <class T> class Reducer_max {
  struct F {
    T value;
    void update(const F new_value) { value = std::max(value, new_value.value); }
    F(T t) : value(t) {}
    F() : value(0) {}
  };
  Reducer<F> reducer;

public:
  Reducer_max() {}
  void update(T new_value) { reducer.update(new_value); }
  T get() const { return reducer.get().value; }
};

template <class T> class Reducer_Vector {

#ifdef __cpp_lib_hardware_interference_size
  using std::hardware_constructive_interference_size;
  using std::hardware_destructive_interference_size;
#else
  // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned
  // │
  // ...
  static constexpr std::size_t hardware_constructive_interference_size = 64;
  static constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

  struct aligned_f {
    alignas(hardware_destructive_interference_size) std::vector<T> f;
  };
  std::vector<aligned_f> data;

public:
  Reducer_Vector() { data.resize(ParallelTools::getWorkers()); }

  template <typename F> void push_back(F arg) {
    int worker_num = getWorkerNum();
    data[worker_num].f.emplace_back(arg);
  }
  template <typename F> std::vector<T> get_sorted(F) const {
    std::vector<size_t> lengths(data.size() + 1);
    for (size_t i = 1; i <= data.size(); i++) {
      lengths[i] += lengths[i - 1] + data[i - 1].f.size();
    }
    std::vector<T> output(lengths[data.size()]);
    ParallelTools::parallel_for(0, data.size(), [&](size_t i) {
      std::memcpy(output.data() + lengths[i], data[i].f.data(),
                  data[i].f.size() * sizeof(T));
    });
    // TODO(wheatman) could be parallel sort
    std::sort(output.begin(), output.end());
    return output;
  }

  std::vector<T> get() const {
    std::vector<size_t> lengths(data.size() + 1);
    for (size_t i = 1; i <= data.size(); i++) {
      lengths[i] += lengths[i - 1] + data[i - 1].f.size();
    }
    std::vector<T> output(lengths[data.size()]);
    ParallelTools::parallel_for(0, data.size(), [&](size_t i) {
      std::memcpy(output.data() + lengths[i], data[i].f.data(),
                  data[i].f.size() * sizeof(T));
    });
    return output;
  }

  template <typename F> void for_each(F f) {
    ParallelTools::parallel_for(0, data.size(), [&](size_t i) {
      ParallelTools::parallel_for(0, data[i].f.size(),
                                  [&](size_t j) { f(data[i].f[j]); });
    });
  }
  template <typename F> void serial_for_each(F f) {
    for (auto &d : data) {
      for (auto &e : d.f) {
        f(e);
      }
    }
  }
  template <typename C, typename R, typename K>
  K find_first_match(C c, R r, K default_return) {
    for (auto &d : data) {
      for (auto &e : d.f) {
        if (c(e)) {
          return r(e);
        }
      }
    }
    return default_return;
  }
};

} // namespace ParallelTools