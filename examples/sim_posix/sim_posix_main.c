/****************************************************************************
 * apps/examples/sim_posix/sim_posix_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SIM_POSIX_HOSTFS_MOUNTPT  "/sim_posix_hostfs"
#define SIM_POSIX_TMP_MOUNTPT     "/tmp"
#define SIM_POSIX_MIN_ROOT_LEN    260
#define SIM_POSIX_PAYLOAD         "sim-posix-hostfs\n"
#define SIM_POSIX_PING            "ping"
#define SIM_POSIX_PONG            "pong"
#define SIM_POSIX_POLL_TIMEOUT_MS 5000
#define SIM_POSIX_LONG_SEGMENT    "xxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define SIM_POSIX_DEFAULT_ROOT    "/tmp/simposix" \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT \
                                  "/" SIM_POSIX_LONG_SEGMENT

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct local_server_s
{
  sem_t ready;
  const char *path;
  int ready_result;
  int result;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int fail_errno(const char *what)
{
  printf("sim_posix: %s failed errno=%d\n", what, errno);
  return -1;
}

static int mount_hostfs(const char *mountpt, const char *hostroot)
{
  char options[PATH_MAX + 4];

  if (snprintf(options, sizeof(options), "fs=%s", hostroot) >=
      (int)sizeof(options))
    {
      printf("sim_posix: hostfs option too long\n");
      return -1;
    }

  if (mkdir(mountpt, 0777) < 0 && errno != EEXIST)
    {
      return fail_errno("mkdir");
    }

  umount(mountpt);

  if (mount(NULL, mountpt, "hostfs", 0, options) < 0)
    {
      printf("sim_posix: mount %s %s failed errno=%d\n",
             mountpt, options, errno);
      return -1;
    }

  return 0;
}

static int write_full(int fd, const void *buf, size_t len)
{
  const char *pos = buf;
  ssize_t nwrite;

  while (len > 0)
    {
      nwrite = write(fd, pos, len);
      if (nwrite < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -1;
        }

      if (nwrite == 0)
        {
          errno = EIO;
          return -1;
        }

      pos += nwrite;
      len -= nwrite;
    }

  return 0;
}

static int read_exact(int fd, void *buf, size_t len)
{
  char *pos = buf;
  ssize_t nread;

  while (len > 0)
    {
      nread = read(fd, pos, len);
      if (nread < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          return -1;
        }

      if (nread == 0)
        {
          errno = EIO;
          return -1;
        }

      pos += nread;
      len -= nread;
    }

  return 0;
}

static int test_netdb_service(const char *name, const char *proto, int port)
{
  struct addrinfo hints;
  struct addrinfo *res;
  struct servent *serv;
  int ret;

  serv = getservbyname(name, proto);
  if (serv == NULL)
    {
      printf("sim_posix: getservbyname(%s, %s) failed errno=%d\n",
             name, proto, errno);
      return -1;
    }

  if (ntohs(serv->s_port) != port || strcmp(serv->s_proto, proto) != 0)
    {
      printf("sim_posix: service mismatch %s/%s port=%d proto=%s\n",
             name, proto, ntohs(serv->s_port), serv->s_proto);
      return -1;
    }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = strcmp(proto, "tcp") == 0 ? SOCK_STREAM : SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;

  ret = getaddrinfo(NULL, name, &hints, &res);
  if (ret != 0)
    {
      printf("sim_posix: getaddrinfo(NULL, %s) failed ret=%d\n",
             name, ret);
      return -1;
    }

  if (res == NULL || res->ai_addr == NULL ||
      ((struct sockaddr_in *)res->ai_addr)->sin_port != htons(port))
    {
      printf("sim_posix: getaddrinfo port mismatch for %s\n", name);
      freeaddrinfo(res);
      return -1;
    }

  freeaddrinfo(res);
  printf("sim_posix: netdb %s/%s ok\n", name, proto);
  return 0;
}

static int test_netdb(void)
{
  if (test_netdb_service("http", "tcp", 80) < 0)
    {
      return -1;
    }

  if (test_netdb_service("https", "tcp", 443) < 0)
    {
      return -1;
    }

  return 0;
}

static int test_hostfs_locks(const char *path)
{
  struct flock lock;
  int fd;

  fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd < 0)
    {
      return fail_errno("open lock file");
    }

  memset(&lock, 0, sizeof(lock));
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 1;

  if (fcntl(fd, F_GETLK, &lock) < 0)
    {
      close(fd);
      return fail_errno("fcntl F_GETLK");
    }

  if (lock.l_type != F_UNLCK)
    {
      printf("sim_posix: unexpected initial lock type=%d\n", lock.l_type);
      close(fd);
      return -1;
    }

  memset(&lock, 0, sizeof(lock));
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  lock.l_start = 0;
  lock.l_len = 1;

  if (fcntl(fd, F_SETLK, &lock) < 0)
    {
      close(fd);
      return fail_errno("fcntl F_SETLK");
    }

  lock.l_start = 1;
  if (fcntl(fd, F_SETLKW, &lock) < 0)
    {
      close(fd);
      return fail_errno("fcntl F_SETLKW");
    }

  lock.l_type = F_UNLCK;
  lock.l_start = 0;
  lock.l_len = 2;

  if (fcntl(fd, F_SETLK, &lock) < 0)
    {
      close(fd);
      return fail_errno("fcntl unlock");
    }

  close(fd);
  printf("sim_posix: hostfs locks ok\n");
  return 0;
}

static int test_hostfs(const char *hostroot)
{
  char path[PATH_MAX];
  char buffer[sizeof(SIM_POSIX_PAYLOAD)];
  int fd;

  if (strlen(hostroot) < SIM_POSIX_MIN_ROOT_LEN)
    {
      printf("sim_posix: host root too short, need >= %d bytes\n",
             SIM_POSIX_MIN_ROOT_LEN);
      return -1;
    }

  if (mount_hostfs(SIM_POSIX_HOSTFS_MOUNTPT, hostroot) < 0)
    {
      return -1;
    }

  if (snprintf(path, sizeof(path), "%s/probe.txt",
               SIM_POSIX_HOSTFS_MOUNTPT) >= (int)sizeof(path))
    {
      printf("sim_posix: hostfs probe path too long\n");
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return -1;
    }

  fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (fd < 0)
    {
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return fail_errno("open hostfs probe");
    }

  if (write_full(fd, SIM_POSIX_PAYLOAD, strlen(SIM_POSIX_PAYLOAD)) < 0)
    {
      close(fd);
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return fail_errno("write hostfs probe");
    }

  if (lseek(fd, 0, SEEK_SET) < 0)
    {
      close(fd);
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return fail_errno("lseek hostfs probe");
    }

  memset(buffer, 0, sizeof(buffer));
  if (read_exact(fd, buffer, strlen(SIM_POSIX_PAYLOAD)) < 0)
    {
      close(fd);
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return fail_errno("read hostfs probe");
    }

  close(fd);

  if (strcmp(buffer, SIM_POSIX_PAYLOAD) != 0)
    {
      printf("sim_posix: hostfs payload mismatch: %s\n", buffer);
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return -1;
    }

  if (snprintf(path, sizeof(path), "%s/lock.txt",
               SIM_POSIX_HOSTFS_MOUNTPT) >= (int)sizeof(path))
    {
      printf("sim_posix: hostfs lock path too long\n");
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return -1;
    }

  if (test_hostfs_locks(path) < 0)
    {
      umount(SIM_POSIX_HOSTFS_MOUNTPT);
      return -1;
    }

  umount(SIM_POSIX_HOSTFS_MOUNTPT);
  printf("sim_posix: hostfs long root ok\n");
  return 0;
}

static int poll_fd(int fd, int events)
{
  struct pollfd pfd;
  int ret;

  memset(&pfd, 0, sizeof(pfd));
  pfd.fd = fd;
  pfd.events = events;

  ret = poll(&pfd, 1, SIM_POSIX_POLL_TIMEOUT_MS);
  if (ret < 0)
    {
      return fail_errno("poll");
    }

  if (ret == 0)
    {
      printf("sim_posix: poll timeout fd=%d events=%x\n", fd, events);
      return -1;
    }

  if ((pfd.revents & events) == 0)
    {
      printf("sim_posix: poll unexpected revents=%x expected=%x\n",
             pfd.revents, events);
      return -1;
    }

  return 0;
}

static int fill_local_addr(struct sockaddr_un *addr, const char *path,
                           socklen_t *addrlen)
{
  size_t pathlen;

  pathlen = strlen(path);
  if (pathlen >= sizeof(addr->sun_path))
    {
      printf("sim_posix: AF_LOCAL path too long: %s\n", path);
      return -1;
    }

  memset(addr, 0, sizeof(*addr));
  addr->sun_family = AF_LOCAL;
  memcpy(addr->sun_path, path, pathlen + 1);
  *addrlen = offsetof(struct sockaddr_un, sun_path) + pathlen + 1;

  return 0;
}

static void *local_server_thread(void *arg)
{
  struct local_server_s *ctx = arg;
  struct sockaddr_un addr;
  socklen_t addrlen;
  char buffer[sizeof(SIM_POSIX_PING)];
  bool posted = false;
  int listenfd = -1;
  int connfd = -1;
  int ret = -1;

  listenfd = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (listenfd < 0)
    {
      fail_errno("server socket");
      goto out;
    }

  if (fill_local_addr(&addr, ctx->path, &addrlen) < 0)
    {
      goto out;
    }

  if (bind(listenfd, (struct sockaddr *)&addr, addrlen) < 0)
    {
      fail_errno("server bind");
      goto out;
    }

  if (listen(listenfd, 1) < 0)
    {
      fail_errno("server listen");
      goto out;
    }

  ctx->ready_result = 0;
  sem_post(&ctx->ready);
  posted = true;

  if (poll_fd(listenfd, POLLIN) < 0)
    {
      goto out;
    }

  connfd = accept(listenfd, NULL, NULL);
  if (connfd < 0)
    {
      fail_errno("server accept");
      goto out;
    }

  if (poll_fd(connfd, POLLIN) < 0)
    {
      goto out;
    }

  memset(buffer, 0, sizeof(buffer));
  if (recv(connfd, buffer, strlen(SIM_POSIX_PING), 0) !=
      (ssize_t)strlen(SIM_POSIX_PING))
    {
      fail_errno("server recv");
      goto out;
    }

  if (strcmp(buffer, SIM_POSIX_PING) != 0)
    {
      printf("sim_posix: server received unexpected payload: %s\n",
             buffer);
      goto out;
    }

  if (send(connfd, SIM_POSIX_PONG, strlen(SIM_POSIX_PONG), 0) !=
      (ssize_t)strlen(SIM_POSIX_PONG))
    {
      fail_errno("server send");
      goto out;
    }

  ret = 0;

out:
  if (!posted)
    {
      ctx->ready_result = ret;
      sem_post(&ctx->ready);
    }

  if (connfd >= 0)
    {
      close(connfd);
    }

  if (listenfd >= 0)
    {
      close(listenfd);
    }

  ctx->result = ret;
  return NULL;
}

static int test_local_socket_client(const char *path)
{
  struct sockaddr_un addr;
  struct sockaddr_un peer;
  socklen_t addrlen;
  socklen_t peerlen;
  char buffer[sizeof(SIM_POSIX_PONG)];
  int fd;
  int ret = -1;

  fd = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (fd < 0)
    {
      return fail_errno("client socket");
    }

  if (fill_local_addr(&addr, path, &addrlen) < 0)
    {
      goto out;
    }

  if (connect(fd, (struct sockaddr *)&addr, addrlen) < 0)
    {
      fail_errno("client connect");
      goto out;
    }

  peerlen = sizeof(peer);
  if (getpeername(fd, (struct sockaddr *)&peer, &peerlen) < 0)
    {
      fail_errno("client getpeername");
      goto out;
    }

  if (peer.sun_family != AF_LOCAL || strcmp(peer.sun_path, path) != 0)
    {
      printf("sim_posix: peer mismatch family=%d path=%s\n",
             peer.sun_family, peer.sun_path);
      goto out;
    }

  if (poll_fd(fd, POLLOUT) < 0)
    {
      goto out;
    }

  if (send(fd, SIM_POSIX_PING, strlen(SIM_POSIX_PING), 0) !=
      (ssize_t)strlen(SIM_POSIX_PING))
    {
      fail_errno("client send");
      goto out;
    }

  if (poll_fd(fd, POLLIN) < 0)
    {
      goto out;
    }

  memset(buffer, 0, sizeof(buffer));
  if (recv(fd, buffer, strlen(SIM_POSIX_PONG), 0) !=
      (ssize_t)strlen(SIM_POSIX_PONG))
    {
      fail_errno("client recv");
      goto out;
    }

  if (strcmp(buffer, SIM_POSIX_PONG) != 0)
    {
      printf("sim_posix: client received unexpected payload: %s\n",
             buffer);
      goto out;
    }

  ret = 0;

out:
  close(fd);
  return ret;
}

static int test_local_socket(void)
{
  struct local_server_s ctx;
  pthread_t thread;
  char sockpath[108];
  int ret = -1;

  if (mount_hostfs(SIM_POSIX_TMP_MOUNTPT, "/tmp") < 0)
    {
      return -1;
    }

  snprintf(sockpath, sizeof(sockpath), "/tmp/nuttx_sim_posix_%ld.sock",
           (long)getpid());
  unlink(sockpath);

  memset(&ctx, 0, sizeof(ctx));
  ctx.path = sockpath;
  ctx.ready_result = -1;
  ctx.result = -1;

  if (sem_init(&ctx.ready, 0, 0) < 0)
    {
      umount(SIM_POSIX_TMP_MOUNTPT);
      return fail_errno("sem_init");
    }

  if (pthread_create(&thread, NULL, local_server_thread, &ctx) != 0)
    {
      sem_destroy(&ctx.ready);
      umount(SIM_POSIX_TMP_MOUNTPT);
      return fail_errno("pthread_create");
    }

  sem_wait(&ctx.ready);
  if (ctx.ready_result == 0)
    {
      ret = test_local_socket_client(sockpath);
    }

  pthread_join(thread, NULL);

  if (ret == 0 && ctx.result < 0)
    {
      ret = -1;
    }

  unlink(sockpath);
  sem_destroy(&ctx.ready);
  umount(SIM_POSIX_TMP_MOUNTPT);

  if (ret < 0)
    {
      return -1;
    }

  printf("sim_posix: AF_LOCAL usrsock ok\n");
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR const char *hostroot = SIM_POSIX_DEFAULT_ROOT;

  if (argc > 2)
    {
      printf("usage: %s [long-host-root]\n", argv[0]);
      return 2;
    }

  if (argc == 2)
    {
      hostroot = argv[1];
    }

  if (test_netdb() < 0)
    {
      return 1;
    }

  if (test_hostfs(hostroot) < 0)
    {
      return 1;
    }

  if (test_local_socket() < 0)
    {
      return 1;
    }

  printf("sim_posix: PASS\n");
  return 0;
}
