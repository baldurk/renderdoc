/*****************************************************************************\

    Copyright © 2018, Intel Corporation

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.

    File Name:  metrics_discovery_api.h

    Abstract:   Interface for metrics discovery DLL

    Notes:

\*****************************************************************************/
#include <stdint.h>

#ifndef __METRICS_DISCOVERY_H_
#define __METRICS_DISCOVERY_H_

#ifdef _MSC_VER
  #define MD_STDCALL __stdcall
#else
  #define MD_STDCALL
#endif // _MSC_VER

namespace MetricsDiscovery
{
//*****************************************************************************/
// API major version number:
//*****************************************************************************/
    typedef enum EMD_API_MAJOR_VERSION
    {
        MD_API_MAJOR_NUMBER_1       = 1,
        MD_API_MAJOR_NUMBER_CURRENT = MD_API_MAJOR_NUMBER_1,
        MD_API_MAJOR_NUMBER_CEIL    = 0xFFFFFFFF

    } MD_API_MAJOR_VERSION;

//*****************************************************************************/
// API minor version number:
//*****************************************************************************/
    typedef enum EMD_API_MINOR_VERSION
    {
        MD_API_MINOR_NUMBER_0       = 0,
        MD_API_MINOR_NUMBER_1       = 1,         // CalculationAPI
        MD_API_MINOR_NUMBER_2       = 2,         // OverridesAPI
        MD_API_MINOR_NUMBER_3       = 3,         // BatchBuffer Sampling (aka DMA Sampling)
        MD_API_MINOR_NUMBER_4       = 4,         // GT dependent MetricSets
        MD_API_MINOR_NUMBER_5       = 5,         // MaxValue calculation for CalculationAPI
        MD_API_MINOR_NUMBER_CURRENT = MD_API_MINOR_NUMBER_5,
        MD_API_MINOR_NUMBER_CEIL    = 0xFFFFFFFF

    } MD_API_MINOR_VERSION;

//*****************************************************************************/
// API build number:
//*****************************************************************************/
    #define MD_API_BUILD_NUMBER_CURRENT 94

//*****************************************************************************/
// Completion codes:
//*****************************************************************************/
    typedef enum ECompletionCode
    {
        CC_OK                      = 0,
        CC_READ_PENDING            = 1,
        CC_ALREADY_INITIALIZED     = 2,
        CC_STILL_INITIALIZED       = 3,
        CC_CONCURRENT_GROUP_LOCKED = 4,
        CC_WAIT_TIMEOUT            = 5,
        CC_TRY_AGAIN               = 6,
        CC_INTERRUPTED             = 7,
        // ...
        CC_ERROR_INVALID_PARAMETER = 40,
        CC_ERROR_NO_MEMORY         = 41,
        CC_ERROR_GENERAL           = 42,
        CC_ERROR_FILE_NOT_FOUND    = 43,
        CC_ERROR_NOT_SUPPORTED     = 44,
        // ...
        CC_LAST_1_0                = 45

    } TCompletionCode;


/* Forward declarations */

//*******************************************************************************/
// Abstract interface for the GPU metrics root object.
//*******************************************************************************/
    class IMetricsDevice_1_0;
    class IMetricsDevice_1_1;
    class IMetricsDevice_1_2;
    class IMetricsDevice_1_5;

//*******************************************************************************/
// Abstract interface for Metrics Device overrides.
//*******************************************************************************/
    class IOverride_1_2;

//*******************************************************************************/
// Abstract interface for the metrics groups that can be collected concurrently
// to another group.
//*******************************************************************************/
    class IConcurrentGroup_1_0;
    class IConcurrentGroup_1_1;
    class IConcurrentGroup_1_5;

//*******************************************************************************/
// Abstract interface for the metric sets mapping to different HW configuration
// that should be used exclusively to each other metric set in the concurrent
// group.
//*******************************************************************************/
    class IMetricSet_1_0;
    class IMetricSet_1_1;
    class IMetricSet_1_4;
    class IMetricSet_1_5;

//*******************************************************************************/
// Abstract interface for the metric that is sampled.
//*******************************************************************************/
    class IMetric_1_0;

//*******************************************************************************/
// Abstract interface for the measurement information (report reason, etc.).
//*******************************************************************************/
    class IInformation_1_0;

//*******************************************************************************/
// Abstract interface for the metric read and normalization equation.
//*******************************************************************************/
    class IEquation_1_0;


//*******************************************************************************/
// Value types:
//*******************************************************************************/
    typedef enum EValueType
    {
        VALUE_TYPE_UINT32,
        VALUE_TYPE_UINT64,
        VALUE_TYPE_FLOAT,
        VALUE_TYPE_BOOL,
        VALUE_TYPE_CSTRING,
        // ...
        VALUE_TYPE_LAST,

    } TValueType;

//*******************************************************************************/
// Typed value:
//*******************************************************************************/
    typedef struct STypedValue_1_0
    {
        TValueType ValueType;
        union {
            uint32_t ValueUInt32;
            uint64_t ValueUInt64;
            struct
            {
                uint32_t Low;
                uint32_t High;
            } ValueUInt64Fields;
            float    ValueFloat;
            bool     ValueBool;
            char*    ValueCString;
        };

    } TTypedValue_1_0;

//*******************************************************************************/
// Global symbol:
//     Global symbols will be available to describe SKU specific information.
//     Example global symbols:
//     "EuCoresTotalCount", "EuThreadsCount", "EuSlicesTotalCount", "EuSubslicesTotalCount",
//     "SamplersTotalCount", "PciDeviceId", "NumberOfShadingUnits", "GpuTimestampFrequency",
//     "MaxTimestamp", "GpuMinFrequencyMHz", "GpuMaxFrequencyMHz"
//*******************************************************************************/
    typedef struct SGlobalSymbol_1_0
    {
        const char*     SymbolName;
        TTypedValue_1_0 SymbolTypedValue;

    } TGlobalSymbol_1_0;

//*******************************************************************************/
// Global parameters of Metrics Device:
//*******************************************************************************/
    typedef struct SMetricsDeviceParams_1_0
    {
        // API version
        struct SApiVersion
        {
            uint32_t MajorNumber;
            uint32_t MinorNumber;
            uint32_t BuildNumber;
        } Version;

        uint32_t ConcurrentGroupsCount;

        uint32_t GlobalSymbolsCount;
        uint32_t DeltaFunctionsCount;
        uint32_t EquationElementTypesCount;
        uint32_t EquationOperationsCount;

        const char*  DeviceName;

    } TMetricsDeviceParams_1_0;

//*******************************************************************************/
// Global parameters of Metrics Device 1.2:
//*******************************************************************************/
    typedef struct SMetricsDeviceParams_1_2 : public SMetricsDeviceParams_1_0
    {
        uint32_t OverrideCount;

    } TMetricsDeviceParams_1_2;

//*******************************************************************************/
// Metric API types:
//*******************************************************************************/
    typedef enum EMetricApiType
    {
        API_TYPE_IOSTREAM   = 0x00000001,        // API independent method
        API_TYPE_DX9        = 0x00000002,
        API_TYPE_DX10       = 0x00000004,
        API_TYPE_DX11       = 0x00000008,
        API_TYPE_OGL        = 0x00000010,
        API_TYPE_OGL4_X     = 0x00000020,
        API_TYPE_OCL        = 0x00000040,
        API_TYPE_MEDIA      = 0x00000080,        // Only option would be using DmaBuffer sampling
        API_TYPE_DX12       = 0x00000100,
        API_TYPE_BBSTREAM   = 0x00000200,
        API_TYPE_VULKAN     = 0x00000400,
        API_TYPE_RESERVED   = 0x00000800,
        API_TYPE_ALL        = 0xffffffff

    } TMetricApiType;

//*******************************************************************************/
// Measurement types:
//*******************************************************************************/
    typedef enum EMeasurementType
    {
        MEASUREMENT_TYPE_SNAPSHOT_IO    = 0x00000001,
        MEASUREMENT_TYPE_SNAPSHOT_QUERY = 0x00000002,
        MEASUREMENT_TYPE_DELTA_QUERY    = 0x00000004,
        MEASUREMENT_TYPE_ALL            = 0x0000ffff,

    } TMeasurementType;

//*******************************************************************************/
// Usage flags:
//*******************************************************************************/
    typedef enum EMetricUsageFlag
    {
        USAGE_FLAG_OVERVIEW   = 0x00000001,      // GPU system overview metric
                                                 // Useful for high level workload characterization
        USAGE_FLAG_INDICATE   = 0x00000002,      // Metric indicating a performance problem
                                                 // Useful when comparing with threshold
        USAGE_FLAG_CORRELATE  = 0x00000004,      // Metric correlating with performance problem
                                                 // Useful for proving to false only
        //...
        USAGE_FLAG_SYSTEM     = 0x00000020,      // Metric useful at system level
        USAGE_FLAG_FRAME      = 0x00000040,      // Metric useful at frame level
        USAGE_FLAG_BATCH      = 0x00000080,      // Metric useful at batch level
        USAGE_FLAG_DRAW       = 0x00000100,      // Metric useful at draw level

        // ...
        USAGE_FLAG_TIER_1     = 0x00000400,
        USAGE_FLAG_TIER_2     = 0x00000800,
        USAGE_FLAG_TIER_3     = 0x00001000,
        USAGE_FLAG_TIER_4     = 0x00002000,
        USAGE_FLAG_GLASS_JAW  = 0x00004000,

        USAGE_FLAG_ALL        = 0x0000ffff,

    } TMetricUsageFlag;

//*******************************************************************************/
// Sampling types:
//*******************************************************************************/
    typedef enum ESamplingType
    {
        SAMPLING_TYPE_OA_TIMER      = 0x00000001,
        SAMPLING_TYPE_OA_EVENT      = 0x00000002,
        SAMPLING_TYPE_GPU_QUERY     = 0x00000004,
        SAMPLING_TYPE_DMA_BUFFER    = 0x00000008,// Possible future extension for media
        SAMPLING_TYPE_ALL           = 0x0000ffff,

    } TSamplingType;

//*******************************************************************************/
// Metric categories:
//*******************************************************************************/
    typedef enum EMetricCategory
    {
        GPU_RENDER  = 0x0001,
        GPU_COMPUTE = 0x0002,
        GPU_MEDIA   = 0x0004,
        GPU_GENERIC = 0x0008,                    // Does not belong to any specific category like memory traffic

    } TMetricCategory;

//*******************************************************************************/
// IoStream read flags:
//*******************************************************************************/
    typedef enum EIoReadFlag
    {
        IO_READ_FLAG_DROP_OLD_REPORTS    = 0x00000001,
        IO_READ_FLAG_GET_CONTEXT_ID_TAGS = 0x00000002,

    } TIoReadFlag;

//*******************************************************************************/
// Override modes:
//*******************************************************************************/
    typedef enum EOverrideMode
    {
        OVERRIDE_MODE_GLOBAL = 0x0001,
        OVERRIDE_MODE_LOCAL  = 0x0002,

    } TOverrideMode;

//*******************************************************************************/
// Global parameters of Concurrent Group:
//*******************************************************************************/
    typedef struct SConcurrentGroupParams_1_0
    {
        const char*         SymbolName;          // For example "PerfMon" or "OA" or "PipeStats"
        const char*         Description;         // For example "PerfMon and ODLAT Uncore ring counters"

        uint32_t            MeasurementTypeMask;

        uint32_t            MetricSetsCount;
        uint32_t            IoMeasurementInformationCount;
        uint32_t            IoGpuContextInformationCount;

    } TConcurrentGroupParams_1_0;

//*******************************************************************************/
// Global parameters of an Override:
//*******************************************************************************/
    typedef struct SOverrideParams_1_2
    {
        const char*         SymbolName;          // For example "FrequencyOverride"
        const char*         Description;         // For example "Overrides device GPU frequency with a static value."

        uint32_t            ApiMask;
        uint32_t            PlatformMask;

        uint32_t            OverrideModeMask;

    } TOverrideParams_1_2;

//*******************************************************************************/
// Base params of SetOverride method:
//*******************************************************************************/
    typedef struct SSetOverrideParams_1_2
    {
        bool Enable;

    } TSetOverrideParams_1_2;

//*******************************************************************************/
// Frequency override specific SetOverride params:
//*******************************************************************************/
    typedef struct SSetFrequencyOverrideParams_1_2 : SSetOverrideParams_1_2
    {
        uint32_t FrequencyMhz;
        uint32_t Pid;

    } TSetFrequencyOverrideParams_1_2;


//*******************************************************************************/
// Query override specific SetOverride params:
//*******************************************************************************/
    typedef struct SSetQueryOverrideParams_1_2 : SSetOverrideParams_1_2
    {
        uint32_t Period;    // Nanoseconds

    } TSetQueryOverrideParams_1_2;

//*******************************************************************************/
// Driver override params:
//*******************************************************************************/
    typedef struct SSetDriverOverrideParams_1_2 : SSetOverrideParams_1_2
    {
        uint32_t Value;

    } SSetDriverOverrideParams_1_2;

//*******************************************************************************/
// API specific id:
//*******************************************************************************/
    typedef struct SApiSpecificId_1_0
    {
        uint32_t     D3D9QueryId;                // D3D9 Query ID
        uint32_t     D3D9Fourcc;                 // D3D9 FourCC
        uint32_t     D3D1XQueryId;               // D3D1X Query ID
        uint32_t     D3D1XDevDependentId;        // D3D1X device dependent counter ID
        const char*  D3D1XDevDependentName;      // Device dependent counter name
        uint32_t     OGLQueryIntelId;            // Intel OGL query extension ID
        const char*  OGLQueryIntelName;          // Intel OGL query extension name
        uint32_t     OGLQueryARBTargetId;        // ARB OGL Query Target ID
        uint32_t     OCL;                        // OCL configuration ID
        uint32_t     HwConfigId;                 // Config ID for IO stream
        uint32_t     placeholder[1];

    } TApiSpecificId_1_0;

//*******************************************************************************/
// Global parameters of Metric set:
//*******************************************************************************/
    typedef struct SMetricSetParams_1_0
    {
        const char*         SymbolName;          // For example "Dx11Tessellation"
        const char*         ShortName;           // For example "DX11 Tessellation Metrics Set"

        uint32_t            ApiMask;
        uint32_t            CategoryMask;

        uint32_t            RawReportSize;       // As in HW
        uint32_t            QueryReportSize;     // As in Query API

        uint32_t            MetricsCount;
        uint32_t            InformationCount;
        uint32_t            ComplementarySetsCount;

        TApiSpecificId_1_0  ApiSpecificId;

        uint32_t            PlatformMask;
        //...

    } TMetricSetParams_1_0;

//*******************************************************************************/
// GT differenced MetricSet params:
//*******************************************************************************/
    typedef struct SMetricSetParams_1_4 : SMetricSetParams_1_0
    {
        uint32_t GtMask;

    } TMetricSetParams_1_4;

//*******************************************************************************/
// Metric result types:
//*******************************************************************************/
    typedef enum EMetricResultType
    {
        RESULT_UINT32,
        RESULT_UINT64,
        RESULT_BOOL,
        RESULT_FLOAT,
        // ...
        RESULT_LAST

    } TMetricResultType;

//*******************************************************************************/
// Metric types:
//*******************************************************************************/
    typedef enum EMetricType
    {
        METRIC_TYPE_DURATION,
        METRIC_TYPE_EVENT,
        METRIC_TYPE_EVENT_WITH_RANGE,
        METRIC_TYPE_THROUGHPUT,
        METRIC_TYPE_TIMESTAMP,
        METRIC_TYPE_FLAG,
        METRIC_TYPE_RATIO,
        METRIC_TYPE_RAW,
        // ...
        METRIC_TYPE_LAST

    } TMetricType;

//*******************************************************************************/
// Information types:
//*******************************************************************************/
    typedef enum EInformationType
    {
        INFORMATION_TYPE_REPORT_REASON,
        INFORMATION_TYPE_VALUE,
        INFORMATION_TYPE_FLAG,
        INFORMATION_TYPE_TIMESTAMP,
        INFORMATION_TYPE_CONTEXT_ID_TAG,
        INFORMATION_TYPE_SAMPLE_PHASE,
        INFORMATION_TYPE_GPU_NODE,
        // ...
        INFORMATION_TYPE_LAST

    } TInformationType;

//*******************************************************************************/
// Report reasons:
//*******************************************************************************/
    typedef enum EReportReason
    {
        REPORT_REASON_UNDEFINED                 = 0x0000,
        REPORT_REASON_INTERNAL_TIMER            = 0x0001,
        REPORT_REASON_INTERNAL_TRIGGER1         = 0x0002,
        REPORT_REASON_INTERNAL_TRIGGER2         = 0x0004,
        REPORT_REASON_INTERNAL_CONTEXT_SWITCH   = 0x0008,
        REPORT_REASON_INTERNAL_GO               = 0x0010,
        REPORT_REASON_INTERNAL_FREQUENCY_CHANGE = 0x0020,
        REPORT_REASON_QUERY_DEFAULT             = 0x0100,
        REPORT_REASON_QUERY_INTERNAL_RESOLVE    = 0x0200,
        REPORT_REASON_QUERY_INTERNAL_CLEAR      = 0x0400,

    } TReportReason;

//*******************************************************************************/
// Sample phase:
//*******************************************************************************/
    typedef enum ESamplePhase
    {
        SAMPLE_PHASE_END,
        SAMPLE_PHASE_BEGIN,
        // ...
        SAMPLE_PHASE_LAST

    } TSamplePhase;

//*******************************************************************************/
// Gpu Node:
//*******************************************************************************/
    typedef enum EInformationGpuNode
    {
        INFORMATION_GPUNODE_3D       = 0,        // Available by default on all platform
        INFORMATION_GPUNODE_VIDEO    = 1,        // Available on CTG+
        INFORMATION_GPUNODE_BLT      = 2,        // Available on GT
        INFORMATION_GPUNODE_VE       = 3,        // Available on HSW+ (VideoEnhancement)
        INFORMATION_GPUNODE_VCS2     = 4,        // Available on BDW+ GT3+
        INFORMATION_GPUNODE_REAL_MAX = 5,        // All nodes beyond this are virtual nodes - they don't have an actual GPU engine
        // ...
        INFORMATION_GPUNODE_LAST

    } TInformationGpuNode;

//*******************************************************************************/
// Hardware unit types:
//*******************************************************************************/
    typedef enum EHwUnitType
    {
        HW_UNIT_GPU,
        HW_UNIT_SLICE,
        HW_UNIT_SUBSLICE,
        HW_UNIT_SUBSLICE_BANK,
        HW_UNIT_EU_UNIT,
        HW_UNIT_UNCORE,
        // ...
        HW_UNIT_LAST

    } THwUnitType;

//*******************************************************************************/
// Delta function types:
//*******************************************************************************/
    typedef enum EDeltaFunctionType
    {
        DELTA_FUNCTION_NULL = 0,
        DELTA_N_BITS,
        DELTA_BOOL_OR,                           // Logic OR - good for exceptions
        DELTA_BOOL_XOR,                          // Logic XOR - good to check if bits were changed
        DELTA_GET_PREVIOUS,                      // Preserve previous value
        DELTA_GET_LAST,                          // Preserve last value
        DELTA_NS_TIME,                           // Delta for nanosecond timestamps (GPU timestamp wraps at 32 bits but was value multiplied by 80)
        DELTA_FUNCTION_LAST_1_0

    } TDeltaFunctionType;

//*******************************************************************************/
// Delta function:
//*******************************************************************************/
    typedef struct SDeltaFunction_1_0
    {
        TDeltaFunctionType   FunctionType;
        union {
            uint32_t         BitsCount;          // Used for DELTA_N_BITS to specify bits count
        };

    } TDeltaFunction_1_0;

//*******************************************************************************/
// Equation element types:
//*******************************************************************************/
    typedef enum EEquationElementType
    {
        EQUATION_ELEM_OPERATION,                 // See TEquationOperation enumeration

        EQUATION_ELEM_RD_BITFIELD,
        EQUATION_ELEM_RD_UINT8,
        EQUATION_ELEM_RD_UINT16,
        EQUATION_ELEM_RD_UINT32,
        EQUATION_ELEM_RD_UINT64,
        EQUATION_ELEM_RD_FLOAT,

        // Extended RD operation
        EQUATION_ELEM_RD_40BIT_CNTR,             // Assemble 40 bit counter that is in two locations, result in unsigned integer 64b

        EQUATION_ELEM_IMM_UINT64,
        EQUATION_ELEM_IMM_FLOAT,
        EQUATION_ELEM_SELF_COUNTER_VALUE,        // Defined by $Self token, the UINT64 result of DeltaFunction for IO or QueryReadEquation
        EQUATION_ELEM_GLOBAL_SYMBOL,             // Defined by $"SymbolName", available in MetricsDevice SymbolTable
        EQUATION_ELEM_LOCAL_COUNTER_SYMBOL,      // Defined by $"SymbolName", refers to counter delta value in the local set
        EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL,  // Defined by concatenated string of $"setSymbolName/SymbolName", refers to counter
                                                 // Delta value in the other set

        EQUATION_ELEM_LOCAL_METRIC_SYMBOL,       // Defined by $$"SymbolName", refers to metric normalized value in the local set
        EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL,   // Defined by concatenated string of $$"setSymbolName/SymbolName", refers to metric
                                                 // Normalized value in the other set

        EQUATION_ELEM_INFORMATION_SYMBOL,        // Defined by i$"SymbolName", refers to information value type only

        // Extended types - standard normalization functions
        EQUATION_ELEM_STD_NORM_GPU_DURATION,     // Action is $Self $GpuCoreClocks FDIV 100 FMUL
        EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION, // Action is $Self $GpuCoreClocks $EuCoresTotalCount UMUL FDIV 100 FMUL

        EQUATION_ELEM_LAST_1_0

    } TEquationElementType;

//*******************************************************************************/
// Equation operations:
//*******************************************************************************/
    typedef enum EEquationOperation
    {
        EQUATION_OPER_RSHIFT,                    // 64b unsigned integer right shift
        EQUATION_OPER_LSHIFT,                    // 64b unsigned integer left shift
        EQUATION_OPER_AND,                       // Bitwise AND of two unsigned integers, 64b each
        EQUATION_OPER_OR,                        // Bitwise OR of two unsigned integers, 64b each
        EQUATION_OPER_XOR,                       // Bitwise XOR of two unsigned integers, 64b each
        EQUATION_OPER_XNOR,                      // Bitwise XNOR of two unsigned integers, 64b each
        EQUATION_OPER_AND_L,                     // Logical AND (C-like "&&") of two unsigned integers, 64b each, result is true(1) if both values are true(greater than 0)
        EQUATION_OPER_EQUALS,                    // Equality (C-like "==") of two unsigned integers, 64b each, result is true(1) or false(0)
        EQUATION_OPER_UADD,                      // Unsigned integer add, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
        EQUATION_OPER_USUB,                      // Unsigned integer subtract, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
        EQUATION_OPER_UMUL,                      // Unsigned integer mul, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
        EQUATION_OPER_UDIV,                      // Unsigned integer div, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
        EQUATION_OPER_FADD,                      // Floating point add, arguments are casted to be 32b floating points, result is a 32b float
        EQUATION_OPER_FSUB,                      // Floating point subtract, arguments are casted to be 32b floating points, result is a 32b float
        EQUATION_OPER_FMUL,                      // Floating point multiply, arguments are casted to be 32b floating points, result is a 32b float
        EQUATION_OPER_FDIV,                      // Floating point divide, arguments are casted to be 32b floating points, result is a 32b float

        EQUATION_OPER_UGT,                       // 64b unsigned integers comparison of is greater than, result is bool true(1) or false(0)
        EQUATION_OPER_ULT,                       // 64b unsigned integers comparison of is less than, result is bool true(1) or false(0)
        EQUATION_OPER_UGTE,                      // 64b unsigned integers comparison of is greater than or equal, result is bool true(1) or false(0)
        EQUATION_OPER_ULTE,                      // 64b unsigned integers comparison of is less than or equal, result is bool true(1) or false(0)

        EQUATION_OPER_FGT,                       // 32b floating point numbers comparison of is greater than, result is bool true(1) or false(0)
        EQUATION_OPER_FLT,                       // 32b floating point numbers comparison of is less than, result is bool true(1) or false(0)
        EQUATION_OPER_FGTE,                      // 32b floating point numbers comparison of is greater than or equal, result is bool true(1) or false(0)
        EQUATION_OPER_FLTE,                      // 32b floating point numbers comparison of is less than or equal, result is bool true(1) or false(0)

        EQUATION_OPER_UMIN,                      // Unsigned integer MIN function, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
        EQUATION_OPER_UMAX,                      // Unsigned integer MAX function, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b

        EQUATION_OPER_FMIN,                      // Floating point MIN function, arguments are casted to be 32b floating points, result is a 32b float
        EQUATION_OPER_FMAX,                      // Floating point MAX function, arguments are casted to be 32b floating points, result is a 32b float

        EQUATION_OPER_LAST_1_0

    } TEquationOperation;

//*******************************************************************************/
// Read params:
//*******************************************************************************/
    typedef struct SReadParams_1_0
    {
        uint32_t                 ByteOffset;
        uint32_t                 BitOffset;
        uint32_t                 BitsCount;
        uint32_t                 ByteOffsetExt;

    } TReadParams_1_0;

//*******************************************************************************/
// Equation element:
//*******************************************************************************/
    typedef struct SEquationElement_1_0
    {
        TEquationElementType     Type;
        union
        {
            uint64_t             ImmediateUInt64;
            float                ImmediateFloat;
            TEquationOperation   Operation;
            TReadParams_1_0      ReadParams;
        };
        char*                    SymbolName;

    } TEquationElement_1_0;

/*****************************************************************************\

Class:
    IEquation_1_0

Description:
    Abstract interface for the equation object.

\*****************************************************************************/
    class IEquation_1_0
    {
    public:
        virtual ~IEquation_1_0();

