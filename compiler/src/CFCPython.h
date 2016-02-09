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

#ifndef H_CFCPYTHON
#define H_CFCPYTHON

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCPython CFCPython;
struct CFCParcel;
struct CFCHierarchy;

/** Clownfish::CFC::Binding::Python - Python bindings for a
 * Clownfish::CFC::Model::Hierarchy.
 *
 * Clownfish::CFC::Binding::Python presents an interface for auto-generating
 * Python C API code to bind to a Clownfish class hierarchy.
 */

/**
 * @param hierarchy A CFCHierarchy.
 */

CFCPython*
CFCPython_new(struct CFCHierarchy *hierarchy);

/** Set the text which will be prepended to generated C files -- for
 * instance, an "autogenerated file" warning.
 */
void
CFCPython_set_header(CFCPython *self, const char *header);

/** Set the text which will be appended to the end of generated C files.
 */
void
CFCPython_set_footer(CFCPython *self, const char *footer);

/** Generate Python bindings for the specified parcel.
 */
void
CFCPython_write_bindings(CFCPython *self, const char *parcel, const char *dest);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCPYTHON */

