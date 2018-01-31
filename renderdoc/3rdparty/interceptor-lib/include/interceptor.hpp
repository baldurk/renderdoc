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

#ifndef INTERCEPTOR_INTERCEPTOR_HPP_
#define INTERCEPTOR_INTERCEPTOR_HPP_

#include <array>
#include <sstream>
#include <string>

#include "interceptor.h"

//------------------------------------------------------------------------------
// Header only C++ wrapper interface around the basic extren "C" interface for
// convinince. The interface simplifies the usecase when linking agains the
// interceptor-lib with automated resource management and with adding a list
// of templatized functions for type safety and for intercepting multiple
// symbols with a single target function.
//------------------------------------------------------------------------------
class Interceptor {
 public:
  Interceptor() { interceptor_ = ::InitializeInterceptor(); }

  ~Interceptor() { ::TerminateInterceptor(interceptor_); }

  void *FindFunctionByName(const char *symbol_name) {
    return ::FindFunctionByName(interceptor_, symbol_name);
  }

  template <typename Ret, typename... Args>
  bool InterceptFunction(Ret (*old_function)(Args...),
                         Ret (*new_function)(Args...),
                         Ret (**callback_function)(Args...),
                         std::string *error_message = nullptr);

  template <typename Ret, typename... Args>
  bool InterceptFunction(const char *symbol_name, Ret (*new_function)(Args...),
                         Ret (**callback_function)(Args...),
                         std::string *error_message = nullptr);

  // Template class to define the type of the replacement function for
  // intercepting a function with the given signature. The replacement function
  // needs the same arguments as the original function with an additional
  // argument what is a function poinetr with the type of the intercepted
  // functions (C++ member functions are treated as free functions with this as
  // the first argument).

  template <typename DATA, typename FUN_TYPE>
  struct CallbackSignature;

  template <typename DATA, typename RET, typename... ARGS>
  struct CallbackSignature<DATA, RET(ARGS...)> {
    using type = RET (*)(DATA, RET (*)(ARGS...), ARGS...);
  };

  template <typename DATA, typename FUN_TYPE,
            typename CallbackSignature<DATA, FUN_TYPE>::type FUN,
            size_t FUN_COUNT>
  bool InterceptMultipleFunction(
      const std::array<std::pair<DATA, std::string>, FUN_COUNT> &functions,
      std::string *error_message = nullptr);

 private:
  void *interceptor_;

  // Helper method for collecting the error messages into a string stream
  static void ErrorCollector(void *baton, const char *message) {
    std::ostringstream *oss = static_cast<std::ostringstream *>(baton);
    (*oss) << message << '\n';
  }

  // The following helper classes are used for implementing
  // InterceptMultipleFunction using heavy template metaprogramming. The goal
  // of the metaprogramming is to generate a unique function stub for every
  // symbol passed in to InterceptMultiple function so we have a unique jump
  // target for each intercepted function what can add the information required
  // to execute the callback (address of the compensation function) to the
  // actual intercepting function. Without the unique jump targets it would be
  // impossible to call the original function from the replacement function
  // what would mean thet replacement functions couldn't be shared between
  // intercepted symbols.

  template <typename DATA, size_t N, typename RET, typename... ARGS>
  struct SignleFunctionInterceptor {
    template <RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...)>
    static bool Impl(Interceptor &inteptor, const char *symbol_name, DATA data,
                     std::string *error_message);

    template <RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...)>
    static RET TrampolineFunction(ARGS... args);

    static DATA s_data;
    static RET (*s_callback)(ARGS...);
  };

  template <typename DATA, typename FUN_TYPE,
            typename CallbackSignature<DATA, FUN_TYPE>::type FUN,
            size_t FUN_COUNT, size_t N>
  struct MultiFunctionInterceptor;

  template <typename DATA, typename RET, typename... ARGS,
            RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...), size_t FUN_COUNT,
            size_t N>
  struct MultiFunctionInterceptor<DATA, RET(ARGS...), FUN, FUN_COUNT, N> {
    static bool Impl(
        Interceptor &interceptor,
        const std::array<std::pair<DATA, std::string>, FUN_COUNT> &functions,
        std::string *error_message);
  };
};