        virtual uint32_t              GetEquationElementsCount( void );
        virtual TEquationElement_1_0* GetEquationElement( uint32_t index );
    };

//*******************************************************************************/
// Global parameters of Metric:
//*******************************************************************************/
    typedef struct SMetricParams_1_0
    {
        uint32_t                IdInSet;         // Position in the set
        uint32_t                GroupId;         // Specific metric group id
        const char*             SymbolName;      // Symbol name, used in equations
        const char*             ShortName;       // Consistent metric name, not changed platform to platform
        const char*             GroupName;       // VertexShader for example
        const char*             LongName;        // Hint about the metric shown to users

        const char*             DxToOglAlias;    // To replace DX pixels with OGL fragments

        uint32_t                UsageFlagsMask;
        uint32_t                ApiMask;

        TMetricResultType       ResultType;
        const char*             MetricResultUnits;
        TMetricType             MetricType;

        uint64_t                LowWatermark;    // Low watermark for hotspot indication (USAGE_FLAG_INDICATE only)
        uint64_t                HighWatermark;   // High watermark for hotspot indication (USAGE_FLAG_INDICATE only)

        THwUnitType             HwUnitType;

        // Read equation specification for IO stream (accessing raw values potentially spread in report in several locations)
        IEquation_1_0*          IoReadEquation;
        // Read equation specification for query (accessing calculated delta values)
        IEquation_1_0*          QueryReadEquation;

        TDeltaFunction_1_0      DeltaFunction;

        // Normalization equation to get normalized value to bytes transfered or to a percentage of utilization
        IEquation_1_0*          NormEquation;

        // To calculate metrics max value as a function of other metrics and device parameters (e.g. 100 for percentage)
        IEquation_1_0*          MaxValueEquation;

    } TMetricParams_1_0;

//*******************************************************************************/
// Global parameters of Information:
//*******************************************************************************/
    typedef struct SInformationParams_1_0
    {
        uint32_t                IdInSet;         // Position in the set
        const char*             SymbolName;      // Symbol name, used in equations
        const char*             ShortName;       // Consistent name, not changed platform to platform
        const char*             GroupName;       // Some more global context of the information
        const char*             LongName;        // Hint about the information shown to users

        uint32_t                ApiMask;

        TInformationType        InfoType;
        const char*             InfoUnits;

        // Read equation specification for IO stream (accessing raw values potentially spread in report in several locations)
        IEquation_1_0*          IoReadEquation;
        // Read equation specification for query (accessing calculated delta values)
        IEquation_1_0*          QueryReadEquation;

        TDeltaFunction_1_0      OverflowFunction;

    } TInformationParams_1_0;

/*****************************************************************************\

Class:
    IInformation_1_0

Description:
    Abstract interface for the measurement information parameter.

\*****************************************************************************/
    class IInformation_1_0
    {
    public:
        virtual ~IInformation_1_0();

