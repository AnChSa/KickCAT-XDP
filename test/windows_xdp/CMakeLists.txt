# AnChSa, 2026-11-05: native MSVC/Windows XDP integration.
add_executable(kickcat_windows_xdp_checks SocketChecks.cc)
target_include_directories(kickcat_windows_xdp_checks PRIVATE ../../lib/src/OS/Windows/Xdp)
target_link_libraries(kickcat_windows_xdp_checks PRIVATE kickcat)
set_kickcat_properties(kickcat_windows_xdp_checks)
if (BUILD_SHARED_LIBS)
  add_custom_command(TARGET kickcat_windows_xdp_checks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kickcat> $<TARGET_FILE_DIR:kickcat_windows_xdp_checks>)
endif()
add_test(NAME kickcat_windows_xdp_checks COMMAND kickcat_windows_xdp_checks)
set_tests_properties(kickcat_windows_xdp_checks PROPERTIES TIMEOUT 30)
