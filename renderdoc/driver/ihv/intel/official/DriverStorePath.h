/*
// Copyright (c) 2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
*/
#pragma once

#include <initguid.h>
#include <ntddvdeo.h>
#include <Devpkey.h>
#include <SetupAPI.h>
#include <Shlwapi.h>

#define DEBUG 0

#if DEBUG
#define DBG(std, fmt, ...) fprintf_s(stderr, fmt, __VA_ARGS__)
#else
#define DBG(std, fmt, ...)
#endif

/********************************************************************************************/
/* GetPropertyFromDevice                                                                    */
/*                                                                                          */
/* This function can be used to request a value of any device property.                     */
/* There are many types of properties values. Refer to devpkey.h for more details.          */
/* Function SetupDiGetDeviceProperty() inside GetPropertyFromDevice() fills a property value*/
/* in a correct format, but always returns the value as a pointer to a string of bytes.     */
/* The string of bytes must be casted outside of the function GetPropertyFromDevice()       */
/* to get a value in a format suitable for the given type of property.                      */
/*                                                                                          */
/********************************************************************************************/
static bool GetPropertyFromDevice(
    void* pDevInfo,
    PSP_DEVINFO_DATA pDevInfoData,
    const DEVPROPKEY* pPropertyKey,
    unsigned char** ppStringOut,
    unsigned long* pStringOutSize)
{
    unsigned long propertyType = 0;
    unsigned long propertySize = 0;

    // request a size, in bytes, required for a buffer in which property value will be stored
    // SetupDiGetDeviceProperty() returns false and ERROR_INSUFFICIENT_BUFFER for the call
    if (SetupDiGetDevicePropertyW(pDevInfo, pDevInfoData, pPropertyKey, &propertyType, NULL, 0, &propertySize, 0))
    {
        DBG(stderr, "%s [%d] ---> SetupDiGetDeviceProperty() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
        return false;
    }

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        DBG(stderr, "%s [%d] ---> SetupDiGetDeviceProperty() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
        return false;
    }

    if (ppStringOut == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        DBG(stderr, "%s [%d] ---> Error: input parameter ppStringOut = NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    // allocate memory for the buffer
    *ppStringOut = new (std::nothrow) unsigned char[propertySize];
    if (*ppStringOut == NULL)
    {
        SetLastError(ERROR_INVALID_HANDLE);
        DBG(stderr, "%s [%d] ---> Error: *ppStringOut = NULL\n", __FUNCTION__, __LINE__);
        return false;
    }

    // fill in the buffer with property value
    if (!SetupDiGetDevicePropertyW(pDevInfo, pDevInfoData, pPropertyKey, &propertyType, *ppStringOut, propertySize, NULL, 0))
    {
        DBG(stderr, "%s [%d] ---> SetupDiGetDeviceProperty() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
        delete[] *ppStringOut;
        *ppStringOut = NULL;
        return false;
    }

    if (pStringOutSize)
    {
        *pStringOutSize = propertySize;
    }

    return true;
}

/************************************************************************/
/* GetIntelDriverStoreFullPath                                          */
/************************************************************************/
static bool GetIntelDriverStoreFullPath(
    wchar_t* pDriverStorePath,
    unsigned long driverStorePathSizeInCharacters,
    unsigned long* pDriverStorePathLengthInCharacters)
{
    bool result = false;
    // allocated memory must be freed with delete operation
    unsigned char* pPropertyInfName = NULL;
    // allocated memory must be freed with delete operation
    unsigned char* pPropertyDevServiceName = NULL;

    // guid defined for display adapters
    const GUID guid = GUID_DISPLAY_DEVICE_ARRIVAL;

    // create device information set containing display adapters which support interfaces and are currently present in the system
    void* pDevInfo = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (pDevInfo == INVALID_HANDLE_VALUE)
    {
        DBG(stderr, "%s [%d] ---> SetupDiGetClassDevs() failed with the error code 0x%02x, pDevInfo = INVALID_HANDLE_VALUE\n", __FUNCTION__, __LINE__, GetLastError());
        goto END;
    }

    unsigned long deviceIndex = 0;
    SP_DEVINFO_DATA devInfoData;
    ZeroMemory(&devInfoData, sizeof(SP_DEVINFO_DATA));
    unsigned long interfaceIndex = 0;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    ZeroMemory(&deviceInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
    DEVPROPKEY devPropKey;
    ZeroMemory(&devPropKey, sizeof(DEVPROPKEY));
    unsigned long driverStorePathLengthInCharacters = 0;

    // enumerate display adapters
    while (true)
    {
        ZeroMemory(&devInfoData, sizeof(SP_DEVINFO_DATA));
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (!SetupDiEnumDeviceInfo(pDevInfo, deviceIndex, &devInfoData))
        {
            if (GetLastError() != ERROR_NO_MORE_ITEMS)
            {
                DBG(stderr, "%s [%d] ---> SetupDiEnumDeviceInfo() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
            }

            deviceIndex = 0;
            goto END;
        }

        // enumerate interfaces of display adapters
        while (true)
        {
            ZeroMemory(&deviceInterfaceData, sizeof(SP_DEVICE_INTERFACE_DATA));
            deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

            if (!SetupDiEnumDeviceInterfaces(pDevInfo, &devInfoData, &guid, interfaceIndex, &deviceInterfaceData))
            {
                if (GetLastError() == ERROR_NO_MORE_ITEMS)
                {
                    interfaceIndex = 0;
                    break;
                }
                else
                {
                    DBG(stderr, "%s [%d] ---> SetupDiEnumDeviceInterfaces() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
                    interfaceIndex = 0;
                    goto END;
                }
            }

            // get an inf file name for a display adapter
            if (GetPropertyFromDevice(pDevInfo, &devInfoData, &DEVPKEY_Device_DriverInfPath, &pPropertyInfName, NULL) == false)
            {
                DBG(stderr, "%s [%d] ---> GetPropertyFromDevice() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
                goto END;
            }

            // to read DEVPKEY_Device_DriverInfPath property value correctly just cast unsigned char* (means PBYTE) to const wchar_t*
            const wchar_t* pInfName = reinterpret_cast<const wchar_t*>(pPropertyInfName);
            DBG(stdout, "\n");
            DBG(stdout, "pPropertyInfName = %ws\n", pInfName);
            wchar_t driverStorePath[MAX_PATH];
            ZeroMemory(driverStorePath, sizeof(driverStorePath));

            // get a fully qualified name of an inf file (directory path and file name)
            if (!SetupGetInfDriverStoreLocationW(pInfName, NULL, NULL, driverStorePath, ARRAYSIZE(driverStorePath), NULL))
            {
                DBG(stderr, "%s [%d] ---> SetupGetInfDriverStoreLocation() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
                goto END;
            }

            // remove backslash and file name from the fully qualified name
            PathRemoveFileSpecW(driverStorePath);
            DBG(stdout, "driverStorePath = %ws\n", driverStorePath);
            driverStorePathLengthInCharacters = (unsigned long)wcsnlen_s(driverStorePath, ARRAYSIZE(driverStorePath));
            DBG(stdout, "driverStorePathLengthInCharacters = %d\n", driverStorePathLengthInCharacters);

            // get service name for a display adapter
            if (GetPropertyFromDevice(pDevInfo, &devInfoData, &DEVPKEY_Device_Service, &pPropertyDevServiceName, NULL) == false)
            {
                DBG(stderr, "%s [%d] ---> GetPropertyFromDevice() failed with the error code 0x%02x\n", __FUNCTION__, __LINE__, GetLastError());
                goto END;
            }

            // to read DEVPKEY_Device_Service property value correctly just cast unsigned char* (means PBYTE) to const wchar_t*
            const wchar_t* pDevServiceName = reinterpret_cast<const wchar_t*>(pPropertyDevServiceName);
            DBG(stdout, "pDevServiceName = %ws\n", pDevServiceName);

            // check if a given display adapter is from Intel based on driver device service name "igfx"
            // check if pDevServiceName contains "igfx" name
            if (wcsstr(pDevServiceName, L"igfx"))
            {
                // display adapter is from Intel
                DBG(stdout, "this display adapter is from Intel\n");

                if (pDriverStorePath == NULL)
                {
                    DBG(stderr, "%s [%d] ---> Error: input parameter pDriverStorePath = NULL\n", __FUNCTION__, __LINE__);
                    goto END;
                }

                if (driverStorePathSizeInCharacters < driverStorePathLengthInCharacters + 1)
                {
                    DBG(stderr, "%s [%d] ---> Error: input parameter driverStorePathSizeInCharacters = %d is too small, the required size is %d\n", __FUNCTION__, __LINE__, driverStorePathSizeInCharacters, driverStorePathLengthInCharacters + 1);
                    goto END;
                }

                wcscpy_s(pDriverStorePath, driverStorePathSizeInCharacters, driverStorePath);

                if (pDriverStorePathLengthInCharacters)
                {
                    *pDriverStorePathLengthInCharacters = driverStorePathLengthInCharacters;
                }

                result = (driverStorePathLengthInCharacters > 0) ? true : false;
                if (result == false)
                {
                    SetLastError(ERROR_BAD_LENGTH);
                    DBG(stderr, "%s [%d] ---> Error: driverStorePathLengthInCharacters = 0, result = false\n", __FUNCTION__, __LINE__);
                }
                goto END;
            }
            else
            {
                // display adapter is from other vendor
                DBG(stdout, "this display adapter is NOT from Intel\n");

                if (pPropertyDevServiceName)
                {
                    delete[] pPropertyDevServiceName;
                    pPropertyDevServiceName = NULL;
                }

                if (pPropertyInfName)
                {
                    delete[] pPropertyInfName;
                    pPropertyInfName = NULL;
                }
            }

            ++interfaceIndex;
        }

        ++deviceIndex;
    }

END:
    DBG(stdout, "\n");

    if (pPropertyDevServiceName)
    {
        delete[] pPropertyDevServiceName;
        pPropertyDevServiceName = NULL;
    }

    if (pPropertyInfName)
    {
        delete[] pPropertyInfName;
        pPropertyInfName = NULL;
    }

    if (pDevInfo)
    {
        SetupDiDestroyDeviceInfoList(pDevInfo);
        pDevInfo = NULL;
    }

    return result;
}

/************************************************************************/
/* LoadDynamicLibrary                                                   */
/************************************************************************/
static HMODULE LoadDynamicLibrary(const wchar_t* pFileName, HANDLE hFile = NULL, unsigned long flags = 0)
{
    HMODULE hModule = NULL;
    unsigned long driverStorePathLengthInCharacters = 0;
    // contains fully qualified name of DriverStore (directory path only without backslash at the end) for Intel display driver
    // for example: C:\Windows\System32\DriverStore\FileRepository\igdlh64.inf_amd64_1d1d318ad2d391db
    wchar_t driverStorePath[MAX_PATH] = { 0 };

    // find fully qualified name of DriverStore for Intel graphics driver
    if (GetIntelDriverStoreFullPath(driverStorePath, ARRAYSIZE(driverStorePath), &driverStorePathLengthInCharacters) == true)
    {
        std::wstring driverStoreFullPath(driverStorePath);

        if (driverStorePathLengthInCharacters)
        {
            driverStoreFullPath += L"\\";
            driverStoreFullPath += pFileName;
        }

        DBG(stdout, "full path: %ws\n\n", driverStoreFullPath.c_str());

        // load dll from DriverStore full path
        hModule = LoadLibraryExW(driverStoreFullPath.c_str(), NULL, 0);
    }
    else
    {
        DBG(stderr, "%s [%d] ---> GetIntelDriverStoreFullPath() failed\n", __FUNCTION__, __LINE__);
        hModule = NULL;
    }

    return hModule;
}