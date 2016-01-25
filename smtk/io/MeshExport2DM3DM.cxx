//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================


#include "smtk/io/MeshExport2DM.h"
#include "smtk/io/MeshExport3DM.h"

#include "smtk/mesh/CellTypes.h"
#include "smtk/mesh/CellSet.h"
#include "smtk/mesh/Collection.h"
#include "smtk/mesh/DimensionTypes.h"
#include "smtk/mesh/ExtractTessellation.h"
#include "smtk/mesh/MeshSet.h"

#include "smtk/model/EntityRef.h"
#include "smtk/model/Manager.h"

#include <fstream>
#include <iomanip>

namespace smtk {
  namespace io {

namespace {

//----------------------------------------------------------------------------
struct OpenFile
{
  OpenFile(const std::string& path):
    m_stream(path.c_str(), std::ios::out)
    {
    this->m_path = path;
    this->m_canWrite = this->m_stream.is_open(); //verify file is open
    this->m_deleteFile = true;
    }

  ~OpenFile()
    {
    if(this->m_canWrite)
      {
      this->m_stream.close();

      if(this->m_deleteFile)
        {
        std::remove(this->m_path.c_str());
        }
      }
    }

  void fileWritten(bool written)
  {
    //If the file was written, we don't need to delete it
    this->m_deleteFile = !written;
  }




  std::string m_path;
  std::fstream m_stream;
  bool m_canWrite;
  bool m_deleteFile;
};

//----------------------------------------------------------------------------
struct MeshByRegion
{
  MeshByRegion(const smtk::mesh::MeshSet& ms,
               int regionId,
               smtk::mesh::DimensionType dim):
    m_meshSet(ms),
    m_dim(dim),
    m_regionId(regionId)
  { }

  int region() const { return m_regionId; }

  //return number cells of the set dimension
  std::size_t numCells() const
    { return this->m_meshSet.cells(m_dim).size(); }

  //return all the cells of a given type
  smtk::mesh::CellSet cells(const smtk::mesh::CellType& type) const
    { return this->m_meshSet.cells(type); }

  //return all the cells, restricted to the dimension passed in during
  //the constructor
  smtk::mesh::CellSet cells() const
    { return this->m_meshSet.cells(m_dim); }


  smtk::mesh::MeshSet m_meshSet;
  smtk::mesh::DimensionType m_dim;
  int m_regionId;
};

//----------------------------------------------------------------------------
std::string to_CardType(smtk::mesh::CellType type)
{

 if(type == smtk::mesh::Line)
  {
    return std::string("E2L");
  }
 if(type == smtk::mesh::Triangle)
  {
    return std::string("E3T");
  }
 else if(type == smtk::mesh::Quad)
  {
    return std::string("E4Q");
  }
 else if(type == smtk::mesh::Tetrahedron)
  {
    return std::string("E4T");
  }
 else if(type == smtk::mesh::Pyramid)
  {
    return std::string("E5P");
  }
 else if(type == smtk::mesh::Wedge)
  {
    return std::string("E6W");
  }
 else if(type == smtk::mesh::Hexahedron)
  {
    return std::string("E8H");
  }
 return std::string("B4D");
}

//----------------------------------------------------------------------------
class WriteCellsPerRegion
{
  smtk::mesh::PointSet m_PointSet;
  std::ostream& m_Stream;
  int m_CellId;

public:
  WriteCellsPerRegion(const smtk::mesh::PointSet& ps,
                      std::ostream& stream):
    m_PointSet(ps),
    m_Stream(stream),
    m_CellId(1) //2dm/3dm requires id values to start at 1
  {
  }

