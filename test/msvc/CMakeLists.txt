# AnChSa, 2026-10-30. CeCILL-C, see project LICENSE.
add_executable(kickcat_core_checks CoreChecks.cpp)
target_link_libraries(kickcat_core_checks PRIVATE kickcat)
set_kickcat_properties(kickcat_core_checks)
# Force all library objects into this test: missing symbols must not hide in a
# static archive just because the smoke test doesn't call every master method.
if (NOT BUILD_SHARED_LIBS)
  target_link_options(kickcat_core_checks PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:kickcat>")
endif()
if (BUILD_SHARED_LIBS)
  add_custom_command(TARGET kickcat_core_checks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kickcat> $<TARGET_FILE_DIR:kickcat_core_checks>)
endif()
add_test(NAME kickcat_core_checks COMMAND kickcat_core_checks)
set_tests_properties(kickcat_core_checks PROPERTIES TIMEOUT 30)
