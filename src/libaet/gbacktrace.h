

#ifndef __G_BACKTRACE_H__
#define __G_BACKTRACE_H__



#include <glib/gtypes.h>
#include <signal.h>

G_BEGIN_DECLS

void g_on_error_query (const gchar *prg_name);
void g_on_error_stack_trace (const gchar *prg_name);

/**
 * G_BREAKPOINT:
 *
 * Inserts a breakpoint instruction into the code.
 *
 * On architectures which support it, this is implemented as a soft interrupt
 * and on other architectures it raises a `SIGTRAP` signal.
 *
 * `SIGTRAP` is used rather than abort() to allow breakpoints to be skipped past
 * in a debugger if they are not the desired target of debugging.
 */
#  define G_BREAKPOINT()        G_STMT_START{ __asm__ __volatile__ ("int $03"); }G_STMT_END



#endif /* __G_BACKTRACE_H__ */
