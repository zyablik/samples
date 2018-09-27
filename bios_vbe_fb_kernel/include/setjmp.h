#ifndef _SETJMP_H_
#define _SETJMP_H_

#define	_JBLEN	10		

typedef long jmp_buf[_JBLEN];

#ifdef __cplusplus
extern "C" {
#endif

int setjmp (jmp_buf);
void longjmp (jmp_buf, int);

#ifdef __cplusplus
}
#endif

#endif
