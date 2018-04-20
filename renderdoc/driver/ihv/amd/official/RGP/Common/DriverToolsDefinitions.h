//=============================================================================
/// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Shared definitions for the Developer Driver components.
//=============================================================================

#ifndef _DRIVER_TOOLS_DEFINITIONS_H_
#define _DRIVER_TOOLS_DEFINITIONS_H_

#include "Version.h"
#include "../DevDriverComponents/inc/gpuopen.h"
#include "../DevDriverComponents/inc/ddPlatform.h"

#ifdef WIN32
#include <windows.h>
#endif

#ifdef _DEBUG
    #define SHOW_DEBUG_WINDOW 1
#endif

#define SAFE_DELETE(x) {if(x){delete (x); (x) = nullptr; }}
#define SAFE_DELETE_ARRAY(x) {if(x){delete [] (x); x = nullptr; }}

#ifdef HEADLESS
#define HEADLESS_FILENAME_SUFFIX "CLI"
#else
#define HEADLESS_FILENAME_SUFFIX ""
#endif // HEADLESS

#define BUILD_FILENAME_SUFFIX ""

static const char* gs_RDP_SETTINGS_DIRECTORY    = "RadeonDeveloperDriver";                          ///< RDP and RDS use this directory to write settings to.
static const char* gs_RGP_EXECUTABLE_FILENAME   = "RadeonGPUProfiler" BUILD_FILENAME_SUFFIX;        ///< RGP's executable filename.
static const char* gs_RDP_EXECUTABLE_FILENAME   = "RadeonDeveloperPanel" BUILD_FILENAME_SUFFIX;     ///< RDP's executable filename.
static const char* gs_RDP_APPLICATION_GUID      = "C9EB0587-F8F7-4B8C-B35A-F7C2862CFDA7";           ///< Unique identifier for the RDP application
static const char* gs_RDS_EXECUTABLE_FILENAME   = "RadeonDeveloperService" HEADLESS_FILENAME_SUFFIX BUILD_FILENAME_SUFFIX;   ///< RDS's executable filename.
static const char* gs_RDS_APPLICATION_GUID      = "D0939873-BA4B-4C4E-9729-D82DED85BC41";           ///< Unique identifier for the RDS service
static const char* gs_RGP_TRACE_EXTENSION       = ".rgp";                                           ///< The extension used when saving RGP trace files.

// Help for Command line options
#ifdef WIN32
static const char* gs_RDS_CLI_USAGE_DESCRIPTION            = " [--help] | [--port <portnumber>] | [--enableUWP]\n\n";
#else
static const char* gs_RDS_CLI_USAGE_DESCRIPTION = " [--help] | [--port <portnumber>]\n\n";
#endif // WIN32
static const char* gs_RDS_CLI_HELP_OPTION_DESCRIPTION      = "--help               This help message.";                                                      ///< Command line help for --help
static const char* gs_RDS_CLI_PORT_OPTION_DESCRIPTION      = "--port <portnumber>  The listener port.  Where <portnumber> is a value between 1 and 65535.";  ///< Command line help for --port
static const char* gs_RDS_CLI_UWPENABLE_OPTION_DESCRIPTION = "--enableUWP          Enable UWP support.";                                                     ///< Command line help for --enableUWP

// Default RDS connection info.
static const char* gs_DEFAULT_HOST_ADDRESS = "0.0.0.0";         ///< The default address to listen on for incoming connections.
static const unsigned int gs_DEFAULT_CONNECTION_PORT = 27300;   ///< The default port used to connect the Developer Panel to the Developer Service.
static const unsigned short gs_MAX_LISTEN_PORT = 65535;         ///< The highest port that the Developer Service can listen on.

// Control window sizes.
static const int gs_DESKTOP_MARGIN = 25;
static const int gs_OS_TITLE_BAR_HEIGHT = 35;
static const float gs_DESKTOP_AVAIL_WIDTH_PCT = 99.0f;
static const float gs_DESKTOP_AVAIL_HEIGHT_PCT = 95.0f;
static const float gs_MAIN_WINDOW_DESKTOP_WIDTH_PCT = 100.0f;
static const float gs_MAIN_WINDOW_DESKTOP_HEIGHT_PCT = 85.0f;
static const float gs_DBG_WINDOW_DESKTOP_WIDTH_PCT = 35.0f;

#endif // _DRIVER_TOOLS_DEFINITIONS_H_