        virtual TInformationParams_1_0* GetParams();
    };

/*****************************************************************************\

Class:
    IMetric_1_0

Description:
    Abstract interface for the metric that is sampled.

\*****************************************************************************/
    class IMetric_1_0
    {
    public:
        virtual ~IMetric_1_0();

        virtual TMetricParams_1_0*    GetParams();
    };

/*****************************************************************************\

Class:
    IMetricSet_1_0

Description:
    Abstract interface for the metric sets mapping to different HW configuration that should be used
    exclusively to each other metric set in the concurrent group.

\*****************************************************************************/
    class IMetricSet_1_0
    {
    public:
        virtual ~IMetricSet_1_0();

        virtual TMetricSetParams_1_0* GetParams( void );

        // To get particular metric
        virtual IMetric_1_0*          GetMetric( uint32_t index );

        // To get particular information about measurement
        virtual IInformation_1_0*     GetInformation( uint32_t index );

        // Below proposal to address multi-passes at the set level
        virtual IMetricSet_1_0*       GetComplementaryMetricSet( uint32_t index );

        // To enable this configuration before query instance is created
        virtual TCompletionCode       Activate( void );

        // To disable this configuration after query instance is created
        virtual TCompletionCode       Deactivate( void );

        // To add an additional custom metric to this set
        virtual IMetric_1_0*          AddCustomMetric(
            const char* symbolName, const char* shortName, const char* groupName, const char* longName, const char* dxToOglAlias,
            uint32_t usageFlagsMask, uint32_t apiMask, TMetricResultType resultType, const char* resultUnits, TMetricType metricType,
            int64_t loWatermark, int64_t hiWatermark, THwUnitType hwType, const char* ioReadEquation, const char* deltaFunction,
            const char* queryReadEquation, const char* normalizationEquation, const char* maxValueEquation, const char* signalName );
    };

/*****************************************************************************\

Class:
    IMetricSet_1_1

Description:
    Updated 1.0 version to use with 1.1 interface version.
    Introduces an ability to calculate metrics from raw data.

    New:
        - SetApiFiltering
        - CalculateMetrics
        - CalculateIoMeasurementInformation

\*****************************************************************************/
    class IMetricSet_1_1 : public IMetricSet_1_0
    {
    public:
        virtual ~IMetricSet_1_1();

