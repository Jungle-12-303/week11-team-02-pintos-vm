#ifndef __LIB_CSTR_H
#define __LIB_CSTR_H

#include "string.h"

/*
 * C 문자열의 널 문자까지 포함한 바이트 수를 구한다.
 */
#define CSTR_SIZE(str) (strlen (str) + 1)

#endif /* lib/cstr.h */
