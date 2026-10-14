add_subdirectory(OGL)

if(NOT DOLPHIN_SWITCH)
  add_subdirectory(Null)
  add_subdirectory(Software)
endif()

if(WIN32)
  add_subdirectory(D3DCommon)
  add_subdirectory(D3D)
  add_subdirectory(D3D12)
endif()

if(APPLE)
  add_subdirectory(Metal)
endif()

if(ENABLE_VULKAN)
  add_subdirectory(Vulkan)
endif()

if(DOLPHIN_SWITCH AND (ENABLE_DEKO3D OR DOLPHIN_SWITCH_BUNDLE_DEKO3D))
  add_subdirectory(Deko3D)
endif()