        // To filter available metrics/information for the given API. Use TMetricApiType to build the mask.
        virtual TCompletionCode       SetApiFiltering( uint32_t apiMask );

        // To calculate normalized metrics/information from the raw data.
        virtual TCompletionCode       CalculateMetrics( const unsigned char* rawData, uint32_t rawDataSize, TTypedValue_1_0* out,
            uint32_t outSize, uint32_t* outReportCount, bool enableContextFiltering );

        // To calculate additional information for stream measurements.
        virtual TCompletionCode       CalculateIoMeasurementInformation( TTypedValue_1_0* out, uint32_t outSize );
    };

/*****************************************************************************\

Class:
    IMetricSet_1_4

Description:
    Updated 1.1 version to use with 1.4 interface version.
    Extends set params with gtType information.

    Updates:
        - GetParams

\*****************************************************************************/
    class IMetricSet_1_4 : public IMetricSet_1_1
    {
    public:
        virtual ~IMetricSet_1_4();

        virtual TMetricSetParams_1_4* GetParams( void );
    };

/*****************************************************************************\

Class:
    IMetricSet_1_5

Description:
    Updated 1.4 version to use with 1.5 interface version.
    Adds an ability to calculate MaxValueEquations (maximal value) for each metric.
    Param 'enableContextFiltering' becomes deprecated.

    Updates:
        - GetComplementaryMetricSet
        - CalculateMetrics

\*****************************************************************************/
    class IMetricSet_1_5 : public IMetricSet_1_4
    {
    public:
        // Update to 1.5 interface
        virtual IMetricSet_1_5*       GetComplementaryMetricSet( uint32_t index );

