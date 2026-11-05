# Windows XDP fork

Maintained by **AnChSa**: [AnChSa/KickCAT-XDP](https://github.com/AnChSa/KickCAT-XDP).
Based on [leducp/KickCAT](https://github.com/leducp/KickCAT), under its CeCILL-C license.

## Purpose and boundary

Provide a small, native **MSVC x64 EtherCAT master library** with an optional
**Microsoft XDP for Windows** socket backend. A Windows application can consume
an installed DLL/import library or static library from its ThirdParty directory,
without building KickCAT or using CMake in that application.

This fork contains general porting and transport support. Application configuration,
device-specific commissioning, sensor decoding, image generation and image ring
buffers belong to the consuming application and are not implemented here.
The XDP descriptor rings are part of the transport and remain inside this library.

The additions are opt-in (`ENABLE_WINDOWS_XDP=ON`). The upstream Linux, embedded,
GCC/Npcap Windows and Linux AF_XDP paths remain in the tree. Their presence is not
a claim that they were revalidated as part of this native Windows port.

## What the Windows XDP build includes

| Area | Included or used |
|---|---|
| EtherCAT master | Existing Frame, Link, Bus, discovery, SII and process-data mechanisms |
| Mailboxes | Existing common mailbox code and master protocol implementations; the acquisition application uses CoE SDO reads/writes |
| Windows transport | `kickcat::WindowsXdpSocket`, injected into the existing Link API |
| XDP implementation | `kickcat::xdp::Transport`, Generic XDP, queue 0, registered memory and RX/TX/completion rings |
| Adapter information | GUID, physical MAC, name and interface status; no application-specific interface selection |
| Time | Monotonic clock and bounded Windows waitable-timer sleep with compatibility fallback |
| Shared core helpers | AbstractESC helpers needed by common mailbox code; this does not enable a slave backend |
| Other master functionality | Existing DC, FoE, diagnostics and drive-related code may compile into the master core; compilation alone does not qualify their use with Windows XDP |

The restricted native build does **not** enable the full upstream slave stack,
ESI parser, emulator, Python bindings, GUI tools or master/slave examples.
Protocol layout checks for FoE/EoE do not demonstrate application use of those
protocols. The existing application uses input process data and CoE commissioning.

The backend targets a dedicated EtherCAT Ethernet adapter. Its inherited receive
setup redirects all frames on that adapter to the XDP socket, where EtherCAT
frames are selected. Windows scheduling and Generic XDP provide no hard real-time
guarantee. Tests below do not demonstrate latency under production load.

## Changes relative to the original source snapshot

- Native MSVC compiler options and a restricted master build
  (`KICKCAT_MSVC_MINIMAL`), independent of GCC/w64devkit, pthread, Npcap and Conan.
- Scoped packing macros for wire structures, preserving the caller's packing;
  MSVC-compatible formatting and CRT compatibility.
- A Windows time implementation and physical source-MAC propagation at both
  Link send paths, using the existing constructor parameters.
- Optional Windows XDP socket/transport in `lib/include/kickcat/OS/Windows` and
  `lib/src/OS/Windows/Xdp`, with no application namespace or device configuration.
- Static or shared library builds; DLL symbols exported through CMake's
  `WINDOWS_EXPORT_ALL_SYMBOLS`, without adding export annotations throughout the core.
- Installed headers, DLL/import or static library, CeCILL-C license, relocatable
  CMake package and Visual Studio property sheet. The consumer does not need the
  XDP SDK. The Microsoft runtime/driver is a separate deployment dependency.
- Hardware-free native port and socket regression checks.

The binary basename is `KickCAT-XDP` (`KickCAT-XDPd` in Debug); the CMake target
keeps the upstream name `KickCAT::kickcat`. The API remains C++17/MSVC with STL
types, so compiler toolset and runtime settings must match the consumer.

Build/install instructions, consumption examples and formatting configuration:
[docs/WINDOWS_XDP.md](docs/WINDOWS_XDP.md).

## Tests and what they establish

The native build has two small CTest executables, enabled by
`BUILD_MSVC_CORE_CHECKS`. They deliberately avoid network/driver access and test
framework dependencies. They are port-specific checks, not a replacement for
the upstream GoogleTest suite in `unit/`.

| Check | Evidence provided | Reason for keeping it |
|---|---|---|
| `kickcat_core_checks` | Wire sizes/offsets, restored host packing, expected frame/mailbox bytes, simulated discovery and configured MAC at both send paths; basic monotonic-clock check | Detects MSVC layout/bitfield and porting regressions, plus missing library symbols through whole-archive linking for static builds |
| `kickcat_windows_xdp_checks` | TX slots remain reserved until completion; complete frame copies; EtherCAT filtering; zero/finite timeouts; backpressure; cancellation; reopen and cleanup after injected failure; receive-notification dispatch | Protects the socket contract while moving the backend out of its original application integration |

The core check originated in the native-MSVC proof of concept. The Windows XDP
check was extracted from the proof-of-concept application's adapter checks;
application/device/image tests are not copied into this fork.

Code formatting follows KickCAT's namespace/bracing conventions and `.cc` naming
for new backend sources. Test infrastructure differs from upstream: the existing
upstream suite uses GoogleTest/GoogleMock, while these standalone checks use
explicit assertions via exceptions and an exit status. This keeps the restricted
native build dependency-free; it is an intentional, documented difference.
The authoritative formatter is the unchanged `uncrustify.cfg` (0.83.x).

Passing these checks establishes software-level contracts with a fake transport.
It does **not** establish actual XDP driver operation, adapter compatibility,
EtherCAT hardware interoperability, production timing, or a full upstream-suite
pass. Those require separate hardware/platform qualification.

Local verification before transfer to the remote fork: DLL Release/Debug and
static Release each passed both library checks. The consuming acquisition project
passed its existing 11 checks against each of those installed packages, and its
source-integrated Release build passed 13 checks (11 application + 2 library).
The exact Uncrustify 0.83.x check and VS/MSBuild project-generation check remain
open in that preparation environment; MSVC/v142 builds via Ninja succeeded.

## Attribution and license

Port modifications are attributed to **AnChSa**, with dates in the changed files.
Upstream copyright and license notices are retained. KickCAT and these library
modifications remain under [CeCILL-C](LICENSE).
