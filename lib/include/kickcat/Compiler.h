// AnChSa, 2026-10-30.
// Packing portability for KickCAT, distributed under the project's CeCILL-C license.
#ifndef KICKCAT_COMPILER_H
#define KICKCAT_COMPILER_H

// Scope packing to individual wire structs, never entire public headers that
// also declare STL-owning host objects. Restore the caller's packing afterward.
#if defined(_MSC_VER)
#define KICKCAT_PACKED_BEGIN __pragma(pack(push, 1))
#define KICKCAT_PACKED_END __pragma(pack(pop))
#define KICKCAT_PACKED
#else
#define KICKCAT_PACKED_BEGIN
#define KICKCAT_PACKED_END
#define KICKCAT_PACKED __attribute__((__packed__))
#endif

#endif
