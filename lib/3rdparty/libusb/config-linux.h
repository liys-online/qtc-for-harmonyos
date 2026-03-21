/* config-linux.h — minimal autotools-style config for Linux (libusb, usbfs + netlink). */

#define DEFAULT_VISIBILITY __attribute__((visibility("default")))

#define ENABLE_LOGGING 1
/* #undef ENABLE_DEBUG_LOGGING */

#define PRINTF_FORMAT(a, b) __attribute__((format(printf, a, b)))

#define PLATFORM_POSIX 1

#define HAVE_SYS_TIME_H 1
#define HAVE_SYSLOG 1
#define HAVE_UNISTD_H 1
#define HAVE_POLL_H 1

/* linux_netlink.c */
#define HAVE_ASM_TYPES_H 1
