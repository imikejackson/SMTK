<?xml version="1.0" encoding="utf-8" ?>
<!-- Description of the model "Triangulate Face" Operator -->
<SMTK_AttributeSystem Version="2">
  <Definitions>
    <!-- Operator -->
    <AttDef Type="triangulate faces" Label="Faces - Triangulate" BaseType="operator">
      <BriefDescription>Triangulate model faces.</BriefDescription>
      <DetailedDescription>
        Triangulate model faces into a mesh using Delaunay.

        This operation creates smtk::mesh::MeshSets associated with
        smtk::mesh::Faces using Delaunay. The MeshSets reside in a new
        smtk::mesh::Collection associated with the faces' model. The
        resulting triangulation is composed only of the boundary points.
      </DetailedDescription>
      <AssociationsDef Name="faces" NumberOfRequiredValues="1" Extensible="true">
        <MembershipMask>face</MembershipMask>
        <BriefDescription>The faces to triangulate.</BriefDescription>
      </AssociationsDef>
      <ItemDefinitions>
        <Void Name="validate polygons" Label="Validate Polygons prior to Triangulation"
              Optional="true" AdvanceLevel="1">
          <BriefDescription>Ensure the polygons describing the
          boundaries are valid before triangulating the faces.</BriefDescription>
        </Void>
      </ItemDefinitions>
    </AttDef>
    <!-- Result -->
    <AttDef Type="result(triangulate faces)" BaseType="result">
      <ItemDefinitions>
        <ModelEntity Name="mesh_created" NumberOfRequiredValues="1" Extensible="true"/>
      </ItemDefinitions>
    </AttDef>
  </Definitions>
  <Views>
    <View Type="Operator" Title="Triangulate Face" FilterByAdvanceLevel="true" UseSelectionManager="true">
      <InstancedAttributes>
        <Att Type="triangulate face"/>
      </InstancedAttributes>
    </View>
  </Views>
</SMTK_AttributeSystem>
