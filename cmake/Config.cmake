set(WARNINGS_FLAGS "-Wall -Wextra -pedantic -Wcast-qual -Wcast-align -Wduplicated-cond -Wshadow -Wmissing-noreturn")

macro(set_kickcat_properties binary)
    set_target_properties(${binary} PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED YES
      CXX_EXTENSIONS NO
    )
    # AnChSa, 2026-10-30: do not pass GCC flags to MSVC.
    if(MSVC)
      target_compile_options(${binary} PRIVATE /W4 /EHsc /utf-8 /permissive-)
      target_compile_definitions(${binary} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    else()
      set_target_properties(${binary} PROPERTIES COMPILE_FLAGS ${WARNINGS_FLAGS})
    endif()
    # Only the wheel build (SKBUILD) folds static libkickcat into the nanobind .so, which
    # needs PIC. Bare-metal arm (KickOS) has no loader to place the resulting .got/
    # .data.rel.ro, so it must never get PIC -- and SKBUILD is never set there.
    if (SKBUILD)
      set_target_properties(${binary} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
endmacro()
