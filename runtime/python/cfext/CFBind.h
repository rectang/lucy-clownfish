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

#include "Clownfish/Obj.h"
#include "Clownfish/Class.h"
#include "Clownfish/String.h"
#include "Python.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Wrap the current state of Python's sys.exc_info in a Clownfish Err and
  * throw it.
  *
  * One refcount of `mess` will be consumed by this function.
  * 
  * TODO: Leave the original exception intact.
  */
void
CFBind_reraise_pyerr(cfish_Class *err_klass, cfish_String *mess);

/** Perform recursive conversion of Clownfish objects to Python, returning an
  * incremented PyObject.
  *
  *     String   -> string
  *     VArray   -> list
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

/** Perform recursive conversion of Python objects to Clownfish, return an
  * incremented Clownfish Obj.
  *
  *     string -> String
  *     list   -> VArray
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

typedef struct CFBindArg {
    cfish_Class *klass;
    void        *ptr;
} CFBindArg;

typedef struct CFBindStringArg {
    cfish_String **ptr;
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
CFBind_convert_array(PyObject *input, cfish_VArray **ptr);
int
CFBind_maybe_convert_obj(PyObject *input, CFBindArg *arg);
int
CFBind_maybe_convert_string(PyObject *input, CFBindStringArg *arg);
int
CFBind_maybe_convert_hash(PyObject *input, cfish_Hash **ptr);
int
CFBind_maybe_convert_array(PyObject *input, cfish_VArray **ptr);

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

#ifdef __cplusplus
}
#endif

#endif // H_CFISH_PY_CFBIND

