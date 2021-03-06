// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#ifndef LIBCARLA_INCLUDED_DISABLE_UE4_MACROS_HEADER
#define LIBCARLA_INCLUDED_DISABLE_UE4_MACROS_HEADER

#include "Carla.h"

#ifndef BOOST_ERROR_CODE_HEADER_ONLY
#  define BOOST_ERROR_CODE_HEADER_ONLY
#endif // BOOST_ERROR_CODE_HEADER_ONLY

#ifndef BOOST_NO_EXCEPTIONS
#  error LibCarla should be compiled with -DBOOST_NO_EXCEPTIONS inside UE4.
#endif // BOOST_NO_EXCEPTIONS
#ifndef ASIO_NO_EXCEPTIONS
#  error LibCarla should be compiled with -DASIO_NO_EXCEPTIONS inside UE4.
#endif // ASIO_NO_EXCEPTIONS
#ifndef LIBCARLA_NO_EXCEPTIONS
#  error LibCarla should be compiled with -DLIBCARLA_NO_EXCEPTIONS inside UE4.
#endif // LIBCARLA_NO_EXCEPTIONS

#endif // LIBCARLA_INCLUDED_DISABLE_UE4_MACROS_HEADER

#define LIBCARLA_INCLUDED_FROM_UE4

// NOTE(Andrei): disable warning generated by undefined macros
// __GNUC__, __GNUC_MINOR__
// MSGPACK_ARCH_AMD64
// DBG, BETA, OFFICIAL_BUILD
// NTDDI_WIN7SP1
// _APISET_RTLSUPPORT_VER
// _APISET_INTERLOCKED_VER
// _APISET_SECURITYBASE_VER
// _WIN32_WINNT_WINTHRESHOLD
// NOTE(Andrei): Macros to detect which compiler is
// http://nadeausoftware.com/articles/2012/10/c_c_tip_how_detect_compiler_name_and_version_using_compiler_predefined_macros
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable: 4668 4191 4647)
#endif

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmissing-braces"
#  pragma clang diagnostic ignored "-Wunusable-partial-specialization"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wshadow"
#endif

#pragma push_macro("TEXT")
#undef TEXT

#pragma push_macro("check")
#undef check
