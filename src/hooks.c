/* Copyright (C) 2011-2013 Zeex
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

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <subhook.h>

#include <sampgdk/amx.h>
#include <sampgdk/bool.h>
#include <sampgdk/core.h>
#include <sampgdk/plugin.h>

#include "callback.h"
#include "fakeamx.h"
#include "init.h"
#include "log.h"
#include "native.h"

#include "sdk/amx/amx.h"

/* The main AMX instance. */
static AMX *main_amx = NULL;

/* The name of the currently executing public function. */
static char *public_name = NULL;

static subhook_t amx_FindPublic_hook;
static subhook_t amx_Exec_hook;
static subhook_t amx_Register_hook;
static subhook_t amx_Callback_hook;
static subhook_t amx_Allot_hook;

/* The "funcidx" native uses amx_FindPublic() to get public function index
 * but our FindPublic always succeeds regardless of public existence, so
 * here's a fixed version.
 *
 * Thanks to Incognito for finding this bug!
 */
static cell AMX_NATIVE_CALL funcidx(AMX *amx, cell *params) {
  char *funcname;
  int index;
  int error;

  amx_StrParam(amx, params[1], funcname);
  if (funcname == NULL) {
    return -1;
  }

  error = amx_FindPublic(amx, funcname, &index);
  if (error != AMX_ERR_NONE || (error == AMX_ERR_NONE &&
      index == AMX_EXEC_GDK)) {
    return -1;
  }

  return index;
}

static void hook_native(AMX *amx, const char *name, AMX_NATIVE address) {
  AMX_HEADER *hdr;
  AMX_FUNCSTUBNT *cur;
  AMX_FUNCSTUBNT *end;

  hdr = (AMX_HEADER*)(amx->base);
  cur = (AMX_FUNCSTUBNT*)(amx->base + hdr->natives);
  end = (AMX_FUNCSTUBNT*)(amx->base + hdr->libraries);

  while (cur < end) {
    if (strcmp((char*)(cur->nameofs + amx->base), name) == 0) {
      cur->address = (cell)address;
      break;
    }
    cur++;
  }
}

static int AMXAPI amx_Register_(AMX *amx, const AMX_NATIVE_INFO *nativelist,
                                int number) {
  int i;
  int error;

  subhook_remove(amx_Register_hook);

  hook_native(amx, "funcidx", funcidx);

  for (i = 0; nativelist[i].name != 0 && (i < number || number == -1); i++) {
    sampgdk_native_register(nativelist[i].name, nativelist[i].func);
  }

  error = amx_Register(amx, nativelist, number);
  subhook_install(amx_Register_hook);

  return error;
}

/* The SA-MP server always makes a call to amx_FindPublic() and depending on
 * the value returned it may invoke amx_Exec() to call the public function.
 *
 * In order to make it always execute publics regardless of their existence
 * we have to make amx_FindPublic() always return OK.
 */
static int AMXAPI amx_FindPublic_(AMX *amx, const char *name, int *index) {
  int error;
  bool proceed = false;

  subhook_remove(amx_FindPublic_hook);

  /* We are interested in calling publics against two AMX instances:
   * - the main AMX (the gamemode)
   * - the fake AMX
   */
  proceed = (amx == main_amx || amx == sampgdk_fakeamx_amx());

  error = amx_FindPublic(amx, name, index);
  if (error != AMX_ERR_NONE && proceed) {
    /* Trick the server to make it call this public with amx_Exec()
     * even though the public doesn't actually exist.
     */
    error = AMX_ERR_NONE;
    *index = AMX_EXEC_GDK;
  }

  if (proceed) {
    /* Store the function name in a global string to later access
     * it in amx_Exec_().
     */
    if (public_name != NULL) {
      free(public_name);
    }

    if ((public_name = malloc(strlen(name) + 1)) == NULL) {
      sampgdk_log_error(strerror(ENOMEM));
      return error;
    }

    strcpy(public_name, name);
  }

  subhook_install(amx_FindPublic_hook);

  return error;
}

