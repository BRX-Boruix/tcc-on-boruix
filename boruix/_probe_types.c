#include <stdint.h>
/* 探针：确认本目标下各「最快/最小」定宽类型究竟是什么，PRI 宏必须与之匹配。 */
_Static_assert(sizeof(int_fast8_t)  == sizeof(signed char), "FAST8 != signed char");
_Static_assert(sizeof(int_fast16_t) == sizeof(long),        "FAST16 != long");
_Static_assert(sizeof(int_fast32_t) == sizeof(long),        "FAST32 != long");
_Static_assert(sizeof(int_fast64_t) == sizeof(long),        "FAST64 != long");
_Static_assert(sizeof(int_least8_t)  == sizeof(signed char), "LEAST8 != signed char");
_Static_assert(sizeof(int_least16_t) == sizeof(short),      "LEAST16 != short");
_Static_assert(sizeof(int_least32_t) == sizeof(int),        "LEAST32 != int");
_Static_assert(sizeof(int_least64_t) == sizeof(long),       "LEAST64 != long");
_Static_assert(sizeof(intmax_t) == sizeof(long),            "intmax_t != long");
_Static_assert(sizeof(intptr_t) == sizeof(long),            "intptr_t != long");
int probe_ok(void) { return 0; }