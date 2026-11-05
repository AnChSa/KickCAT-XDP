target_sources(kickcat PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/AbstractLink.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Bus.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/CoE.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/CoE/CiA/DS402/Drive.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dc.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/FoE.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Diagnostics.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Gateway.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Link.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Prints.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/MailboxSequencer.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/MasterOD.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/Slave.cc
)

# AnChSa, 2026-10-30: caller injects AbstractSocket directly.
if(NOT KICKCAT_MSVC_MINIMAL)
  target_sources(kickcat PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/helpers.cc)
endif()

kickcat_publish_includes(kickcat ${CMAKE_CURRENT_SOURCE_DIR}/include)
