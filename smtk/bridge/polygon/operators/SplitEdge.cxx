//=============================================================================
// Copyright (c) Kitware, Inc.
// All rights reserved.
// See LICENSE.txt for details.
//
// This software is distributed WITHOUT ANY WARRANTY; without even
// the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE.  See the above copyright notice for more information.
//=============================================================================
#include "smtk/bridge/polygon/operators/SplitEdge.h"

#include "smtk/bridge/polygon/Session.h"
#include "smtk/bridge/polygon/internal/Model.h"
#include "smtk/bridge/polygon/internal/Model.txx"

#include "smtk/io/Logger.h"

#include "smtk/model/Vertex.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/DoubleItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ModelEntityItem.h"
#include "smtk/attribute/StringItem.h"

#include "smtk/bridge/polygon/SplitEdge_xml.h"

namespace smtk
{
namespace bridge
{
namespace polygon
{

smtk::model::OperatorResult SplitEdge::operateInternal()
{
  smtk::bridge::polygon::SessionPtr sess = this->polygonSession();
  smtk::model::Manager::Ptr mgr;
  if (!sess)
    return this->createResult(smtk::operation::Operator::OPERATION_FAILED);

  mgr = sess->manager();

  smtk::attribute::IntItem::Ptr pointIdItem = this->findInt("point id");
  smtk::attribute::DoubleItem::Ptr pointItem = this->findDouble("point");
  smtk::attribute::ModelEntityItem::Ptr edgeItem = this->specification()->associations();
  smtk::model::Edge edgeToSplit(edgeItem->value(0));
  if (!edgeToSplit.isValid())
  {
    smtkErrorMacro(this->log(), "The input edge (" << edgeToSplit.entity() << ") is invalid.");
    return this->createResult(smtk::operation::Operator::OPERATION_FAILED);
  }

  internal::edge::Ptr storage = this->findStorage<internal::edge>(edgeToSplit.entity());
  internal::pmodel* mod = storage->parentAs<internal::pmodel>();
  if (!storage || !mod)
  {
    smtkErrorMacro(this->log(), "The input edge has no storage or no parent model set.");
    return this->createResult(smtk::operation::Operator::OPERATION_FAILED);
  }

  std::vector<double> point;
  smtk::model::EntityRefArray created;
  bool ok;
  if (pointIdItem && pointIdItem->value(0) >= 0 &&
    pointIdItem->value(0) < static_cast<int>(storage->pointsSize()))
  {
    ok = mod->splitModelEdgeAtIndex(
      mgr, edgeToSplit.entity(), pointIdItem->value(0), created, this->m_debugLevel);
  }
  else
  {
    point = std::vector<double>(pointItem->begin(), pointItem->end());
    ok = mod->splitModelEdgeAtPoint(mgr, edgeToSplit.entity(), point, created, this->m_debugLevel);
  }
  smtk::model::OperatorResult opResult;
  if (ok)
  {
    opResult = this->createResult(smtk::operation::Operator::OPERATION_SUCCEEDED);
    this->addEntitiesToResult(opResult, created, CREATED);
    this->addEntityToResult(opResult, edgeToSplit, EXPUNGED);
  }
  else
  {
    smtkErrorMacro(this->log(), "Failed to split edge.");
    opResult = this->createResult(smtk::operation::Operator::OPERATION_FAILED);
  }

  return opResult;
}

} // namespace polygon
} //namespace bridge
} // namespace smtk

smtkImplementsModelOperator(SMTKPOLYGONSESSION_EXPORT, smtk::bridge::polygon::SplitEdge,
  polygon_split_edge, "split edge", SplitEdge_xml, smtk::bridge::polygon::Session);
