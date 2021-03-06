//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCompositeDataDisplayAttributes.h>
#include <vtkCompositeDataIterator.h>
#include <vtkCompositePolyDataMapper2.h>
#include <vtkFieldData.h>
#include <vtkGlyph3DMapper.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkObjectFactory.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>
#include <vtksys/SystemTools.hxx>

#include <vtkPVCacheKeeper.h>
#include <vtkPVRenderView.h>
#include <vtkPVTrivialProducer.h>

#include "vtkDataObjectTreeIterator.h"

#include "smtk/extension/paraview/server/vtkSMTKModelReader.h"
#include "smtk/extension/paraview/server/vtkSMTKModelRepresentation.h"
#include "smtk/extension/paraview/server/vtkSMTKResourceManagerWrapper.h"
#include "smtk/extension/vtk/source/vtkModelMultiBlockSource.h"
#include "smtk/model/Entity.h"
#include "smtk/model/Manager.h"
#include "smtk/resource/Component.h"
#include "smtk/resource/Manager.h"
#include "smtk/view/Selection.h"

namespace
{

void ColorBlockAsEntity(vtkCompositePolyDataMapper2* mapper, vtkDataObject* block,
  const std::string& uuid, const smtk::resource::ResourcePtr& res)
{
  using namespace smtk::model;
  auto modelMan = std::static_pointer_cast<Manager>(res);
  EntityRef entity(modelMan, smtk::common::UUID(uuid));
  FloatList color = entity.color();
  color = color[3] < 0 ? FloatList({ 1., 1., 1., 1. }) : color;

  // FloatList is a typedef for std::vector<double>, so it is safe to
  // pass the raw pointer to its data.
  auto atts = mapper->GetCompositeDataDisplayAttributes();
  atts->SetBlockColor(block, color.data());
}
}

///////////////////////////////////////////////////////////////////////////////
vtkStandardNewMacro(vtkSMTKModelRepresentation);

vtkSMTKModelRepresentation::vtkSMTKModelRepresentation()
  : Superclass()
  , EntityMapper(vtkSmartPointer<vtkCompositePolyDataMapper2>::New())
  , SelectedEntityMapper(vtkSmartPointer<vtkCompositePolyDataMapper2>::New())
  , EntityCacheKeeper(vtkSmartPointer<vtkPVCacheKeeper>::New())
  , GlyphMapper(vtkSmartPointer<vtkGlyph3DMapper>::New())
  , SelectedGlyphMapper(vtkSmartPointer<vtkGlyph3DMapper>::New())
  , Entities(vtkSmartPointer<vtkActor>::New())
  , SelectedEntities(vtkSmartPointer<vtkActor>::New())
  , GlyphEntities(vtkSmartPointer<vtkActor>::New())
  , SelectedGlyphEntities(vtkSmartPointer<vtkActor>::New())
{
  this->SetupDefaults();
  this->SetNumberOfInputPorts(3);
}

vtkSMTKModelRepresentation::~vtkSMTKModelRepresentation() = default;

void vtkSMTKModelRepresentation::SetupDefaults()
{
  vtkNew<vtkCompositeDataDisplayAttributes> compAtt;
  this->EntityMapper->SetCompositeDataDisplayAttributes(compAtt);

  vtkNew<vtkCompositeDataDisplayAttributes> selCompAtt;
  this->SelectedEntityMapper->SetCompositeDataDisplayAttributes(selCompAtt);

  vtkNew<vtkCompositeDataDisplayAttributes> glyphAtt;
  this->GlyphMapper->SetBlockAttributes(glyphAtt);

  vtkNew<vtkCompositeDataDisplayAttributes> selGlyphAtt;
  this->SelectedGlyphMapper->SetBlockAttributes(selGlyphAtt);

  this->Entities->SetMapper(this->EntityMapper);
  this->SelectedEntities->SetMapper(this->SelectedEntityMapper);
  this->GlyphEntities->SetMapper(this->GlyphMapper);
  this->SelectedGlyphEntities->SetMapper(this->SelectedGlyphMapper);

  // Share vtkProperty between model mappers
  this->Property = this->Entities->GetProperty();
  this->GlyphEntities->SetProperty(this->Property);
}

void vtkSMTKModelRepresentation::SetOutputExtent(vtkAlgorithmOutput* output, vtkInformation* inInfo)
{
  if (inInfo->Has(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()))
  {
    vtkPVTrivialProducer* prod = vtkPVTrivialProducer::SafeDownCast(output->GetProducer());
    if (prod)
    {
      prod->SetWholeExtent(inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT()));
    }
  }
}