        // CalculateMetrics extended with max values calculation.
        // Optional param 'outMaxValues' should have a memory for at least 'MetricCount * RawReportCount' values, can be NULL.
        virtual TCompletionCode       CalculateMetrics( const unsigned char* rawData, uint32_t rawDataSize, TTypedValue_1_0* out,
            uint32_t outSize, uint32_t* outReportCount, TTypedValue_1_0* outMaxValues, uint32_t outMaxValuesSize );
    };

/*****************************************************************************\

Class:
    IConcurrentGroup_1_0

Description:
    Abstract interface for the metrics groups that can be collected concurrently to another group.

\*****************************************************************************/
    class IConcurrentGroup_1_0
    {
    public:
        virtual ~IConcurrentGroup_1_0();

        virtual TConcurrentGroupParams_1_0* GetParams( void );
        virtual IMetricSet_1_0*       GetMetricSet( uint32_t index );

        virtual TCompletionCode       OpenIoStream( IMetricSet_1_0* metricSet, uint32_t processId, uint32_t* nsTimerPeriod, uint32_t* oaBufferSize );
        virtual TCompletionCode       ReadIoStream( uint32_t* reportsCount, char* reportData, uint32_t readFlags );
        virtual TCompletionCode       CloseIoStream( void );
        virtual TCompletionCode       WaitForReports( uint32_t milliseconds );
        virtual IInformation_1_0*     GetIoMeasurementInformation( uint32_t index );
        virtual IInformation_1_0*     GetIoGpuContextInformation( uint32_t index );
    };

/*****************************************************************************\

Class:
    IConcurrentGroup_1_1

Description:
    Updated 1.0 version to use with 1.1 interface version.

    Updates:
        - GetMetricSet

\*****************************************************************************/
    class IConcurrentGroup_1_1 : public IConcurrentGroup_1_0
    {
    public:
        // Update to 1.1 interface
        virtual IMetricSet_1_1*       GetMetricSet( uint32_t index );
    };

/*****************************************************************************\

Class:
    IConcurrentGroup_1_3

Description:
    Updated 1.1 version to use with 1.3 interface version.
    Introduces setting Stream Sampling Type.

    New:
        - SetIoStreamSamplingType

\*****************************************************************************/
    class IConcurrentGroup_1_3 : public IConcurrentGroup_1_1
    {
    public:
        // To set sampling type during IoStream measurements
        virtual TCompletionCode       SetIoStreamSamplingType( TSamplingType type );
    };

/*****************************************************************************\

Class:
    IConcurrentGroup_1_5

Description:
    Updated 1.3 version to use with 1.5 interface version.

    Updates:
        - GetMetricSet

\*****************************************************************************/
    class IConcurrentGroup_1_5 : public IConcurrentGroup_1_3
    {
    public:
        // Update to 1.5 interface
        virtual IMetricSet_1_5*       GetMetricSet( uint32_t index );
    };

/*****************************************************************************\

Class:
    IOverride_1_2

Description:
    Abstract interface for Metrics Device overrides.

\*****************************************************************************/
    class IOverride_1_2
    {
    public:
        virtual ~IOverride_1_2();