  //--------------------------------------------------------------------------
  void operator()(const MeshByRegion& mbr,
                  const smtk::mesh::CellType& type)
  {
    const int regionId = mbr.region();
    smtk::mesh::CellSet cells = mbr.cells(type);

    //If we have zero cells, don't do anything
    if(cells.is_empty())
      {
      return;
      }

    //Use the extractTessellation helpers to convert the connectivity
    //to map properly to the PointSet that represents ALL points we are
    //using, not just the points these cells are using
    boost::int64_t connectivityLen = cells.pointConnectivity().size();
    std::vector<boost::int64_t> conn(connectivityLen);

    smtk::mesh::PreAllocatedTessellation connectivityInfo(&conn[0]);
    connectivityInfo.disableVTKStyleConnectivity(true);
    smtk::mesh::extractTessellation( cells, this->m_PointSet, connectivityInfo);

    //now we just need to write out the cells
    std::string cardType = to_CardType(type);
    int nVerts = smtk::mesh::verticesPerCell(type);
    std::size_t nCells = cells.size();
    for(std::size_t i=0; i < nCells; ++i)
      {
      this->m_Stream << cardType << " \t " << this->m_CellId++ << " ";
      for(int j=0; j < nVerts; ++j)
        {
        this->m_Stream << std::setw(8) << conn[nVerts*i + j] << " ";
        }
      this->m_Stream << std::setw(8) << regionId << std::endl;
      }
  }

};

//----------------------------------------------------------------------------
std::vector<MeshByRegion> subsetByRegion(smtk::mesh::CollectionPtr collection,
                                         smtk::mesh::DimensionType type)
{
  std::vector<MeshByRegion> meshesByModelRef;
  smtk::mesh::MeshSet meshes = collection->meshes(type);
  if(meshes.is_empty())
    { //if we have no meshes, stop
    return meshesByModelRef;
    }

  smtk::model::EntityRefArray modelIds = meshes.modelEntities();

  if(modelIds.size() <= 1)
    { //explicitly done so that no model relationship is equal to everything
      //being in the same region
    meshesByModelRef.push_back( MeshByRegion(meshes, 1, type) );
    }
  else
    {
    meshesByModelRef.reserve(modelIds.size());
    int region = 1;

    typedef smtk::model::EntityRefArray::const_iterator it;
    for(it i=modelIds.begin(); i!=modelIds.end(); ++i)
      {
      smtk::mesh::MeshSet subset = collection->findAssociatedMeshes(*i, type);
      if(!subset.is_empty())
        {
        meshesByModelRef.push_back( MeshByRegion(subset, region, type) );
        ++region;
        }
      }
    }
  return meshesByModelRef;
}

//----------------------------------------------------------------------------
std::vector<MeshByRegion> subsetByModelProperty(smtk::mesh::CollectionPtr collection,
                                                smtk::model::ManagerPtr manager,
                                                const std::string& modelPropertyName,
                                                smtk::mesh::DimensionType type)
{
  std::vector<MeshByRegion> meshesByModelRef;
  smtk::mesh::MeshSet meshes = collection->meshes(type);
  if(meshes.is_empty())
    { //if we have no meshes, stop
    return meshesByModelRef;
    }

  smtk::model::EntityRefArray modelIds = meshes.modelEntities();
  if(modelIds.size() <= 1)
    { //if don't have any associations to the model we should fail
    return meshesByModelRef;
    }

  typedef smtk::model::EntityRefArray::const_iterator it;
  for(it i=modelIds.begin(); i!=modelIds.end(); ++i)
    {
    const smtk::model::IntegerList& values = manager->integerProperty(i->entity(), modelPropertyName);
    smtk::mesh::MeshSet subset = collection->findAssociatedMeshes(*i, type);
    if(values.size() == 1 && !subset.is_empty())
      { //only accept model properties that have single values
        //since that is what we think region id's should be
      const int& region = values[0];
      meshesByModelRef.push_back( MeshByRegion(subset, region, type) );
      }
    }

  return meshesByModelRef;
}

//----------------------------------------------------------------------------
smtk::mesh::PointSet pointsUsed(const std::vector<MeshByRegion>& meshes)
{
  //make a single mesh that represents all the cells of a given dimension
  smtk::mesh::CellSet cs = meshes[0].cells();
  const std::size_t size = meshes.size();
  for(std::size_t i=1; i < size; ++i)
    {
    cs.append( meshes[i].cells() );
    }
  return cs.points();
}

//----------------------------------------------------------------------------
bool write_dm(const std::vector<MeshByRegion>& meshes,
              std::ostream& stream,
              smtk::mesh::DimensionType type)
{
  smtk::mesh::PointSet pointSet = pointsUsed(meshes);

  //write the header block to the stream
  if(type == smtk::mesh::Dims1)
    {
    stream << "MESH1D" << std::endl;
    }
  else if(type == smtk::mesh::Dims2)
    {
    stream << "MESH2D" << std::endl;
    }
  else if(type == smtk::mesh::Dims3)
    {
    stream << "MESH3D" << std::endl;
    }
  else
    { //bad dimension bail!
    return false;
    }

  //write out some comment lines that contain
  //info on the number of cells and points
  const std::size_t numMeshes = meshes.size();
  std::size_t numPoints = pointSet.numberOfPoints();
  std::size_t numCells = 0;
  for(std::size_t i=0; i < numMeshes; ++i)
    { numCells += meshes[i].numCells(); }

  stream << "#NELEM " << numCells << std::endl;
  stream << "#NNODE " << numPoints << std::endl;

  //now that we have the meshes on a per region basis we can
  //start to dump them to file
  //we just need to:
  //Determine all the cells that will
  //1. Iterate the meshes
  //2. For each mesh subset by cell type
  //3. iterate the cells in each subtype
  //4. dump out the points
  WriteCellsPerRegion writer(pointSet, stream);
  for(std::size_t i=0; i < numMeshes; ++i)
    {
    const MeshByRegion& mbr = meshes[i];
    //subset the cells of the requested dimension by type
    if(type == 2)
      {
      writer( mbr, smtk::mesh::Triangle );
      writer( mbr, smtk::mesh::Quad );
      }
    else
      { //we presume 3d
      writer( mbr, smtk::mesh::Tetrahedron );
      writer( mbr, smtk::mesh::Pyramid );
      writer( mbr, smtk::mesh::Wedge );
      writer( mbr, smtk::mesh::Hexahedron );
      }
    }

  //now that the cells have been written
  //we do step 4. For step 4 we manual extract
  //the points

  std::vector<double> xyz( numPoints * 3);
  pointSet.get(&xyz[0]); //fill our buffer

  for(std::size_t i=0; i < numPoints; ++i)
    {
    stream << "ND \t "
               << std::setw(8) << 1 + i << " " << std::fixed
               << std::setw(12) <<  xyz[3 * i] << " "
               << std::setw(12) <<  xyz[(3 * i)+1] << " "
               << std::setw(12) <<  xyz[(3 * i)+2] << std::endl;
    }

  return true;
}

//----------------------------------------------------------------------------
bool write_dm(smtk::mesh::CollectionPtr collection,
              std::ostream& stream,
              smtk::mesh::DimensionType type)
{
  if(!collection)
    { //can't write out an empty collection
    return false;
    }

  std::vector<MeshByRegion> meshes  = subsetByRegion(collection, type);
  if(meshes.size() == 0)
    { //nothing to write out
    return false;
    }

  return write_dm(meshes,stream,type);

}

//----------------------------------------------------------------------------
bool write_dm(smtk::mesh::CollectionPtr collection,
              smtk::model::ManagerPtr manager,
              const std::string& modelPropertyName,
              std::ostream& stream,
              smtk::mesh::DimensionType type)
{
  if(!collection)
    { //can't write out an empty collection
    return false;
    }

  std::vector<MeshByRegion> meshes  = subsetByModelProperty(collection,
                                                            manager,
                                                            modelPropertyName,
                                                            type);
  if(meshes.size() == 0)
    { //nothing to write out
    return false;
    }

  return write_dm(meshes,stream,type);
}

}

//----------------------------------------------------------------------------
MeshExport2DM::MeshExport2DM()
{

}

//----------------------------------------------------------------------------
bool MeshExport2DM::write(smtk::mesh::CollectionPtr collection,
                          const std::string& filePath) const
{
  bool result = false;
  OpenFile of(filePath);
  if(of.m_canWrite)
  {
    result = this->write(collection, of.m_stream);
    of.fileWritten(result);
  }
  return result;
}

//----------------------------------------------------------------------------
bool MeshExport2DM::write(smtk::mesh::CollectionPtr collection,
                          smtk::model::ManagerPtr manager,
                          const std::string& modelPropertyName,
                          const std::string& filePath) const
{
  bool result = false;
  OpenFile of(filePath);
  if(of.m_canWrite)
  {
    result = this->write(collection,manager,modelPropertyName,of.m_stream);
    of.fileWritten(result);
  }
  return result;
}

//----------------------------------------------------------------------------
bool MeshExport2DM::write(smtk::mesh::CollectionPtr collection,
                          std::ostream& stream) const
{
  return write_dm(collection, stream, smtk::mesh::Dims2);
}

//----------------------------------------------------------------------------
bool MeshExport2DM::write(smtk::mesh::CollectionPtr collection,
                          smtk::model::ManagerPtr manager,
                          const std::string& modelPropertyName,
                          std::ostream& stream) const
{
  return write_dm(collection, manager, modelPropertyName, stream, smtk::mesh::Dims2);
}

//----------------------------------------------------------------------------
MeshExport3DM::MeshExport3DM()
{

}

//----------------------------------------------------------------------------
bool MeshExport3DM::write(smtk::mesh::CollectionPtr collection,
                          const std::string& filePath) const
{
  bool result = false;
  OpenFile of(filePath);
  if(of.m_canWrite)
  {
    result = this->write(collection, of.m_stream);
    of.fileWritten(result);
  }
  return result;
}

//----------------------------------------------------------------------------
bool MeshExport3DM::write(smtk::mesh::CollectionPtr collection,
                          smtk::model::ManagerPtr manager,
                          const std::string& modelPropertyName,
                          const std::string& filePath) const
{
  bool result = false;
  OpenFile of(filePath);
  if(of.m_canWrite)
  {
    result = this->write(collection,manager,modelPropertyName,of.m_stream);
    of.fileWritten(result);
  }
  return result;
}

//----------------------------------------------------------------------------
bool MeshExport3DM::write(smtk::mesh::CollectionPtr collection,
                          std::ostream& stream) const
{
  return write_dm(collection, stream, smtk::mesh::Dims3);
}

//----------------------------------------------------------------------------
bool MeshExport3DM::write(smtk::mesh::CollectionPtr collection,
                          smtk::model::ManagerPtr manager,
                          const std::string& modelPropertyName,
                          std::ostream& stream) const
{
  return write_dm(collection, manager, modelPropertyName, stream, smtk::mesh::Dims3);
}

}
}
