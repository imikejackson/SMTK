//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "Import.h"

#include "smtk/bridge/polygon/Session.h"
#include "smtk/bridge/polygon/internal/Model.h"

#include "smtk/io/Logger.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/DoubleItem.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ModelEntityItem.h"
#include "smtk/attribute/StringItem.h"

#include "smtk/model/Manager.h"
#include "smtk/model/Model.h"
#include "smtk/model/Operator.h"
#include "smtk/model/SessionRef.h"

#include "smtk/extension/vtk/reader/vtkCMBGeometryReader.h"
#ifdef SMTK_ENABLE_REMUS_SUPPORT
  #include "smtk/extension/vtk/reader/vtkCMBPolygonModelImporter.h"
  #include "smtk/extension/vtk/reader/vtkCMBMapReader.h"
#endif

#include "smtk/bridge/polygon/Import_xml.h"

#include "smtk/io/ExportJSON.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkDataSetSurfaceFilter.h"
#include "vtkIdTypeArray.h"
#include "vtkNew.h"
#include "vtkPDataSetReader.h"
#include "vtkPolyData.h"
#include "vtkXMLPolyDataWriter.h"
#include <vtksys/SystemTools.hxx>

#include "cJSON.h"

using namespace smtk::model;

namespace smtk {
  namespace bridge {