        // To get this Override params
        virtual TOverrideParams_1_2*  GetParams( void );

        // To enable/disable this Override
        virtual TCompletionCode       SetOverride( TSetOverrideParams_1_2* params, uint32_t paramsSize );
    };

/*****************************************************************************\

Class:
    IMetricsDevice_1_0

Description:
    Abstract interface for the GPU metrics root object.

\*****************************************************************************/
    class IMetricsDevice_1_0
    {
    public:
        virtual ~IMetricsDevice_1_0();

        // To get MetricsDevice params
        virtual TMetricsDeviceParams_1_0* GetParams( void );

        // Child objects are of IConcurrentGroup
        virtual IConcurrentGroup_1_0* GetConcurrentGroup( uint32_t index );

        // To get GlobalSymbol at the given index
        virtual TGlobalSymbol_1_0*    GetGlobalSymbol( uint32_t index );

        // To get GlobalSymbol with the given name
        virtual TTypedValue_1_0*      GetGlobalSymbolValueByName( const char* name );

        // To get last error from TCompletionCode enum
        virtual TCompletionCode       GetLastError( void );

        // To get both GPU and CPU timestamp at the same time
        virtual TCompletionCode       GetGpuCpuTimestamps( uint64_t* gpuTimestampNs, uint64_t* cpuTimestampNs,
            uint32_t* cpuId );
    };

/*****************************************************************************\

Class:
    IMetricsDevice_1_1

Description:
    Updated 1.0 version to use with 1.1 interface version.

    Updates:
        - GetConcurrentGroup

\*****************************************************************************/
    class IMetricsDevice_1_1 : public IMetricsDevice_1_0
    {
    public:
        // Update to 1.1 interface
        virtual IConcurrentGroup_1_1* GetConcurrentGroup( uint32_t index );
    };

/*****************************************************************************\

Class:
    IMetricsDevice_1_2

Description:
    Updated 1.1 version to use with 1.2 interface version.
    Introduces an interface for getting overrides.

    Updates:
        - GetParams
    New:
        - GetOverride
        - GetOverrideByName

\*****************************************************************************/
    class IMetricsDevice_1_2 : public IMetricsDevice_1_1
    {
    public:
        // Update returned params
        virtual TMetricsDeviceParams_1_2* GetParams( void );

