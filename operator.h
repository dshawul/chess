/*
 * Zinc, an UCI chess interface. Copyright (C) 2012 Lucas Braesch.
 *
 * Zinc is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zinc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not,
 * see <http://www.gnu.org/licenses/>.
*/
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
