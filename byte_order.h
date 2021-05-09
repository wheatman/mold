#pragma once

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

enum ByteOrder { LITTLE, BIG };

template <ByteOrder X, typename T, int SIZE>
class Int {
public:
  Int() : Int(0) {}

  Int(T x) {
    *this = x;
  }

  operator T() const {
    T ret = 0;
    for (int i = 0; i < SIZE; i++) {
      if (X == LITTLE)
        ret |= (u64)val[i] << (i * 8);
      else
        ret = (ret << 8) | val[i];
    }
    return ret;
  }

  Int &operator=(T x) {
    for (int i = 0; i < SIZE; i++) {
      if (X == LITTLE)
        val[i] = x >> (i * 8);
      else
        val[SIZE - 1 - i] = x >> (i * 8);
    }
    return *this;
  }

  Int &operator++() {
    return *this = *this + 1;
  }

  Int operator++(int) {
    T ret = *this;
    *this = *this + 1;
    return ret;
  }

  Int &operator--() {
    return *this = *this - 1;
  }

  Int operator--(int) {
    T ret = *this;
    *this = *this - 1;
    return ret;
  }

  Int &operator+=(T x) {
    return *this = *this + x;
  }

  Int &operator&=(T x) {
    return *this = *this & x;
  }

  Int &operator|=(T x) {
    return *this = *this | x;
  }

private:
  u8 val[SIZE];
};

template <ByteOrder> class i16_;
template <ByteOrder> class i32_;
template <ByteOrder> class i64_;
template <ByteOrder> class u16_;
template <ByteOrder> class u24_;
template <ByteOrder> class u32_;
template <ByteOrder> class u64_;

template <> class i16_<LITTLE> : public Int<LITTLE, i16, 2> { using Int::Int; };
template <> class i32_<LITTLE> : public Int<LITTLE, i32, 4> { using Int::Int; };
template <> class i64_<LITTLE> : public Int<LITTLE, i64, 8> { using Int::Int; };
template <> class u16_<LITTLE> : public Int<LITTLE, u16, 2> { using Int::Int; };
template <> class u24_<LITTLE> : public Int<LITTLE, u32, 3> { using Int::Int; };
template <> class u32_<LITTLE> : public Int<LITTLE, u32, 4> { using Int::Int; };
template <> class u64_<LITTLE> : public Int<LITTLE, u64, 8> { using Int::Int; };
template <> class i16_<BIG> : public Int<BIG, i16, 2> { using Int::Int; };
template <> class i32_<BIG> : public Int<BIG, i32, 4> { using Int::Int; };
template <> class i64_<BIG> : public Int<BIG, i64, 8> { using Int::Int; };
template <> class u16_<BIG> : public Int<BIG, u16, 2> { using Int::Int; };
template <> class u24_<BIG> : public Int<BIG, u32, 3> { using Int::Int; };
template <> class u32_<BIG> : public Int<BIG, u32, 4> { using Int::Int; };
template <> class u64_<BIG> : public Int<BIG, u64, 8> { using Int::Int; };