        // To get override at the given index
        virtual IOverride_1_2*        GetOverride( uint32_t index );

        // To get override with the given name
        virtual IOverride_1_2*        GetOverrideByName( const char* symbolName );
    };

/*****************************************************************************\

Class:
    IMetricsDevice_1_5

Description:
    Updated 1.2 version to use with 1.5 interface version.

    Updates:
        - GetConcurrentGroup

\*****************************************************************************/
    class IMetricsDevice_1_5 : public IMetricsDevice_1_2
    {
    public:
        // Update to 1.5 interface
        virtual IConcurrentGroup_1_5* GetConcurrentGroup( uint32_t index );
    };

#ifdef __cplusplus
    extern "C" {
#endif

    // Factory functions
    typedef TCompletionCode (MD_STDCALL *OpenMetricsDevice_fn)(IMetricsDevice_1_5** device);
    typedef TCompletionCode (MD_STDCALL *OpenMetricsDeviceFromFile_fn)(const char* fileName, void* openParams, IMetricsDevice_1_5** device);
    typedef TCompletionCode (MD_STDCALL *CloseMetricsDevice_fn)(IMetricsDevice_1_5* device);
    typedef TCompletionCode (MD_STDCALL *SaveMetricsDeviceToFile_fn)(const char* fileName, void* saveParams, IMetricsDevice_1_5* device);

#ifdef __cplusplus
    }
#endif

};
#endif // __METRICS_DISCOVERY_H_

