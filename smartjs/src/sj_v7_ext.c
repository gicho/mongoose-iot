#include "sj_v7_ext.h"

#include <string.h>

#include <cs_dbg.h>
#include <v7.h>
#include "sj_hal.h"

static v7_val_t Sys_prof(struct v7 *v7) {
  v7_val_t result = v7_create_object(v7);
  v7_own(v7, &result);

  v7_set(v7, result, "sysfree", 7, 0,
         v7_create_number(sj_get_free_heap_size()));
  v7_set(v7, result, "used_by_js", 10, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_HEAP_USED)));
  v7_set(v7, result, "used_by_fs", 10, 0,
         v7_create_number(sj_get_fs_memory_usage()));

  v7_disown(v7, &result);
  return result;
}

static v7_val_t Sys_wdtFeed(struct v7 *v7) {
  (void) v7;
  sj_wdt_feed();

  return v7_create_boolean(1);
}

static v7_val_t Sys_reboot(struct v7 *v7) {
  (void) v7;
  sj_system_restart();

  /* Unreachable */
  return v7_create_boolean(1);
}

static v7_val_t Sys_setLogLevel(struct v7 *v7) {
  v7_val_t llv = v7_arg(v7, 0);
  int ll;
  if (!v7_is_number(llv)) return v7_create_boolean(0);
  ll = v7_to_number(llv);
  if (ll <= _LL_MIN || ll >= _LL_MAX) return v7_create_boolean(0);
  cs_log_set_level((enum cs_log_level) ll);
  return v7_create_boolean(1);
}

static v7_val_t global_usleep(struct v7 *v7) {
  v7_val_t usecsv = v7_arg(v7, 0);
  int usecs;
  if (!v7_is_number(usecsv)) {
    printf("usecs is not a double\n\r");
    return v7_create_undefined();
  }
  usecs = v7_to_number(usecsv);
  sj_usleep(usecs);
  return v7_create_undefined();
}

/*
 * Returns an object describing the free memory.
 *
 * sysfree: free system heap bytes
 * jssize: size of JS heap in bytes
 * jsfree: free JS heap bytes
 * strres: size of reserved string heap in bytes
 * struse: portion of string heap with used data
 * objnfree: number of free object slots in js heap
 * propnfree: number of free property slots in js heap
 * funcnfree: number of free function slots in js heap
 */
static v7_val_t GC_stat(struct v7 *v7) {
  /* take a snapshot of the stats that would change as we populate the result */
  size_t sysfree = sj_get_free_heap_size();
  size_t jssize = v7_heap_stat(v7, V7_HEAP_STAT_HEAP_SIZE);
  size_t jsfree = jssize - v7_heap_stat(v7, V7_HEAP_STAT_HEAP_USED);
  size_t strres = v7_heap_stat(v7, V7_HEAP_STAT_STRING_HEAP_RESERVED);
  size_t struse = v7_heap_stat(v7, V7_HEAP_STAT_STRING_HEAP_USED);
  size_t objfree = v7_heap_stat(v7, V7_HEAP_STAT_OBJ_HEAP_FREE);
  size_t propnfree = v7_heap_stat(v7, V7_HEAP_STAT_PROP_HEAP_FREE);
  v7_val_t f = v7_create_undefined();
  v7_own(v7, &f);
  f = v7_create_object(v7);

  v7_set(v7, f, "sysfree", ~0, 0, v7_create_number(sysfree));
  v7_set(v7, f, "jssize", ~0, 0, v7_create_number(jssize));
  v7_set(v7, f, "jsfree", ~0, 0, v7_create_number(jsfree));
  v7_set(v7, f, "strres", ~0, 0, v7_create_number(strres));
  v7_set(v7, f, "struse", ~0, 0, v7_create_number(struse));
  v7_set(v7, f, "objfree", ~0, 0, v7_create_number(objfree));
  v7_set(v7, f, "objncell", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_OBJ_HEAP_CELL_SIZE)));
  v7_set(v7, f, "propnfree", ~0, 0, v7_create_number(propnfree));
  v7_set(v7, f, "propncell", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_PROP_HEAP_CELL_SIZE)));
  v7_set(v7, f, "funcnfree", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_FUNC_HEAP_FREE)));
  v7_set(v7, f, "funcncell", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_FUNC_HEAP_CELL_SIZE)));
  v7_set(v7, f, "astsize", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_FUNC_AST_SIZE)));
  v7_set(v7, f, "owned", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_FUNC_OWNED)));
  v7_set(v7, f, "owned_max", ~0, 0,
         v7_create_number(v7_heap_stat(v7, V7_HEAP_STAT_FUNC_OWNED_MAX)));

  v7_disown(v7, &f);
  return f;
}

