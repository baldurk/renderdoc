//=============================================================================
/// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief   Retrieve driver version information for AMD Radeon Drivers on Windows using ADL
//=============================================================================
#ifndef ADL_GET_DRIVER_VERSION_H_
#define ADL_GET_DRIVER_VERSION_H_

#ifdef _WIN32
//-----------------------------------------------------------------------------
/// Use ADL on Windows to retrieve the driver version number
/// \param majorVar The major version number
/// \param minorVer The minor version number
/// \param subminorVer The subminor version number
/// \return true if successful, or false on error
//-----------------------------------------------------------------------------
bool ADLGetDriverVersion(unsigned int& majorVer, unsigned int& minorVer, unsigned int& subminorVer);

//-----------------------------------------------------------------------------
/// Use ADL on Windows to retrieve the driver version string
/// \param outVersionName Pointer to s string to receive the version number.
///  Presetnly, the string should be at least ADL_MAX_PATH bytes long
/// \return true if successful, or false on error
//-----------------------------------------------------------------------------
bool ADLGetDriverVersionString(char* outVersionString);

#endif // _WIN32

#endif // ADL_GET_DRIVER_VERSION_H_
