#ifndef MAIN_H_
#define MAIN_H_

#include "autoconf.h"

/* PIN task interactions */
#if CONFIG_USR_APP_U2F2_DEBUG
# define _log_printf(...) printf(__VA_ARGS__)
#else
# define _log_printf(...)
#endif


#endif/*MAIN_H_*/
