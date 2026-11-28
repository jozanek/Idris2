#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#include <errno.h>
#include <paths.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#endif

#include "idris_system.h"

#ifdef __APPLE__
// The `system` function of macOS holds a process-wide lock until the command
// ends, so only one thread at a time can run a command. `run_shell` behaves
// like `system`, but holds its lock only while it changes the signal handling.

static pthread_mutex_t signal_lock = PTHREAD_MUTEX_INITIALIZER;
static int running_commands = 0;
static struct sigaction saved_sigint, saved_sigquit;

// While any command runs, this process ignores SIGINT and SIGQUIT. The first
// command saves the old handling. `child_defaults` gets the signals that a new
// command must not inherit as ignored.
static void ignore_interrupts(sigset_t *child_defaults) {
  struct sigaction ignore;
  ignore.sa_handler = SIG_IGN;
  ignore.sa_flags = 0;
  sigemptyset(&ignore.sa_mask);
  sigemptyset(child_defaults);

  pthread_mutex_lock(&signal_lock);
  if (running_commands++ == 0) {
    sigaction(SIGINT, &ignore, &saved_sigint);
    sigaction(SIGQUIT, &ignore, &saved_sigquit);
  }
  if (saved_sigint.sa_handler != SIG_IGN)
    sigaddset(child_defaults, SIGINT);
  if (saved_sigquit.sa_handler != SIG_IGN)
    sigaddset(child_defaults, SIGQUIT);
  pthread_mutex_unlock(&signal_lock);
}

// The last command that ends restores the old handling.
static void restore_interrupts(void) {
  pthread_mutex_lock(&signal_lock);
  if (--running_commands == 0) {
    sigaction(SIGINT, &saved_sigint, NULL);
    sigaction(SIGQUIT, &saved_sigquit, NULL);
  }
  pthread_mutex_unlock(&signal_lock);
}

// Returns a wait status, or -1, in the same cases as `system`.
static int run_shell(const char *command) {
  if (command == NULL)
    return system(command);

  posix_spawnattr_t attr;
  int error = posix_spawnattr_init(&attr);
  if (error != 0) {
    errno = error;
    return -1;
  }

  // Block SIGCHLD in this thread until the command has been waited for.
  sigset_t block, old_mask, child_defaults;
  sigemptyset(&block);
  sigaddset(&block, SIGCHLD);
  pthread_sigmask(SIG_BLOCK, &block, &old_mask);
  ignore_interrupts(&child_defaults);

  posix_spawnattr_setsigmask(&attr, &old_mask);
  posix_spawnattr_setsigdefault(&attr, &child_defaults);
  posix_spawnattr_setflags(&attr,
                           POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

  const char *argv[] = {"sh", "-c", command, NULL};
  pid_t pid;
  int status;
  error = posix_spawn(&pid, _PATH_BSHELL, NULL, &attr, (char *const *)argv,
                      *_NSGetEnviron());
  posix_spawnattr_destroy(&attr);

  if (error == 0) {
    pid_t waited;
    do {
      waited = waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1)
      status = -1;
  } else if (error == ENOMEM || error == EAGAIN) {
    status = -1;
  } else {
    status = W_EXITCODE(127, 0); // could not start the shell
  }

  restore_interrupts();
  pthread_sigmask(SIG_SETMASK, &old_mask, NULL);
  return status;
}
#endif

int idris2_system(const char *command) {
#ifdef __APPLE__
  int status = run_shell(command);
#else
  int status = system(command);
#endif
#ifdef _WIN32
  return status;
#else
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

// A Chez call that lets other threads run cannot take a string. So the caller
// copies the command first, and `idris2_systemCopy` frees the copy.
void *idris2_copyCommand(const char *command) {
  return strdup(command);
}

int idris2_systemCopy(void *command) {
  if (command == NULL) // the copy failed
    return -1;
  int status = idris2_system(command);
  free(command);
  return status;
}
