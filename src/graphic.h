#ifndef GRAPHIC_H_
#define GRAPHIC_H_

#include "libc/types.h"

mbed_error_t handle_petname_check(char * const string, uint32_t string_len);
mbed_error_t handle_pin(uint32_t    pintype);

#endif/*!GRAPHIC_H_*/
