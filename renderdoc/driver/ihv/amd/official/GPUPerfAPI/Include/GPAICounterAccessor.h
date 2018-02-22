//==============================================================================
// Copyright (c) 2012-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  An accessor interface for the GPA_CounterGeneratorBase class
//==============================================================================


#ifndef _GPA_I_COUNTER_ACCESSOR_H_
#define _GPA_I_COUNTER_ACCESSOR_H_

#include <vector>
#include "GPUPerfAPITypes.h"

struct GPA_HardwareCounterDescExt;
class GPA_HWInfo;
class GPA_PublicCounter;
class GPA_CounterResultLocation;

/// Types of counter
enum GPACounterType { PUBLIC_COUNTER, HARDWARE_COUNTER, SOFTWARE_COUNTER, UNKNOWN_COUNTER };

/// Stores the type of counter and its local index into that family of counters
struct GPACounterTypeInfo
{
    gpa_uint32 m_localIndex;      ///< the local index of the counter
    GPACounterType m_counterType; ///< the type of the counter

    /// Sets the data for
    /// \param localIndex the local index to set
    /// \param type the type to set
    void Set(gpa_uint32 localIndex, GPACounterType type)
    {
        m_localIndex = localIndex;
        m_counterType = type;
    }
};

/// An accessor interface for the GPA_CounterGeneratorBase class
class GPA_ICounterAccessor
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
    virtual const char* GetCounterCategory(gpa_uint32 index) const = 0;

    /// Gets a counter's description
    /// \param index The index of a counter, must be between 0 and the value returned from GetNumPublicCounters()
    /// \return The counter description
    virtual const char* GetCounterDescription(gpa_uint32 index) const = 0;

    /// Gets the data type of a public counter
    /// \param index The index of a counter
    /// \return The data type of the the desired counter
    virtual GPA_Type GetCounterDataType(gpa_uint32 index) const = 0;

    /// Gets the usage type of a public counter
    /// \param index The index of a counter
    /// \return The usage of the the desired counter
    virtual GPA_Usage_Type GetCounterUsageType(gpa_uint32 index) const = 0;

    /// Gets a public counter
    /// \param index The index of the public counter to return
    /// \return A public counter
    virtual const GPA_PublicCounter* GetPublicCounter(gpa_uint32 index) const = 0;

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
    virtual void ComputePublicCounterValue(gpa_uint32 counterIndex, std::vector<char*>& results, std::vector<GPA_Type>& internalCounterTypes, void* pResult, GPA_HWInfo* pHwInfo) = 0;

    /// Gets the counter type information based on the global counter index
    /// \param globalIndex The index into the main list of counters
    /// \return The info about the counter
    virtual GPACounterTypeInfo GetCounterTypeInfo(gpa_uint32 globalIndex) const = 0;

    /// Gets a counter's index
    /// \param pName The name of a counter
    /// \param[out] pIndex The index of the counter
    /// \return true if the counter is found, false otherwise
    virtual bool GetCounterIndex(const char* pName, gpa_uint32* pIndex) const = 0;

    /// Virtual Destructor
    virtual ~GPA_ICounterAccessor() = default;
};

#endif //_GPA_I_COUNTER_ACCESSOR_H_