  namespace polygon {

//----------------------------------------------------------------------------
int polyLines2modelEdges(vtkPolyData *mesh,
                         smtk::model::Operator::Ptr edgeOp,
                         smtk::model::EntityRefArray& createdEds,
                         smtk::attribute::DoubleItem::Ptr pointsItem,
                         vtkIdType *pts, vtkIdType npts,
                         smtk::io::Logger& logger)
{
  double p[3];
  // create edge for current line cell
  pointsItem->setNumberOfValues(npts * 3);
  for (vtkIdType j=0; j < npts; ++j)
    {
    mesh->GetPoint(pts[j],p);
    for (int i = 0; i < 3; ++i)
      {
      pointsItem->setValue(3 * j + i, p[i]);
      }
    }
  OperatorResult edgeResult = edgeOp->operate();
  if (edgeResult->findInt("outcome")->value() != OPERATION_SUCCEEDED)
    {
    smtkDebugMacro(logger, "\"create edge\" op failed to creat edge with given line cells.");
    return 0;
    }
  smtk::attribute::ModelEntityItem::Ptr newEdges = edgeResult->findModelEntity("created");
  createdEds.insert(createdEds.end(), newEdges->begin(), newEdges->end());
  return newEdges->numberOfValues();

}

//----------------------------------------------------------------------------
int polyLines2modelEdgesAndFaces(vtkPolyData *mesh,
                          smtk::model::Model& model,
                          smtk::bridge::polygon::Session* sess,
                          smtk::io::Logger& logger)
{
  int numEdges = 0;
  vtkCellArray* lines = mesh->GetLines();
  if (lines)
    {
    smtk::model::Operator::Ptr edgeOp = sess->op("create edge");
    smtk::attribute::AttributePtr spec = edgeOp->specification();
    spec->associateEntity(model);
    smtk::attribute::IntItem::Ptr constructMethod = spec->findInt("construction method");
    constructMethod->setDiscreteIndex(0); // "points coornidates"
    smtk::attribute::IntItem::Ptr numCoords = spec->findInt("coordinates");
    numCoords->setValue(3); // number of elements in coordinates
    smtk::attribute::DoubleItem::Ptr pointsItem = spec->findDouble("points");

    smtk::model::Operator::Ptr faceOp = sess->op("force create face");
    smtk::attribute::AttributePtr faceSpec = faceOp->specification();
    faceSpec->findInt("construction method")->setDiscreteIndex(1); // "edges"

    vtkIdTypeArray* pedigreeIds = vtkIdTypeArray::SafeDownCast(
      mesh->GetCellData()->GetPedigreeIds());
    vtkIdType numPedIDs = pedigreeIds ? pedigreeIds->GetNumberOfTuples() : 0;
    vtkIdType* pedigree = numPedIDs == lines->GetNumberOfCells() ?
      pedigreeIds->GetPointer(0) : NULL;
/*
    std::cout << "number of line cells: " << lines->GetNumberOfCells() << std::endl;
    if(pedigreeIds)
      {
      std::cout << "number of pedigreeIds: " << numPedIDs << std::endl;
      }
*/
    vtkIdType pidx = 0;
    vtkIdType *pts,npts;
    for (lines->SetTraversalLocation(0);lines->GetNextCell(npts,pts); ++pidx)
      {
      std::vector<int> counts; // how many edges make up the outer loop, how many inner loops are there and how many edges for each?
      smtk::model::EntityRefArray createdEds;
      // create edge for current line cell
      int numNewEdges = polyLines2modelEdges(mesh, edgeOp, createdEds, pointsItem,
                                         pts, npts, logger);

      counts.push_back(numNewEdges);
      counts.push_back(0);
      // peek at next pedigree id if possible, to see if the pedId is the same, if yes,
      // the next cell is the inner loop
      vtkIdType pedId = -1;
      if (pedigree && numNewEdges > 0)
        {
        pedId = pedigree[pidx];
        //std::cout << "pedid: " << pedId << std::endl;
        while(pidx < numPedIDs - 1 && pedId == pedigree[pidx + 1])
          {
          // The next line cell is an inner loop
          if(lines->GetNextCell(npts,pts))
            {
            ++counts[1]; // increment the number of inner loops
            numNewEdges += polyLines2modelEdges(mesh, edgeOp, createdEds, pointsItem,
                                                pts, npts, logger);
            counts.push_back(numNewEdges);
            }
          //std::cout << "inner pedid: " << pedId << std::endl;
          ++pidx;
          }
        }
      if (numNewEdges > 0)
        {
        numEdges += numNewEdges;
        faceSpec->associations()->setValues(createdEds.begin(), createdEds.end());
        //std::cout << "number of created new edges: " << createdEds.size() << std::endl;
        smtk::attribute::IntItem::Ptr orientArr = faceSpec->findInt("orientations");
        std::vector<int> orients (numNewEdges, -1);
        orients[0] = +1; // first one is outer loop
        orientArr->setValues(orients.begin(), orients.end());
        smtk::attribute::IntItem::Ptr countsArr = faceSpec->findInt("counts");
        countsArr->setValues(counts.begin(), counts.end());

        OperatorResult faceResult = faceOp->operate();
        if (faceResult->findInt("outcome")->value() != OPERATION_SUCCEEDED)
          {
          smtkDebugMacro(logger, "\"force create face\" op failed to creat face with given edges.");
          }
        // Add a pedigree ID (if we have it, or -1 otherwise) to each face:
        faceResult->findModelEntity("created")->value(0).setIntegerProperty("pedigree", pedId);
        }
      }
    }
  return numEdges;
}

Import::Import()
{
}

bool Import::ableToOperate()
{
  if(!this->specification()->isValid())
    return false;

  std::string filename = this->specification()->findFile("filename")->value();
  if (filename.empty())
    return false;
  // support 2d models by vtkCMBGeometryReader
  std::string ext = vtksys::SystemTools::GetFilenameLastExtension(filename);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext == ".2dm" ||
#ifdef SMTK_ENABLE_REMUS_SUPPORT
      ext == ".poly" || ext == ".smesh" || ext == ".map" || ext == ".shp" ||
#endif
      ext == ".stl" ||
      ext == ".vtp" ||
      ext == ".vtk")
    {
    return true;
    }

