
#include "config.h"
#include "glibconfig.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/types.h>

#include <time.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>


#include <string.h>


#include <fcntl.h>

#include "gbacktrace.h"

#include "gtypes.h"
#include "gmain.h"
#include "gprintfint.h"
#include "gutils.h"

static void stack_trace (const char * const *args);


#define DEBUGGER "gdb"

/* People want to hit this from their debugger... */
volatile gboolean glib_on_error_halt;
volatile gboolean glib_on_error_halt = TRUE;

/**
 * g_on_error_query:
 * @prg_name: the program name, needed by gdb for the "[S]tack trace"
 *     option. If @prg_name is %NULL, g_get_prgname() is called to get
 *     the program name (which will work correctly if gdk_init() or
 *     gtk_init() has been called)
 *
 * Prompts the user with
 * `[E]xit, [H]alt, show [S]tack trace or [P]roceed`.
 * This function is intended to be used for debugging use only.
 * The following example shows how it can be used together with
 * the g_log() functions.
 *
 * |[<!-- language="C" -->
 * #include <glib.h>
 *
 * static void
 * log_handler (const gchar   *log_domain,
 *              GLogLevelFlags log_level,
 *              const gchar   *message,
 *              gpointer       user_data)
 * {
 *   g_log_default_handler (log_domain, log_level, message, user_data);
 *
 *   g_on_error_query (MY_PROGRAM_NAME);
 * }
 *
 * int
 * main (int argc, char *argv[])
 * {
 *   g_log_set_handler (MY_LOG_DOMAIN,
 *                      G_LOG_LEVEL_WARNING |
 *                      G_LOG_LEVEL_ERROR |
 *                      G_LOG_LEVEL_CRITICAL,
 *                      log_handler,
 *                      NULL);
 *   ...
 * ]|
 *
 * If "[E]xit" is selected, the application terminates with a call
 * to _exit(0).
 *
 * If "[S]tack" trace is selected, g_on_error_stack_trace() is called.
 * This invokes gdb, which attaches to the current process and shows
 * a stack trace. The prompt is then shown again.
 *
 * If "[P]roceed" is selected, the function returns.
 *
 * This function may cause different actions on non-UNIX platforms.
 *
 * On Windows consider using the `G_DEBUGGER` environment
 * variable (see [Running GLib Applications](glib-running.html)) and
 * calling g_on_error_stack_trace() instead.
 */
void g_on_error_query (const gchar *prg_name)
{
  static const gchar * const query1 = "[E]xit, [H]alt";
  static const gchar * const query2 = ", show [S]tack trace";
  static const gchar * const query3 = " or [P]roceed";
  gchar buf[16];

  if (!prg_name)
    prg_name = g_get_prgname ();

 retry:

  if (prg_name)
    _g_fprintf (stdout,
                "%s (pid:%u): %s%s%s: ",
                prg_name,
                (guint) getpid (),
                query1,
                query2,
                query3);
  else
    _g_fprintf (stdout,
                "(process:%u): %s%s: ",
                (guint) getpid (),
                query1,
                query3);
  fflush (stdout);

  if (isatty(0) && isatty(1))
    fgets (buf, 8, stdin);
  else
    strcpy (buf, "E\n");

  if ((buf[0] == 'E' || buf[0] == 'e')
      && buf[1] == '\n')
    _exit (0);
  else if ((buf[0] == 'P' || buf[0] == 'p')
           && buf[1] == '\n')
    return;
  else if (prg_name
           && (buf[0] == 'S' || buf[0] == 's')
           && buf[1] == '\n')
    {
      g_on_error_stack_trace (prg_name);
      goto retry;
    }
  else if ((buf[0] == 'H' || buf[0] == 'h')
           && buf[1] == '\n')
    {
      while (glib_on_error_halt)
        ;
      glib_on_error_halt = TRUE;
      return;
    }
  else
    goto retry;

}

/**
 * g_on_error_stack_trace:
 * @prg_name: the program name, needed by gdb for the "[S]tack trace"
 *     option
 *
 * Invokes gdb, which attaches to the current process and shows a
 * stack trace. Called by g_on_error_query() when the "[S]tack trace"
 * option is selected. You can get the current process's program name
 * with g_get_prgname(), assuming that you have called gtk_init() or
 * gdk_init().
 *
 * This function may cause different actions on non-UNIX platforms.
 *
 * When running on Windows, this function is *not* called by
 * g_on_error_query(). If called directly, it will raise an
 * exception, which will crash the program. If the `G_DEBUGGER` environment
 * variable is set, a debugger will be invoked to attach and
 * handle that exception (see [Running GLib Applications](glib-running.html)).
 */
