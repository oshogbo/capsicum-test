/*-
 * Copyright (c) 2008-2009 Robert N. M. Watson
 * Copyright (c) 2011 Jonathan Anderson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Test routines to make sure a variety of system calls are or are not
 * available in capability mode.  The goal is not to see if they work, just
 * whether or not they return the expected ECAPMODE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include "capsicum.h"
#include "syscalls.h"
#include "capsicum-test.h"

FORK_TEST_ON(Capmode, Syscalls, "/tmp/cap_capmode") {
  // Open some files to play with.
  int fd_file = open("/tmp/cap_capmode", O_RDWR|O_CREAT, 0644);
  EXPECT_OK(fd_file);
  int fd_close = open("/dev/null", O_RDWR);
  EXPECT_OK(fd_close);
  int fd_dir = open("/tmp", O_RDONLY);
  EXPECT_OK(fd_dir);
  int fd_socket = socket(PF_INET, SOCK_DGRAM, 0);
  EXPECT_OK(fd_socket);
  int fd_tcp_socket = socket(PF_INET, SOCK_STREAM, 0);
  EXPECT_OK(fd_socket);

  // mmap() some memory.
  size_t mem_size = getpagesize();
  void *mem = mmap(NULL, mem_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  EXPECT_TRUE(mem != NULL);

  // Record some identifiers
  gid_t my_gid = getgid();
  pid_t my_pid = getpid();
  pid_t my_ppid = getppid();
  uid_t my_uid = getuid();
  pid_t my_sid = getsid(my_pid);

  // Enter capability mode.
  unsigned int mode = -1;
  EXPECT_OK(cap_getmode(&mode));
  EXPECT_EQ(0, mode);
  EXPECT_OK(cap_enter());
  EXPECT_OK(cap_getmode(&mode));
  EXPECT_EQ(1, mode);

  // System calls that are not permitted in capability mode.
  EXPECT_CAPMODE(access("/tmp/cap_capmode_access", F_OK));
  EXPECT_CAPMODE(acct("/tmp/cap_capmode_acct"));
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = 0;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  EXPECT_CAPMODE(bind(fd_socket, (sockaddr*)&addr, sizeof(addr)));
  EXPECT_CAPMODE(chdir("/tmp/cap_capmode_chdir"));
#ifdef HAVE_CHFLAGS
  EXPECT_CAPMODE(chflags("/tmp/cap_capmode_chflags", UF_NODUMP));
#endif
  EXPECT_CAPMODE(chmod("/tmp/cap_capmode_chmod", 0644));
  EXPECT_CAPMODE(chown("/tmp/cap_capmode_chown", -1, -1));
  EXPECT_CAPMODE(chroot("/tmp/cap_capmode_chroot"));
  addr.sin_family = AF_INET;
  addr.sin_port = 53;
  addr.sin_addr.s_addr = htonl(0x08080808);
  EXPECT_CAPMODE(connect(fd_tcp_socket, (sockaddr*)&addr, sizeof(addr)));
  EXPECT_CAPMODE(creat("/tmp/cap_capmode_creat", 0644));
  EXPECT_CAPMODE(fchdir(fd_dir));
#ifdef HAVE_GETFSSTAT
  struct statfs statfs;
  EXPECT_CAPMODE(getfsstat(&statfs, sizeof(statfs), MNT_NOWAIT));
#endif
  EXPECT_CAPMODE(link("/tmp/foo", "/tmp/bar"));
  struct stat sb;
  EXPECT_CAPMODE(lstat("/tmp/cap_capmode_lstat", &sb));
  EXPECT_CAPMODE(mknod("/tmp/capmode_mknod", 06440, 0));
#ifdef OMIT
  // TODO(drysdale): autoconf away the difference between Linux & FreeBSD mount syscalls
  EXPECT_CAPMODE(mount("procfs", "/not_mounted", 0, NULL));
#endif
  EXPECT_CAPMODE(open("/dev/null", O_RDWR));
  EXPECT_CAPMODE(readlink("/tmp/cap_capmode_readlink", NULL, 0));
#ifdef HAVE_REVOKE
  EXPECT_CAPMODE(revoke("/tmp/cap_capmode_revoke"));
#endif
  EXPECT_CAPMODE(stat("/tmp/cap_capmode_stat", &sb));
  EXPECT_CAPMODE(symlink("/tmp/cap_capmode_symlink_from", "/tmp/cap_capmode_symlink_to"));
  EXPECT_CAPMODE(unlink("/tmp/cap_capmode_unlink"));
  EXPECT_CAPMODE(umount2("/not_mounted", 0));

  // System calls that are permitted in capability mode.
  EXPECT_OK(close(fd_close));
  int fd_dup = dup(fd_file);
  EXPECT_OK(fd_dup);
  EXPECT_OK(dup2(fd_file, fd_dup));
#ifdef HAVE_DUP3
  EXPECT_OK(dup3(fd_file, fd_dup, 0));
#endif
  if (fd_dup >= 0) close(fd_dup);

  EXPECT_OK(fstat(fd_file, &sb));
  EXPECT_OK(lseek(fd_file, 0, SEEK_SET));
  EXPECT_OK(msync(mem, mem_size, MS_ASYNC));
  EXPECT_OK(profil(NULL, 0, 0, 0));
  char ch;
  EXPECT_OK(read(fd_file, &ch, sizeof(ch)));
  // recvfrom() either returns -1 with EAGAIN, or 0.
  int rc = recvfrom(fd_socket, NULL, 0, MSG_DONTWAIT, NULL, NULL);
  if (rc < 0) EXPECT_EQ(EAGAIN, errno);
  EXPECT_OK(setuid(getuid()));
  EXPECT_OK(write(fd_file, &ch, sizeof(ch)));

  // These calls will fail for lack of e.g. a proper name to send to,
  // but they are allowed in capability mode, so errno != ECAPMODE.
  EXPECT_FAIL_NOT_CAPMODE(accept(fd_socket, NULL, NULL));
  EXPECT_FAIL_NOT_CAPMODE(getpeername(fd_socket, NULL, NULL));
  EXPECT_FAIL_NOT_CAPMODE(getsockname(fd_socket, NULL, NULL));
#ifdef HAVE_CHFLAGS
  rc = fchflags(fd_file, UF_NODUMP);
  if (rc < 0)  EXPECT_NE(ECAPMODE, errno);
#endif
  EXPECT_FAIL_NOT_CAPMODE(recvmsg(fd_socket, NULL, 0));
  EXPECT_FAIL_NOT_CAPMODE(sendmsg(fd_socket, NULL, 0));
  EXPECT_FAIL_NOT_CAPMODE(sendto(fd_socket, NULL, 0, 0, NULL, 0));
  off_t offset = 0;
  EXPECT_FAIL_NOT_CAPMODE(sendfile_(fd_socket, fd_file, &offset, 1));

  // System calls which should be allowed in capability mode, but which
  // don't return errors.
  EXPECT_EQ(my_gid, getegid());
  EXPECT_EQ(my_uid, geteuid());
  EXPECT_EQ(my_gid, getgid());
  EXPECT_EQ(my_pid, getpid());
  EXPECT_EQ(my_ppid, getppid());
  EXPECT_EQ(my_uid, getuid());
  EXPECT_EQ(my_sid, getsid(my_pid));
  gid_t grps[128];
  EXPECT_OK(getgroups(128, grps));
  uid_t ruid;
  uid_t euid;
  uid_t suid;
  EXPECT_OK(getresuid(&ruid, &euid, &suid));
  gid_t rgid;
  gid_t egid;
  gid_t sgid;
  EXPECT_OK(getresgid(&rgid, &egid, &sgid));

  EXPECT_OK(setgid(my_gid));
#ifdef HAVE_SETFSGID
  EXPECT_OK(setfsgid(my_gid));
#endif
  EXPECT_OK(setuid(my_uid));
#ifdef HAVE_SETFSUID
  EXPECT_OK(setfsuid(my_uid));
#endif
  EXPECT_OK(setregid(my_gid, my_gid));
  EXPECT_OK(setresgid(my_gid, my_gid, my_gid));
  EXPECT_OK(setreuid(my_uid, my_uid));
  EXPECT_OK(setresuid(my_uid, my_uid, my_uid));
  EXPECT_OK(setsid());

  struct timespec ts;
  EXPECT_OK(clock_getres(CLOCK_REALTIME, &ts));
  EXPECT_OK(clock_gettime(CLOCK_REALTIME, &ts));
  struct itimerval itv;
  EXPECT_OK(getitimer(ITIMER_REAL, &itv));
  EXPECT_OK(setitimer(ITIMER_REAL, &itv, NULL));
  errno = 0;
  rc = getpriority(PRIO_PROCESS, 0);
  EXPECT_EQ(0, errno);
  EXPECT_OK(setpriority(PRIO_PROCESS, 0, rc));
  struct rlimit rlim;
  EXPECT_OK(getrlimit(RLIMIT_CORE, &rlim));
  EXPECT_OK(setrlimit(RLIMIT_CORE, &rlim));
  struct rusage ruse;
  EXPECT_OK(getrusage(RUSAGE_SELF, &ruse));
  struct timeval tv;
  struct timezone tz;
  EXPECT_OK(gettimeofday(&tv, &tz));
  char buf[1024];
  rc = getdents_(fd_dir, (void*)buf, sizeof(buf));
  EXPECT_OK(rc);
  EXPECT_OK(madvise(mem, mem_size, MADV_NORMAL));
  unsigned char vec[2];
  EXPECT_OK(mincore_(mem, mem_size, vec));
  EXPECT_OK(mprotect(mem, mem_size, PROT_READ|PROT_WRITE));
  if (!MLOCK_REQUIRES_ROOT || my_uid == 0) {
    EXPECT_OK(mlock(mem, mem_size));
    EXPECT_OK(munlock(mem, mem_size));
    EXPECT_OK(mlockall(MCL_CURRENT));
    EXPECT_OK(munlockall());
  }

  ts.tv_sec = 0;
  ts.tv_nsec = 1;
  EXPECT_OK(nanosleep(&ts, NULL));

  char data[] = "123";
  EXPECT_OK(pwrite(fd_file, data, 1, 0));
  EXPECT_OK(pread(fd_file, data, 1, 0));

  struct iovec io;
  io.iov_base = data;
  io.iov_len = 2;
  EXPECT_OK(pwritev(fd_file, &io, 1, 0));
  EXPECT_OK(preadv(fd_file, &io, 1, 0));
  EXPECT_OK(writev(fd_file, &io, 1));
  EXPECT_OK(readv(fd_file, &io, 1));

  int policy = sched_getscheduler(0);
  EXPECT_OK(policy);
  struct sched_param sp;
  EXPECT_OK(sched_getparam(0, &sp));
  if (policy >= 0 && (!SCHED_SETSCHEDULER_REQUIRES_ROOT || my_uid == 0)) {
    EXPECT_OK(sched_setscheduler(0, policy, &sp));
  }
  EXPECT_OK(sched_setparam(0, &sp));
  EXPECT_OK(sched_get_priority_max(policy));
  EXPECT_OK(sched_get_priority_min(policy));
  EXPECT_OK(sched_rr_get_interval(0, &ts));
  EXPECT_OK(sched_yield());

  EXPECT_OK(umask(022)); // TODO(drysdale): why does this work on Linux?
  stack_t ss;
  EXPECT_OK(sigaltstack(NULL, &ss));

  // Finally, tests for system calls that don't fit the pattern very well.
  pid_t pid = fork();
  EXPECT_OK(pid);
  if (pid == 0) {
    // Child: immediately exit.
    exit(0);
  } else if (pid > 0) {
    EXPECT_CAPMODE(waitpid(pid, NULL, 0));
  }

#ifdef HAVE_GETLOGIN
  EXPECT_TRUE(getlogin() != NULL);
#endif

  // TODO(rnmw): ktrace

#ifndef __linux__
  // TODO(drysdale): reinstate when pipe works in capsicum-linux capability mode.
  int fd2[2];
  rc = pipe(fd2);
  EXPECT_EQ(0, rc);
  if (rc == 0) {
    close(fd2[0]);
    close(fd2[1]);
  };
#ifdef HAVE_PIPE2
  rc = pipe2(fd2, 0);
  EXPECT_EQ(0, rc);
  if (rc == 0) {
    close(fd2[0]);
    close(fd2[1]);
  };
#endif
#endif

  // TODO(rnmw): ptrace

#ifdef HAVE_SYSARCH
  // sysarch() is, by definition, architecture-dependent
#if defined (__amd64__) || defined (__i386__)
  long sysarch_arg = 0;
  EXPECT_CAPMODE(sysarch(I386_SET_IOPERM, &sysarch_arg));
#else
  // TOOD(jra): write a test for arm
  FAIL("capmode:no sysarch() test for current architecture");
#endif
#endif

  // No error return from sync(2) to test, but check errno remains unset.
  errno = 0;
  sync();
  EXPECT_EQ(0, errno);

  // Close files and unmap memory.
  munmap(mem, mem_size);
  if (fd_file >= 0) close(fd_file);
  if (fd_close >= 0) close(fd_close);
  if (fd_dir >= 0) close(fd_dir);
  if (fd_socket >= 0) close(fd_socket);
  if (fd_tcp_socket >= 0) close(fd_tcp_socket);
}
