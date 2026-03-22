/* libusb config.h — dispatch to a platform-specific manual config.
 *
 * The vendored tree originally shipped only MSVC + Windows sources; Qt Creator
 * on macOS/Linux needs Darwin / Linux configs and matching sources under os/.
 */
#ifndef LIBUSB_QTC_BUNDLED_CONFIG_H
#define LIBUSB_QTC_BUNDLED_CONFIG_H

#if defined(__APPLE__)
#include "config-darwin.h"
#elif defined(__linux__)
#include "config-linux.h"
#elif defined(_WIN32) && defined(_MSC_VER)
#include "config-msvc.h"
#elif defined(_WIN32)
#error "Bundled libusb: Windows builds require MSVC (clang-cl with _MSC_VER is ok). MinGW is not configured."
#else
#error "Bundled libusb: unsupported platform (expected macOS, Linux, or Windows+MSVC)."
#endif

#endif /* LIBUSB_QTC_BUNDLED_CONFIG_H */
