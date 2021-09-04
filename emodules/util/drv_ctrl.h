#pragma once
#include <stdint.h>

typedef uintptr_t (*cmd_handler)(uintptr_t cmd, uintptr_t arg0, uintptr_t arg1,
				 uintptr_t arg2);

#define QUERY_INFO -1