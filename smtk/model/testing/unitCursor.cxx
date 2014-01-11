#include "smtk/model/Cursor.h"

#include "smtk/model/CellEntity.h"
#include "smtk/model/Chain.h"
#include "smtk/model/ExportJSON.h"
#include "smtk/model/Edge.h"
#include "smtk/model/EdgeUse.h"
#include "smtk/model/Face.h"
#include "smtk/model/FaceUse.h"
#include "smtk/model/GroupEntity.h"
#include "smtk/model/InstanceEntity.h"
#include "smtk/model/Loop.h"
#include "smtk/model/ModelEntity.h"
#include "smtk/model/Shell.h"
#include "smtk/model/Storage.h"
#include "smtk/model/UseEntity.h"
#include "smtk/model/Vertex.h"
#include "smtk/model/VertexUse.h"
#include "smtk/model/Volume.h"

#include "smtk/util/Testing/helpers.h"
#include "smtk/model/testing/helpers.h"

#include <fstream>
#include <sstream>

using namespace smtk::util;
using namespace smtk::model;
using namespace smtk::model::testing;
using smtk::shared_ptr;

void testComplexVertexChain()
{
  StoragePtr sm = Storage::create();
  Vertices verts;
  VertexUses vu;
  for (int i = 0; i < 6; ++i)
    {
    verts.push_back(sm->addVertex());
    vu.push_back(sm->addVertexUse(verts.back(), 0));
    }
  VertexUses rvu; // reversed vertex-uses for testing eun.
  for (int i = 0; i < 6; ++i)
    {
    rvu.push_back(vu[6 - (i + 1)]);
    }
  // Create an edge and uses to hold our chains.
  Edge e = sm->addEdge();
  EdgeUse eun = sm->addEdgeUse(e, 0, NEGATIVE);
  EdgeUse eup = sm->addEdgeUse(e, 0, POSITIVE);
  // Create 2 top-level segments and add a subsegment to the second.
  // First top-level chain:
  Chain t0p = sm->addChain(eup).addUse(vu[0]).addUse(vu[1]);
  // Second top-level chain:
  Chain t1p = sm->addChain(eup).addUse(vu[2]).addUse(vu[5]);
  // Sub-chain:
  Chain tsp = sm->addChain(t1p).addUse(vu[3]).addUse(vu[4]);

  // Do the same in reverse order:
  Chain t1n = sm->addChain(eun).addUse(vu[5]).addUse(vu[2]);
  Chain tsn = sm->addChain(t1n).addUse(vu[4]).addUse(vu[3]);
  Chain t0n = sm->addChain(eun).addUse(vu[1]).addUse(vu[0]);

  // Now, ask the positive edge use for all of its vertex uses.
  // It should return an ordered list verts[0]...verts[5].
  test(eup.vertexUses() == vu, "Complex chain traversal failed.");
  /*
  VertexUses trvu = eun.vertexUses();
  for (int i = 0; i < 6; ++i)
    {
    std::cout << trvu[i] << "  " << rvu[i] << "\n";
    }
    */
  test(eun.vertexUses() == rvu, "Complex chain traversal failed (reversed).");
}

