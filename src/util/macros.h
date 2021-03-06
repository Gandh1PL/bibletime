/*********
*
* In the name of the Father, and of the Son, and of the Holy Spirit.
*
* This file is part of BibleTime's source code, http://www.bibletime.info/.
*
* Copyright 1999-2014 by the BibleTime developers.
* The BibleTime source code is licensed under the GNU General Public License version 2.0.
*
**********/

#ifndef MACROS_H
#define MACROS_H


/**
  \file macros.h
  \brief This file is for listing reusable macros used in the BibleTime source code.
*/


/**
  \def DEPRECATED(func)
  \brief Macro for declaring functions and methods as deprecated.
*/

#ifdef __GNUC__
    #define DEPRECATED(func) func __attribute__ ((deprecated))
#elif defined(_MSC_VER)
    #define DEPRECATED(func) __declspec(deprecated) func
#else
    #define DEPRECATED(func) func
#endif


/**
  \def LIKELY(c)
  \brief Gives the compiler a hint that the given conditional is likely to
         evaluate to true.

  This helps GCC to generate code which is optimized in respect to branch
  prediction.
*/

/**
  \def UNLIKELY(c)
  \brief Gives the compiler a hint that the given conditional is likely to
         evaluate to false.

  This helps GCC to generate code which is optimized in respect to branch
  prediction.
*/

#ifdef __GNUC__
    #define LIKELY(c)   __builtin_expect(!!(c),true)
    #define UNLIKELY(c) __builtin_expect(!!(c),false)
#else
    #define LIKELY(c)   !!(c)
    #define UNLIKELY(c) !!(c)
#endif


#endif // #ifdef MACROS_H
