/* Copyright (C) 2012-2014 Zeex
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <sampgdk/platform.h>

#if SAMPGDK_WINDOWS
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/mman.h>
#endif

#include "hook.h"

#define _SAMPGDK_HOOK_JMP_OPCODE 0xE9

#pragma pack(push, 1)

struct _sampgdk_hook_jmp {
  unsigned char opcpde;
  ptrdiff_t     offset;
};

struct _sampgdk_hook_jmp_8 {
  struct _sampgdk_hook_jmp jmp;
  unsigned char            pad[3];
};

#pragma pack(pop)

struct _sampgdk_hook {
  void *src;
  void *dst;
  unsigned char code[sizeof(struct _sampgdk_hook_jmp_8)];
  unsigned char jump[sizeof(struct _sampgdk_hook_jmp_8)];
};

#if SAMPGDK_WINDOWS

static void *_sampgdk_hook_unprotect(void *address, size_t size) {
  DWORD old;

  if (VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old) == 0)
    return NULL;

  return address;
}

#else /* SAMPGDK_WINDOWS */

static void *_sampgdk_hook_unprotect(void *address, size_t size) {
  long pagesize;

  pagesize = sysconf(_SC_PAGESIZE);
  address = (void *)((long)address & ~(pagesize - 1));

  if (mprotect(address, size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0)
    return NULL;

  return address;
}

#endif /* !SAMPGDK_WINDOWS */

sampgdk_hook_t sampgdk_hook_new(void *src, void *dst) {
  sampgdk_hook_t hook;

  if ((hook = malloc(sizeof(*hook))) == NULL)
    return NULL;

  _sampgdk_hook_unprotect(src, sizeof(struct _sampgdk_hook_jmp_8));

  hook->src = src;
  hook->dst = dst;

  memcpy(hook->code, src, sizeof(hook->code));
  memcpy(hook->jump, src, sizeof(hook->code));

  {
    struct _sampgdk_hook_jmp jmp = {
      _SAMPGDK_HOOK_JMP_OPCODE,
      (unsigned char *)hook->dst -
      (unsigned char *)hook->src - sizeof(jmp)
    };
    memcpy(hook->jump, &jmp, sizeof(jmp));
  }

  return hook;
}

void sampgdk_hook_free(sampgdk_hook_t hook) {
  free(hook);
}

void sampgdk_hook_install(sampgdk_hook_t hook) {
  memcpy(hook->src, hook->jump, sizeof(hook->jump));
}

void sampgdk_hook_remove(sampgdk_hook_t hook) {
  memcpy(hook->src, hook->code, sizeof(hook->code));
}
