/*
  +--------------------------------------------------------------------------+
  | libcat                                                                   |
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

#include "cat_cpu.h"

CAT_API unsigned int cat_cpu_get_available_parallelism(void)
{
    return uv_available_parallelism();
}

CAT_API unsigned int cat_cpu_count(void)
{
#if defined(__linux__)
    return get_nprocs();
#elif defined(__hpux__)
    struct pst_dynamic psd;
    if (pstat_getdynamic (&psd, sizeof (psd), (size_t) 1, 0) != -1) {
      return psd.psd_max_proc_cnt;
    }
    return 1;
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
    int mib[2];
    int maxproc;
    size_t len;
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    len = sizeof (maxproc);
    if (-1 == sysctl (mib, 2, &maxproc, &len, NULL, 0)) {
      return 1;
    }
    return len;
#elif defined(__APPLE__) || defined(__sun) || defined(_AIX)
    int ncpu = (int) sysconf (_SC_NPROCESSORS_ONLN);
    return (ncpu > 0) ? ncpu : 1;
#elif defined(_MSC_VER) || defined(_WIN32)
   SYSTEM_INFO si;
   GetSystemInfo (&si);
   return si.dwNumberOfProcessors;
#else
#warning "cat_cpu_count() not supported, defaulting to 1."
   return 1;
#endif
}
