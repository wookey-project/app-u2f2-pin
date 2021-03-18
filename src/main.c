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
#include "libc/nostd.h"


#  include "fido.h"
#  include "libspi.h"
#  include "libtouch.h"
#  include "libtft.h"
#  include "main.h"
#  include "graphic.h"

#include "libu2f2.h"

#include "libc/sanhandlers.h"

int fido_msq = 0;

int get_fido_msq(void) {
    return fido_msq;
}


bool handle_user_presence(void) {
#if CONFIG_APP_U2FPIN_INPUT_SCREEN
    tft_fill_rectangle(0,240,0,320,0x0,0x0,0x0);
    tft_setfg(0xff, 0xff, 0xff);
    tft_set_cursor_pos(100, 150);
    tft_puts("[X]");
    /* wait for touchscreen */
    while (!touch_is_touched()) {
        ;
    }
#else
    printf("[USB] userpresence: waiting for XX (FIX: timeout to add)\n");
    sys_sleep (1000, SLEEP_MODE_INTERRUPTIBLE);
#endif
    return true;
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

    fido_msq = msgget("fido", IPC_CREAT | IPC_EXCL);
    if (fido_msq == -1) {
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

    printf("Registered SPI, Touchscreen and TFT.\n");
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

    printf("set screen bootimg\n");
    /* Register our callback as a valid one */
    tft_fill_rectangle(0,240,0,320,0xff,0xff,0xff);
    /* FIDO logo */
    tft_rle_image(0,0,fido_width,fido_height,fido_colormap,fido,sizeof(fido));

    printf("wait for FIDO tsk\n");

    /* acknowledge that u2fpin backend is now ready */
    if (handle_signal(fido_msq, MAGIC_IS_BACKEND_READY, MAGIC_BACKEND_IS_READY, NULL) != MBED_ERROR_NONE) {
        printf("failed to handle signal!\n");
        goto err;
    }

    printf("starting main loop\n");

    /* main loop */
    struct msgbuf msgbuf = { 0 };
    ssize_t msqr;
    while (1) {
        // PetPin
        msqr = msgrcv(fido_msq, &msgbuf.mtext, 0, MAGIC_PETPIN_INSERT, IPC_NOWAIT);
        if (msqr >= 0) {
            printf("[u2fPIN] Pet PIN requested\n");
            handle_pin(MAGIC_PETPIN_INSERT);
            goto endloop;
        }
        // PassPhrase check
        msqr = msgrcv(fido_msq, &msgbuf.mtext, 64, MAGIC_PASSPHRASE_CONFIRM, IPC_NOWAIT);
        if (msqr >= 0) {
            printf("[u2fPIN] Pet name check requested (len:%d): %s\n", msqr, &msgbuf.mtext.c[0]);
            handle_petname_check(&msgbuf.mtext.c[0], msqr);
            goto endloop;
        }
        // UserPin
        msqr = msgrcv(fido_msq, &msgbuf.mtext, 0, MAGIC_USERPIN_INSERT, IPC_NOWAIT);
        if (msqr >= 0) {
            printf("[u2fPIN] User PIN requested\n");
            handle_pin(MAGIC_USERPIN_INSERT);
            goto endloop;
        }
        // User Presence
        msqr = msgrcv(fido_msq, &msgbuf.mtext, 0, MAGIC_USER_PRESENCE_REQ, IPC_NOWAIT);
        if (msqr >= 0) {
            /* Wink request received */
            printf("[u2fPIN] User Presence requested\n");
            /* check for other waiting msg before sleeping */
            if (handle_user_presence()) {
                msgbuf.mtype = MAGIC_USER_PRESENCE_ACK;
                msgsnd(fido_msq, &msgbuf, 0, 0);
            }
            goto endloop;
        }
        msqr = msgrcv(fido_msq, &msgbuf.mtext, 0, MAGIC_TOKEN_UNLOCKED, IPC_NOWAIT);
        if (msqr >= 0) {
            /* Wink request received */
            printf("[u2fPIN] token unlocked\n");
            tft_rle_image(0,0,fido_width,fido_height,fido_colormap,fido,sizeof(fido));
            goto endloop;
        }



        sys_sleep(1000, SLEEP_MODE_INTERRUPTIBLE);
endloop:
        continue;
    }

    return 0;
err:
    while (1);
    return 1;
}