bool vtkSMTKModelRepresentation::GetModelBounds()
{
  // Entity tessellation bounds
  double entityBounds[6];
  this->GetEntityBounds(this->GetInternalOutputPort(0)->GetProducer()->GetOutputDataObject(0),
    entityBounds, this->EntityMapper->GetCompositeDataDisplayAttributes());

  // Instance placement bounds
  double* instanceBounds = this->GlyphMapper->GetBounds();

  vtkBoundingBox bbox;
  if (vtkBoundingBox::IsValid(entityBounds))
  {
    bbox.AddPoint(entityBounds[0], entityBounds[2], entityBounds[4]);
    bbox.AddPoint(entityBounds[1], entityBounds[3], entityBounds[5]);
  }

  if (vtkBoundingBox::IsValid(instanceBounds))
  {
    bbox.AddPoint(instanceBounds[0], instanceBounds[2], instanceBounds[4]);
    bbox.AddPoint(instanceBounds[1], instanceBounds[3], instanceBounds[5]);
  }

  if (bbox.IsValid())
  {
    bbox.GetBounds(this->DataBounds);
    return true;
  }

  vtkMath::UninitializeBounds(this->DataBounds);
  return false;
}

bool vtkSMTKModelRepresentation::GetEntityBounds(
  vtkDataObject* dataObject, double bounds[6], vtkCompositeDataDisplayAttributes* cdAttributes)
{
  vtkMath::UninitializeBounds(bounds);
  if (vtkCompositeDataSet* cd = vtkCompositeDataSet::SafeDownCast(dataObject))
  {
    // computing bounds with only visible blocks
    vtkCompositeDataDisplayAttributes::ComputeVisibleBounds(cdAttributes, cd, bounds);
    if (vtkBoundingBox::IsValid(bounds))
    {
      return true;
    }
  }
  return false;
}

int vtkSMTKModelRepresentation::RequestData(
  vtkInformation* request, vtkInformationVector** inVec, vtkInformationVector* outVec)
{
  this->EntityCacheKeeper->RemoveAllCaches();
  if (inVec[0]->GetNumberOfInformationObjects() == 1)
  {
    vtkInformation* inInfo = inVec[0]->GetInformationObject(0);
    this->SetOutputExtent(this->GetInternalOutputPort(0), inInfo);

    // Model entities
    this->EntityCacheKeeper->SetInputConnection(this->GetInternalOutputPort(0));

    // Glyph points (2) and prototypes (1)
    this->GlyphMapper->SetInputConnection(this->GetInternalOutputPort(2));
    this->GlyphMapper->SetInputConnection(1, this->GetInternalOutputPort(1));
    this->ConfigureGlyphMapper(this->GlyphMapper.GetPointer());

    this->SelectedGlyphMapper->SetInputConnection(this->GetInternalOutputPort(2));
    this->SelectedGlyphMapper->SetInputConnection(1, this->GetInternalOutputPort(1));
    this->ConfigureGlyphMapper(this->SelectedGlyphMapper.GetPointer());
  }
  this->EntityCacheKeeper->Update();

  this->EntityMapper->Modified();
  this->GlyphMapper->Modified();

  this->GetModelBounds();
  return Superclass::RequestData(request, inVec, outVec);
}

int vtkSMTKModelRepresentation::ProcessViewRequest(
  vtkInformationRequestKey* request_type, vtkInformation* inInfo, vtkInformation* outInfo)
{
  if (!Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
  {
    // i.e. this->GetVisibility() == false, hence nothing to do.
    return 0;
  }

  if (request_type == vtkPVView::REQUEST_UPDATE())
  {
    // provide the "geometry" to the view so the view can deliver it to the
    // rendering nodes as and when needed.
    // When this process doesn't have any valid input, the cache-keeper is setup
    // to provide a place-holder dataset of the right type. This is essential
    // since the vtkPVRenderView uses the type specified to decide on the
    // delivery mechanism, among other things.
    vtkPVRenderView::SetPiece(inInfo, this, this->EntityCacheKeeper->GetOutputDataObject(0), 0, 0);

    // Since we are rendering polydata, it can be redistributed when ordered
    // compositing is needed. So let the view know that it can feel free to
    // redistribute data as and when needed.
    vtkPVRenderView::MarkAsRedistributable(inInfo, this);

    // Tell the view if this representation needs ordered compositing. We need
    // ordered compositing when rendering translucent geometry. We need to extend
    // this condition to consider translucent LUTs once we start supporting them.
    if (this->Entities->HasTranslucentPolygonalGeometry() ||
      this->GlyphEntities->HasTranslucentPolygonalGeometry())
    {
      outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);
    }

    // Finally, let the view know about the geometry bounds. The view uses this
    // information for resetting camera and clip planes. Since this
    // representation allows users to transform the geometry, we need to ensure
    // that the bounds we report include the transformation as well.
    vtkNew<vtkMatrix4x4> matrix;
    this->Entities->GetMatrix(matrix.GetPointer());
    vtkPVRenderView::SetGeometryBounds(inInfo, this->DataBounds, matrix.GetPointer());
  }
  else if (request_type == vtkPVView::REQUEST_UPDATE_LOD())
  {
    /// TODO Add LOD Mappers
  }
  else if (request_type == vtkPVView::REQUEST_RENDER())
  {
    // Update model entity and glyph attributes
    // vtkDataObject* (blocks) in the multi-block may have changed after
    // updating the pipeline, so UpdateEntityAttributes and UpdateInstanceAttributes
    // are called here to ensure the block attributes are updated with the current
    // block pointers. To do this, it uses the flat-index mapped attributes stored in
    // this class.
    auto producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this, 0);
    this->EntityMapper->SetInputConnection(0, producerPort);
    auto data = producerPort->GetProducer()->GetOutputDataObject(0);
    this->UpdateColoringParameters(data);
    this->UpdateRepresentationSubtype();

    // Update selected entities
    // FIXME Selection should only update if it has changed
    auto multiBlock = vtkMultiBlockDataSet::SafeDownCast(data);
    if (multiBlock)
    {
      this->SelectedEntityMapper->SetInputConnection(0, producerPort);
      auto attr = this->SelectedEntityMapper->GetCompositeDataDisplayAttributes();
      this->UpdateSelection(multiBlock, attr, this->SelectedEntities);
    }

    // Update selected glyphs
    // FIXME Selection should only update if it has changed
    multiBlock = vtkMultiBlockDataSet::SafeDownCast(data);
    if (multiBlock)
    {
      auto attr = this->SelectedGlyphMapper->GetBlockAttributes();
      this->UpdateSelection(multiBlock, attr, this->SelectedGlyphEntities);
    }
  }

  return 1;
}

void vtkSMTKModelRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

bool vtkSMTKModelRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->AddActor(this->Entities);
    rview->GetRenderer()->AddActor(this->GlyphEntities);
    rview->GetRenderer()->AddActor(this->SelectedEntities);
    rview->GetRenderer()->AddActor(this->SelectedGlyphEntities);

    rview->RegisterPropForHardwareSelection(this, this->Entities);
    rview->RegisterPropForHardwareSelection(this, this->GlyphEntities);
    rview->RegisterPropForHardwareSelection(this, this->SelectedEntities);
    rview->RegisterPropForHardwareSelection(this, this->SelectedGlyphEntities);

    return Superclass::AddToView(view);
  }
  return false;
}

bool vtkSMTKModelRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->RemoveActor(this->Entities);
    rview->GetRenderer()->RemoveActor(this->GlyphEntities);
    rview->GetRenderer()->RemoveActor(this->SelectedEntities);
    rview->GetRenderer()->RemoveActor(this->SelectedGlyphEntities);

    rview->UnRegisterPropForHardwareSelection(this, this->Entities);
    rview->UnRegisterPropForHardwareSelection(this, this->GlyphEntities);
    rview->UnRegisterPropForHardwareSelection(this, this->SelectedEntities);
    rview->UnRegisterPropForHardwareSelection(this, this->SelectedGlyphEntities);

    return Superclass::RemoveFromView(view);
  }
  return false;
}

void vtkSMTKModelRepresentation::SetVisibility(bool val)
{
  this->Entities->SetVisibility(val);
  this->GlyphEntities->SetVisibility(val);

  this->SelectedEntities->SetVisibility(val);
  this->SelectedGlyphEntities->SetVisibility(val);

  Superclass::SetVisibility(val);
}

static void AddEntityVisibilities(
  vtkCompositeDataDisplayAttributes* attr, std::map<smtk::common::UUID, int>& visdata)
{
  if (!attr)
  {
    return;
  }

  attr->VisitVisibilities([&visdata](vtkDataObject* obj, bool vis) {
    if (!obj)
      return true;

    auto uid = vtkModelMultiBlockSource::GetDataObjectUUID(obj->GetInformation());
    if (uid)
    {
      visdata[uid] |= vis ? 1 : 0; // visibility in any mapper is visibility.
    }
    return true;
  });
}

void vtkSMTKModelRepresentation::GetEntityVisibilities(std::map<smtk::common::UUID, int>& visdata)
{
  visdata.clear();
  AddEntityVisibilities(this->EntityMapper->GetCompositeDataDisplayAttributes(), visdata);
  AddEntityVisibilities(this->SelectedEntityMapper->GetCompositeDataDisplayAttributes(), visdata);
  AddEntityVisibilities(this->GlyphMapper->GetBlockAttributes(), visdata);
  AddEntityVisibilities(this->SelectedGlyphMapper->GetBlockAttributes(), visdata);
}

vtkDataObject* FindEntityData(vtkMultiBlockDataSet* mbds, smtk::model::EntityPtr ent)
{
  if (mbds)
  {
    auto it = mbds->NewTreeIterator();
    it->VisitOnlyLeavesOn();
    for (it->GoToFirstItem(); !it->IsDoneWithTraversal(); it->GoToNextItem())
    {
      vtkDataObject* data = it->GetCurrentDataObject();
      auto uid = vtkModelMultiBlockSource::GetDataObjectUUID(
        it->GetCurrentMetaData()); // data->GetInformation());
      // std::cout << "    " << uid << " == " << ent->id() << "\n";
      if (uid == ent->id())
      {
        it->Delete();
        return data;
      }
    }
    it->Delete();
  }
  return nullptr;
}

