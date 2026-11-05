<!-- AnChSa, 2026-11-05: contribution guidance for the Windows XDP fork. -->
# Contributing

This repository is AnChSa's Windows XDP fork of
[leducp/KickCAT](https://github.com/leducp/KickCAT). Pull requests are disabled
for this fork. Please support the original project whenever a change benefits
KickCAT generally.

## Support upstream KickCAT

General EtherCAT fixes, portable improvements, documentation and tests should be
discussed or contributed to [upstream KickCAT](https://github.com/leducp/KickCAT).
Follow the upstream project's
[contribution guidelines](https://github.com/leducp/KickCAT/blob/master/CONTRIBUTING.md)
when opening an issue or pull request there. Choose the upstream base branch and
keep a proposed change focused on the problem it solves.

Describe the behavior before and after the change, its motivation and the tests
performed. Keep commit messages clear and preserve or improve test coverage.
Supporting upstream helps fixes remain available to all KickCAT users and keeps
the changes carried by this fork small.

## Windows XDP-specific changes

For behavior specific to this fork's native MSVC/XDP backend, discuss it with
AnChSa. The fork's scope and qualification limits are documented in
[FORK.md](FORK.md); the build instructions are in
[docs/WINDOWS_XDP.md](docs/WINDOWS_XDP.md).

When describing a problem, include the commit, compiler/toolset, build options,
XDP SDK/runtime versions and the relevant error or trace. Distinguish failures
reproduced by the hardware-free checks from observations on real hardware.
Report actual hardware, adapter and driver details when they affect the result.

Keep transport and portability code independent of application configuration,
sensor interpretation and image buffers. Use the existing KickCAT style and
`uncrustify.cfg`. Run the native core/socket checks for port changes; hardware
tests and packet traces provide additional evidence when available.

The standalone native checks do not replace the upstream GoogleTest suite or
hardware interoperability testing. Passing them is not a real-time guarantee.
