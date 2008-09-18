#ifndef _STDARG_H_
#ifndef _ANSI_STDARG_H_
#ifndef __need___va_list
#define _STDARG_H_
#define _ANSI_STDARG_H_
#endif
#endif
#endif
#undef __need___va_list

#ifndef _VA_LIST
typedef char * va_list;
#define _VA_LIST
#endif

#define va_start(ap, last)      __builtin_stdarg_start((ap), last)
#define va_arg(ap, type)        __builtin_va_arg((ap), type)
#define va_end(ap)              __builtin_va_end((ap))
#define va_copy(dest, src)      __builtin_va_copy((dest), (src))

/* For broken glibc headers */
#ifndef __GNUC_VA_LIST
#define __gnuc_va_list void *
#endif
