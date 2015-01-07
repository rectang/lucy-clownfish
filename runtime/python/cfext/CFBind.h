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

/** Associate Clownfish classes with Python type objects.  (Internal-only,
  * used during bootstrapping.)
  */
void
CFBind_assoc_py_types(cfish_Class ***klass_handles, PyTypeObject **py_types,
                      int32_t num_items);

#ifdef __cplusplus
}
#endif

#endif // H_CFISH_PY_CFBIND

