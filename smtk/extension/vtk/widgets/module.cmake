set (__dependencies)

# Test for targets that might be required or
# might not exist.
foreach(target
    vtkInteractionStyle
    vtkRenderingFreeType
    vtkRenderingOpenGL2
    vtkRenderingVolumeOpenGL2
    vtkRenderingGL2PSOpenGL2
)
  if (TARGET ${target})
    list(APPEND __dependencies ${target})
  endif()
endforeach()

vtk_module(vtkSMTKWidgetsExt
  DEPENDS
    vtkInteractionWidgets
  PRIVATE_DEPENDS
    ${__dependencies}
  TEST_DEPENDS
    vtkTestingRendering
  EXCLUDE_FROM_PYTHON_WRAPPING
)
