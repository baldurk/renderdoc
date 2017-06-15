//==============================================================================
// Copyright (c) 2012-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  An interface for scheduling counters in terms of enabling, disabling, and
///         obtaining the number of necessary passes.
//==============================================================================

#ifndef _GPA_I_COUNTER_SCHEDULER_H_
#define _GPA_I_COUNTER_SCHEDULER_H_

#include "GPAICounterAccessor.h"
#include "GPUPerfAPITypes.h"
#include <vector>
#include <map>


typedef std::map<unsigned int, GPA_CounterResultLocation> CounterResultLocationMap; ///< typedef for map of Counter Result Locations

/// An interface for enabling and disabling counters and getting the resulting number of necessary passes
class GPA_ICounterScheduler
{
public:

    /// Reset the counter scheduler
    virtual void Reset() = 0;

    /// Set the counter accessor that should be used when scheduling counters
    /// \param pCounterAccessor The counter accessor
    /// \param vendorId the vendor id of the GPU hardware
    /// \param deviceId the device id of the GPU hardware
    /// \param revisionId the revision id of the GPU hardware
    /// \return GPA_STATUS_ERROR_NULL_POINTER If pCounterAccessor is nullptr
    /// \return GPA_STATUS_OK
    virtual GPA_Status SetCounterAccessor(GPA_ICounterAccessor* pCounterAccessor, gpa_uint32 vendorId, gpa_uint32 deviceId, gpa_uint32 revisionId) = 0;

    /// Enables a counter
    /// \param index The index of a counter to enable
    /// \return GPA_STATUS_OK on success
    virtual GPA_Status EnableCounter(gpa_uint32 index) = 0;

    /// Disables a counter
    /// \param index The index of a counter to disable
    /// \return GPA_STATUS_OK on success
    virtual GPA_Status DisableCounter(gpa_uint32 index) = 0;

    /// Disables all counters
    virtual void DisableAllCounters() = 0;

    /// Get the number of enabled counters
    virtual gpa_uint32 GetNumEnabledCounters() = 0;

    /// Gets the counter index of the specified enabled counter
    /// \param enabledIndex the enabled counter whose counter index is needed
    /// \param[out] pCounterAtIndex the counter index of the specified enabled counter
    /// \return GPA_STATUS_OK on success
    virtual GPA_Status GetEnabledIndex(gpa_uint32 enabledIndex, gpa_uint32* pCounterAtIndex) = 0;

    /// Checks if the specified counter is enabled
    /// \param counterIndex the index of the counter to check
    /// \return GPA_STATUS_OK if the counter is enabled
    virtual GPA_Status IsCounterEnabled(gpa_uint32 counterIndex) = 0;

    /// Obtains the number of passes required to collect the enabled counters
    /// \param[inout] pNumRequiredPassesOut Will contain the number of passes needed to collect the set of enabled counters
    /// \return GPA_STATUS_OK on success
    virtual GPA_Status GetNumRequiredPasses(gpa_uint32* pNumRequiredPassesOut) = 0;

    /// Get a flag indicating if the counter selection has changed
    /// \return true if the counter selection has changed, false otherwise
    virtual bool GetCounterSelectionChanged() = 0;

    /// Begin profiling -- sets pass index to zero
    /// \return GPA_STATUS_OK on success
    virtual GPA_Status BeginProfile() = 0;

    /// Begin a pass -- increments the pass index
    virtual void BeginPass() = 0;

    /// Gets the counters for the specified pass
    /// \param passIndex the pass whose counters are needed
    /// \return a list of counters for the specified pass
    virtual std::vector<unsigned int>* GetCountersForPass(gpa_uint32 passIndex) = 0;

    /// End a pass
    virtual void EndPass() = 0;

    /// End profiling
    /// \return GPA_STATUS_OK on success
    virtual GPA_Status EndProfile() = 0;

    /// Gets the counter result locations for the specified public counter
    /// \param publicCounterIndex the counter index whose result locations are needed
    /// \return a map of counter result locations
    virtual CounterResultLocationMap* GetCounterResultLocations(unsigned int publicCounterIndex) = 0;

    /// Set draw call counts (internal support)
    /// \param iCounts the count of draw calls
    virtual void SetDrawCallCounts(const int iCounts) = 0;
};

#endif //_GPA_I_COUNTER_SCHEDULER_H_
