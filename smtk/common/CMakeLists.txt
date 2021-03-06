# set up sources to build
set(commonSrcs
  Color.cxx
  DateTime.cxx
  DateTimeZonePair.cxx
  Environment.cxx
  Extension.cxx
  FileLocation.cxx
  json/jsonUUID.cxx
  Paths.cxx
  StringUtil.cxx
  TimeZone.cxx
  UUID.cxx
  UUIDGenerator.cxx
)

set(commonHeaders
  Color.h
  CompilerInformation.h
  DateTime.h
  DateTimeZonePair.h
  Environment.h
  Extension.h
  FileLocation.h
  Generator.h
  GeometryUtilities.h
  json/jsonUUID.h
  Paths.h
  RangeDetector.h
  StringUtil.h
  TimeZone.h
  UUID.h
  UUIDGenerator.h
  ${CMAKE_CURRENT_BINARY_DIR}/Version.h
)

if (APPLE)
  set(commonSrcs ${commonSrcs} PathsHelperMacOSX.mm)
  set(commonHeaders ${commonHeaders} PathsHelperMacOSX.h)
endif()
if (UNIX)
  set(commonSrcs ${commonSrcs} PathsHelperUnix.cxx)
  set(commonHeaders ${commonHeaders} PathsHelperUnix.h)
endif()
if (WIN32)
  set(commonSrcs ${commonSrcs} PathsHelperWindows.cxx)
  set(commonHeaders ${commonHeaders} PathsHelperWindows.h)
endif()

# Generate the version header using CMake variables.
configure_file(
  ${SMTK_SOURCE_DIR}/CMake/Version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/Version.h
  @ONLY
)

if (SMTK_ENABLE_PYTHON_WRAPPING)
  list(APPEND commonSrcs
    PythonInterpreter.cxx
    )
  list(APPEND commonHeaders
    PythonInterpreter.h
    )

  add_subdirectory(pybind11)
endif()

#install the headers
smtk_public_headers(${commonHeaders})

if (SMTK_ENABLE_TESTING)
  add_subdirectory(testing)
  target_link_libraries(TestBuild_smtk_common
    PUBLIC
    nlohmann_json)
endif()
