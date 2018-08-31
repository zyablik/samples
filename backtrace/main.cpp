#include <execinfo.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/* This implementation assumes a stack layout that matches the defaults
   used by gcc's `__builtin_frame_address' and `__builtin_return_address'
   (FP is the frame pointer register):
      +-----------------+     +-----------------+
    FP -> | previous FP --------> | previous FP ------>...
      |                 |     |                 |
      | return address  |     | return address  |
      +-----------------+     +-----------------+
  */

struct layout {
  void * next;
  void * return_address;
};
/* Get some notion of the current stack.  Need not be exactly the top
   of the stack, just something somewhere in the current frame.  */
#define CURRENT_STACK_FRAME  ({ char __csf; &__csf; })

/* By default we assume that the stack grows downward.  */

/* By default assume the `next' pointer in struct layout points to the
   next struct layout.  */
#define ADVANCE_STACK_FRAME(next) ((struct layout *) (next))

/* By default, the frame pointer is just what we get from gcc.  */
# define FIRST_FRAME_POINTER  __builtin_frame_address (0)

int kos_backtrace(void ** array, int size) {
    int cnt = 0;
    void * top_frame = FIRST_FRAME_POINTER;
    void * top_stack = CURRENT_STACK_FRAME;

    printf("kos_backtrace top_frame = %p top_stack = %p\n", top_frame, top_stack);

    /* We skip the call to this function, it makes no sense to record it.  */
    struct layout * current = ((struct layout *) top_frame);
    while (cnt < size) {
        printf("cnt = %d current = %p top_stack= %p\n", cnt, current, top_stack);
      if ((void *) current < top_stack) {
         /* This means the address is out of range.  Note that for the
        toplevel we see a frame pointer with value NULL which clearly is
        out of range.  */
        break;
      }

      array[cnt++] = current->return_address;
      current = ADVANCE_STACK_FRAME (current->next);
    }
    return cnt;
}


void print_backtrace(void) {
    void * bt[1024];

    int bt_size = backtrace(bt, 1024);
    char ** bt_syms = backtrace_symbols(bt, bt_size);
    printf("GCC BACKTRACE ------------\n");
    for (int i = 1; i < bt_size; i++) {
        printf("%s\n", bt_syms[i]);
    }
    printf("----------------------\n");
    free(bt_syms);
    
    printf("KOS BACKTRACE ------------\n");
    bt_size = kos_backtrace(bt, 1024);
    bt_syms = backtrace_symbols(bt, bt_size);
    for (int i = 1; i < bt_size; i++) {
        printf("%s\n", bt_syms[i]);
    }
    printf("----------------------\n");
    free(bt_syms);
}

extern void bar() {
    print_backtrace();
}

extern void foo() {
    bar();
}

int main() {
    printf("foo = %p bar = %p\n", foo, bar);
    foo();
    return 0;
}