template <typename Ret, typename... Args>
bool Interceptor::InterceptFunction(Ret (*old_function)(Args...),
                                    Ret (*new_function)(Args...),
                                    Ret (**callback_function)(Args...),
                                    std::string *error_message) {
  std::ostringstream error_oss;
  void *error_callback_baton = &error_oss;
  void (*error_callback)(void *, const char *) =
      error_message ? &ErrorCollector : nullptr;
  bool res =
      ::InterceptFunction(interceptor_, reinterpret_cast<void *>(old_function),
                          reinterpret_cast<void *>(new_function),
                          reinterpret_cast<void **>(callback_function),
                          error_callback, error_callback_baton);
  if (error_message) *error_message = error_oss.str();
  return res;
}

template <typename Ret, typename... Args>
bool Interceptor::InterceptFunction(const char *symbol_name,
                                    Ret (*new_function)(Args...),
                                    Ret (**callback_function)(Args...),
                                    std::string *error_message) {
  std::ostringstream error_oss;
  void *error_callback_baton = &error_oss;
  void (*error_callback)(void *, const char *) =
      error_message ? &ErrorCollector : nullptr;
  bool res = ::InterceptSymbol(interceptor_, symbol_name,
                               reinterpret_cast<void *>(new_function),
                               reinterpret_cast<void **>(callback_function),
                               error_callback, error_callback_baton);
  if (error_message) *error_message = error_oss.str();
  return res;
}

template <typename DATA, typename FUN_TYPE,
          typename Interceptor::CallbackSignature<DATA, FUN_TYPE>::type FUN,
          size_t FUN_COUNT>
bool Interceptor::InterceptMultipleFunction(
    const std::array<std::pair<DATA, std::string>, FUN_COUNT> &functions,
    std::string *error_message) {
  if (error_message) error_message->clear();
  return MultiFunctionInterceptor<DATA, FUN_TYPE, FUN, FUN_COUNT,
                                  FUN_COUNT>::Impl(*this, functions,
                                                   error_message);
}

template <typename DATA, size_t N, typename RET, typename... ARGS>
template <RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...)>
RET Interceptor::SignleFunctionInterceptor<
    DATA, N, RET, ARGS...>::TrampolineFunction(ARGS... args) {
  return FUN(s_data, s_callback, std::forward<ARGS>(args)...);
}

template <typename DATA, size_t N, typename RET, typename... ARGS>
template <RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...)>
bool Interceptor::SignleFunctionInterceptor<DATA, N, RET, ARGS...>::Impl(
    Interceptor &interceptor, const char *symbol_name, DATA data,
    std::string *error_message) {
  s_data = data;
  return interceptor.InterceptFunction(symbol_name, &TrampolineFunction<FUN>,
                                       &s_callback, error_message);
}

template <typename DATA, typename RET, typename... ARGS,
          RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...), size_t FUN_COUNT,
          size_t N>
bool Interceptor::
    MultiFunctionInterceptor<DATA, RET(ARGS...), FUN, FUN_COUNT, N>::Impl(
        Interceptor &interceptor,
        const std::array<std::pair<DATA, std::string>, FUN_COUNT> &functions,
        std::string *error_message) {
  bool res =
      MultiFunctionInterceptor<DATA, RET(ARGS...), FUN, FUN_COUNT, N - 1>::Impl(
          interceptor, functions, error_message);
  if (!functions[N - 1].second.empty()) {
    std::string temp_str;
    res &= SignleFunctionInterceptor<DATA, N - 1, RET, ARGS...>::template Impl<
        FUN>(interceptor, functions[N - 1].second.c_str(),
             functions[N - 1].first, error_message ? &temp_str : nullptr);
    if (error_message && !temp_str.empty()) error_message->append(temp_str);
  }
  return res;
}

template <typename DATA, typename RET, typename... ARGS,
          RET (*FUN)(DATA, RET (*)(ARGS...), ARGS...), size_t FUN_COUNT>
struct Interceptor::MultiFunctionInterceptor<DATA, RET(ARGS...), FUN, FUN_COUNT,
                                             0> {
  static bool Impl(
      Interceptor &interceptor,
      const std::array<std::pair<DATA, std::string>, FUN_COUNT> &functions,
      std::string *error_message) {
    return true;
  }
};

template <typename DATA, size_t N, typename RET, typename... ARGS>
RET (*Interceptor::SignleFunctionInterceptor<
    DATA, N, RET, ARGS...>::s_callback)(ARGS...) = nullptr;

template <typename DATA, size_t N, typename RET, typename... ARGS>
DATA Interceptor::SignleFunctionInterceptor<DATA, N, RET, ARGS...>::s_data;

#endif  // INTERCEPTOR_INTERCEPTOR_H_
