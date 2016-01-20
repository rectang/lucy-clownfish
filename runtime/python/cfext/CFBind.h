/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_CFISH_PY_CFBIND
#define H_CFISH_PY_CFBIND 1

#ifdef __cplusplus
extern "C" {
#endif

#include "cfish_platform.h"
#include "Python.h"
#include "Clownfish/Obj.h"

struct cfish_Class;
struct cfish_String;

#include "Clownfish/Class.h"

/** Wrap the current state of Python's sys.exc_info in a Clownfish Err and
  * throw it.
  *
  * One refcount of `mess` will be consumed by this function.
  *
  * TODO: Leave the original exception intact.
  */
void
CFBind_reraise_pyerr(struct cfish_Class *err_klass, struct cfish_String *mess);

/** Perform recursive conversion of Clownfish objects to Python, returning an
  * incremented PyObject.
  *
  *     String   -> string
  *     Vector   -> list
  *     Hash     -> dict
  *     NULL     -> None
  *     ByteBuf  -> bytes
  *     BoolNum  -> bool
  *     IntNum   -> int
  *     FloatNum -> float
  * 
  * Anything else will be left intact, since Clownfish objects are valid Python
  * objects.
  */
PyObject*
CFBind_cfish_to_py(cfish_Obj *obj);

/* Perform the same conversion as `CFBind_cfish_to_py`, but ensure that the
 * result is refcount-neutral, taking ownership of a refcount from `obj`.
 */
PyObject*
CFBind_cfish_to_py_zeroref(cfish_Obj *obj);

/** Perform recursive conversion of Python objects to Clownfish, return an
  * incremented Clownfish Obj.
  *
  *     string -> String
  *     list   -> Vector
  *     dict   -> Hash
  *     None   -> NULL
  *     bytes  -> ByteBuf
  *     bool   -> BoolNum
  *     int    -> IntNum
  *     float  -> FloatNum
  *
  * Python dict keys will be stringified.  Other Clownfish objects will be
  * left intact.  Anything else will be stringified.   
  */
cfish_Obj*
CFBind_py_to_cfish(PyObject *py_obj);

/** FIXME temp hack.
  */
cfish_Obj*
CFBind_maybe_py_to_cfish(PyObject *py_obj, cfish_Class *klass);

/** Associate Clownfish classes with Python type objects.  (Internal-only,
  * used during bootstrapping.)
  */
void
CFBind_assoc_py_types(cfish_Class ***klass_handles, PyTypeObject **py_types,
                      int32_t num_items);

typedef union {
    void*    ptr;
    int8_t   int8_t_;
    int16_t  int16_t_;
    int32_t  int32_t_;
    int64_t  int64_t_;
    uint8_t  uint8_t_;
    uint16_t uint16_t_;
    uint32_t uint32_t_;
    uint64_t uint64_t_;
    int      char_;
    short    short_;
    int      int_;
    long     long_;
    size_t   size_t_;
    bool     bool_;
    float    float_;
    double   double_;
} cfbind_any_t;

typedef struct CFBindTrapContext {
    cfbind_any_t  retval;
    cfbind_any_t *args;
    void         *decrefs;
    int           num_decrefs;
} CFBindTrapContext;

#define CFBIND_TYPE_void     1
#define CFBIND_TYPE_int8_t   2
#define CFBIND_TYPE_int16_t  3
#define CFBIND_TYPE_int32_t  4
#define CFBIND_TYPE_int64_t  5
#define CFBIND_TYPE_uint8_t  6
#define CFBIND_TYPE_uint16_t 7
#define CFBIND_TYPE_uint32_t 8
#define CFBIND_TYPE_uint64_t 9
#define CFBIND_TYPE_char     10
#define CFBIND_TYPE_short    11
#define CFBIND_TYPE_int      12
#define CFBIND_TYPE_long     13
#define CFBIND_TYPE_size_t   14
#define CFBIND_TYPE_bool     15
#define CFBIND_TYPE_float    16
#define CFBIND_TYPE_double   17
#define CFBIND_TYPE_obj      18
#define CFBIND_TYPE_inc_obj  19
#define CFBIND_TYPE_raw_obj  20

#define CFBIND_TYPE_MASK 0x1F

PyObject*
CFBind_run_trapped(CFISH_Err_Attempt_t func, void *vcontext,
                   int retval_type);

#define CFBIND_RUN_TRAPPED_void(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_void)
#define CFBIND_RUN_TRAPPED_int8_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_int8_t)
#define CFBIND_RUN_TRAPPED_int16_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_int16_t)
#define CFBIND_RUN_TRAPPED_int32_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_int32_t)
#define CFBIND_RUN_TRAPPED_int64_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_int64_t)
#define CFBIND_RUN_TRAPPED_uint8_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_uint8_t)
#define CFBIND_RUN_TRAPPED_uint16_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_uint16_t)
#define CFBIND_RUN_TRAPPED_uint32_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_uint32_t)
#define CFBIND_RUN_TRAPPED_uint64_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_uint64_t)
#define CFBIND_RUN_TRAPPED_char(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_char)
#define CFBIND_RUN_TRAPPED_short(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_short)
#define CFBIND_RUN_TRAPPED_long(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_long)
#define CFBIND_RUN_TRAPPED_size_t(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_size_t)
#define CFBIND_RUN_TRAPPED_bool(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_bool)
#define CFBIND_RUN_TRAPPED_float(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_float)
#define CFBIND_RUN_TRAPPED_double(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_double)
#define CFBIND_RUN_TRAPPED_obj(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_obj)
#define CFBIND_RUN_TRAPPED_inc_obj(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_inc_obj)
#define CFBIND_RUN_TRAPPED_raw_obj(func, vcontext) \
    CFBind_run_trapped(func, vcontext, CFBIND_TYPE_raw_obj)

typedef struct CFBindArg {
    cfish_Class *klass;
    void        *ptr;
} CFBindArg;

typedef struct CFBindStringArg {
    void *ptr;
    void *stack_mem;
    PyObject *stringified;
} CFBindStringArg;

/* ParseTuple conversion routines for reference types.
 *
 * If `input` is `None`, the "maybe_convert" variants will leave `ptr`
 * untouched, while the "convert" routines will raise a TypeError.
 */
int
CFBind_convert_obj(PyObject *input, CFBindArg *arg);
int
CFBind_convert_string(PyObject *input, CFBindStringArg *arg);
int
CFBind_convert_hash(PyObject *input, cfish_Hash **ptr);
int
CFBind_convert_array(PyObject *input, cfish_Vector **ptr);
int
CFBind_maybe_convert_obj(PyObject *input, CFBindArg *arg);
int
CFBind_maybe_convert_string(PyObject *input, CFBindStringArg *arg);
int
CFBind_maybe_convert_hash(PyObject *input, cfish_Hash **ptr);
int
CFBind_maybe_convert_array(PyObject *input, cfish_Vector **ptr);

/* ParseTuple conversion routines for primitive numeric types.
 *
 * If the value of `input` is out of range for the an integer C type, an
 * OverflowError will be raised.
 *
 * If `input` is `None`, the "maybe_convert" variants will leave `ptr`
 * untouched, while the "convert" routines will raise a TypeError.
 */
int
CFBind_convert_char(PyObject *input, char *ptr);
int
CFBind_convert_short(PyObject *input, short *ptr);
int
CFBind_convert_int(PyObject *input, int *ptr);
int
CFBind_convert_long(PyObject *input, long *ptr);
int
CFBind_convert_int8_t(PyObject *input, int8_t *ptr);
int
CFBind_convert_int16_t(PyObject *input, int16_t *ptr);
int
CFBind_convert_int32_t(PyObject *input, int32_t *ptr);
int
CFBind_convert_int64_t(PyObject *input, int64_t *ptr);
int
CFBind_convert_uint8_t(PyObject *input, uint8_t *ptr);
int
CFBind_convert_uint16_t(PyObject *input, uint16_t *ptr);
int
CFBind_convert_uint32_t(PyObject *input, uint32_t *ptr);
int
CFBind_convert_uint64_t(PyObject *input, uint64_t *ptr);
int
CFBind_convert_bool(PyObject *input, bool *ptr);
int
CFBind_convert_size_t(PyObject *input, size_t *ptr);
int
CFBind_convert_float(PyObject *input, float *ptr);
int
CFBind_convert_double(PyObject *input, double *ptr);
int
CFBind_maybe_convert_char(PyObject *input, char *ptr);
int
CFBind_maybe_convert_short(PyObject *input, short *ptr);
int
CFBind_maybe_convert_int(PyObject *input, int *ptr);
int
CFBind_maybe_convert_long(PyObject *input, long *ptr);
int
CFBind_maybe_convert_int8_t(PyObject *input, int8_t *ptr);
int
CFBind_maybe_convert_int16_t(PyObject *input, int16_t *ptr);
int
CFBind_maybe_convert_int32_t(PyObject *input, int32_t *ptr);
int
CFBind_maybe_convert_int64_t(PyObject *input, int64_t *ptr);
int
CFBind_maybe_convert_uint8_t(PyObject *input, uint8_t *ptr);
int
CFBind_maybe_convert_uint16_t(PyObject *input, uint16_t *ptr);
int
CFBind_maybe_convert_uint32_t(PyObject *input, uint32_t *ptr);
int
CFBind_maybe_convert_uint64_t(PyObject *input, uint64_t *ptr);
int
CFBind_maybe_convert_bool(PyObject *input, bool *ptr);
int
CFBind_maybe_convert_size_t(PyObject *input, size_t *ptr);
int
CFBind_maybe_convert_float(PyObject *input, float *ptr);
int
CFBind_maybe_convert_double(PyObject *input, double *ptr);

void
CFBind_class_bootstrap_hook1(cfish_Class *self);

#ifdef __cplusplus
}
#endif

#endif // H_CFISH_PY_CFBIND

