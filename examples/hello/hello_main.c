/****************************************************************************
 * apps/examples/hello/hello_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HELLO_HDR_SIZE          12
#define HELLO_HOST_MAGIC        "H2NX"
#define HELLO_NUTTX_MAGIC       "N2HX"
#define HELLO_ACK_MAGIC         "ACK!"
#define HELLO_HOST_TO_NUTTX_LEN 32768
#define HELLO_NUTTX_TO_HOST_LEN 49152
#define HELLO_HOST_SEED         0x13579bdf
#define HELLO_NUTTX_SEED        0x2468ace0
#define HELLO_FNV_OFFSET        2166136261u
#define HELLO_FNV_PRIME         16777619u

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint8_t g_hello_buf[127];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint8_t hello_pattern(uint32_t seed, uint32_t index)
{
  uint32_t value;

  value = seed ^ (index * 1103515245u);
  value ^= index >> 3;
  value ^= value >> 16;
  value ^= value >> 8;

  return value & 0xff;
}

static uint32_t hello_hash_update(uint32_t hash, uint8_t byte)
{
  hash ^= byte;
  hash *= HELLO_FNV_PRIME;

  return hash;
}

static uint32_t hello_pattern_hash(uint32_t seed, uint32_t len)
{
  uint32_t hash = HELLO_FNV_OFFSET;
  uint32_t i;

  for (i = 0; i < len; i++)
    {
      hash = hello_hash_update(hash, hello_pattern(seed, i));
    }

  return hash;
}

static uint32_t hello_get_le32(FAR const uint8_t *buf)
{
  return ((uint32_t)buf[0]) |
         ((uint32_t)buf[1] << 8) |
         ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
}

static void hello_put_le32(FAR uint8_t *buf, uint32_t value)
{
  buf[0] = value & 0xff;
  buf[1] = (value >> 8) & 0xff;
  buf[2] = (value >> 16) & 0xff;
  buf[3] = (value >> 24) & 0xff;
}

static int hello_read_exact(int fd, FAR uint8_t *buf, size_t len)
{
  size_t off = 0;
  int retry = 0;

  while (off < len)
    {
      ssize_t ret;

      ret = read(fd, buf + off, len - off);

      if (ret > 0)
        {
          off += ret;
          retry = 0;
          continue;
        }

      if (ret == 0 || errno == EAGAIN || errno == EINTR)
        {
          if (++retry > 30000)
            {
              return -1;
            }

          usleep(1000);
          continue;
        }

      return -1;
    }

  return 0;
}

static int hello_write_all(int fd, FAR const uint8_t *buf, size_t len)
{
  size_t off = 0;
  int retry = 0;

  while (off < len)
    {
      ssize_t ret;

      ret = write(fd, buf + off, len - off);
      if (ret > 0)
        {
          off += ret;
          retry = 0;
          continue;
        }

      if (ret == 0 || errno == EAGAIN || errno == EINTR)
        {
          if (++retry > 30000)
            {
              return -1;
            }

          usleep(1000);
          continue;
        }

      return -1;
    }

  return 0;
}

static int hello_read_payload(int fd, uint32_t len, uint32_t seed,
                              uint32_t expected_hash)
{
  uint32_t hash = HELLO_FNV_OFFSET;
  uint32_t done = 0;

  while (done < len)
    {
      size_t chunk;
      size_t i;

      chunk = len - done;
      if (chunk > sizeof(g_hello_buf))
        {
          chunk = sizeof(g_hello_buf);
        }

      if (hello_read_exact(fd, g_hello_buf, chunk) < 0)
        {
          return -1;
        }

      for (i = 0; i < chunk; i++)
        {
          uint8_t expected = hello_pattern(seed, done + i);

          if (g_hello_buf[i] != expected)
            {
              printf("sim_uart_pty_test: RX mismatch offset=%lu "
                     "got=%02x expected=%02x\n",
                     (unsigned long)(done + i), g_hello_buf[i], expected);
              return -1;
            }

          hash = hello_hash_update(hash, g_hello_buf[i]);
        }

      done += chunk;
    }

  if (hash != expected_hash)
    {
      printf("sim_uart_pty_test: RX checksum mismatch got=%08lx "
             "expected=%08lx\n",
             (unsigned long)hash, (unsigned long)expected_hash);
      return -1;
    }

  return 0;
}

static int hello_write_payload(int fd, uint32_t len, uint32_t seed)
{
  uint32_t done = 0;

  while (done < len)
    {
      size_t chunk;
      size_t i;

      chunk = len - done;
      if (chunk > sizeof(g_hello_buf))
        {
          chunk = sizeof(g_hello_buf);
        }

      for (i = 0; i < chunk; i++)
        {
          g_hello_buf[i] = hello_pattern(seed, done + i);
        }

      if (hello_write_all(fd, g_hello_buf, chunk) < 0)
        {
          return -1;
        }

      done += chunk;
    }

  return 0;
}

static int hello_read_header(int fd, FAR const char *magic,
                             FAR uint32_t *len, FAR uint32_t *hash)
{
  uint8_t hdr[HELLO_HDR_SIZE];

  if (hello_read_exact(fd, hdr, sizeof(hdr)) < 0)
    {
      return -1;
    }

  if (memcmp(hdr, magic, 4) != 0)
    {
      printf("sim_uart_pty_test: bad magic %02x %02x %02x %02x\n",
             hdr[0], hdr[1], hdr[2], hdr[3]);
      return -1;
    }

  *len = hello_get_le32(&hdr[4]);
  *hash = hello_get_le32(&hdr[8]);

  return 0;
}

static int hello_write_header(int fd, FAR const char *magic, uint32_t len,
                              uint32_t hash)
{
  uint8_t hdr[HELLO_HDR_SIZE];

  memcpy(hdr, magic, 4);
  hello_put_le32(&hdr[4], len);
  hello_put_le32(&hdr[8], hash);

  return hello_write_all(fd, hdr, sizeof(hdr));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  uint32_t len;
  uint32_t hash;
  int fd;

  fd = open("/dev/ttySIM0", O_RDWR | O_NONBLOCK);
  if (fd < 0)
    {
      printf("sim_uart_pty_test: open failed errno=%d\n", errno);
      return 1;
    }

  if (hello_read_header(fd, HELLO_HOST_MAGIC, &len, &hash) < 0 ||
      len != HELLO_HOST_TO_NUTTX_LEN)
    {
      printf("sim_uart_pty_test: host header failed errno=%d len=%lu\n",
             errno, (unsigned long)len);
      close(fd);
      return 1;
    }

  if (hello_read_payload(fd, len, HELLO_HOST_SEED, hash) < 0)
    {
      printf("sim_uart_pty_test: binary RX failed errno=%d\n", errno);
      close(fd);
      return 1;
    }

  len = HELLO_NUTTX_TO_HOST_LEN;
  hash = hello_pattern_hash(HELLO_NUTTX_SEED, len);

  if (hello_write_header(fd, HELLO_NUTTX_MAGIC, len, hash) < 0 ||
      hello_write_payload(fd, len, HELLO_NUTTX_SEED) < 0)
    {
      printf("sim_uart_pty_test: binary TX failed errno=%d\n", errno);
      close(fd);
      return 1;
    }

  if (hello_read_header(fd, HELLO_ACK_MAGIC, &len, &hash) < 0 ||
      len != 0 || hash != 0)
    {
      printf("sim_uart_pty_test: ACK failed errno=%d len=%lu hash=%08lx\n",
             errno, (unsigned long)len, (unsigned long)hash);
      close(fd);
      return 1;
    }

  close(fd);
  printf("sim_uart_pty_test: binary RX %u TX %u passed\n",
         HELLO_HOST_TO_NUTTX_LEN, HELLO_NUTTX_TO_HOST_LEN);
  return 0;
}