int main(int argc, char* argv[])
{
  (void) argc;
  (void) argv;

  try
    {
    StoragePtr sm = Storage::create();
    UUIDArray uids = createTet(sm);

    ModelEntity model = sm->addModel(3, 3, "TestModel");
    // Test ModelEntity::parent()
    test(!model.parent().isValid(), "Toplevel model should have an invalid parent.");
    // Test that free cells may be added to a model,
    // and that they are only reported back as free
    // cells (and not other related entities via
    // casts to invalid cursor types).
    Volume tet = Volume(sm, uids[21]);
    // Test ModelEntity::addCell()
    model.addCell(tet);
    test(model.isEmbedded(tet), "Tetrahedron not embedded in model");
    test(
      !model.cells().empty() &&
      model.cells()[0] == tet,
      "Tetrahedron not a free cell of model");
    // Test ModelEntity::groups() when empty
    test(model.groups().empty(), "Tetrahedron should not be reported as a group.");
    // Test ModelEntity::submodels()
    test(model.submodels().empty(), "Tetrahedron should not be reported as a submodel.");

    sm->assignDefaultNames();
    Cursors entities;
    Cursor::CursorsFromUUIDs(entities, sm, uids);
    std::cout << uids.size() << " uids, " << entities.size() << " entities\n";
    test(uids.size() == entities.size(),
      "Translation from UUIDs to cursors should not omit entries");
    GroupEntity bits = sm->addGroup(0, "Bits'n'pieces");
    // Test GroupEntity::addEntities()
    bits.addEntities(entities);
    // Test ModelEntity::addGroup()
    model.addGroup(bits);
    // Test GroupEntity::parent()
    test(bits.parent() == model, "Parent of group incorrect.");
    // Test ModelEntity::groups() when non-empty
    test(
      !model.groups().empty() &&
      model.groups()[0] == bits,
      "Group should be reported as member of model.");
    // Test GroupEntity::members().
    CellEntities groupCells = bits.members<CellEntities>();
    UseEntities groupUses = bits.members<UseEntities>();
    ShellEntities groupShells = bits.members<ShellEntities>();
    std::cout
      << uids.size() << " entities = "
      << groupCells.size() << " cells + "
      << groupUses.size() << " uses + "
      << groupShells.size() << " shells\n";
    test(groupCells.size() == 22, "Cell group has wrong number of members.");
    test(groupUses.size() == 30, "Use group has wrong number of members.");
    test(groupShells.size() == 25, "Shell group has wrong number of members.");

    InstanceEntity ie = sm->addInstance(model);
    test(ie.prototype() == model, "Instance parent should be its prototype.");
    InstanceEntities ies = model.instances<InstanceEntities>();
    test(ies.size() == 1 && ies[0] == ie, "Prototype should list its instances.");

    std::string json = ExportJSON::fromModel(sm);
    std::ofstream jsonFile("/tmp/cursor.json");
    jsonFile << json;
    jsonFile.close();

    Cursor entity(sm, uids[0]);
    test(entity.isValid() == true);
    test(entity.dimension() == 0);
    test((entity.entityFlags() & ANY_ENTITY) == (CELL_ENTITY | DIMENSION_0));

    for (int dim = 1; dim <= 3; ++dim)
      {
      entities = entity.bordantEntities(dim);
      test(entities.size() == sm->bordantEntities(uids[0], dim).size());
      entities = entity.higherDimensionalBordants(dim);
      test(entities.size() == sm->higherDimensionalBordants(uids[0], dim).size());
      }

    test( entity.isCellEntity(),     "isCellEntity() incorrect");
    test(!entity.isUseEntity(),      "isUseEntity() incorrect");
    test(!entity.isShellEntity(),    "isShellEntity() incorrect");
    test(!entity.isGroupEntity(),    "isGroupEntity() incorrect");
    test(!entity.isModelEntity(),    "isModelEntity() incorrect");
    test(!entity.isInstanceEntity(), "isInstanceEntity() incorrect");

    test( entity.isVertex(),     "isVertex() incorrect");
    test(!entity.isEdge(),       "isEdge() incorrect");
    test(!entity.isFace(),       "isFace() incorrect");
    test(!entity.isVolume(),     "isVolume() incorrect");
    test(!entity.isChain(),      "isChain() incorrect");
    test(!entity.isLoop(),       "isLoop() incorrect");
    test(!entity.isShell(),      "isShell() incorrect");
    test(!entity.isVertexUse(),  "isVertexUse() incorrect");
    test(!entity.isEdgeUse(),    "isEdgeUse() incorrect");
    test(!entity.isFaceUse(),    "isFaceUse() incorrect");

    // Test that "cast"ing to Cursor subclasses works
    // and that they are valid or invalid as appropriate.
    CellEntity cell = entity.as<CellEntity>();
    test(cell.model() == model, "Vertex has incorrect owning-model");

    UseEntity use = entity.as<UseEntity>();
    Vertex vert = entity.as<Vertex>();
    std::cout << vert << "\n";
    //std::cout << vert.coordinates().transpose() << "\n";
    test(cell.isValid(), "CellEntity::isValid() incorrect");
    test(!use.isValid(), "UseEntity::isValid() incorrect");
    // Test obtaining uses from cells. Currently returns an empty set
    // because we are not properly constructing/arranging the solid.
    VertexUses vertexUses = cell.uses<VertexUses>();

    entity = Cursor(sm, uids[21]);
    test(entity.isValid() == true);
    test(entity.dimension() == 3);

    for (int dim = 2; dim >= 0; --dim)
      {
      entities = entity.boundaryEntities(dim);
      test(entities.size() == sm->boundaryEntities(uids[21], dim).size());
      entities = entity.lowerDimensionalBoundaries(dim);
      test(entities.size() == sm->lowerDimensionalBoundaries(uids[21], dim).size());
      }

    entity.setFloatProperty("perpendicular", 1.57);
    test(
      entity.floatProperty("perpendicular").size() == 1 &&
      entity.floatProperty("perpendicular")[0] == 1.57);

    entity.setStringProperty("name", "Tetrahedron");
    test(
      entity.stringProperty("name").size() == 1 &&
      entity.stringProperty("name")[0] == "Tetrahedron");

    entity.setIntegerProperty("deadbeef", 3735928559);
    test(
      entity.integerProperty("deadbeef").size() == 1 &&
      entity.integerProperty("deadbeef")[0] == 3735928559);

    std::cout << entity << "\n";

    entity = Cursor(sm, UUID::null());
    test(entity.dimension() == -1);
    test(entity.isValid() == false);

    // Verify that setting properties on an invalid cursor works.
    entity.setFloatProperty("perpendicular", 1.57);
    entity.setStringProperty("name", "Tetrahedron");
    entity.setIntegerProperty("deadbeef", 3735928559);
    // The above should have had no effect since the cursor is invalid:
    test(entity.hasFloatProperty("perpendicular") == false);
    test(entity.hasStringProperty("name") == false);
    test(entity.hasIntegerProperty("deadbeef") == false);

    // Verify that attribute assignment works (with some
    // made-up attribute IDs)
    test(!entity.hasAttributes(), "Detecting an un-associated attribute");
    test( entity.attachAttribute(1), "Attaching an attribute");
    test(!entity.attachAttribute(1), "Re-attaching a repeated attribute");
    test( entity.detachAttribute(1), "Detaching an associated attribute");
    test(!entity.detachAttribute(2), "Detaching an un-associated attribute");

    // Test that face entity was created with invalid (but present) face uses.
    Face f(sm, uids[20]);
    test(f.volumes().size() == 1 && f.volumes()[0].isVolume());
    test(!f.positiveUse().isValid());
    test( f.negativeUse().isValid());

    Vertex v = sm->addVertex();
    sm->addVertexUse(v, 0);
    v.setStringProperty("name", "Loopy");
    std::cout << v << "\n";
    sm->findOrAddInclusionToCell(uids[21], v.entity());
    // Now perform the same operation another way to ensure that
    // the existing arrangement blocks the new one from being
    // redundantly created:
    Volume vol(sm, uids[21]);
    vol.embedEntity(v);

    test(
      !vol.inclusions<CellEntities>().empty() &&
      vol.inclusions<CellEntities>()[0] == v,
      "Volume should have an included vertex.");

    // Test Volume::shells()
    Shells shells = vol.shells();
    test(shells == vol.use().shells(), "Volume::use() test failed.");
    test((shells.size() == 1) && shells[0].isValid(), "Volume should have 1 top-level shell.");

    Shell sh(shells[0]);

    test(!sh.containingShell().isValid(), "Top-level shell should have no container.");
    test(sh.containedShells().empty(), "Top-level shell should no containees.");
    test(sh.containedShells().empty(), "Top-level shell should no containees.");
    test(sh.volume() == vol, "Top-level shell's volume does not match volume it came from.");

    FaceUses fu(sh.faceUses());

    test(fu.size() == 5, "Shell should have 5 faces.");
    test(fu[0].orientation() == NEGATIVE, "Face-uses of sample tet should all have negative orientation.");
    test(fu[0].sense() == 0, "Face-uses of sample tet should be sense 0.");
    test(fu[0].volume() == vol, "Face-use did not report the correct volume it bounds.");

    // createTet generates the first face of the tet with a hole containing
    // another triangular face.
    // We test that ShellEntity::parentCell() properly traverses the
    // containment relationship to find the toplevel Loop when we start with
    // the inner loop of the face.
    Loops loops = fu[0].loops();
    test(loops.size() == 1, "FaceUse should only report 1 outer loop.");
    loops = loops[0].containedLoops();
    test(loops.size() == 1, "First face-use should have 1 inner loop.");
    Loop innerLoop = loops[0];
    test(innerLoop.face() == fu[0].face(),
      "Inner loop of face's loop should report face as parent cell.");
    EdgeUses ieus = innerLoop.edgeUses();
    test(ieus.size() == 3, "Inner loop of face 0 should have 3 edges.");
    test(ieus[0].faceUse() == fu[0],
      "EdgeUse failed to report proper FaceUse associated with its Loop.");

    // Now test EdgeUse methods.
    // Test EdgeUse::vertices(); the first edge of the inner loop should
    // connect vertices 3-4. The order is reversed because fu[0] is a
    // negatively-oriented face-use and the inner loop is a hole in the face.
    Vertices ieverts = ieus[0].vertices();
    test(ieverts.size() == 2, "Bad number of innerLoop edge vertices.");
    test(
      ieverts[0].entity() == uids[4] &&
      ieverts[1].entity() == uids[3], "Bad innerLoop, edge 0 vertices.");

    // Let's look at uses of the same edge from different triangles.
    EdgeUses oeus = fu[1].loops()[0].edgeUses(); // edge uses of outer loop of "hole" face-use
    test(oeus.size() == 3, "Expecting a triangle.");
    for (int i = 0; i < 3; ++i)
      {
      std::ostringstream msg;
      msg
        << "Edge-uses sharing the same edge in different contexts should be different.\n"
        << "Loop entry " << i << ":  "
        << ieus[i] << " (" << ieus[i].edge() << " "
        << ieus[i].sense() << " " << (ieus[i].orientation() == POSITIVE ? "+" : "-") << ")  "
        << oeus[i] << " (" << oeus[i].edge() << " "
        << oeus[i].sense() << " " << (oeus[i].orientation() == POSITIVE ? "+" : "-") << ")\n";
      test(
        oeus[2 - i].edge() == ieus[i].edge() &&
        oeus[2 - i].sense() != ieus[i].sense() &&
        oeus[2 - i].orientation() != ieus[i].orientation(),
        msg.str());
      test(ieus[i].loop() == innerLoop, "EdgeUse did not point to proper parent loop.");
      }

    // Test Edge and Chain methods:
    Edges allEdges;
    Cursor::CursorsFromUUIDs(allEdges, sm, sm->entitiesMatchingFlags(EDGE, true));
    for (Edges::iterator edgeIt = allEdges.begin(); edgeIt != allEdges.end(); ++edgeIt)
      {
      Edge curEdge(*edgeIt);
      EdgeUses curUses(curEdge.edgeUses());
      //std::cout << "Edge \"" << curEdge << "\" has " << curUses.size() << " uses\n";
      for (EdgeUses::iterator useIt = curUses.begin(); useIt != curUses.end(); ++useIt)
        {
        //std::cout << "    " << *useIt << " chains:\n";
        Chains curChains(useIt->chains());
        test(!curChains.empty(), "EdgeUses should not have empty chains.");
        for (Chains::iterator chainIt = curChains.begin(); chainIt != curChains.end(); ++chainIt)
          {
          test(!chainIt->containingChain().isValid(), "Top-level chains should not have parent chain.");
          Chains subchains = chainIt->containedChains();
          //std::cout << "        " << *chainIt << " with " << subchains.size() << " subchains\n";
          VertexUses vu = chainIt->vertexUses();
          test(vu.size() == 2, "All sample tet edges should have 2 vertex uses.");
          }
        }
      // Every edge should have at least 2 uses and they should come in pairs
      // with opposite orientations.
      test(!curUses.empty() && curUses.size() % 2 == 0);
      }

    // Test Vertex and VertexUse methods:
    Vertices allVerts;
    Cursor::CursorsFromUUIDs(allVerts, sm, sm->entitiesMatchingFlags(VERTEX, true));
    int n6 = 0;
    int n4 = 0;
    for (Vertices::iterator vertIt = allVerts.begin(); vertIt != allVerts.end(); ++vertIt)
      {
      test(vertIt->uses<VertexUses>().size() == 1, "All sample tet vertices should have a single use.");
      VertexUse vu(vertIt->uses<VertexUses>()[0]);
      int n = static_cast<int>(vu.chains().size());
      test(n == 0 || n == 4 || n == 6, "VertexUses on sample tet should have 0, 4, or 6 chains each.");
      n6 += (n == 6 ? 1 : 0);
      n4 += (n == 4 ? 1 : 0);
      //std::cout << vu << " (" << vu.vertex() << " sense " << vu.sense() << ") " << n << "\n";
      }
    test(n6 == 4, "4 corner vertex-uses should have 6 chains each.");
    test(n4 == 3, "3 inner-face vertex-uses should have 4 chains each.");

    testComplexVertexChain();
    }
  catch (const std::string& msg)
    {
    (void)msg; // the test will already have printed the message.
    return 1;
    }

  return 0;
}