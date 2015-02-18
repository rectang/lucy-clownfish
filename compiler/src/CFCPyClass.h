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

#ifndef H_CFCPYCLASS
#define H_CFCPYCLASS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCPyClass CFCPyClass;
struct CFCParcel;
struct CFCClass;

/** Clownfish::CFC::Binding::Python::Class - Generate Python binding code for a
 * Clownfish::CFC::Model::Class.
 */

/**
 * @param parcel A Clownfish::CFC::Model::Parcel.
 * @param class_name The name of the class to be registered.
 */
CFCPyClass*
CFCPyClass_new(struct CFCParcel *parcel, const char *class_name);

CFCPyClass*
CFCPyClass_init(CFCPyClass *self, struct CFCParcel *parcel,
                const char *class_name);

void
CFCPyClass_destroy(CFCPyClass *self);

/** Add a new class binding to the registry.  Each unique parcel/class-name
 * combination may only be registered once.
 */
void
CFCPyClass_add_to_registry(CFCPyClass *self);

/** Given a class name, return a class binding if one exists.
 */
CFCPyClass*
CFCPyClass_singleton(const char *class_name);

/** All registered class bindings.
 */
CFCPyClass**
CFCPyClass_registry();

/** Release all memory and references held by the registry.
 */
void
CFCPyClass_clear_registry(void);

char*
CFCPyClass_pytype_struct_def(struct CFCClass *klass, const char *pymod_name);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCPYCLASS */

