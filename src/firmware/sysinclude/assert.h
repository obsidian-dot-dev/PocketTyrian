#ifndef _ASSERT_H
#define _ASSERT_H
#include <stdio.h>
#define assert(x) ((x) ? (void)0 : printf("ASSERT FAIL: %s at %s:%d\n", #x, __FILE__, __LINE__))
#endif
