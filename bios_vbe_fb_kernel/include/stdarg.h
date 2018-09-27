#ifndef __CORE_STDARG_H
#define __CORE_STDARG_H

#define va_start(PTR, LASTARG)  __builtin_va_start (PTR, LASTARG)
#define va_end(PTR)             __builtin_va_end (PTR)
#define va_arg(PTR, TYPE)       __builtin_va_arg (PTR, TYPE)
#define va_list                 __builtin_va_list

#endif
