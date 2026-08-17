/****************************************************************************
 * apps/examples/loadelf/samples/loadelf_sample/loadelf_sample.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  char *scratch;
  long sum = 0;
  int i;

  printf("loadelf_sample: loadable ELF loaded by NuttX\n");
  printf("loadelf_sample: pid=%d argc=%d\n", (int)getpid(), argc);

  for (i = 0; i < argc; i++)
    {
      printf("loadelf_sample: argv[%d]=%s\n", i, argv[i]);
    }

  for (i = 1; i < argc; i++)
    {
      sum += strtol(argv[i], NULL, 0);
    }

  scratch = malloc(32);
  if (scratch == NULL)
    {
      printf("loadelf_sample: malloc failed\n");
      return 1;
    }

  snprintf(scratch, 32, "sum=%ld", sum);
  printf("loadelf_sample1111: %s\n", scratch);
  free(scratch);

  return 0;
}
