//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/model/operators/DeleteMesh.h"

#include "smtk/model/Manager.h"
#include "smtk/model/Session.h"
#include "smtk/model/Session.h"
#include "smtk/mesh/Manager.h"
#include "smtk/mesh/Collection.h"
#include "smtk/mesh/MeshSet.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/MeshItem.h"

namespace smtk {
  namespace model {

bool DeleteMesh::ableToOperate()
{
  if(!this->ensureSpecification())
    return false;
  smtk::attribute::MeshItem::Ptr meshItem =
    this->specification()->findMesh("mesh");
  return meshItem && meshItem->numberOfValues() > 0;
}

smtk::model::OperatorResult DeleteMesh::operateInternal()
{
  // ableToOperate should have verified that mesh(s) are set
  smtk::attribute::MeshItem::Ptr meshItem =
    this->specification()->findMesh("mesh");

  smtk::mesh::MeshSets expunged;
  bool success = true;
  smtk::mesh::ManagerPtr meshMgr = this->manager()->meshes();
  if(meshMgr)
    {
    for (attribute::MeshItem::const_mesh_it mit = meshItem->begin();
      mit != meshItem->end(); ++mit)
      {
      if(!meshMgr->removeCollection(mit->collection()))
        {
        success = false;
        break;
        }
      expunged.insert(*mit);
      }
    }

  OperatorResult result =
    this->createResult(
      success ?  OPERATION_SUCCEEDED : OPERATION_FAILED);

  if (success)
    {
    result->findMesh("mesh_expunged")->appendValues(expunged);
    }
  return result;
}

  } //namespace model
} // namespace smtk

#include "smtk/model/DeleteMesh_xml.h"

smtkImplementsModelOperator(
  SMTKCORE_EXPORT,
  smtk::model::DeleteMesh,
  delete_mesh,
  "delete mesh",
  DeleteMesh_xml,
  smtk::model::Session);