static int AMXAPI amx_Exec_(AMX *amx, cell *retval, int index) {
  bool proceed;
  sampgdk_public_hook hook;
  int error;

  subhook_remove(amx_Exec_hook);
  subhook_install(amx_Callback_hook);

  proceed = true;

  if ((hook = sampgdk_get_public_hook()) != NULL) {
    proceed = hook(amx, public_name);
  }

  /* Since filterscripts don't use main() we can assume that the AMX
   * that executes main() is indeed the main AMX i.e. the gamemode.
   */
  if (index == AMX_EXEC_MAIN) {
    main_amx = amx;
    sampgdk_callback_invoke(main_amx, "OnGameModeInit", retval);
  } else {
    if (index != AMX_EXEC_CONT && public_name != NULL
        && (amx == main_amx || amx == sampgdk_fakeamx_amx())) {
      proceed = sampgdk_callback_invoke(amx, public_name, retval);
    }
  }

  error = AMX_ERR_NONE;

  if (proceed && index != AMX_EXEC_GDK) {
    error = amx_Exec(amx, retval, index);
  } else {
    amx->stk += amx->paramcount * sizeof(cell);
  }

  amx->paramcount = 0;

  subhook_remove(amx_Callback_hook);
  subhook_install(amx_Exec_hook);

  return error;
}

static int AMXAPI amx_Callback_(AMX *amx, cell index, cell *result,
                                cell *params) {
  int error;

  subhook_remove(amx_Callback_hook);
  subhook_install(amx_Exec_hook);

  /* Prevent the default AMX callback from replacing SYSREQ.C instructions
   * with SYSREQ.D.
   */
  amx->sysreq_d = 0;

  error = amx_Callback(amx, index, result, params);

  subhook_remove(amx_Exec_hook);
  subhook_install(amx_Callback_hook);

  return error;
}

static int AMXAPI amx_Allot_(AMX *amx, int cells, cell *amx_addr,
                             cell **phys_addr) {
  int error;

  subhook_remove(amx_Allot_hook);

  /* There is a bug in amx_Allot() where it returns success even though
   * there's no enough space on the heap:
   *
   * if (amx->stk - amx->hea - cells*sizeof(cell) < STKMARGIN)
   *   return AMX_ERR_MEMORY;
   *
   * The expression on the right side of the comparison is converted to
   * an unsigned type and in fact never results in a negative value.
   */
  #define STKMARGIN (cell)(16 * sizeof(cell))
  if ((size_t)amx->stk < (size_t)(amx->hea + cells*sizeof(cell) + STKMARGIN)) {
    error =  AMX_ERR_MEMORY;
  } else {
    error = amx_Allot(amx, cells, amx_addr, phys_addr);
  }

  /* If called against the fake AMX and failed to allocate the requested
   * amount of space, grow the heap and try again. */
  if (error == AMX_ERR_MEMORY && amx == sampgdk_fakeamx_amx()) {
    cell new_size = ((amx->hea + STKMARGIN) / sizeof(cell)) + cells + 2;
    if (sampgdk_fakeamx_resize_heap(new_size) >= 0) {
      error = amx_Allot(amx, cells, amx_addr, phys_addr);
    }
  }

  subhook_install(amx_Allot_hook);

  return error;
}

#define FOR_EACH_FUNC(C) \
  C(Register) \
  C(FindPublic) \
  C(Exec) \
  C(Callback) \
  C(Allot)

static int create_hooks(void) {
  #define CREATE_HOOK(name) \
    if ((amx_##name##_hook = subhook_new()) == NULL) \
      goto no_memory;
  FOR_EACH_FUNC(CREATE_HOOK)
  return 0;
no_memory:
  return -ENOMEM;
  #undef CREATE_HOOK
}

static void destroy_hooks(void) {
  #define DESTROY_HOOK(name) \
    subhook_free(amx_##name##_hook); \
  FOR_EACH_FUNC(DESTROY_HOOK)
  #undef DESTROY_HOOK
}

static void install_hooks(void) {
  #define INSTALL_HOOK(name) \
    subhook_set_src(amx_##name##_hook, \
                    ((void**)(amx_exports))[PLUGIN_AMX_EXPORT_##name]); \
    subhook_set_dst(amx_##name##_hook, (void*)amx_##name##_); \
    subhook_install(amx_##name##_hook);
  FOR_EACH_FUNC(INSTALL_HOOK)
  #undef INSTALL_HOOK
}

static void remove_hooks(void) {
  #define REMOVE_HOOK(name) \
    subhook_remove(amx_##name##_hook);
  FOR_EACH_FUNC(REMOVE_HOOK)
  #undef REMOVE_HOOK
}

DEFINE_INIT_FUNC(hooks) {
  int error;

  error = create_hooks();
  if (error < 0) {
    destroy_hooks();
    return error;
  }

  install_hooks();

  return 0;
}

DEFINE_CLEANUP_FUNC(hooks) {
  remove_hooks();
  destroy_hooks();
}
