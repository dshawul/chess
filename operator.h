#pragma once

#define ENABLE_OPERATORS_ON(T)												\
/* binary */																\
inline T operator+(const T d1, const T d2) { return T(int(d1) + int(d2)); }	\
inline T operator+(const T d, int i) { return T(int(d) + i); }				\
inline T operator-(const T d1, const T d2) { return T(int(d1) - int(d2)); }	\
inline T operator-(const T d, int i) { return T(int(d) - i); }				\
inline T operator^(const T d1, const T d2) { return T(int(d1) ^ int(d2)); } \
inline T operator^(const T d, int i) { return T(int(d) ^ i); }				\
/* assignment */															\
inline T& operator+=(T& d1, const T d2) { d1 = d1 + d2; return d1; }        \
inline T& operator+=(T& d, int i) { d = T(int(d) + i); return d; }			\
inline T& operator-=(T& d1, const T d2) { d1 = d1 - d2; return d1; }        \
inline T& operator-=(T& d, int i) { d = T(int(d) - i); return d; }			\
/* unary */																	\
inline T operator++(T& d) { d = T(int(d) + 1); return d; }             		\
inline T operator++(T& d, int) { T r = d; d = T(int(d) + 1); return r; }	\
inline T operator--(T& d) { d = T(int(d) - 1); return d; }             		\
inline T operator--(T& d, int) { T r = d; d = T(int(d) - 1); return r; }	\
inline T operator-(const T d) { return T(-int(d)); }
