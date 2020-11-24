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

#ifndef CAT_SIGNAL_H
#define CAT_SIGNAL_H
#ifdef __cplusplus
extern "C" {
#endif

#include "cat.h"

CAT_API cat_bool_t cat_kill(int pid, int signum);
CAT_API cat_bool_t cat_signal_wait(int signum, cat_timeout_t timeout);

/* signal map and enum */

#ifdef SIGHUP
#define CAT_SIGNAL_MAP_HUP(XX) XX(HUP)
#else
#define CAT_SIGNAL_MAP_HUP(XX)
#endif
#ifdef SIGINT
#define CAT_SIGNAL_MAP_INT(XX) XX(INT)
#else
#define CAT_SIGNAL_MAP_INT(XX)
#endif
#ifdef SIGQUIT
#define CAT_SIGNAL_MAP_QUIT(XX) XX(QUIT)
#else
#define CAT_SIGNAL_MAP_QUIT(XX)
#endif
#ifdef SIGILL
#define CAT_SIGNAL_MAP_ILL(XX) XX(ILL)
#else
#define CAT_SIGNAL_MAP_ILL(XX)
#endif
#ifdef SIGTRAP
#define CAT_SIGNAL_MAP_TRAP(XX) XX(TRAP)
#else
#define CAT_SIGNAL_MAP_TRAP(XX)
#endif
#ifdef SIGABRT
#define CAT_SIGNAL_MAP_ABRT(XX) XX(ABRT)
#else
#define CAT_SIGNAL_MAP_ABRT(XX)
#endif
#ifdef SIGIOT
#define CAT_SIGNAL_MAP_IOT(XX) XX(IOT)
#else
#define CAT_SIGNAL_MAP_IOT(XX)
#endif
#ifdef SIGBUS
#define CAT_SIGNAL_MAP_BUS(XX) XX(BUS)
#else
#define CAT_SIGNAL_MAP_BUS(XX)
#endif
#ifdef SIGFPE
#define CAT_SIGNAL_MAP_FPE(XX) XX(FPE)
#else
#define CAT_SIGNAL_MAP_FPE(XX)
#endif
#ifdef SIGKILL
#define CAT_SIGNAL_MAP_KILL(XX) XX(KILL)
#else
#define CAT_SIGNAL_MAP_KILL(XX)
#endif
#ifdef SIGUSR1
#define CAT_SIGNAL_MAP_USR1(XX) XX(USR1)
#else
#define CAT_SIGNAL_MAP_USR1(XX)
#endif
#ifdef SIGSEGV
#define CAT_SIGNAL_MAP_SEGV(XX) XX(SEGV)
#else
#define CAT_SIGNAL_MAP_SEGV(XX)
#endif
#ifdef SIGUSR2
#define CAT_SIGNAL_MAP_USR2(XX) XX(USR2)
#else
#define CAT_SIGNAL_MAP_USR2(XX)
#endif
#ifdef SIGPIPE
#define CAT_SIGNAL_MAP_PIPE(XX) XX(PIPE)
#else
#define CAT_SIGNAL_MAP_PIPE(XX)
#endif
#ifdef SIGALRM
#define CAT_SIGNAL_MAP_ALRM(XX) XX(ALRM)
#else
#define CAT_SIGNAL_MAP_ALRM(XX)
#endif
#ifdef SIGTERM
#define CAT_SIGNAL_MAP_TERM(XX) XX(TERM)
#else
#define CAT_SIGNAL_MAP_TERM(XX)
#endif
#ifdef SIGCHLD
#define CAT_SIGNAL_MAP_CHLD(XX) XX(CHLD)
#else
#define CAT_SIGNAL_MAP_CHLD(XX)
#endif
#ifdef SIGSTKFLT
#define CAT_SIGNAL_MAP_STKFLT(XX) XX(STKFLT)
#else
#define CAT_SIGNAL_MAP_STKFLT(XX)
#endif
#ifdef SIGCONT
#define CAT_SIGNAL_MAP_CONT(XX) XX(CONT)
#else
#define CAT_SIGNAL_MAP_CONT(XX)
#endif
#ifdef SIGSTOP
#define CAT_SIGNAL_MAP_STOP(XX) XX(STOP)
#else
#define CAT_SIGNAL_MAP_STOP(XX)
#endif
#ifdef SIGTSTP
#define CAT_SIGNAL_MAP_TSTP(XX) XX(TSTP)
#else
#define CAT_SIGNAL_MAP_TSTP(XX)
#endif
#ifdef SIGBREAK
#define CAT_SIGNAL_MAP_BREAK(XX) XX(BREAK)
#else
#define CAT_SIGNAL_MAP_BREAK(XX)
#endif
#ifdef SIGTTIN
#define CAT_SIGNAL_MAP_TTIN(XX) XX(TTIN)
#else
#define CAT_SIGNAL_MAP_TTIN(XX)
#endif
#ifdef SIGTTOU
#define CAT_SIGNAL_MAP_TTOU(XX) XX(TTOU)
#else
#define CAT_SIGNAL_MAP_TTOU(XX)
#endif
#ifdef SIGURG
#define CAT_SIGNAL_MAP_URG(XX) XX(URG)
#else
#define CAT_SIGNAL_MAP_URG(XX)
#endif
#ifdef SIGXCPU
#define CAT_SIGNAL_MAP_XCPU(XX) XX(XCPU)
#else
#define CAT_SIGNAL_MAP_XCPU(XX)
#endif
#ifdef SIGXFSZ
#define CAT_SIGNAL_MAP_XFSZ(XX) XX(XFSZ)
#else
#define CAT_SIGNAL_MAP_XFSZ(XX)
#endif
#ifdef SIGVTALRM
#define CAT_SIGNAL_MAP_VTALRM(XX) XX(VTALRM)
#else
#define CAT_SIGNAL_MAP_VTALRM(XX)
#endif
#ifdef SIGPROF
#define CAT_SIGNAL_MAP_PROF(XX) XX(PROF)
#else
#define CAT_SIGNAL_MAP_PROF(XX)
#endif
#ifdef SIGWINCH
#define CAT_SIGNAL_MAP_WINCH(XX) XX(WINCH)
#else
#define CAT_SIGNAL_MAP_WINCH(XX)
#endif
#ifdef SIGIO
#define CAT_SIGNAL_MAP_IO(XX) XX(IO)
#else
#define CAT_SIGNAL_MAP_IO(XX)
#endif
#ifdef SIGPOLL
#define CAT_SIGNAL_MAP_POLL(XX) XX(POLL)
#else
#define CAT_SIGNAL_MAP_POLL(XX)
#endif
#ifdef SIGLOST
#define CAT_SIGNAL_MAP_LOST(XX) XX(LOST)
#else
#define CAT_SIGNAL_MAP_LOST(XX)
#endif
#ifdef SIGPWR
#define CAT_SIGNAL_MAP_PWR(XX) XX(PWR)
#else
#define CAT_SIGNAL_MAP_PWR(XX)
#endif
#ifdef SIGINFO
#define CAT_SIGNAL_MAP_INFO(XX) XX(INFO)
#else
#define CAT_SIGNAL_MAP_INFO(XX)
#endif
#ifdef SIGSYS
#define CAT_SIGNAL_MAP_SYS(XX) XX(SYS)
#else
#define CAT_SIGNAL_MAP_SYS(XX)
#endif
#ifdef SIGUNUSED
#define CAT_SIGNAL_MAP_UNUSED(XX) XX(UNUSED)
#else
#define CAT_SIGNAL_MAP_UNUSED(XX)
#endif

#define CAT_SIGNAL_MAP(XX) \
        CAT_SIGNAL_MAP_HUP(XX) \
        CAT_SIGNAL_MAP_INT(XX) \
        CAT_SIGNAL_MAP_QUIT(XX) \
        CAT_SIGNAL_MAP_ILL(XX) \
        CAT_SIGNAL_MAP_TRAP(XX) \
        CAT_SIGNAL_MAP_ABRT(XX) \
        CAT_SIGNAL_MAP_IOT(XX) \
        CAT_SIGNAL_MAP_BUS(XX) \
        CAT_SIGNAL_MAP_FPE(XX) \
        CAT_SIGNAL_MAP_KILL(XX) \
        CAT_SIGNAL_MAP_USR1(XX) \
        CAT_SIGNAL_MAP_SEGV(XX) \
        CAT_SIGNAL_MAP_USR2(XX) \
        CAT_SIGNAL_MAP_PIPE(XX) \
        CAT_SIGNAL_MAP_ALRM(XX) \
        CAT_SIGNAL_MAP_TERM(XX) \
        CAT_SIGNAL_MAP_CHLD(XX) \
        CAT_SIGNAL_MAP_STKFLT(XX) \
        CAT_SIGNAL_MAP_CONT(XX) \
        CAT_SIGNAL_MAP_STOP(XX) \
        CAT_SIGNAL_MAP_TSTP(XX) \
        CAT_SIGNAL_MAP_BREAK(XX) \
        CAT_SIGNAL_MAP_TTIN(XX) \
        CAT_SIGNAL_MAP_TTOU(XX) \
        CAT_SIGNAL_MAP_URG(XX) \
        CAT_SIGNAL_MAP_XCPU(XX) \
        CAT_SIGNAL_MAP_XFSZ(XX) \
        CAT_SIGNAL_MAP_VTALRM(XX) \
        CAT_SIGNAL_MAP_PROF(XX) \
        CAT_SIGNAL_MAP_WINCH(XX) \
        CAT_SIGNAL_MAP_IO(XX) \
        CAT_SIGNAL_MAP_POLL(XX) \
        CAT_SIGNAL_MAP_LOST(XX) \
        CAT_SIGNAL_MAP_PWR(XX) \
        CAT_SIGNAL_MAP_INFO(XX) \
        CAT_SIGNAL_MAP_SYS(XX) \
        CAT_SIGNAL_MAP_UNUSED(XX) \

enum cat_signal_e {
#define CAT_SIGNAL_GEN(name) CAT_SIG##name = SIG##name,
    CAT_SIGNAL_MAP(CAT_SIGNAL_GEN)
#undef CAT_SIGNAL_GEN
};

#ifdef __cplusplus
}
#endif
#endif /* CAT_SIGNAL_H */
