/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INTERCEPTOR_INTERCEPTOR_H_
#define INTERCEPTOR_INTERCEPTOR_H_

// -----------------------------------------------------------------------------
// extern "C" interface designed for users who dlopen the interceptor-lib
// instead of linking against it. The API for these functions using C structures
// only to support users compiled with different STL library and to support
// usesrs who want to use dlopen/dlsym for loading the library.
// -----------------------------------------------------------------------------

extern "C" {

// Initializes the internal state of the interceptor library and returns a baton
// what have to be passed in to every other function. If called multiple times
// then multiple independent copies of the interceptor will be created.
void *InitializeInterceptor();

// Terminate an instance of the interceptor, deletes the trampolines set up by
// the instance and frees up all resources allocated by it. After this call the
// baton is a dangling pointer and passing it to any of the API function is
// undefined behaviour.
void TerminateInterceptor(void *interceptor);

// Find a function in the application by it's mangled symbol name by searching
// both the public symbol table and the internal symbol table if available. The
// returned pointer can be used to call the function after casted to the
// approptiate type or can be passed to other functions provided by the
// interceptor. If failes to find a symbol with the given name or more then 1
// symbol found then nullptr is returned.
void *FindFunctionByName(void *interceptor, const char *symbol_name);

// Intercepts a function specified by "old_function" with the one specified by
// "new_function". If "callback_function" is not nullptr then a callback stub
// is generated and returned in the pointer specified by "callback_function"
// what can be used to call the original (not intercepted) function after
// casting it to the correct function signature. If an "error_callback" is
// specifed then it will be called for every error encountered during
// interception with the baton specified in "error_callback_baton" and the error
// message itself. The return value of the function will specify if the
// interception was successfull (return true) or not (return false). In case
// of an interception failure the error_callback (if specified) called at least
// once and the original function isn't modified.
bool InterceptFunction(void *interceptor, void *old_function,
                       void *new_function, void **callback_function = nullptr,
                       void (*error_callback)(void *, const char *) = nullptr,
                       void *error_callback_baton = nullptr);

// Intercepts a function specifyind by "symbol_name" if exactly 1 symbol matches
// the specified name. The function has the same semantics as InterceptFunction
// regarding all other argument.
bool InterceptSymbol(void *interceptor, const char *symbol_name,
                     void *new_function, void **callback_function,
                     void (*error_callback)(void *, const char *) = nullptr,
                     void *error_callback_baton = nullptr);

}  // extern "C"

#endif  // INTERCEPTOR_INTERCEPTOR_H_
