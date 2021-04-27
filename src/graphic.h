#ifndef GRAPHIC_H_
#define GRAPHIC_H_

#include "libc/types.h"
#include  "libfido.h"
#include "libfidostorage.h"

mbed_error_t handle_petname_check(char * const string, uint32_t string_len);
mbed_error_t handle_pin(uint32_t    pintype);

bool request_user_presence(u2f_fido_action  action, uint16_t timeout, fidostorage_appid_slot_t    *metadata, uint8_t  *icon);

#endif/*!GRAPHIC_H_*/
