//==============================================================================
// Copyright (c) 2012-2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  An accessor interface for the GPA_CounterGeneratorBase class
//==============================================================================


#ifndef _GPA_I_COUNTER_ACCESSOR_H_
#define _GPA_I_COUNTER_ACCESSOR_H_

#include <cstdint>
#include <vector>
#include "GPUPerfAPITypes.h"

struct GPA_HardwareCounterDescExt;
class GPA_HWInfo;
class GPA_HardwareCounters;
class GPA_SoftwareCounters;
class GPA_DerivedCounter;
class GPA_CounterResultLocation;

/// Indicates the source or origin of a counter
enum class GPACounterSource : uint32_t
{
    UNKNOWN,        /// Invalid or unknown counter
    PUBLIC,         /// Counter is defined by GPA using other Hardware counters or hardware info
    HARDWARE,       /// Counter comes from the hardware
    SOFTWARE,       /// Counter comes from software (ie, an API-level query)
};

/// Stores the source of the counter and its local index into that family of counters
struct GPACounterSourceInfo
{
    gpa_uint32 m_localIndex;            ///< The local index of the counter
    GPACounterSource m_counterSource;   ///< The source of the counter

    /// Sets the data for
    /// \param localIndex the local index to set
    /// \param source the type to set
    void Set(gpa_uint32 localIndex, GPACounterSource source)
    {
        m_localIndex = localIndex;
        m_counterSource = source;
    }
};

/// An accessor interface for the GPA_CounterGeneratorBase class
class IGPACounterAccessor
{
public:

    /// Set the flags indicating which counters are allowed
    /// \param bAllowPublicCounters flag indicating whether or not public counters are allowed
    /// \param bAllowHardwareCounters flag indicating whether or not hardware counters are allowed
    /// \param bAllowSoftwareCounters flag indicating whether or not software counters are allowed
    virtual void SetAllowedCounters(bool bAllowPublicCounters, bool bAllowHardwareCounters, bool bAllowSoftwareCounters) = 0;

    /// Get the number of available counters
    /// \return the number of available counters
    virtual gpa_uint32 GetNumCounters() const = 0;

    /// Gets a counter's name
    /// \param index The index of a counter, must be between 0 and the value returned from GetNumPublicCounters()
    /// \return The counter name
    virtual const char* GetCounterName(gpa_uint32 index) const = 0;

    /// Gets the category of the specified counter
    /// \param index The index of the counter whose category is needed
    /// \return The category of the specified counter
    virtual const char* GetCounterGroup(gpa_uint32 index) const = 0;

    /// Gets a counter's description
    /// \param index The index of a counter, must be between 0 and the value returned from GetNumPublicCounters()
    /// \return The counter description
    virtual const char* GetCounterDescription(gpa_uint32 index) const = 0;

    /// Gets the data type of a public counter
    /// \param index The index of a counter
    /// \return The data type of the the desired counter
    virtual GPA_Data_Type GetCounterDataType(gpa_uint32 index) const = 0;

    /// Gets the usage type of a public counter
    /// \param index The index of a counter
    /// \return The usage of the the desired counter
    virtual GPA_Usage_Type GetCounterUsageType(gpa_uint32 index) const = 0;

    /// Gets a counter's GPA_UUID
    /// \param index The index of a counter, must be between 0 and the value returned from GetNumPublicCounters()
    /// \return The counter UUID
    virtual GPA_UUID GetCounterUuid(gpa_uint32 index) const = 0;

    /// Gets the supported sample type of a counter
    /// \param index The index of a counter
    /// \return the counter's supported sample type
    virtual GPA_Counter_Sample_Type GetCounterSampleType(gpa_uint32 index) const = 0;

    /// Gets a public counter
    /// \param index The index of the public counter to return
    /// \return A public counter
    virtual const GPA_DerivedCounter* GetPublicCounter(gpa_uint32 index) const = 0;

    /// Gets a hardware counter
    /// \param index The index of a hardware counter to return
    /// \return A hardware counter
    virtual const GPA_HardwareCounterDescExt* GetHardwareCounterExt(gpa_uint32 index) const = 0;

    /// Gets the number of public counters available
    /// \return The number of public counters
    virtual gpa_uint32 GetNumPublicCounters() const = 0;

    /// Gets the internal counters required for the specified public counter index
    /// \param index The index of a public counter
    /// \return A vector of internal counter indices
    virtual std::vector<gpa_uint32> GetInternalCountersRequired(gpa_uint32 index) const = 0;

    /// Computes a public counter value pased on supplied results and hardware info
    /// \param[in] counterIndex The public counter index to calculate
    /// \param[in] results A vector of hardware counter results
    /// \param[in] internalCounterTypes A vector of counter types
    /// \param[inout] pResult The computed counter result
    /// \param[in] pHwInfo Information about the hardware on which the result was generated
    /// \return GPA_STATUS_OK on success, otherwise an error code
    virtual GPA_Status ComputePublicCounterValue(gpa_uint32 counterIndex, std::vector<gpa_uint64*>& results, std::vector<GPA_Data_Type>& internalCounterTypes, void* pResult, const GPA_HWInfo* pHwInfo) const = 0;

    /// Compute a software counter value
    /// \param softwareCounterIndex the index of the counter (within the range of software counters) whose value is needed
    /// \param value the value of the counter
    /// \param[out] pResult the resulting value
    /// \param pHwInfo the hardware info
    virtual void ComputeSWCounterValue(gpa_uint32 softwareCounterIndex, gpa_uint64 value, void* pResult, const GPA_HWInfo* pHwInfo) const = 0;

    /// Gets the counter type information based on the global counter index
    /// \param globalIndex The index into the main list of counters
    /// \return The info about the counter
    virtual GPACounterSourceInfo GetCounterSourceInfo(gpa_uint32 globalIndex) const = 0;

    /// Gets a counter's index
    /// \param pName The name of a counter
    /// \param[out] pIndex The index of the counter
    /// \return true if the counter is found, false otherwise
    virtual bool GetCounterIndex(const char* pName, gpa_uint32* pIndex) const = 0;

    /// Get the hardware counters
    /// \return the hardware counters
    virtual const GPA_HardwareCounters* GetHardwareCounters() const = 0;

    /// Get the software counters
    /// \return the software counters
    virtual const GPA_SoftwareCounters* GetSoftwareCounters() const = 0;

    /// Virtual Destructor
    virtual ~IGPACounterAccessor() = default;
};

#endif //_GPA_I_COUNTER_ACCESSOR_H_
