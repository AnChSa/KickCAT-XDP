<!-- AnChSa, 2026-11-05: native MSVC/Windows XDP fork changes. -->
# Native MSVC Windows XDP backend

This fork adds an optional Windows XDP backend to the existing KickCAT master.
It contains no Bow station configuration, sensor decoding or image buffers.
The existing GCC/Npcap Windows path and Linux AF_XDP backend remain separate.

## Build and install

Requires Visual Studio 2019 C++ tools (v142, x64), a Windows 10 SDK, CMake 3.15+
and the Microsoft XDP for Windows **1.3.0** SDK (`build/native/include`).
No MinGW, pthread, Npcap or Conan dependencies are required for this build.

```powershell
cmake -S . -B ../build/kickcat-xdp-shared -G "Visual Studio 16 2019" -A x64 -T v142 -DENABLE_WINDOWS_XDP=ON -DBUILD_SHARED_LIBS=ON -DXDP_INCLUDE_DIR="C:/dependencies/xdp-sdk/build/native/include" -DCMAKE_INSTALL_PREFIX="C:/application/ThirdParty/KickCAT-XDP"
cmake --build ../build/kickcat-xdp-shared --config Release
ctest --test-dir ../build/kickcat-xdp-shared -C Release --output-on-failure
cmake --install ../build/kickcat-xdp-shared --config Release
```

Use `BUILD_SHARED_LIBS=OFF` and a separate build/install directory for a static
library. The shared package installs `bin/KickCAT-XDP.dll` and its import library
`lib/KickCAT-XDP.lib`. The static package has a static `lib/KickCAT-XDP.lib`.
Debug builds use the `KickCAT-XDPd` basename. Do not mix import/static libraries
or Debug/Release headers/binaries from different builds.

`ENABLE_WINDOWS_XDP` selects `KICKCAT_MSVC_MINIMAL`: the existing restricted
master build, without the POSIX/pthread backends, slave applications, ESI parser,
GUI tools, Python bindings or simulation. `BUILD_MSVC_CORE_CHECKS=OFF` skips the
hardware-free checks. It does not change the normal upstream build defaults.

## Consume the installed package

```cmake
find_package(KickCAT CONFIG REQUIRED)
target_link_libraries(my_application PRIVATE KickCAT::kickcat)
```

Set `CMAKE_PREFIX_PATH` to the installed ThirdParty directory. The consumer needs
neither the fork source tree nor the XDP SDK. With a shared package, deploy the
KickCAT DLL next to the executable. The XDP runtime/driver is a separate Microsoft
dependency: installation and access to an adapter are deployment tasks.

For an existing Visual Studio project, use the installed `include` and `lib`
directories, add `KickCAT-XDP.lib`, and define `_CRT_DECLARE_NONSTDC_NAMES=0`,
`NOMINMAX`, `WIN32_LEAN_AND_MEAN`, and `KICKCAT_WINDOWS_XDP_ENABLED`.
Static consumers also link `iphlpapi.lib`, `ws2_32.lib`, `ole32.lib` and `ntdll.lib`.
Use C++17, the same MSVC toolset and matching `/MD` (Release) or `/MDd` (Debug).
The exported API is C++ with STL types; it is not a compiler-independent C ABI.

Alternatively, import the installed **KickCAT-XDP.props** in Visual Studio's
Property Manager for the x64 configurations. It supplies includes, definitions,
C++17 and the appropriate libraries, and copies the KickCAT DLL for shared
packages. Keep the project's v142 toolset and `/MD` or `/MDd` runtime settings.
Custom configuration names must set `KickCATXdpSuffix` explicitly when using
Debug binaries. The property sheet needs no CMake in the consuming application.

Include `kickcat/OS/Windows/XdpSocket.h` and construct
`kickcat::WindowsXdpSocket`. Adapter enumeration and raw transport abstractions
live in `kickcat::xdp`. The socket can be injected into the normal `kickcat::Link`.
Only one thread owns open/read/write/close. `requestStop()` can be called from
another thread; join the owner before closing or destroying the socket.

The inherited PoC uses Generic XDP, queue 0 and an RX redirect for all frames on
the selected dedicated Ethernet adapter. The socket filters EtherCAT frames.
This backend is intended for a dedicated EtherCAT interface. It does not provide
hard real-time guarantees. The hardware-free checks do not open a driver or NIC.

## Style and provenance

Keep `.cc` sources, guarded public headers, the `kickcat` namespace, four-space
indentation and Allman braces. The repository's unchanged `uncrustify.cfg` is the
authoritative formatting configuration (Uncrustify 0.83.x).
Run Uncrustify directly with this configuration for the changed port sources.

The source is CeCILL-C; retain LICENSE and contribution notices. Port changes are
attributed to AnChSa; upstream notices remain intact. See [FORK.md](../FORK.md)
for scope, changes, test rationale and qualification limits. The XDP SDK is an
external build dependency; no application source or ZIP-baseline directory is
needed to build a Git checkout of this fork.
