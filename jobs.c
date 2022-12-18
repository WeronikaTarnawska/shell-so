#include "shell.h"

typedef struct proc {
  pid_t pid;    /* process identifier */
  int state;    /* RUNNING or STOPPED or FINISHED */
  int exitcode; /* -1 if exit status not yet received */
} proc_t;

typedef struct job {
  pid_t pgid;            /* 0 if slot is free */
  proc_t *proc;          /* array of processes running in as a job */
  struct termios tmodes; /* saved terminal modes */
  int nproc;             /* number of processes */
  int state;             /* changes when live processes have same state */
  char *command;         /* textual representation of command line */
} job_t;

static job_t *jobs = NULL;          /* array of all jobs */
static int njobmax = 1;             /* number of slots in jobs array */
static int tty_fd = -1;             /* controlling terminal file descriptor */
static struct termios shell_tmodes; /* saved shell terminal modes */

static void sigchld_handler(int sig) {
  int old_errno = errno;
  pid_t pid;
  int status;
  /* TODO: Change state (FINISHED, RUNNING, STOPPED) of processes and jobs.
   * Bury all children that finished saving their status in jobs. */
#ifdef STUDENT
  for (int i = 0; i < njobmax; i++) {
    job_t *j = &jobs[i];
    if (j->pgid == 0)
      continue;
    for (int p = 0; p < j->nproc; p++) {
      pid = waitpid(j->proc[p].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
      //   safe_printf("sigchild handler, job %d, searched pid %d, got pid
      //   %d\n", i,
      //               j->pgid, pid);
      if (pid <= 0)
        continue;
      if (WIFSTOPPED(status)) {
        // safe_printf("[%d] stopped\n", j->pgid);
        j->state = STOPPED;
        j->proc[p].state = STOPPED;
      } else if (WIFCONTINUED(status)) {
        // safe_printf("[%d] resumed\n", j->pgid);
        j->state = RUNNING;
        j->proc[p].state = RUNNING;
      } else if (WIFEXITED(status) /*|| WIFSIGNALED(status)*/) {
        // safe_printf("[%d] exited '%s', status=%d\n", i, j->command,
        //             WEXITSTATUS(status));
        j->proc[p].state = FINISHED;
        j->proc[p].exitcode = status;
        int done = 0;
        for (int k = 0; k < j->nproc; k++)
          if (j->proc[k].state == FINISHED)
            done++;
        if (done == j->nproc)
          j->state = FINISHED;
      } else if (WIFSIGNALED(status)) {
        // safe_printf("[%d] killed '%s' by signal %d\n", i, j->command,
        //             WTERMSIG(status));
        j->proc[p].state = FINISHED;
        j->proc[p].exitcode = status;
        int done = 0;
        for (int k = 0; k < j->nproc; k++)
          if (j->proc[k].state == FINISHED)
            done++;
        if (done == j->nproc)
          j->state = FINISHED;
      }
    }
  }
  (void)status;
  (void)pid;
#endif /* !STUDENT */
  errno = old_errno;
}

/* When pipeline is done, its exitcode is fetched from the last process. */
static int exitcode(job_t *job) {
  return job->proc[job->nproc - 1].exitcode;
}

static int allocjob(void) {
  /* Find empty slot for background job. */
  for (int j = BG; j < njobmax; j++)
    if (jobs[j].pgid == 0)
      return j;

  /* If none found, allocate new one. */
  jobs = realloc(jobs, sizeof(job_t) * (njobmax + 1));
  memset(&jobs[njobmax], 0, sizeof(job_t));
  return njobmax++;
}

static int allocproc(int j) {
  job_t *job = &jobs[j];
  job->proc = realloc(job->proc, sizeof(proc_t) * (job->nproc + 1));
  return job->nproc++;
}

int addjob(pid_t pgid, int bg) {
  int j = bg ? allocjob() : FG;
  job_t *job = &jobs[j];
  /* Initial state of a job. */
  job->pgid = pgid;
  job->state = RUNNING;
  job->command = NULL;
  job->proc = NULL;
  job->nproc = 0;
  job->tmodes = shell_tmodes;
  return j;
}

static void deljob(job_t *job) {
  assert(job->state == FINISHED);
  free(job->command);
  free(job->proc);
  job->pgid = 0;
  job->command = NULL;
  job->proc = NULL;
  job->nproc = 0;
}

static void movejob(int from, int to) {
  assert(jobs[to].pgid == 0);
  memcpy(&jobs[to], &jobs[from], sizeof(job_t));
  memset(&jobs[from], 0, sizeof(job_t));
}

static void mkcommand(char **cmdp, char **argv) {
  if (*cmdp)
    strapp(cmdp, " | ");

  for (strapp(cmdp, *argv++); *argv; argv++) {
    strapp(cmdp, " ");
    strapp(cmdp, *argv);
  }
}

void addproc(int j, pid_t pid, char **argv) {
  assert(j < njobmax);
  job_t *job = &jobs[j];

  int p = allocproc(j);
  proc_t *proc = &job->proc[p];
  /* Initial state of a process. */
  proc->pid = pid;
  proc->state = RUNNING;
  proc->exitcode = -1;
  mkcommand(&job->command, argv);
}

/* Returns job's state.
 * If it's finished, delete it and return exitcode through statusp. */
static int jobstate(int j, int *statusp) {
  assert(j < njobmax);
  job_t *job = &jobs[j];
  int state = job->state;

  /* TODO: Handle case where job has finished. */
#ifdef STUDENT
  if (state == FINISHED) {
    *statusp = exitcode(job);
    deljob(job);
  }
  (void)exitcode;
#endif /* !STUDENT */

  return state;
}

char *jobcmd(int j) {
  assert(j < njobmax);
  job_t *job = &jobs[j];
  return job->command;
}

/* Continues a job that has been stopped. If move to foreground was requested,
 * then move the job to foreground and start monitoring it. */
bool resumejob(int j, int bg, sigset_t *mask) {
  if (j < 0) {
    for (j = njobmax - 1; j > 0 && jobs[j].state == FINISHED; j--)
      continue;
  }

  if (j >= njobmax || jobs[j].state == FINISHED)
    return false;

    /* TODO: Continue stopped job. Possibly move job to foreground slot. */
#ifdef STUDENT

  safe_printf("[%d] continue '%s'\n", j, jobs[j].command);
  if (bg) {
    jobs[j].state = RUNNING;
    for (int i = 0; i < jobs[j].nproc; i++)
      jobs[j].proc[i].state = RUNNING;
    Kill(-jobs[j].pgid, SIGCONT);
    // safe_printf("[%d] resumed (bg)\n", jobs[j].pgid);
  } else {
    if (jobs[FG].pgid != 0) {
      Tcgetattr(tty_fd, &jobs[FG].tmodes);
      int nj = addjob(0, true);
      movejob(0, nj);
      Kill(-jobs[nj].pgid, SIGSTOP);
      //   safe_printf("fg job stopped\n");
    }
    movejob(j, FG);
    deljob(&jobs[j]);
    jobs[FG].state = RUNNING;
    for (int i = 0; i < jobs[j].nproc; i++)
      jobs[FG].proc[i].state = RUNNING;
    Tcsetpgrp(tty_fd, jobs[FG].pgid);
    Tcsetattr(tty_fd, TCSADRAIN, &jobs[FG].tmodes);
    Kill(-jobs[FG].pgid, SIGCONT);
    // safe_printf("[%d] resumed (fg)\n", jobs[0].pgid);
    monitorjob(mask);
  }
  (void)movejob;
#endif /* !STUDENT */

  return true;
}

/* Kill the job by sending it a SIGTERM. */
bool killjob(int j) {
  if (j >= njobmax || jobs[j].state == FINISHED)
    return false;
  debug("[%d] killing '%s'\n", j, jobs[j].command);

  /* TODO: I love the smell of napalm in the morning. */
#ifdef STUDENT
  Kill(-jobs[j].pgid, SIGTERM);
  Kill(-jobs[j].pgid, SIGCONT);
#endif /* !STUDENT */

  return true;
}

/* Report state of requested background jobs. Clean up finished jobs. */
void watchjobs(int which) {
  for (int j = BG; j < njobmax; j++) {
    if (jobs[j].pgid == 0)
      continue;

      /* TODO: Report job number, state, command and exit code or signal. */
#ifdef STUDENT
    int s = jobs[j].state;
    int wstatus = jobs[j].proc[jobs[j].nproc - 1].exitcode;
    if (which == ALL || which == s) {
      if (s == RUNNING)
        safe_printf("[%d] running '%s'\n", j, jobs[j].command);
      else if (s == STOPPED)
        safe_printf("[%d] suspended '%s'\n", j, jobs[j].command);
      else if (s == FINISHED) {
        if (WIFEXITED(wstatus))
          safe_printf("[%d] exited '%s', status=%d\n", j, jobs[j].command,
                      WEXITSTATUS(wstatus));
        if (WIFSIGNALED(wstatus))
          safe_printf("[%d] killed '%s' by signal %d\n", j, jobs[j].command,
                      WTERMSIG(wstatus));
        deljob(&jobs[j]);
      }
    }
    (void)deljob;
#endif /* !STUDENT */
  }
}

/* Monitor job execution. If it gets stopped move it to background.
 * When a job has finished or has been stopped move shell to foreground. */
int monitorjob(sigset_t *mask) {
  int exitcode = 0, state;

  /* TODO: Following code requires use of Tcsetpgrp of tty_fd. */
#ifdef STUDENT
  Tcsetpgrp(tty_fd, jobs[0].pgid);
  do {
    // safe_printf("xd\n");
    Sigsuspend(mask);
    state = jobstate(0, &exitcode);
    if (jobs[0].state == STOPPED) {
      //   safe_printf("fg stopped");
      Tcgetattr(tty_fd, &jobs[0].tmodes);
      int j = addjob(0, true);
      movejob(0, j);
      state = jobstate(j, &exitcode);
    }
  } while (jobs[0].state == RUNNING);

  Tcsetpgrp(tty_fd, getpid());
  Tcsetattr(tty_fd, TCSADRAIN, &shell_tmodes);
  Sigprocmask(SIG_SETMASK, mask, NULL);

  (void)jobstate;
  (void)exitcode;
  (void)state;
#endif /* !STUDENT */

  return exitcode;
}

/* Called just at the beginning of shell's life. */
void initjobs(void) {
  struct sigaction act = {
    .sa_flags = SA_RESTART,
    .sa_handler = sigchld_handler,
  };

  /* Block SIGINT for the duration of `sigchld_handler`
   * in case `sigint_handler` does something crazy like `longjmp`. */
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGINT);
  Sigaction(SIGCHLD, &act, NULL);

  jobs = calloc(sizeof(job_t), 1);

  /* Assume we're running in interactive mode, so move us to foreground.
   * Duplicate terminal fd, but do not leak it to subprocesses that execve. */
  assert(isatty(STDIN_FILENO));
  tty_fd = Dup(STDIN_FILENO);
  fcntl(tty_fd, F_SETFD, FD_CLOEXEC);

  /* Take control of the terminal. */
  Tcsetpgrp(tty_fd, getpgrp());

  /* Save default terminal attributes for the shell. */
  Tcgetattr(tty_fd, &shell_tmodes);
}

/* Called just before the shell finishes. */
void shutdownjobs(void) {
  sigset_t mask;
  Sigprocmask(SIG_BLOCK, &sigchld_mask, &mask);

  /* TODO: Kill remaining jobs and wait for them to finish. */
#ifdef STUDENT
  for (int i = 0; i < njobmax; i++) {
    if (jobs[i].pgid > 0 && jobs[i].state != FINISHED) {
      //   safe_printf("trying to kill %d\n", jobs[i].pgid);
      killjob(i);
      Sigsuspend(&mask);
      //   safe_printf("killed %d\n", jobs[i].pgid);
      //   deljob(&jobs[i]);
    }
  }
#endif /* !STUDENT */

  watchjobs(FINISHED);

  Sigprocmask(SIG_SETMASK, &mask, NULL);

  Close(tty_fd);
}

/* Sets foreground process group to `pgid`. */
void setfgpgrp(pid_t pgid) {
  Tcsetpgrp(tty_fd, pgid);
}
