/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Samsung Electronics (UK) Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#ifndef LIB_LIZARD_API_H
#define LIB_LIZARD_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif    // End of __cplusplus

struct LizardApi;
typedef void *LizardInstance;
typedef uint32_t LizardCounterId;

enum LizardVersion
{
  LIZARD_VERSION_0_1 = 1,
};

typedef enum {
  LZD_OK = 0,
  LZD_FAILURE,
} LZD_Result;

/**
 * LizardInstance* LZD_Init(const char* host, int port);
 *
 * Initializes a Lizard Instance with the given host:port arguments.
 * The Lizard Instance must be destroyed with the LZD_Destroy method.
 *
 * :param host: IP address of the target gatord.
 * :param port: Port number of the target gatord.
 */
typedef LizardInstance (*LZD_Init_PFN)(const char *host, int port);

/**
 * void LZD_Destroy(LizardInstance** ctx);
 *
 * Destroy the Lizard Instance and sets the `ctx` pointer's value to NULL.
 */
typedef void (*LZD_Destroy_PFN)(LizardInstance ctx);

/**
 * Get the number of the available counters.
 *
 * The LizardIds are in the range of [1, MAX_UINT].
 *
 * :param ctx: LizardInstance created via LZD_Init.
 * :returns: Number of available counters.
 */
typedef uint32_t (*LZD_GetAvailableCountersCount_PFN)(LizardInstance ctx);


struct LizardCounterDescription
{
  LizardCounterId id;
  const char *short_name;
  const char *name;
  const char *title;
  const char *description;
  const char *category;
  double multiplier;
  uint32_t units;
  uint32_t class_type;
  uint32_t result_type;
};

/**
 * Get information about the counter
 *
 * :param ctx: LizardInstance created via LZD_Init.
 * :param id: id of the counter, must be 1 or above.
 * :param lzdDesc: LizardCounterDescription to be populated.
 * :returns: LZD_OK if succeeded
 */
typedef LZD_Result (*LZD_GetCounterDescription_PFN)(LizardInstance ctx, LizardCounterId id,
                                                    LizardCounterDescription *lzdDesc);

/**
 * Enable the counter for capture.
 *
 * :param ctx: Lizard Instance created via LZD_Init.
 * :param id: Id of the counter to enable.
 */
typedef void (*LZD_EnableCounter_PFN)(LizardInstance ctx, LizardCounterId id);

/**
 * Disable the counter for capture.
 *
 * :param ctx: Lizard Instance created via LZD_Init
 * :param id: Id of the counter to disable.
 */
typedef void (*LZD_DisableCounter_PFN)(LizardInstance ctx, LizardCounterId id);

/**
 * Disable all counters for capture.
 *
 * By default all counters are disabled.
 *
 * :param ctx: Lizard Instance created via LZD_Init
 */
typedef void (*LZD_DisableAllCounters_PFN)(LizardInstance ctx);

/**
 * Start capture.
 *
 * The actual capture is performed in a different thread.
 *
 * :param ctx: Lizard Instance created via LZD_Init
 * :returns: LZD_OK if succeeded
 */
typedef LZD_Result (*LZD_StartCapture_PFN)(LizardInstance ctx);

/**
 * Stop capture.
 *
 * :param ctx: Lizard Instance created via LZD_Init
 * :returns: LZD_OK if succeeded
 */
typedef LZD_Result (*LZD_StopCapture_PFN)(LizardInstance ctx);

typedef enum {
  LZD_ABSOLUTE = 1,
  LZD_DELTA = 2,
} LZD_CounterClassType;

/**
 * Get the measured counter value as an integer.
 *
 * :param ctx: Lizard Instance created via LZD_Init
 * :param id: The id of the counter which value is measured.
 * :returns: The measured value.
 */
typedef int64_t (*LZD_ReadCounterInt_PFN)(LizardInstance ctx, LizardCounterId id);

/**
 * Get the measured counter value as a double.
 *
 * :param ctx: Lizard Instance created via LZD_Init
 * :param id: The id of the counter which value is measured.
 * :returns: The measured value.
 */
typedef double (*LZD_ReadCounterDouble_PFN)(LizardInstance ctx, LizardCounterId id);

typedef enum {
  LZD_UNITS_UNKNOWN,
  LZD_UNITS_BYTE,
  LZD_UNITS_CELSIUS,
  LZD_UNITS_HZ,
  LZD_UNITS_PAGES,
  LZD_UNITS_RPM,
  LZD_UNITS_S,
  LZD_UNITS_V,

  LZD_TYPE_INT,
  LZD_TYPE_DOUBLE,
} LZD_CounterAttribute;

struct LizardApi
{
  int struct_size;
  int version;
  LZD_Init_PFN Init;
  LZD_Destroy_PFN Destroy;
  LZD_GetAvailableCountersCount_PFN GetAvailableCountersCount;
  LZD_GetCounterDescription_PFN GetCounterDescription;
  LZD_EnableCounter_PFN EnableCounter;
  LZD_DisableCounter_PFN DisableCounter;
  LZD_DisableAllCounters_PFN DisableAllCounters;

  LZD_StartCapture_PFN StartCapture;
  LZD_StopCapture_PFN StopCapture;

  LZD_ReadCounterInt_PFN ReadCounterInt;
  LZD_ReadCounterDouble_PFN ReadCounterDouble;
};

/**
 * Entry point of the API.
 *
 * To load the api search for the "LoadApi" function symbol and
 * invoke the method with a `LizardApi` pointer to get access to all
 * API functions.
 *
 * Example usage:
 *
 *  void* lib = dlopen("liblizard.so", RTLD_LAZY);
 *  LZD_LoadApi_PFN loadApi = (LZD_LoadApi_PFN)dlsym(lib, "LoadApi");
 *
 *  struct LizardApi* api;
 *
 *  if (loadApi(&api) != LZD_OK) {
 *   // report failure and return
 *  }
 *  if (api->version != LIZARD_VERSION_0_1) {
 *   // report version mismatch and return
 *  }
 *
 *  LizardInstance* ctx = api->Init("127.0.0.1", 8080);
 *
 * :param api: LizardApi struct pointer to initialize the API pointers.
 * :returns: LZD_OK if the counter initialization was ok.
 */
typedef LZD_Result (*LZD_LoadApi_PFN)(struct LizardApi **api);
LZD_Result LoadApi(struct LizardApi **api);

#ifdef __cplusplus
}
#endif    // End of __cplusplus

#endif /* LIB_LIZARD_API_H */
