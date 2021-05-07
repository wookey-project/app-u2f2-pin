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
#include "libfido.h"
#  include "libtouch.h"

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

bool request_user_presence(u2f_fido_action  action, uint16_t timeout, fidostorage_appid_slot_t    *metadata, uint8_t  *icon)
{
    bool result = false;
    uint64_t st = 0;
    uint64_t st_curr = 0;

    /* preparing background & foreground  */
    tft_fill_rectangle(0,240,0,320,0x10,0x71,0xaa);
    tft_setfg(0xff, 0xff, 0xff);
    tft_setbg(0x10,0x71,0xaa);
    tft_set_cursor_pos(40, 10);

    switch (action) {
        case U2F_FIDO_REGISTER:
            tft_puts("REGISTER");
            break;
        case U2F_FIDO_AUTHENTICATE:
            tft_puts("AUTHENTICATE");
            break;
        default:
            goto err;
            break;
    }
    /* site name */
    tft_set_cursor_pos(10, 60);
    if (strlen((char*)metadata->name) > 0) {
        tft_puts((char*)metadata->name);
    } else {
        tft_puts((char*)"(unknown service)");
    }

    /* icon: */
    switch (metadata->icon_type) {
        case ICON_TYPE_NONE:
            tft_fill_rectangle(80,125,130,175,0xff,0xff,0xff);
            break;
        case ICON_TYPE_COLOR:
            tft_fill_rectangle(80,125,130,175,metadata->icon.rgb_color[0],metadata->icon.rgb_color[1], metadata->icon.rgb_color[2]);
            break;
        case ICON_TYPE_IMAGE:
            if (icon != NULL) {
                rle_icon_t *icon_data = (rle_icon_t*)icon;
                uint32_t width = icon_data->width;
                uint32_t height = icon_data->height;
                uint32_t nbcoul = icon_data->nbcoul;
                uint8_t* colormap = &icon_data->colormap[0];
                uint8_t nbdata_pos = 12 + 3*nbcoul;
                uint32_t nbdata   = (uint32_t)icon[nbdata_pos];
                /* icon content is after the overall icon metada structure. this structure size is dynamic, depending on the number of colors */
                uint8_t *icon_content = icon + (3*sizeof(uint32_t)+((sizeof(uint8_t)*(3*(nbcoul)))+sizeof(uint32_t)));

                tft_rle_image(80,70,width,height,colormap,icon_content,nbdata);
            } else {
                tft_fill_rectangle(80,125,130,175,0x53,0xaf,0x39);
            }
            break;
        default:
            break;
    }

    /* validate button */
    tft_fill_rectangle(0,240,270,320,0x0,0x0,0x0);
    tft_fill_rectangle(2,238,272,318,0xc8,0xc8,0xc8);
    tft_fill_rectangle(4,238,274,318,0x37,0x37,0x37);
    tft_fill_rectangle(4,236,274,316,0x87,0x87,0x87);
    tft_set_cursor_pos(50, 285);
    tft_setfg(0x00, 0x00, 0x00);
    tft_setbg(0x87,0x87,0x87);
    tft_puts("ACCEPT");
    /* wait for touchscreen */

    printf("[u2fpin] handle user presence, timeout is %d\n", timeout);
    sys_get_systick(&st, PREC_MILLI);
    while (!touch_is_touched()) {
        sys_get_systick(&st_curr, PREC_MILLI);
        if ((st_curr - st) >= timeout) {
            printf("[U2FPIN] userpresence timeouted !\n");
            goto err;
        }
    }
    result = true;
err:
    return result;
}
