/* config-darwin.h — minimal autotools-style config for macOS / Darwin (libusb). */

#define DEFAULT_VISIBILITY __attribute__((visibility("default")))

/* Define to 1 to enable message logging. */
#define ENABLE_LOGGING 1
/* #undef ENABLE_DEBUG_LOGGING */

#define PRINTF_FORMAT(a, b) __attribute__((format(printf, a, b)))

#define PLATFORM_POSIX 1

#define HAVE_SYS_TIME_H 1
#define HAVE_SYSLOG 1
#define HAVE_UNISTD_H 1
#define HAVE_POLL_H 1

/* pthread_threadid_np (macOS) — used by threads_posix.c */
#define HAVE_PTHREAD_THREADID_NP 1

/* IOUSBHost* definitions (current macOS SDKs) */
#define HAVE_IOKIT_USB_IOUSBHOSTFAMILYDEFINITIONS_H 1