void
g_on_error_stack_trace (const gchar *prg_name)
{
#if defined(G_OS_UNIX)
  pid_t pid;
  gchar buf[16];
  const gchar *args[5] = { DEBUGGER, NULL, NULL, NULL, NULL };
  int status;

  if (!prg_name)
    return;

  _g_sprintf (buf, "%u", (guint) getpid ());

#ifdef USE_LLDB
  args[1] = prg_name;
  args[2] = "-p";
  args[3] = buf;
#else
  args[1] = prg_name;
  args[2] = buf;
#endif

  pid = fork ();
  if (pid == 0)
    {
      stack_trace (args);
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("unable to fork " DEBUGGER);
      return;
    }

  /* Wait until the child really terminates. On Mac OS X waitpid ()
   * will also return when the child is being stopped due to tracing.
   */
  while (1)
    {
      pid_t retval = waitpid (pid, &status, 0);
      if (WIFEXITED (retval) || WIFSIGNALED (retval))
        break;
    }
#else
  if (IsDebuggerPresent ())
    G_BREAKPOINT ();
  else
    g_abort ();
#endif
}


static gboolean stack_trace_done = FALSE;

static void stack_trace_sigchld (int signum)
{
  stack_trace_done = TRUE;
}

#define BUFSIZE 1024

static void stack_trace (const char * const *args)
{
  pid_t pid;
  int in_fd[2];
  int out_fd[2];
  fd_set fdset;
  fd_set readset;
  struct timeval tv;
  int sel, idx, state, line_idx;
  char buffer[BUFSIZE];
  char c;

  stack_trace_done = FALSE;
  signal (SIGCHLD, stack_trace_sigchld);

  if ((pipe (in_fd) == -1) || (pipe (out_fd) == -1))
    {
      perror ("unable to open pipe");
      _exit (0);
    }

  pid = fork ();
  if (pid == 0)
    {
      /* Save stderr for printing failure below */
      int old_err = dup (2);
      if (old_err != -1)
        fcntl (old_err, F_SETFD, fcntl (old_err, F_GETFD) | FD_CLOEXEC);

      close (0); dup (in_fd[0]);   /* set the stdin to the in pipe */
      close (1); dup (out_fd[1]);  /* set the stdout to the out pipe */
      close (2); dup (out_fd[1]);  /* set the stderr to the out pipe */

      execvp (args[0], (char **) args);      /* exec gdb */

      /* Print failure to original stderr */
      if (old_err != -1)
        {
          close (2);
          dup (old_err);
        }
      perror ("exec " DEBUGGER " failed");
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("unable to fork");
      _exit (0);
    }

  FD_ZERO (&fdset);
  FD_SET (out_fd[0], &fdset);


  write (in_fd[1], "backtrace\n", 10);
  write (in_fd[1], "p x = 0\n", 8);
  write (in_fd[1], "quit\n", 5);

  idx = 0;
  line_idx = 0;
  state = 0;

  while (1)
    {
      readset = fdset;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      sel = select (FD_SETSIZE, &readset, NULL, NULL, &tv);
      if (sel == -1)
        break;

      if ((sel > 0) && (FD_ISSET (out_fd[0], &readset)))
        {
          if (read (out_fd[0], &c, 1))
            {
              line_idx += 1;
              switch (state)
                {
                case 0:

                  if (c == '#')
                    {
                      state = 1;
                      idx = 0;
                      buffer[idx++] = c;
                    }
                  break;
                case 1:
                  if (idx < BUFSIZE)
                    buffer[idx++] = c;
                  if ((c == '\n') || (c == '\r'))
                    {
                      buffer[idx] = 0;
                      _g_fprintf (stdout, "%s", buffer);
                      state = 0;
                      idx = 0;
                      line_idx = 0;
                    }
                  break;
                default:
                  break;
                }
            }
        }
      else if (stack_trace_done)
        break;
    }

  close (in_fd[0]);
  close (in_fd[1]);
  close (out_fd[0]);
  close (out_fd[1]);
  _exit (0);
}