/*
 * Force a pass of the garbage collector.
 */
static v7_val_t GC_gc(struct v7 *v7) {
  v7_gc(v7, 1);
  return v7_create_undefined();
}

void sj_print_exception(struct v7 *v7, v7_val_t exc, const char *msg) {
  /*
   * TOD(mkm) add some API to hal to fetch the current debug mode
   * and avoid logging to stdout if according no error messages should go
   * there (e.g. because it's used to implement a serial protocol).
   */
  FILE *fs[] = {stdout, stderr};
  size_t i;

  /*
   * own because the exception could be a string,
   * and if not owned here, print_stack_trace could get
   * an unrelocated argument an ASN violation.
   */
  v7_own(v7, &exc);

  for (i = 0; i < sizeof(fs) / sizeof(fs[0]); i++) {
    fprintf(fs[i], "%s: ", msg);
    v7_fprintln(fs[i], v7, exc);
#if V7_ENABLE__StackTrace
    v7_fprint_stack_trace(fs[i], v7, exc);
#endif
  }

  v7_disown(v7, &exc);
}

void _sj_invoke_cb(struct v7 *v7, v7_val_t func, v7_val_t this_obj,
                   v7_val_t args) {
  v7_val_t res;
  if (v7_apply(v7, &res, func, this_obj, args) == V7_EXEC_EXCEPTION) {
    sj_print_exception(v7, res, "cb threw exception");
  }
}

void sj_invoke_cb2_this(struct v7 *v7, v7_val_t cb, v7_val_t this_obj,
                        v7_val_t arg1, v7_val_t arg2) {
  v7_val_t args;
  v7_own(v7, &cb);
  v7_own(v7, &arg1);
  v7_own(v7, &arg2);

  args = v7_create_array(v7);
  v7_own(v7, &args);
  v7_array_push(v7, args, arg1);
  v7_array_push(v7, args, arg2);
  sj_invoke_cb(v7, cb, this_obj, args);
  v7_disown(v7, &args);
  v7_disown(v7, &arg2);
  v7_disown(v7, &arg1);
  v7_disown(v7, &cb);
}

void sj_invoke_cb1_this(struct v7 *v7, v7_val_t cb, v7_val_t this_obj,
                        v7_val_t arg) {
  v7_val_t args;
  v7_own(v7, &cb);
  v7_own(v7, &arg);
  args = v7_create_array(v7);
  v7_own(v7, &args);
  v7_array_push(v7, args, arg);
  sj_invoke_cb(v7, cb, this_obj, args);
  v7_disown(v7, &args);
  v7_disown(v7, &arg);
  v7_disown(v7, &cb);
}

void sj_invoke_cb0_this(struct v7 *v7, v7_val_t cb, v7_val_t this_obj) {
  v7_val_t args;
  v7_own(v7, &cb);
  args = v7_create_array(v7);
  v7_own(v7, &args);
  sj_invoke_cb(v7, cb, this_obj, args);
  v7_disown(v7, &args);
  v7_disown(v7, &cb);
}

void sj_invoke_cb0(struct v7 *v7, v7_val_t cb) {
  sj_invoke_cb0_this(v7, cb, v7_get_global(v7));
}

void sj_invoke_cb1(struct v7 *v7, v7_val_t cb, v7_val_t arg) {
  sj_invoke_cb1_this(v7, cb, v7_get_global(v7), arg);
}

void sj_invoke_cb2(struct v7 *v7, v7_val_t cb, v7_val_t arg1, v7_val_t arg2) {
  sj_invoke_cb2_this(v7, cb, v7_get_global(v7), arg1, arg2);
}

void sj_init_v7_ext(struct v7 *v7) {
  v7_val_t gc, sys;

  v7_set_method(v7, v7_get_global(v7), "usleep", global_usleep);

  gc = v7_create_object(v7);
  v7_set(v7, v7_get_global(v7), "GC", ~0, 0, gc);
  v7_set_method(v7, gc, "stat", GC_stat);
  v7_set_method(v7, gc, "gc", GC_gc);

  sys = v7_create_object(v7);
  v7_set(v7, v7_get_global(v7), "Sys", ~0, 0, sys);
  v7_set_method(v7, sys, "prof", Sys_prof);
  v7_set_method(v7, sys, "wdtFeed", Sys_wdtFeed);
  v7_set_method(v7, sys, "reboot", Sys_reboot);
  v7_set_method(v7, sys, "setLogLevel", Sys_setLogLevel);
}