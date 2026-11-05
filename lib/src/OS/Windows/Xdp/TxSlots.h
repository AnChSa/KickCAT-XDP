// AnChSa, 2026-10-30; Windows XDP fork preparation, 2026-11-05. CeCILL-C; see LICENSE.
#ifndef KICKCAT_WINDOWS_XDP_TX_SLOTS_H
#define KICKCAT_WINDOWS_XDP_TX_SLOTS_H
#include <array>
#include <cstddef>
#include <stdexcept>
namespace kickcat::xdp
{
    // A TX slot stays owned by the driver until its completion is consumed.
    template <std::size_t N>
    class TxSlots
    {
        std::array<bool, N> busy_{};

    public:
        static constexpr std::size_t None = N;
        std::size_t acquire()
        {
            for (std::size_t i = 0; i < N; ++i)
                if (!busy_[i])
                {
                    busy_[i] = true;
                    return i;
                }
            return None;
        }
        void release(std::size_t i)
        {
            if (i >= N || !busy_[i])
                throw std::runtime_error("Invalid or duplicate TX completion");
            busy_[i] = false;
        }
    };
} // namespace kickcat::xdp

#endif
