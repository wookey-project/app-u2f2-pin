/**
 * @file main.c
 *
 * \brief Main of dummy
 *
 */

#include "autoconf.h"


#include "libc/syscall.h"
#include "libc/stdio.h"
#include "libc/nostd.h"
#include "libc/string.h"
#include "libc/sys/msg.h"
#include "libc/errno.h"


#  include "fido.h"
#  include "libspi.h"
#  include "libtouch.h"
#  include "libtft.h"
#  include "main.h"

#include "gui_pin.h"

#include "libc/sanhandlers.h"

int usb_msq = 0;

int get_usb_msq(void) {
    return usb_msq;
}



/******************************************************
 * The task main function, called by do_starttask().
 * This is the user application code entrypoint after
 * the libstd execution.
 * From now on, the SSP is active.
 *****************************************************/
int _main(uint32_t task_id)
{
    uint8_t ret;

    printf("Hello ! I'm u2fpin, my id is %x\n", task_id);

    usb_msq = msgget("usb", IPC_CREAT | IPC_EXCL);
    if (usb_msq == -1) {
        printf("error while requesting SysV message queue. Errno=%x\n", errno);
        goto err;
    }


    /* graphical mode: declaring graphical devices */

#if CONFIG_WOOKEY_V1
    if (spi1_early_init()) {
#elif defined(CONFIG_WOOKEY_V2) || defined(CONFIG_WOOKEY_V3)
    if (spi2_early_init()) {
#else
# error "unsupported board for graphical interface"
#endif
        printf("ERROR: registering SPI1 failed.\n");
        while (1)
            ;
    }
    if (tft_early_init()) {
        printf("ERROR: registering TFT failed.\n");
        while (1)
            ;
    }
    if (touch_early_init()) {
        printf("ERROR: registering Touchscreen failed.\n");
        while (1)
            ;
    }

    printf("Registered SPI1, Touchscreen and TFT.\n");
    /* get back smart task id, as we communicate with it */

    /*******************************************
     * end of the initialization phase
     * no ressource can be declared from now on
     *******************************************/

    printf("set init as done\n");
    if ((ret = sys_init(INIT_DONE))) {
        printf("sys_init returns %s !\n", strerror(ret));
        goto err;

    }

    /*******************************************
     * Nominal phase startup: initialize devices,
     * which are now memory-mapped
     *******************************************/

    if (tft_init()) {
        printf("error during TFT initialization!\n");
    }
    if (touch_init()) {
        printf("error during Touch initialization!\n");
    }
    /* Register our callback as a valid one */
        /* white background */
        tft_fill_rectangle(0,240,0,320,0xff,0xff,0xff);
        /* FIDO logo */
        tft_rle_image(0,0,fido_width,fido_height,fido_colormap,fido,sizeof(fido));

    /* here we request PIN from user */



#if CONFIG_APP_U2FPIN_INPUT_SCREEN
    bool auth_validated = false;
    uint8_t num_auth = 0;
    do {
        if (num_auth >= CONFIG_APP_U2FPIN_MAX_PINTRIES) {
            sys_reset();
        }
        uint8_t pin_len = 15;
        char pin[16] = { 0 };
        pin_len = pin_request_digits("Unlock U2F2", 14, 0,240,60,320,pin,15);
        if (strncmp(pin, "1234", 4) == 0) {
            auth_validated = true;
        }
        num_auth++;
    } while (!auth_validated);
#else/* MOCKUP */
    printf("Mockup mode: UNLOCKED!\n");
#endif

    /* get back auth request from user and acknowledge */
    struct msgbuf msgbuf = { 0 };

    if (msgrcv(usb_msq, &msgbuf.mtext, 0, MAGIC_PIN_CONFIRM_UNLOCK, 0) == -1) {
        printf("failed to recv CONFIRM_UNLOCK from usb! erro=%d\n", errno);
        goto err;
    }
    msgbuf.mtype = MAGIC_PIN_UNLOCK_CONFIRMED;
    msgsnd(usb_msq, &msgbuf, 0, 0);


    while (1) {
        sys_sleep(1000, SLEEP_MODE_INTERRUPTIBLE);
    }

    return 0;
err:
    while (1);
    return 1;
}
