#define HAVE_MALLOC_H 1
#define HAVE_ALLOCA_H 1
#define HAVE_STRING_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDARG_H 1
//#cmakedefine HAVE__SNPRINTF_S 1

#ifdef PHP_WIN32
# include "config.w32.h"
#else
# include "php_config.h"
#endif

#if WIN32
# pragma warning(disable: 4820) // Disable alignment errors in windows headers
# pragma warning(disable: 4255) // Disable prototype () -> (void) conversion warning
# pragma warning(disable: 5045) // Disable /Qspectre mitigation info warnings
#endif
