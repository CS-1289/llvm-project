//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP___RANDOM_UNIFORM_REAL_DISTRIBUTION_H
#define _LIBCPP___RANDOM_UNIFORM_REAL_DISTRIBUTION_H

#include <__config>
#include <__random/generate_canonical.h>
#include <__random/is_valid.h>
#include <iosfwd>
#include <limits>

#if !defined(_LIBCPP_HAS_NO_PRAGMA_SYSTEM_HEADER)
#  pragma GCC system_header
#endif

_LIBCPP_PUSH_MACROS
#include <__undef_macros>

_LIBCPP_BEGIN_NAMESPACE_STD

template <class _RealType = double>
class uniform_real_distribution {
  static_assert(__libcpp_random_is_valid_realtype<_RealType>::value,
                "RealType must be a supported floating-point type");

public:
  // types
  typedef _RealType result_type;

  class param_type {
    result_type __a_;
    result_type __b_;

  public:
    typedef uniform_real_distribution distribution_type;

    _LIBCPP_HIDE_FROM_ABI explicit param_type(result_type __a = 0, result_type __b = 1) : __a_(__a), __b_(__b) {}

    _LIBCPP_HIDE_FROM_ABI result_type a() const { return __a_; }
    _LIBCPP_HIDE_FROM_ABI result_type b() const { return __b_; }

    friend _LIBCPP_HIDE_FROM_ABI bool operator==(const param_type& __x, const param_type& __y) {
      return __x.__a_ == __y.__a_ && __x.__b_ == __y.__b_;
    }
    friend _LIBCPP_HIDE_FROM_ABI bool operator!=(const param_type& __x, const param_type& __y) { return !(__x == __y); }
  };

private:
  param_type __p_;

public:
  // constructors and reset functions
#ifndef _LIBCPP_CXX03_LANG
  _LIBCPP_HIDE_FROM_ABI uniform_real_distribution() : uniform_real_distribution(0) {}
  _LIBCPP_HIDE_FROM_ABI explicit uniform_real_distribution(result_type __a, result_type __b = 1)
      : __p_(param_type(__a, __b)) {}
#else
  _LIBCPP_HIDE_FROM_ABI explicit uniform_real_distribution(result_type __a = 0, result_type __b = 1)
      : __p_(param_type(__a, __b)) {}
#endif
  _LIBCPP_HIDE_FROM_ABI explicit uniform_real_distribution(const param_type& __p) : __p_(__p) {}
  _LIBCPP_HIDE_FROM_ABI void reset() {}

  // generating functions
  template <class _URNG>
  _LIBCPP_HIDE_FROM_ABI result_type operator()(_URNG& __g) {
    return (*this)(__g, __p_);
  }
  template <class _URNG>
  _LIBCPP_HIDE_FROM_ABI result_type operator()(_URNG& __g, const param_type& __p);

  // property functions
  _LIBCPP_HIDE_FROM_ABI result_type a() const { return __p_.a(); }
  _LIBCPP_HIDE_FROM_ABI result_type b() const { return __p_.b(); }

  _LIBCPP_HIDE_FROM_ABI param_type param() const { return __p_; }
  _LIBCPP_HIDE_FROM_ABI void param(const param_type& __p) { __p_ = __p; }

  _LIBCPP_HIDE_FROM_ABI result_type min() const { return a(); }
  _LIBCPP_HIDE_FROM_ABI result_type max() const { return b(); }

  friend _LIBCPP_HIDE_FROM_ABI bool
  operator==(const uniform_real_distribution& __x, const uniform_real_distribution& __y) {
    return __x.__p_ == __y.__p_;
  }
  friend _LIBCPP_HIDE_FROM_ABI bool
  operator!=(const uniform_real_distribution& __x, const uniform_real_distribution& __y) {
    return !(__x == __y);
  }
};

template <class _RealType>
template <class _URNG>
inline typename uniform_real_distribution<_RealType>::result_type
uniform_real_distribution<_RealType>::operator()(_URNG& __g, const param_type& __p) {
  static_assert(__libcpp_random_is_valid_urng<_URNG>::value, "");
  return (__p.b() - __p.a()) * std::generate_canonical<_RealType, numeric_limits<_RealType>::digits>(__g) + __p.a();
}

template <class _CharT, class _Traits, class _RT>
_LIBCPP_HIDE_FROM_ABI basic_ostream<_CharT, _Traits>&
operator<<(basic_ostream<_CharT, _Traits>& __os, const uniform_real_distribution<_RT>& __x) {
  __save_flags<_CharT, _Traits> __lx(__os);
  typedef basic_ostream<_CharT, _Traits> _OStream;
  __os.flags(_OStream::dec | _OStream::left | _OStream::fixed | _OStream::scientific);
  _CharT __sp = __os.widen(' ');
  __os.fill(__sp);
  return __os << __x.a() << __sp << __x.b();
}

template <class _CharT, class _Traits, class _RT>
_LIBCPP_HIDE_FROM_ABI basic_istream<_CharT, _Traits>&
operator>>(basic_istream<_CharT, _Traits>& __is, uniform_real_distribution<_RT>& __x) {
  typedef uniform_real_distribution<_RT> _Eng;
  typedef typename _Eng::result_type result_type;
  typedef typename _Eng::param_type param_type;
  __save_flags<_CharT, _Traits> __lx(__is);
  typedef basic_istream<_CharT, _Traits> _Istream;
  __is.flags(_Istream::dec | _Istream::skipws);
  result_type __a;
  result_type __b;
  __is >> __a >> __b;
  if (!__is.fail())
    __x.param(param_type(__a, __b));
  return __is;
}

_LIBCPP_END_NAMESPACE_STD

_LIBCPP_POP_MACROS

#endif // _LIBCPP___RANDOM_UNIFORM_REAL_DISTRIBUTION_H
