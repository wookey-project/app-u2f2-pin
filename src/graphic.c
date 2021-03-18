#include "autoconf.h"
#include "libc/types.h"
#include "libc/sys/msg.h"
#include "libc/errno.h"
#include "libc/string.h"
#include "libtft.h"
#include "gui_pin.h"
#include "main.h"
#include "libu2f2.h"
#include "process.h"

char const * const pinpad_petpin_title = "Pet PIN";
char const * const pinpad_userpin_title = "User PIN";

mbed_error_t handle_petname_check(char * const string __attribute__((unused)), uint32_t string_len __attribute__((unused)))
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    struct msgbuf msgbuf = { 0 };
#if CONFIG_APP_U2FPIN_INPUT_SCREEN
    if (pin_request_string_validation("Pet name", string, string_len) == 0) {
        msgbuf.mtext.u8[0] = 0xff;
        /* print validation screen :) */
    }
#else
    msgbuf.mtext.u8[0] = 0xff;
#endif
    msgbuf.mtype = MAGIC_PASSPHRASE_RESULT;
    msgsnd(get_fido_msq(), &msgbuf, 1, 0);

    return errcode;
}

mbed_error_t handle_pin(uint32_t    pintype)
{
    mbed_error_t errcode = MBED_ERROR_NONE;
    struct msgbuf msgbuf = { 0 };
    uint8_t pin_len = 15;
    char const * pinpad_title = NULL;
    uint32_t mtype = 0;
#if CONFIG_APP_U2FPIN_INPUT_MOCKUP
    char const * pin_value = NULL;
#endif


    switch (pintype) {
        case MAGIC_PETPIN_INSERT:
            pinpad_title = pinpad_petpin_title;
            mtype = MAGIC_PETPIN_INSERTED;
#if CONFIG_APP_U2FPIN_INPUT_MOCKUP
            pin_value = CONFIG_APP_U2FPIN_MOCKUP_PET_PIN_VALUE;
#endif
            break;
        case MAGIC_USERPIN_INSERT:
            pinpad_title = pinpad_userpin_title;
            mtype = MAGIC_USERPIN_INSERTED;
#if CONFIG_APP_U2FPIN_INPUT_MOCKUP
            pin_value = CONFIG_APP_U2FPIN_MOCKUP_USER_PIN_VALUE;
#endif
            break;
        default:
            log_printf("invalid pin type to handle!\n");
            errcode = MBED_ERROR_INVPARAM;
            goto err;
    }
#if CONFIG_APP_U2FPIN_INPUT_SCREEN
    char pin[16] = { 0 };
    pin_len = pin_request_digits(pinpad_title, 14, 0,240,60,320,pin,15);

    tft_fill_rectangle(0,240,0,320,0x10,0x71,0xaa);
    tft_rle_image(96,120,process_width,process_height,process_colormap,process,sizeof(process));
    strncpy(&msgbuf.mtext.c[0], pin, pin_len);
#else
    strcpy(&msgbuf.mtext.c[0], pin_value);
    pin_len = strlen(pin_value);
#endif
    msgbuf.mtype = mtype;
    msgsnd(get_fido_msq(), &msgbuf, pin_len, 0);

err:
    return errcode;
}
