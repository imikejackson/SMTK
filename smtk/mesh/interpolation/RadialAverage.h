//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#ifndef __smtk_mesh_RadialAverage_h
#define __smtk_mesh_RadialAverage_h

#include "smtk/CoreExports.h"
#include "smtk/PublicPointerDefs.h"

#include <array>
#include <functional>

namespace smtk
{
namespace mesh
{

class PointCloud;
class StructuredGrid;

/**\brief A functor that converts an external data set into a continuous field
   via radial averaging.

   Given an external data set of either structured or unstructured data, this
   functor is a continuous function from R^3->R whose values are computed as the
   average of the points in the data set within a sphere of radius <radius>
   centered at the input point.
  */
class SMTKCORE_EXPORT RadialAverage
{
public:
  RadialAverage(CollectionPtr collection, const PointCloud&, double radius);
  RadialAverage(const StructuredGrid&, double radius);

#ifndef SHIBOKEN_SKIP
  double operator()(std::array<double, 3> x) const { return m_function(x); }
#endif

private:
  std::function<double(std::array<double, 3>)> m_function;
};
}
}

#endif