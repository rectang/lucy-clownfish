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

#ifndef H_CFCPYMETHOD
#define H_CFCPYMETHOD

#ifdef __cplusplus
extern "C" {
#endif

struct CFCFunction;
struct CFCMethod;
struct CFCClass;

/* Check for:
 * - private methods
 * - methods with types which cannot be mapped automatically
 */
int
CFCPyMethod_can_be_bound(struct CFCMethod *method);

int
CFCPyMethod_func_can_be_bound(struct CFCFunction *func);

/** Return C code which knows how to call back into Python for this method.  This
 * code is run when a Python subclass has overridden a method in a Clownfish base
 * class.
 */
char*
CFCPyMethod_callback_def(struct CFCMethod *method);

char*
CFCPyMethod_wrapper(struct CFCMethod *method, struct CFCClass *invoker);

char*
CFCPyMethod_pymethoddef(struct CFCMethod *method, struct CFCClass *invoker);

char*
CFCPyMethod_constructor_wrapper(struct CFCFunction *init_func,
                                struct CFCClass *invoker);

char*
CFCPyFunc_inert_wrapper(struct CFCFunction *func, struct CFCClass *invoker);

char*
CFCPyFunc_static_pymethoddef(struct CFCFunction *func, struct CFCClass *invoker);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCPYMETHOD */

