/****************************************************************************
 * apps/examples/hostfs_longpath/hostfs_longpath_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HOSTFS_LONGPATH_MIN_ROOT 128
#define HOSTFS_LONGPATH_MOUNTPT  "/hostfs_longpath"
#define HOSTFS_LONGPATH_FILE     "probe.txt"
#define HOSTFS_LONGPATH_PAYLOAD  "hostfs-longpath-ok\n"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int write_payload(FAR const char *path)
{
  int fd;
  ssize_t nwrite;

  fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (fd < 0)
    {
      printf("open write failed: %s errno=%d\n", path, errno);
      return -1;
    }

  nwrite = write(fd, HOSTFS_LONGPATH_PAYLOAD,
                 strlen(HOSTFS_LONGPATH_PAYLOAD));
  close(fd);

  if (nwrite != (ssize_t)strlen(HOSTFS_LONGPATH_PAYLOAD))
    {
      printf("write failed: %s nwrite=%ld errno=%d\n",
             path, (long)nwrite, errno);
      return -1;
    }

  return 0;
}

static int verify_payload(FAR const char *path)
{
  char buffer[sizeof(HOSTFS_LONGPATH_PAYLOAD)];
  int fd;
  ssize_t nread;

  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
      printf("open read failed: %s errno=%d\n", path, errno);
      return -1;
    }

  memset(buffer, 0, sizeof(buffer));
  nread = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);

  if (nread != (ssize_t)strlen(HOSTFS_LONGPATH_PAYLOAD) ||
      strcmp(buffer, HOSTFS_LONGPATH_PAYLOAD) != 0)
    {
      printf("readback mismatch: %s nread=%ld data=%s\n",
             path, (long)nread, buffer);
      return -1;
    }

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  char options[256];
  char filepath[256];
  FAR const char *host_root;
  FAR const char *mountpt = HOSTFS_LONGPATH_MOUNTPT;
  size_t host_root_len;
  int ret;

  if (argc < 2 || argc > 3)
    {
      printf("usage: %s <long-host-root> [mountpoint]\n", argv[0]);
      return 2;
    }

  host_root = argv[1];
  if (argc == 3)
    {
      mountpt = argv[2];
    }

  host_root_len = strlen(host_root);
  if (host_root_len < HOSTFS_LONGPATH_MIN_ROOT)
    {
      printf("host root too short: %ld, need >= %d\n",
             (long)host_root_len, HOSTFS_LONGPATH_MIN_ROOT);
      return 2;
    }

  if (snprintf(options, sizeof(options), "fs=%s", host_root) >=
      (int)sizeof(options))
    {
      printf("host root too long: %s\n", host_root);
      return 2;
    }

  if (snprintf(filepath, sizeof(filepath), "%s/%s",
               mountpt, HOSTFS_LONGPATH_FILE) >= (int)sizeof(filepath))
    {
      printf("mount path too long: %s\n", mountpt);
      return 2;
    }

  if (mkdir(mountpt, 0777) < 0 && errno != EEXIST)
    {
      printf("mkdir failed: %s errno=%d\n", mountpt, errno);
      return 1;
    }

  umount(mountpt);
  ret = mount(NULL, mountpt, "hostfs", 0, options);
  if (ret < 0)
    {
      printf("mount failed: %s %s errno=%d\n", mountpt, options, errno);
      return 1;
    }

  ret = write_payload(filepath);
  if (ret == 0)
    {
      ret = verify_payload(filepath);
    }

  umount(mountpt);

  if (ret < 0)
    {
      return 1;
    }

  printf("hostfs_longpath: PASS\n");
  return 0;
}
