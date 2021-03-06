#=============================================================================
#
#  Copyright (c) Kitware, Inc.
#  All rights reserved.
#  See LICENSE.txt for details.
#
#  This software is distributed WITHOUT ANY WARRANTY; without even
#  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#  PURPOSE.  See the above copyright notice for more information.
#
#=============================================================================

set(unit_tests
)

set(unit_tests_which_require_data
  )

if(SMTK_ENABLE_POLYGON_SESSION)
  list(APPEND unit_tests_which_require_data
       UnitTestTriangulateFaces.cxx
       UnitTestTessellateFaces.cxx
       )
  list(APPEND extra_libs
       smtkPolygonSession
       )
endif()

smtk_unit_tests(
  LABEL "ExtDelaunay"
  SOURCES ${unit_tests}
  SOURCES_REQUIRE_DATA ${unit_tests_which_require_data}
  LIBRARIES smtkCore smtkDelaunayExt smtkCoreModelTesting ${extra_libs} ${Boost_LIBRARIES}
)
