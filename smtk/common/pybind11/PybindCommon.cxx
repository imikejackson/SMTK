//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include <pybind11/pybind11.h>
#include <utility>

namespace py = pybind11;

template <typename T, typename... Args>
using PySharedPtrClass = py::class_<T, std::shared_ptr<T>, Args...>;

#include "PybindCompilerInformation.h"
#include "PybindEnvironment.h"
#include "PybindFileLocation.h"
#include "PybindGeometryUtilities.h"
#include "PybindPaths.h"
#if !defined(_WIN32) || defined(__CYGWIN__)
#include "PybindPathsHelperUnix.h"
#  ifdef __APPLE__
#include "PybindPathsHelperMacOSX.h"
#  endif
#else
#include "PybindPathsHelperWindows.h"
#endif
#include "PybindDateTime.h"
#include "PybindDateTimeZonePair.h"
#include "PybindRangeDetector.h"
#include "PybindResource.h"
#include "PybindResourceSet.h"
#include "PybindStringUtil.h"
#include "PybindTimeZone.h"
#include "PybindUnionFind.h"
#include "PybindUUID.h"
#include "PybindUUIDGenerator.h"
#include "PybindVersion.h"
#include "PybindView.h"

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>);

PYBIND11_PLUGIN(smtkPybindCommon)
{
  py::module common("smtkPybindCommon", "<description>");

  // The order of these function calls is important! It was determined by
  // comparing the dependencies of each of the wrapped objects.
  py::class_< smtk::common::DateTime > smtk_common_DateTime = pybind11_init_smtk_common_DateTime(common);
  py::class_< smtk::common::DateTimeZonePair > smtk_common_DateTimeZonePair = pybind11_init_smtk_common_DateTimeZonePair(common);
  py::class_< smtk::common::Environment > smtk_common_Environment = pybind11_init_smtk_common_Environment(common);
  py::class_< smtk::common::FileLocation > smtk_common_FileLocation = pybind11_init_smtk_common_FileLocation(common);
  py::class_< smtk::common::Paths > smtk_common_Paths = pybind11_init_smtk_common_Paths(common);
#if !defined(_WIN32) || defined(__CYGWIN__)
  py::class_< smtk::common::PathsHelperUnix > smtk_common_PathsHelperUnix = pybind11_init_smtk_common_PathsHelperUnix(common);
#  ifdef __APPLE__
  py::class_< smtk::common::PathsHelperMacOSX > smtk_common_PathsHelperMacOSX = pybind11_init_smtk_common_PathsHelperMacOSX(common);
#  endif
#else
  py::class_< smtk::common::PathsHelperWindows > smtk_common_PathsHelperWindows = pybind11_init_smtk_common_PathsHelperWindows(common);
#endif
  py::class_< smtk::common::Resource > smtk_common_Resource = pybind11_init_smtk_common_Resource(common);
  py::class_< smtk::common::ResourceSet > smtk_common_ResourceSet = pybind11_init_smtk_common_ResourceSet(common);
  py::class_< smtk::common::StringUtil > smtk_common_StringUtil = pybind11_init_smtk_common_StringUtil(common);
  py::class_< smtk::common::TimeZone > smtk_common_TimeZone = pybind11_init_smtk_common_TimeZone(common);
  py::class_< smtk::common::UUID > smtk_common_UUID = pybind11_init_smtk_common_UUID(common);
  py::class_< smtk::common::UUIDGenerator > smtk_common_UUIDGenerator = pybind11_init_smtk_common_UUIDGenerator(common);
  py::class_< smtk::common::Version > smtk_common_Version = pybind11_init_smtk_common_Version(common);
  py::class_< smtk::common::View > smtk_common_View = pybind11_init_smtk_common_View(common);

  return common.ptr();
}