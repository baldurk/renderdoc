/************************************************************************************

Filename    :   VrApi_Config.h
Content     :   VrApi preprocessor settings
Created     :   April 23, 2015
Authors     :   James Dolan

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#ifndef OVR_VrApi_Config_h
#define OVR_VrApi_Config_h

/*

OVR_VRAPI_EXPORT
OVR_VRAPI_DEPRECATED

*/

#if defined( _MSC_VER ) || defined( __ICL )

#if defined( OVR_VRAPI_ENABLE_EXPORT )
    #define OVR_VRAPI_EXPORT  __declspec( dllexport )
#else
    #define OVR_VRAPI_EXPORT
#endif

#define OVR_VRAPI_DEPRECATED __declspec( deprecated )

#else

#if defined( OVR_VRAPI_ENABLE_EXPORT )
    #define OVR_VRAPI_EXPORT __attribute__( ( __visibility__( "default" ) ) )
#else
    #define OVR_VRAPI_EXPORT 
#endif

#define OVR_VRAPI_DEPRECATED __attribute__( ( deprecated ) )

#endif

#if defined( __x86_64__ ) || defined( __aarch64__ ) || defined( _WIN64 )
	#define OVR_VRAPI_64_BIT
#else
	#define OVR_VRAPI_32_BIT
#endif

/*

OVR_VRAPI_STATIC_ASSERT( exp )						// static assert
OVR_VRAPI_PADDING( bytes )							// insert bytes of padding
OVR_VRAPI_PADDING_32_BIT( bytes )					// insert bytes of padding only when using a 32-bit compiler
OVR_VRAPI_PADDING_64_BIT( bytes )					// insert bytes of padding only when using a 64-bit compiler
OVR_VRAPI_ASSERT_TYPE_SIZE( type, bytes )			// assert the size of a type
OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( type, bytes )	// assert the size of a type only when using a 32-bit compiler
OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( type, bytes )	// assert the size of a type only when using a 64-bit compiler

*/

#if defined( __cplusplus ) && __cplusplus >= 201103L
	#define OVR_VRAPI_STATIC_ASSERT( exp )					static_assert( exp, #exp )
#endif

#if !defined( OVR_VRAPI_STATIC_ASSERT ) && defined( __clang__ )
	#if __has_feature( cxx_static_assert ) || __has_extension( cxx_static_assert )
		#define OVR_VRAPI_STATIC_ASSERT( exp )				static_assert( exp )
	#endif
#endif

#if !defined( OVR_VRAPI_STATIC_ASSERT )
	#if defined( __COUNTER__ )
		#define OVR_VRAPI_STATIC_ASSERT( exp )				OVR_VRAPI_STATIC_ASSERT_ID( exp, __COUNTER__ )
	#else
		#define OVR_VRAPI_STATIC_ASSERT( exp )				OVR_VRAPI_STATIC_ASSERT_ID( exp, __LINE__ )
	#endif
	#define OVR_VRAPI_STATIC_ASSERT_ID( exp, id )			OVR_VRAPI_STATIC_ASSERT_ID_EXPANDED( exp, id )
	#define OVR_VRAPI_STATIC_ASSERT_ID_EXPANDED( exp, id )	typedef char assert_failed_##id[(exp) ? 1 : -1]
#endif

#if defined( __COUNTER__ )
	#define OVR_VRAPI_PADDING( bytes )						OVR_VRAPI_PADDING_ID( bytes, __COUNTER__ )
#else
	#define OVR_VRAPI_PADDING( bytes )						OVR_VRAPI_PADDING_ID( bytes, __LINE__ )
#endif
#define OVR_VRAPI_PADDING_ID( bytes, id )					OVR_VRAPI_PADDING_ID_EXPANDED( bytes, id )
#define OVR_VRAPI_PADDING_ID_EXPANDED( bytes, id )			unsigned char dead##id[(bytes)]

#define OVR_VRAPI_ASSERT_TYPE_SIZE( type, bytes	)			OVR_VRAPI_STATIC_ASSERT( sizeof( type ) == (bytes) )

#if defined( OVR_VRAPI_64_BIT )
	#define OVR_VRAPI_PADDING_32_BIT( bytes )
	#if defined( __COUNTER__ )
		#define OVR_VRAPI_PADDING_64_BIT( bytes )				OVR_VRAPI_PADDING_ID( bytes, __COUNTER__ )
	#else
		#define OVR_VRAPI_PADDING_64_BIT( bytes )				OVR_VRAPI_PADDING_ID( bytes, __LINE__ )
	#endif
	#define OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( type, bytes	)
	#define OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( type, bytes	)	OVR_VRAPI_STATIC_ASSERT( sizeof( type ) == (bytes) )
#else
	#define OVR_VRAPI_ASSERT_TYPE_SIZE( type, bytes )			OVR_VRAPI_STATIC_ASSERT( sizeof( type ) == (bytes) )
	#if defined( __COUNTER__ )
		#define OVR_VRAPI_PADDING_32_BIT( bytes )				OVR_VRAPI_PADDING_ID( bytes, __COUNTER__ )
	#else
		#define OVR_VRAPI_PADDING_32_BIT( bytes )				OVR_VRAPI_PADDING_ID( bytes, __LINE__ )
	#endif
	#define OVR_VRAPI_PADDING_64_BIT( bytes )
	#define OVR_VRAPI_ASSERT_TYPE_SIZE_32_BIT( type, bytes	)	OVR_VRAPI_STATIC_ASSERT( sizeof( type ) == (bytes) )
	#define OVR_VRAPI_ASSERT_TYPE_SIZE_64_BIT( type, bytes	)
#endif

#endif	// !OVR_VrApi_Config_h	
