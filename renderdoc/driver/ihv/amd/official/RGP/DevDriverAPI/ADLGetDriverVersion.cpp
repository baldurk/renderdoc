//=============================================================================
/// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Retrieve driver version information for AMD Radeon Drivers on Windows using ADL
//=============================================================================

// To compile from the windows CMD prompt into a standalone command-line executable for test
// - use the following commands with Visual Studio 2017
//
// > "c:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\vsvars32.bat"
// > cl ADLGetDriverVersion.cpp -I <path to adl_sdk.h> -D COMMAND_LINE_TEST /EHsc

#ifdef _WIN32
#include <windows.h>
#include <sstream>
#include "adl_sdk.h"

#include "ADLGetDriverVersion.h"

// Definitions of the used ADL function pointers.
typedef int(*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int(*ADL2_GRAPHICS_VERSION_GET)(ADL_CONTEXT_HANDLE context, ADLVersionsInfo * lpVersionsInfo);

// Memory allocation function for use with ADL
static void* __stdcall ADL_Main_Memory_Alloc(int iSize)
{
	void* lpBuffer = malloc(iSize);
	return lpBuffer;
}

//-----------------------------------------------------------------------------
/// Parse version string returned by the driver via ADL to extract version info
/// \param versionString The string returned from the driver via ADL
/// \param major The major version number
/// \param minor The minor version number
/// \param subminor The subminor version number
/// \return true if successful, or false on error
//-----------------------------------------------------------------------------
static bool parseDriverVersionString(char *versionString, unsigned int &major, unsigned int &minor, unsigned int &subminor)
{
	major = minor = subminor = 0;
	std::string strDriverVersion(versionString);

#ifdef COMMAND_LINE_TEST
	printf("\nVersion string from driver: %s\n", versionString);
#endif

	// driver version looks like:  13.35.1005-140131a-167669E-ATI or 14.10-140115n-021649E-ATI, etc...
	// truncate at the first dash
	strDriverVersion = strDriverVersion.substr(0, strDriverVersion.find("-", 0));

	size_t pos = 0;
	std::string strToken;
	std::string strDelimiter = ".";
	std::stringstream ss;

	// parse the major driver version (start of string to first ".")
	pos = strDriverVersion.find(strDelimiter);

	if (pos == std::string::npos)
	{
		// delimiter not found
		return false;
	}
	
	strToken = strDriverVersion.substr(0, pos);
	ss.str(strToken);

	if ((ss >> major).fail())
	{
		major = 0;
		return false;
	}
	
	strDriverVersion.erase(0, pos + strDelimiter.length());  // Delete section of string already parsed

	// parse the minor driver version
	bool subminorAvailable = false;

	pos = strDriverVersion.find(strDelimiter);

	if (pos != std::string::npos)
	{
		// Delimiter found
		strToken = strDriverVersion.substr(0, pos);
		strDriverVersion.erase(0, pos + strDelimiter.length());
		subminorAvailable = true;
	}
	else
	{
		// No delimeter - use entire string
		strToken = strDriverVersion;
	}

	ss.clear();
	ss.str(strToken);

	if ((ss >> minor).fail())
	{
		major = 0;
		minor = 0;
		return false;
	}

	// parse the sub-minor driver version
	if (subminorAvailable)
	{
		pos = strDriverVersion.find(strDelimiter);

		if (pos != std::string::npos)
		{
			// Delimiter found
			strToken = strDriverVersion.substr(0, pos);
		}
		else
		{
			strToken = strDriverVersion;
		}
		ss.clear();
		ss.str(strToken);

		if ((ss >> subminor).fail())
		{
			major = 0;
			minor = 0;
			subminor = 0;
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
/// Use ADL on Windows to retrieve the driver version number
/// \param majorVar The major version number
/// \param minorVer The minor version number
/// \param subminorVer The subminor version number
/// \return true if successful, or false on error
//-----------------------------------------------------------------------------
bool ADLGetDriverVersion(unsigned int& majorVer, unsigned int& minorVer, unsigned int& subminorVer)
{
	bool retStatus = false;

	HINSTANCE hDLL = LoadLibrary(TEXT("atiadlxx.dll"));

	if (NULL == hDLL)
	{
		// A 32 bit calling application on 64 bit OS will fail to LoadLIbrary.
		// Try to load the 32 bit library (atiadlxy.dll) instead
		hDLL = LoadLibrary(TEXT("atiadlxy.dll"));
	}

	if (NULL != hDLL)
	{
		ADL2_MAIN_CONTROL_CREATE ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL2_Main_Control_Create");;
		ADL2_MAIN_CONTROL_DESTROY ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
		ADL2_GRAPHICS_VERSION_GET ADL2_Graphics_Versions_Get = (ADL2_GRAPHICS_VERSION_GET)GetProcAddress(hDLL, "ADL2_Graphics_Versions_Get");

		if (NULL != ADL2_Main_Control_Create &&
			NULL != ADL2_Main_Control_Destroy &&
			NULL != ADL2_Graphics_Versions_Get
			)
		{
			ADL_CONTEXT_HANDLE adlContext = NULL;
			if (ADL_OK == ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &adlContext))
			{
				ADLVersionsInfo versionsInfo;
				int ADLResult = ADL2_Graphics_Versions_Get(adlContext, &versionsInfo);
				if (ADL_OK == ADLResult || ADL_OK_WARNING == ADLResult)
				{
					retStatus = parseDriverVersionString(versionsInfo.strDriverVer, majorVer, minorVer, subminorVer);
				}
				ADL2_Main_Control_Destroy(adlContext);
			}
		}
		FreeLibrary(hDLL);
	}
	return retStatus;
}

#endif // _WIN32

#ifdef COMMAND_LINE_TEST
int main()
{
	unsigned int major, minor, subminor;

	if (ADLGetDriverVersion(major, minor, subminor) == true)
	{
		printf("\nDriver Major Version: %d", major);
		printf("\nDriver Minor Version: %d", minor);
		if (subminor != 0)
		{
			printf("\nDriver SubMinor Version: %d", subminor);
		}
	}
	else
	{
		printf("\nUnable to retrieve driver version information");
	}
	printf("\n");

}
#endif