bool vtkSMTKModelRepresentation::SetEntityVisibility(smtk::model::EntityPtr ent, bool visible)
{
  vtkDataObject* data;
  // std::cout << "Set visibility " << (visible ? "T" : "F") << " for " << ent->flagSummary() << "\n";
  // Find which mapper should have its visibility modified
  auto mbds = vtkMultiBlockDataSet::SafeDownCast(this->EntityMapper->GetInputDataObject(0, 0));
  vtkCompositeDataDisplayAttributes* attr;
  if ((data = FindEntityData(mbds, ent)))
  {
    attr = this->EntityMapper->GetCompositeDataDisplayAttributes();
    if (attr->GetBlockVisibility(data) != visible)
    {
      attr->SetBlockVisibility(data, visible);
      return true;
    }
    else
    {
      return false;
    }
  }
  mbds = vtkMultiBlockDataSet::SafeDownCast(this->GlyphMapper->GetInputDataObject(0, 0));
  if ((data = FindEntityData(mbds, ent)))
  {
    attr = this->GlyphMapper->GetBlockAttributes();
    if (attr->GetBlockVisibility(data) != visible)
    {
      attr->SetBlockVisibility(data, visible);
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

int vtkSMTKModelRepresentation::FillInputPortInformation(int port, vtkInformation* info)
{
  // Saying INPUT_IS_OPTIONAL() is essential, since representations don't have
  // any inputs on client-side (in client-server, client-render-server mode) and
  // render-server-side (in client-render-server mode).
  if (port == 0)
  {
    // Entity tessellations
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
    return 1;
  }
  if (port == 1)
  {
    // Glyph vertices
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkCompositeDataSet");
    return 1;
  }
  else if (port == 2)
  {
    // Glyph sources
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataObjectTree");
    info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPolyData");
    return 1;
  }

  return 0;
}

void vtkSMTKModelRepresentation::ConfigureGlyphMapper(vtkGlyph3DMapper* mapper)
{
  mapper->SetUseSourceTableTree(true);

  mapper->SetSourceIndexArray(VTK_INSTANCE_SOURCE);
  mapper->SetSourceIndexing(true);

  mapper->SetScaleArray(VTK_INSTANCE_SCALE);
  mapper->SetScaling(true);

  mapper->SetOrientationArray(VTK_INSTANCE_ORIENTATION);
  mapper->SetOrientationMode(vtkGlyph3DMapper::ROTATION);

  mapper->SetMaskArray(VTK_INSTANCE_VISIBILITY);
  mapper->SetMasking(true);
}

void vtkSMTKModelRepresentation::SetMapScalars(int val)
{
  if (val < 0 || val > 1)
  {
    vtkWarningMacro(<< "Invalid parameter for vtkSMTKModelRepresentation::SetMapScalars: " << val);
    val = 0;
  }

  int mapToColorMode[] = { VTK_COLOR_MODE_DIRECT_SCALARS, VTK_COLOR_MODE_MAP_SCALARS };
  this->EntityMapper->SetColorMode(mapToColorMode[val]);
  this->GlyphMapper->SetColorMode(mapToColorMode[val]);
}

void vtkSMTKModelRepresentation::SetInterpolateScalarsBeforeMapping(int val)
{
  this->EntityMapper->SetInterpolateScalarsBeforeMapping(val);
  this->GlyphMapper->SetInterpolateScalarsBeforeMapping(val);
}

void vtkSMTKModelRepresentation::UpdateSelection(
  vtkMultiBlockDataSet* data, vtkCompositeDataDisplayAttributes* blockAttr, vtkActor* actor)
{
  auto rm = vtkSMTKResourceManagerWrapper::Instance(); // TODO: Remove the need for this.
  auto sm = rm ? rm->GetSelection() : nullptr;
  if (!sm)
  {
    actor->SetVisibility(0);
    return;
  }

  auto selection = sm->currentSelection();
  if (selection.empty())
  {
    actor->SetVisibility(0);
    return;
  }

  // std::cout << "Updating rep selection from " << sm << ", have " << selection.size() << " entries in map\n";

  int propVis = 0;
  this->ClearSelection(actor->GetMapper());
  for (auto& item : selection)
  {
    auto matchedBlock = this->FindNode(data, item.first->id().toString());
    if (matchedBlock)
    {
      propVis = 1;
      blockAttr->SetBlockVisibility(matchedBlock, true);
      blockAttr->SetBlockColor(matchedBlock, this->SelectionColor);
    }
  }
  actor->SetVisibility(propVis);

  // This is necessary to force an update in the mapper
  actor->GetMapper()->Modified();
}

vtkDataObject* vtkSMTKModelRepresentation::FindNode(
  vtkMultiBlockDataSet* data, const std::string& uuid)
{
  const int numBlocks = data->GetNumberOfBlocks();
  for (int index = 0; index < numBlocks; index++)
  {
    auto currentBlock = data->GetBlock(index);
    if (data->HasMetaData(index))
    {
      auto currentId = data->GetMetaData(index)->Get(vtkModelMultiBlockSource::ENTITYID());
      if (currentId)
      {
        const std::string currentIdStr = currentId;
        if (currentIdStr.compare(uuid) == 0)
        {
          return currentBlock;
        }
      }
    }

    auto childBlock = vtkMultiBlockDataSet::SafeDownCast(currentBlock);
    if (childBlock)
    {
      auto matchedNode = this->FindNode(childBlock, uuid);
      if (matchedNode)
      {
        return matchedNode;
      }
    }
  }

  return nullptr;
}

void vtkSMTKModelRepresentation::ClearSelection(vtkMapper* mapper)
{
  auto clearAttributes = [](vtkCompositeDataDisplayAttributes* attr) {
    attr->RemoveBlockVisibilities();
    attr->RemoveBlockColors();
  };

  auto cpdm = vtkCompositePolyDataMapper2::SafeDownCast(mapper);
  if (cpdm)
  {
    auto blockAttr = cpdm->GetCompositeDataDisplayAttributes();
    clearAttributes(blockAttr);
    auto data = cpdm->GetInputDataObject(0, 0);

    // For vtkCompositePolyDataMapper2, setting the top node as false is enough
    // since the state of the top node will stream down to its nodes.
    blockAttr->SetBlockVisibility(data, false);
    return;
  }

  auto gm = vtkGlyph3DMapper::SafeDownCast(mapper);
  if (gm)
  {
    auto blockAttr = gm->GetBlockAttributes();
    clearAttributes(blockAttr);

    // Glyph3DMapper does not behave as vtkCompositePolyDataMapper2, hence it is
    // necessary to update the block visibility of each node directly.
    auto mbds = vtkMultiBlockDataSet::SafeDownCast(gm->GetInputDataObject(0, 0));
    vtkCompositeDataIterator* iter = mbds->NewIterator();

    iter->GoToFirstItem();
    while (!iter->IsDoneWithTraversal())
    {
      auto dataObj = iter->GetCurrentDataObject();
      blockAttr->SetBlockVisibility(dataObj, false);
      iter->GoToNextItem();
    }
    iter->Delete();
    return;
  }
}

void vtkSMTKModelRepresentation::SetResource(smtk::resource::ResourcePtr res)
{
  this->Resource = res;
}

void vtkSMTKModelRepresentation::SetColorBy(const char* type)
{
  if (vtksys::SystemTools::Strucmp(type, "Entity") == 0)
  {
    this->SetColorBy(ENTITY);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Volume") == 0)
  {
    this->SetColorBy(VOLUME);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Scalars") == 0)
  {
    this->SetColorBy(SCALARS);
  }
  else
  {
    vtkErrorMacro("Invalid type: " << type);
  }

  // Foce update of ColorBy
  this->UpdateColorBy = true;

  // Force update of internal attributes
  this->BlockAttrChanged = true;
  this->InstanceAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetRepresentation(const char* type)
{
  if (vtksys::SystemTools::Strucmp(type, "Points") == 0)
  {
    this->SetRepresentation(POINTS);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Wireframe") == 0)
  {
    this->SetRepresentation(WIREFRAME);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Surface") == 0)
  {
    this->SetRepresentation(SURFACE);
  }
  else if (vtksys::SystemTools::Strucmp(type, "Surface With Edges") == 0)
  {
    this->SetRepresentation(SURFACE_WITH_EDGES);
  }
  else
  {
    vtkErrorMacro("Invalid type: " << type);
  }
}

void vtkSMTKModelRepresentation::SetSelectionPointSize(double val)
{
  this->SelectedEntities->GetProperty()->SetPointSize(val);
  this->SelectedGlyphEntities->GetProperty()->SetPointSize(val);
}

void vtkSMTKModelRepresentation::SetLookupTable(vtkScalarsToColors* val)
{
  this->EntityMapper->SetLookupTable(val);
  this->GlyphMapper->SetLookupTable(val);
}

void vtkSMTKModelRepresentation::SetSelectionLineWidth(double val)
{
  this->SelectedEntities->GetProperty()->SetLineWidth(val);
  this->SelectedGlyphEntities->GetProperty()->SetLineWidth(val);
}

void vtkSMTKModelRepresentation::SetPointSize(double val)
{
  this->Property->SetPointSize(val);
}

void vtkSMTKModelRepresentation::SetLineWidth(double val)
{
  this->Property->SetLineWidth(val);
}

void vtkSMTKModelRepresentation::SetLineColor(double r, double g, double b)
{
  this->Property->SetEdgeColor(r, g, b);
}

void vtkSMTKModelRepresentation::SetOpacity(double val)
{
  this->Property->SetOpacity(val);
}

void vtkSMTKModelRepresentation::SetPosition(double x, double y, double z)
{
  this->Entities->SetPosition(x, y, z);
  this->GlyphEntities->SetPosition(x, y, z);
}

void vtkSMTKModelRepresentation::SetScale(double x, double y, double z)
{
  this->Entities->SetScale(x, y, z);
  this->GlyphEntities->SetScale(x, y, z);
}

void vtkSMTKModelRepresentation::SetOrientation(double x, double y, double z)
{
  this->Entities->SetOrientation(x, y, z);
  this->GlyphEntities->SetOrientation(x, y, z);
}

void vtkSMTKModelRepresentation::SetOrigin(double x, double y, double z)
{
  this->Entities->SetOrigin(x, y, z);
  this->GlyphEntities->SetOrigin(x, y, z);
}

void vtkSMTKModelRepresentation::SetUserTransform(const double matrix[16])
{
  vtkNew<vtkTransform> transform;
  transform->SetMatrix(matrix);
  this->Entities->SetUserTransform(transform.GetPointer());
  this->GlyphEntities->SetUserTransform(transform.GetPointer());
}

void vtkSMTKModelRepresentation::SetPickable(int val)
{
  this->Entities->SetPickable(val);
  this->GlyphEntities->SetPickable(val);
}

void vtkSMTKModelRepresentation::SetTexture(vtkTexture* val)
{
  this->Entities->SetTexture(val);
  this->GlyphEntities->SetTexture(val);
}

void vtkSMTKModelRepresentation::SetSpecularPower(double val)
{
  this->Property->SetSpecularPower(val);
}

void vtkSMTKModelRepresentation::SetSpecular(double val)
{
  this->Property->SetSpecular(val);
}

void vtkSMTKModelRepresentation::SetAmbient(double val)
{
  this->Property->SetAmbient(val);
}

void vtkSMTKModelRepresentation::SetDiffuse(double val)
{
  this->Property->SetDiffuse(val);
}

void vtkSMTKModelRepresentation::UpdateRepresentationSubtype()
{
  // Adjust material properties.
  double diffuse = this->Diffuse;
  double specular = this->Specular;
  double ambient = this->Ambient;

  if (this->Representation != SURFACE && this->Representation != SURFACE_WITH_EDGES)
  {
    diffuse = 0.0;
    ambient = 1.0;
  }

  this->Property->SetAmbient(ambient);
  this->Property->SetSpecular(specular);
  this->Property->SetDiffuse(diffuse);

  switch (this->Representation)
  {
    case SURFACE_WITH_EDGES:
      this->Property->SetEdgeVisibility(1);
      this->Property->SetRepresentation(VTK_SURFACE);
      break;

    default:
      this->Property->SetEdgeVisibility(0);
      this->Property->SetRepresentation(this->Representation);
  }
}

void vtkSMTKModelRepresentation::UpdateColoringParameters(vtkDataObject* data)
{
  auto multiBlock = vtkMultiBlockDataSet::SafeDownCast(data);
  switch (this->ColorBy)
  {
    case VOLUME:
      this->ColorByVolume(multiBlock);
      break;
    case ENTITY:
      this->ColorByEntity(multiBlock);
      break;
    case SCALARS:
      this->ColorByScalars();
      break;
    default:
      this->ColorByScalars();
      break;
  }

  if (this->UseInternalAttributes)
  {
    this->ApplyInternalBlockAttributes();
  }
}

void vtkSMTKModelRepresentation::ColorByScalars()
{
  this->EntityMapper->GetCompositeDataDisplayAttributes()->RemoveBlockColors();

  bool using_scalar_coloring = false;
  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    const char* colorArrayName = info->Get(vtkDataObject::FIELD_NAME());
    int fieldAssociation = info->Get(vtkDataObject::FIELD_ASSOCIATION());
    if (colorArrayName && colorArrayName[0])
    {
      this->EntityMapper->SetScalarVisibility(1);
      this->EntityMapper->SelectColorArray(colorArrayName);
      this->EntityMapper->SetUseLookupTableScalarRange(1);
      this->GlyphMapper->SetScalarVisibility(1);
      this->GlyphMapper->SelectColorArray(colorArrayName);
      this->GlyphMapper->SetUseLookupTableScalarRange(1);
      switch (fieldAssociation)
      {
        case vtkDataObject::FIELD_ASSOCIATION_CELLS:
          this->EntityMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
          this->GlyphMapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
          break;

        case vtkDataObject::FIELD_ASSOCIATION_NONE:
          this->EntityMapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
          this->GlyphMapper->SetScalarMode(VTK_SCALAR_MODE_USE_FIELD_DATA);
          // Color entire block by zeroth tuple in the field data
          this->EntityMapper->SetFieldDataTupleId(0);
          this->GlyphMapper->SetFieldDataTupleId(0);
          break;

        case vtkDataObject::FIELD_ASSOCIATION_POINTS:
        default:
          this->EntityMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
          this->GlyphMapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
          break;
      }
      using_scalar_coloring = true;
    }
  }

  if (!using_scalar_coloring)
  {
    this->EntityMapper->SetScalarVisibility(0);
    this->EntityMapper->SelectColorArray(nullptr);
    this->GlyphMapper->SetScalarVisibility(0);
    this->GlyphMapper->SelectColorArray(nullptr);
  }
}

void vtkSMTKModelRepresentation::ColorByVolume(vtkCompositeDataSet* data)
{
  if (!this->UpdateColorBy)
    return;

  // Traverse the blocks and set the volume's color
  this->EntityMapper->GetCompositeDataDisplayAttributes()->RemoveBlockColors();
  vtkCompositeDataIterator* it = data->NewIterator();
  it->GoToFirstItem();
  while (!it->IsDoneWithTraversal())
  {
    auto dataObj = it->GetCurrentDataObject();
    auto arr = vtkStringArray::SafeDownCast(
      dataObj->GetFieldData()->GetAbstractArray(vtkModelMultiBlockSource::GetVolumeTagName()));
    if (arr)
    {
      if (!this->Resource)
      {
        vtkErrorMacro(<< "Invalid Resource!");
        return;
      }

      // FIXME Do something with additional volumes this block might be bounding
      // (currently only using the first one)
      ColorBlockAsEntity(this->EntityMapper, dataObj, arr->GetValue(0), this->Resource);
    }
    else
    {
      // Set a default color
      std::array<double, 3> white = { { 1., 1., 1. } };
      this->EntityMapper->GetCompositeDataDisplayAttributes()->SetBlockColor(dataObj, white.data());
    }
    it->GoToNextItem();
  }
  it->Delete();
  this->UpdateColorBy = false;
}

void vtkSMTKModelRepresentation::ColorByEntity(vtkMultiBlockDataSet* data)
{
  if (!this->UpdateColorBy)
    return;

  // Traverse the blocks and set the entity's color
  this->EntityMapper->GetCompositeDataDisplayAttributes()->RemoveBlockColors();
  vtkCompositeDataIterator* it = data->NewIterator();
  it->GoToFirstItem();
  while (!it->IsDoneWithTraversal())
  {
    auto dataObj = it->GetCurrentDataObject();
    if (data->HasMetaData(it))
    {
      auto uuid = data->GetMetaData(it)->Get(vtkModelMultiBlockSource::ENTITYID());
      if (uuid)
      {
        ColorBlockAsEntity(this->EntityMapper, dataObj, uuid, this->Resource);
      }
    }
    it->GoToNextItem();
  }
  it->Delete();
  this->UpdateColorBy = false;
}

void vtkSMTKModelRepresentation::ApplyInternalBlockAttributes()
{
  // Update glyph attributes
  auto data = this->GetInternalOutputPort(0)->GetProducer()->GetOutputDataObject(0);
  if (this->BlockAttributeTime < data->GetMTime() || this->BlockAttrChanged)
  {
    this->ApplyEntityAttributes(this->EntityMapper.GetPointer());
    this->BlockAttributeTime.Modified();
    this->BlockAttrChanged = false;
  }

  data = this->GetInternalOutputPort(2)->GetProducer()->GetOutputDataObject(0);
  if (this->InstanceAttributeTime < data->GetMTime() || this->InstanceAttrChanged)
  {
    this->ApplyGlyphBlockAttributes(this->GlyphMapper.GetPointer());
    this->InstanceAttributeTime.Modified();
    this->InstanceAttrChanged = false;
  }
}

void vtkSMTKModelRepresentation::ApplyEntityAttributes(vtkMapper* mapper)
{
  auto cpm = vtkCompositePolyDataMapper2::SafeDownCast(mapper);
  if (!cpm)
  {
    vtkErrorMacro(<< "Invalid mapper!");
    return;
  }

  cpm->RemoveBlockVisibilities();
  for (auto const& item : this->BlockVisibilities)
  {
    cpm->SetBlockVisibility(item.first, item.second);
  }

  // Do not call RemoveBlockColors, since some block attributes could
  // have been set through ColorBy mode
  for (auto const& item : this->BlockColors)
  {
    auto& arr = item.second;
    double color[3] = { arr[0], arr[1], arr[2] };
    cpm->SetBlockColor(item.first, color);
  }

  cpm->RemoveBlockOpacities();
  for (auto const& item : this->BlockOpacities)
  {
    cpm->SetBlockOpacity(item.first, item.second);
  }
}

void vtkSMTKModelRepresentation::ApplyGlyphBlockAttributes(vtkGlyph3DMapper* mapper)
{
  auto instanceData = mapper->GetInputDataObject(0, 0);
  auto blockAttr = mapper->GetBlockAttributes();

  blockAttr->RemoveBlockVisibilities();
  for (auto const& item : this->InstanceVisibilities)
  {
    unsigned int currentIdx = 0;
    auto dob = blockAttr->DataObjectFromIndex(item.first, instanceData, currentIdx);

    if (dob)
    {
      blockAttr->SetBlockVisibility(dob, item.second);
    }
  }

  blockAttr->RemoveBlockColors();
  for (auto const& item : this->InstanceColors)
  {
    unsigned int currentIdx = 0;
    auto dob = blockAttr->DataObjectFromIndex(item.first, instanceData, currentIdx);

    if (dob)
    {
      auto& arr = item.second;
      double color[3] = { arr[0], arr[1], arr[2] };
      blockAttr->SetBlockColor(dob, color);
    }
  }
  // Opacity currently not supported by vtkGlyph3DMapper

  mapper->Modified();
}

void vtkSMTKModelRepresentation::SetBlockVisibility(unsigned int index, bool visible)
{
  this->BlockVisibilities[index] = visible;
  this->BlockAttrChanged = true;
}

bool vtkSMTKModelRepresentation::GetBlockVisibility(unsigned int index) const
{
  auto it = this->BlockVisibilities.find(index);
  if (it == this->BlockVisibilities.cend())
  {
    return true;
  }
  return it->second;
}

void vtkSMTKModelRepresentation::RemoveBlockVisibility(unsigned int index, bool)
{
  auto it = this->BlockVisibilities.find(index);
  if (it == this->BlockVisibilities.cend())
  {
    return;
  }
  this->BlockVisibilities.erase(it);
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::RemoveBlockVisibilities()
{
  this->BlockVisibilities.clear();
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetBlockColor(unsigned int index, double r, double g, double b)
{
  std::array<double, 3> color = { { r, g, b } };
  this->BlockColors[index] = color;
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetBlockColor(unsigned int index, double* color)
{
  if (color)
  {
    this->SetBlockColor(index, color[0], color[1], color[2]);
  }
}

double* vtkSMTKModelRepresentation::GetBlockColor(unsigned int index)
{
  auto it = this->BlockColors.find(index);
  if (it == this->BlockColors.cend())
  {
    return nullptr;
  }
  return it->second.data();
}

void vtkSMTKModelRepresentation::RemoveBlockColor(unsigned int index)
{
  auto it = this->BlockColors.find(index);
  if (it == this->BlockColors.cend())
  {
    return;
  }
  this->BlockColors.erase(it);
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::RemoveBlockColors()
{
  this->BlockColors.clear();
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetBlockOpacity(unsigned int index, double opacity)
{
  this->BlockOpacities[index] = opacity;
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetBlockOpacity(unsigned int index, double* opacity)
{
  if (opacity)
  {
    this->SetBlockOpacity(index, *opacity);
  }
}

double vtkSMTKModelRepresentation::GetBlockOpacity(unsigned int index)
{
  auto it = this->BlockOpacities.find(index);
  if (it == this->BlockOpacities.cend())
  {
    return 0.0;
  }
  return it->second;
}

void vtkSMTKModelRepresentation::RemoveBlockOpacity(unsigned int index)
{
  auto it = this->BlockOpacities.find(index);
  if (it == this->BlockOpacities.cend())
  {
    return;
  }
  this->BlockOpacities.erase(it);
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::RemoveBlockOpacities()
{
  this->BlockOpacities.clear();
  this->BlockAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetInstanceVisibility(unsigned int index, bool visible)
{
  this->InstanceVisibilities[index] = visible;
  this->InstanceAttrChanged = true;
}

bool vtkSMTKModelRepresentation::GetInstanceVisibility(unsigned int index) const
{
  auto it = this->InstanceVisibilities.find(index);
  if (it == this->InstanceVisibilities.cend())
  {
    return true;
  }
  return it->second;
}

void vtkSMTKModelRepresentation::RemoveInstanceVisibility(unsigned int index, bool)
{
  auto it = this->InstanceVisibilities.find(index);
  if (it == this->InstanceVisibilities.cend())
  {
    return;
  }
  this->InstanceVisibilities.erase(it);
  this->InstanceAttrChanged = true;
}

void vtkSMTKModelRepresentation::RemoveInstanceVisibilities()
{
  this->InstanceVisibilities.clear();
  this->InstanceAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetInstanceColor(unsigned int index, double r, double g, double b)
{
  std::array<double, 3> color = { { r, g, b } };
  this->InstanceColors[index] = color;
  this->InstanceAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetInstanceColor(unsigned int index, double* color)
{
  if (color)
  {
    this->SetInstanceColor(index, color[0], color[1], color[2]);
  }
}

double* vtkSMTKModelRepresentation::GetInstanceColor(unsigned int index)
{
  auto it = this->InstanceColors.find(index);
  if (it == this->InstanceColors.cend())
  {
    return nullptr;
  }
  return it->second.data();
}

void vtkSMTKModelRepresentation::RemoveInstanceColor(unsigned int index)
{
  auto it = this->InstanceColors.find(index);
  if (it == this->InstanceColors.cend())
  {
    return;
  }
  this->InstanceColors.erase(it);
  this->InstanceAttrChanged = true;
}

void vtkSMTKModelRepresentation::RemoveInstanceColors()
{
  this->InstanceColors.clear();
  this->InstanceAttrChanged = true;
}

void vtkSMTKModelRepresentation::SetUseInternalAttributes(bool enable)
{
  this->UseInternalAttributes = enable;

  // Force update of internal attributes
  this->BlockAttrChanged = true;
  this->InstanceAttrChanged = true;

  // Force update of ColorBy
  this->UpdateColorBy = true;
}
