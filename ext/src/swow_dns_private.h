/*
  +--------------------------------------------------------------------------+
  | Swow                                                                     |
  +--------------------------------------------------------------------------+
  | Licensed under the Apache License, Version 2.0 (the "License");          |
  | you may not use this file except in compliance with the License.         |
  | You may obtain a copy of the License at                                  |
  | http://www.apache.org/licenses/LICENSE-2.0                               |
  | Unless required by applicable law or agreed to in writing, software      |
  | distributed under the License is distributed on an "AS IS" BASIS,        |
  | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. |
  | See the License for the specific language governing permissions and      |
  | limitations under the License. See accompanying LICENSE file.            |
  +--------------------------------------------------------------------------+
  | Author: Twosee <twosee@php.net>                                          |
  +--------------------------------------------------------------------------+
 */

// capability macros from ext/standard/php_dns.h @ 34ea00fda3045c39e10683465ec552e64f47f4c0

#ifndef SWOW_DNS_PRIVATE_H
#define SWOW_DNS_PRIVATE_H

#include "swow_dns.h"

/*
 * The record functions are hooked only where PHP itself uses a thread-safe resolver, because the
 * query runs in the thread pool while other queries may run concurrently:
 *
 * | PHP resolver (php_config.h)   | dns_check_record | dns_get_record / dns_get_mx  |
 * | dns_search() / res_nsearch()  | hooked           | hooked with HAVE_FULL_DNS    |
 * | res_search() (global state)   | native, blocking | native, blocking             |
 * | Windows DnsQuery_A()          | hooked           | hooked                       |
 */
#ifndef PHP_WIN32
# if defined(HAVE_DNS_SEARCH) || defined(HAVE_RES_NSEARCH)
#  define SWOW_DNS_HAVE_THREAD_SAFE_SEARCH 1
# endif
# if defined(SWOW_DNS_HAVE_THREAD_SAFE_SEARCH) && defined(HAVE_DN_EXPAND) && defined(HAVE_DN_SKIPNAME)
#  define SWOW_DNS_HAVE_FULL_DNS_FUNCS 1
# endif
#endif

#if defined(PHP_WIN32) || defined(SWOW_DNS_HAVE_THREAD_SAFE_SEARCH)
# define SWOW_DNS_HAVE_RECORD_FUNCTIONS 1
#endif

#if defined(PHP_WIN32) || defined(SWOW_DNS_HAVE_FULL_DNS_FUNCS)
# define PHP_DNS_A      0x00000001
# define PHP_DNS_NS     0x00000002
# define PHP_DNS_CNAME  0x00000010
# define PHP_DNS_SOA    0x00000020
# define PHP_DNS_PTR    0x00000800
# define PHP_DNS_HINFO  0x00001000
# ifndef PHP_WIN32
#  define PHP_DNS_CAA   0x00002000
# endif
# define PHP_DNS_MX     0x00004000
# define PHP_DNS_TXT    0x00008000
# define PHP_DNS_A6     0x01000000
# define PHP_DNS_SRV    0x02000000
# define PHP_DNS_NAPTR  0x04000000
# define PHP_DNS_AAAA   0x08000000
# define PHP_DNS_ANY    0x10000000
# ifdef PHP_WIN32
#  define PHP_DNS_NUM_TYPES 12 /* Number of DNS Types Supported by PHP currently */
#  define PHP_DNS_ALL (PHP_DNS_A|PHP_DNS_NS|PHP_DNS_CNAME|PHP_DNS_SOA|PHP_DNS_PTR|PHP_DNS_HINFO|PHP_DNS_MX|PHP_DNS_TXT|PHP_DNS_A6|PHP_DNS_SRV|PHP_DNS_NAPTR|PHP_DNS_AAAA)
# else
#  define PHP_DNS_NUM_TYPES 13 /* Number of DNS Types Supported by PHP currently */
#  define PHP_DNS_ALL (PHP_DNS_A|PHP_DNS_NS|PHP_DNS_CNAME|PHP_DNS_SOA|PHP_DNS_PTR|PHP_DNS_HINFO|PHP_DNS_CAA|PHP_DNS_MX|PHP_DNS_TXT|PHP_DNS_A6|PHP_DNS_SRV|PHP_DNS_NAPTR|PHP_DNS_AAAA)
# endif
#endif

#ifdef SWOW_DNS_HAVE_RECORD_FUNCTIONS
/* dns_check_record(), dns_get_record() and dns_get_mx() with their aliases, platform dependent */
extern const zend_function_entry swow_dns_record_functions[];
#endif

/* PHP 8.4 switched the empty-hostname ValueError of dns_check_record() to the shared helper */
#if PHP_VERSION_ID < 80400
# define SWOW_DNS_HOSTNAME_EMPTY_ERROR() zend_argument_value_error(1, "cannot be empty")
#else
# define SWOW_DNS_HOSTNAME_EMPTY_ERROR() zend_argument_must_not_be_empty_error(1)
#endif

#endif /* SWOW_DNS_PRIVATE_H */
