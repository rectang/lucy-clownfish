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

#ifndef H_CFCGOCLASS
#define H_CFCGOCLASS

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCGoClass CFCGoClass;
struct CFCParcel;
struct CFCClass;
struct CFCFunction;
struct CFCGoMethod;
struct CFCGoConstructor;

/** Clownfish::CFC::Binding::Go::Class - Generate Go binding code for a
 * Clownfish::CFC::Model::Class.
 */

/** 
 * @param parcel A Clownfish::CFC::Model::Parcel.
 * @param class_name The name of the class to be registered.
 */
CFCGoClass*
CFCGoClass_new(struct CFCParcel *parcel, const char *class_name);

CFCGoClass*
CFCGoClass_init(CFCGoClass *self, struct CFCParcel *parcel,
                  const char *class_name);

void
CFCGoClass_destroy(CFCGoClass *self);

/** Add a new class binding to the registry.  Each unique parcel/class-name
 * combination may only be registered once.
 */
void
CFCGoClass_register(CFCGoClass *self);

/** Given a class name, return a class binding if one exists.
 */
CFCGoClass*
CFCGoClass_singleton(const char *class_name);

/** All registered class bindings.
 */
CFCGoClass**
CFCGoClass_registry();

/** Release all memory and references held by the registry.
 */
void
CFCGoClass_clear_registry(void);

#if 0

/** Specify that a constructor should be made available from Go-space.
 *
 * The default alias for the constructor will follow Go conventions:
 * `NewMyType()` for a `MyType` object.
 *
 * @param alias Name to override the default alias for the constructor.
 * @param initializer The Clownfish name for the initialization function which
 * will be invoked (default "init").
 */
void
CFCGoClass_bind_constructor(CFCGoClass *self, const char *alias,
                              const char *initializer);

#endif
/** Block the automatic generation of a method binding.
 */
void
CFCGoClass_exclude_method(CFCGoClass *self, const char *method);
#if 0

/** Block the automatic generation of a constructor binding.
 */
void
CFCGoClass_exclude_constructor(CFCGoClass *self);
#endif

/** Return an array of Clownfish::CFC::Binding::Perl::Method objects
 * representing all bound methods.
 */
struct CFCGoMethod**
CFCGoClass_method_bindings(struct CFCClass *klass);

#if 0
/** Return an array of Clownfish::CFC::Binding::Perl::Constructor objects
 * representing all bound constructors.
 */
struct CFCPerlConstructor**
CFCGoClass_constructor_bindings(struct CFCClass *klass);

/** Accessor for the Clownfish::CFC::Model::Class module to be bound.
 */
struct CFCClass*
CFCGoClass_get_client(CFCGoClass *self);

/** Accessor for class name. 
 */
const char*
CFCGoClass_get_class_name(CFCGoClass *self);

/** Concatenate verbatim XS code.
 */
void
CFCGoClass_append_xs(CFCGoClass *self, const char *xs);

/** Accessor for verbatim XS code.
 */
const char*
CFCGoClass_get_xs_code(CFCGoClass *self);

/** Return C code which initializes method metadata.
 */
char*
CFCGoClass_method_metadata_code(CFCGoClass *self);

#ifdef __cplusplus
}
#endif

#endif

#endif /* H_CFCGOCLASS */

