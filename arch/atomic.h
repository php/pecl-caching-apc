/*
  +----------------------------------------------------------------------+
  | APC                                                                  |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006 The PHP Group                                     |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Brian Shire <shire@php.net>                                 |
  +----------------------------------------------------------------------+

 */

/* $Id$ */

#ifndef APC_ARCH_ATOMIC_H

#define APC_ARCH_ATOMIC_H

#if defined __x86_64__
#include "x86_64/atomic.h"

#elif defined __i386__
#include "i386/atomic.h"

#else
#error "Unknown or Unsupported Architecture.  If you would like futex suupport for your architecture, please file a request at http://pecl.php.net/bugs/report.php?package=APC"

#endif


#endif