  return false;
}

OperatorResult Import::operateInternal()
{
  std::string filename = this->specification()->findFile("filename")->value();
  if (filename.empty())
    {
    std::cerr << "File name is empty!\n";
    return this->createResult(OPERATION_FAILED);
    }

  vtkPolyData* polyOutput = vtkPolyData::New();

// ******************************************************************************
// This is where we should have the logic to import files other than .cmb formats
// ******************************************************************************
  std::string ext = vtksys::SystemTools::GetFilenameLastExtension(filename);
  if (ext == ".2dm" ||
      ext == ".3dm" ||
#ifdef SMTK_ENABLE_REMUS_SUPPORT
      ext == ".poly" || ext == ".smesh" ||
#endif
  /*  ext == ".tin" ||
      ext == ".fac" ||
      ext == ".obj" ||
      ext == ".sol" || */
      ext == ".stl" ||
      ext == ".vtp")
    {
    vtkNew<smtk::vtk::vtkCMBGeometryReader> reader;
    reader->SetFileName(filename.c_str());
    reader->SetPrepNonClosedSurfaceForModelCreation(false);
    reader->SetEnablePostProcessMesh(false);
    reader->Update();
    polyOutput->ShallowCopy( reader->GetOutput() );
/*
    bool hasBoundaryEdges = reader->GetHasBoundaryEdges();

    if(ext == ".poly" || ext == ".smesh" || hasBoundaryEdges)
      {
      this->m_op->Operate(mod.GetPointer(), reader.GetPointer());
      }
    else
      {
      vtkNew<vtkMasterPolyDataNormals> normals;
      normals->SetInputData(0, reader->GetOutputDataObject(0));
      normals->Update();

      vtkNew<vtkMergeDuplicateCells> merge;
      merge->SetModelRegionArrayName(ModelParserHelper::GetShellTagName());
      merge->SetModelFaceArrayName(ModelParserHelper::GetModelFaceTagName());
      merge->SetInputData(0, normals->GetOutputDataObject(0));
      merge->Update();

      this->m_op->Operate(mod.GetPointer(), merge.GetPointer());
      }
*/
    }
#ifdef SMTK_ENABLE_REMUS_SUPPORT
  else if(ext == ".map")
    {
    vtkNew<smtk::vtk::vtkCMBMapReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    polyOutput->ShallowCopy( reader->GetOutput() );
/*
    vtkNew<vtkCMBTriangleMesher> trimesher;
    trimesher->SetPreserveEdgesAndNodes(true);
    trimesher->SetInputData(0, reader->GetOutputDataObject(0));
    trimesher->Update();

    this->m_mapOp->Operate(mod.GetPointer(), trimesher.GetPointer());
*/
    }
  else if(ext == ".shp")
    {
    vtkNew<smtk::vtk::vtkCMBPolygonModelImporter> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    polyOutput->ShallowCopy( reader->GetOutput() );
/* 
    smtk::attribute::StringItem::Ptr boundaryItem =
      this->specification()->findString("ShapeBoundaryStyle");
    if(boundaryItem->isEnabled())
      {
      vtkNew<smtk::vtk::vtkCMBGeometry2DReader> reader;
      reader->SetFileName(filename.c_str());
      std::string boundaryStyle = boundaryItem->value();
      if (boundaryStyle == "None") // default
        {
        reader->SetBoundaryStyle(smtk::vtk::vtkCMBGeometry2DReader::NONE);
        }
      else if (boundaryStyle == "Relative Margin")
        {
        reader->SetBoundaryStyle(smtk::vtk::vtkCMBGeometry2DReader::RELATIVE_MARGIN);
        smtk::attribute::StringItem::Ptr relMarginItem =
          this->specification()->findString("relative margin");
        reader->SetRelativeMarginString(relMarginItem->value().c_str());
        }
      else if (boundaryStyle == "Absolute Margin")
        {
        reader->SetBoundaryStyle(smtk::vtk::vtkCMBGeometry2DReader::ABSOLUTE_MARGIN);
        smtk::attribute::StringItem::Ptr absMarginItem =
          this->specification()->findString("absolute margin");
        reader->SetAbsoluteMarginString(absMarginItem->value().c_str());
        }
      else if (boundaryStyle == "Bounding Box")
        {
        reader->SetBoundaryStyle(smtk::vtk::vtkCMBGeometry2DReader::ABSOLUTE_BOUNDS);
        smtk::attribute::StringItem::Ptr absBoundsItem =
          this->specification()->findString("absolute bounds");
        reader->SetAbsoluteBoundsString(absBoundsItem->value().c_str());
        }
      else if (boundaryStyle == "Bounding File")
        {
        reader->SetBoundaryStyle(smtk::vtk::vtkCMBGeometry2DReader::IMPORTED_POLYGON);
        smtk::attribute::StringItem::Ptr boundsFileItem =
          this->specification()->findString("imported polygon");
        reader->SetBoundaryFile(boundsFileItem->value().c_str());
        }
      else
        {
        std::cerr << "Invalid Shape file boundary. No boundary will be set.\n";
        reader->SetBoundaryStyle(smtk::vtk::vtkCMBGeometry2DReader::NONE);
        }
      reader->Update();
      polyOutput->ShallowCopy( reader->GetOutput() );
      }
    else
      {
      smtkInfoMacro(log(), "Shape file boundary has to be set.");
      return this->createResult(OPERATION_FAILED);
      }
*/
    }
#endif
  else if(ext == ".vtk")
    {
    vtkNew<vtkPDataSetReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();

    vtkNew<vtkDataSetSurfaceFilter> surface;
    surface->SetInputData(0, reader->GetOutputDataObject(0));
    surface->Update();
    polyOutput->ShallowCopy( surface->GetOutput() );
/*
    vtkNew<vtkMasterPolyDataNormals> normals;
    normals->SetInputData(0, surface->GetOutputDataObject(0));
    normals->Update();

    vtkNew<vtkMergeDuplicateCells> merge;
    merge->SetModelRegionArrayName(ModelParserHelper::GetShellTagName());
    merge->SetModelFaceArrayName(ModelParserHelper::GetModelFaceTagName());
    merge->SetInputData(0, normals->GetOutputDataObject(0));
    merge->Update();

    this->m_op->Operate(mod.GetPointer(), merge.GetPointer());
*/
    }
  else
    {
    smtkInfoMacro(log(), "Unhandled file extension " << ext << ".");
    polyOutput->Delete();
    return this->createResult(OPERATION_FAILED);
    }

  OperatorResult result;
  // First create a model with CreateModel op, then use line cells from reader's
  // output polydata to create edges
  smtk::bridge::polygon::Session* sess = this->polygonSession();
  if (sess)
    {
    smtk::model::Operator::Ptr modOp = sess->op("create model");
    if(!modOp)
      {
      smtkInfoMacro(log(), "Failed to create CreateModel op.");
      result = this->createResult(OPERATION_FAILED);
      }
    //modOp->findInt("model scale")->setValue(1);

    double bds[6];
    polyOutput->GetBounds(bds);
    double diam = 0.0;
    for (int i = 0; i < 3; ++i)
      {
      diam += (bds[2*i + 1] - bds[2*i]) * (bds[2*i + 1] - bds[2*i]);
      }
    diam = sqrt(diam);
    //std::cout << "diam " << diam << "\n";

    // Use the lower-left-front bounds as the origin of the plane.
    // This keeps the projected integer coordinates small when the dataset is not
    // well-centered about the origin and makes overflow less likely.
    // However, it does mean that multiple imported polygon models in the same
    // plane will not share coordinates exactly.
    for (int i = 0; i < 3; ++i)
      {
      modOp->findDouble("origin")->setValue(i, bds[2 * i]);
      }
    // Infer a feature size from the bounds:
    modOp->findDouble("feature size")->setValue(diam / 1000.0);

    OperatorResult modResult = modOp->operate();
    if (modResult->findInt("outcome")->value() != OPERATION_SUCCEEDED)
      {
      smtkInfoMacro(log(), "CreateModel operator failed.");
      result = this->createResult(OPERATION_FAILED);
      }
    /*
    vtkNew<vtkXMLPolyDataWriter> pdw;
    pdw->SetFileName("/tmp/shapepoly.vtp");
    pdw->SetInputDataObject(polyOutput);
    pdw->Write();
    */

    smtk::model::Model model = modResult->findModelEntity("created")->value();
    int numEdges = polyLines2modelEdgesAndFaces(polyOutput, model, sess, log());
    smtkDebugMacro(log(), "Number of edges: " << numEdges << "\n");

    result = this->createResult(OPERATION_SUCCEEDED);
    this->addEntityToResult(result, model, CREATED);

  /*
  //#include "smtk/io/ExportJSON.h"
  //#include "cJSON.h"

    cJSON* json = cJSON_CreateObject();
    smtk::io::ExportJSON::fromModelManager(json, this->manager());
    std::cout << "Result " << cJSON_Print(json) << "\n";
    cJSON_Delete(json);
    */
  /*
  std::string json = smtk::io::ExportJSON::fromModelManager(this->manager());
      std::ofstream file("/tmp/import_op_out.json");
      file << json;
      file.close();
  */
    }
  else
    {
    smtkInfoMacro(log(), "Invalid polygon session.");
    result = this->createResult(OPERATION_FAILED);
    }

  polyOutput->Delete();
  return result;
}

    } // namespace polygon
  } // namespace bridge

} // namespace smtk

smtkImplementsModelOperator(
  SMTKPOLYGONSESSION_EXPORT,
  smtk::bridge::polygon::Import,
  polygon_import,
  "import",
  Import_xml,
  smtk::bridge::polygon::Session);
