// AnChSa, 2026-10-30; Windows XDP fork preparation, 2026-11-05. CeCILL-C; see LICENSE.
#ifndef KICKCAT_WINDOWS_XDP_TRANSPORT_H
#define KICKCAT_WINDOWS_XDP_TRANSPORT_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace kickcat::xdp
{
    struct Adapter
    {
        std::string guid, name;
        std::array<std::uint8_t, 6> mac{};
        std::uint32_t index = 0;
        bool up = false;
    };
    struct TransportStats
    {
        std::uint64_t submitted = 0, completed = 0, received = 0, dropped = 0, truncated = 0, invalid = 0;
    };
    // Nonblocking packet operations; implementations copy TX input before returning.
    class RawTransport
    {
    public:
        virtual ~RawTransport() = default;
        virtual void open(std::string const& guid) = 0;
        virtual void close() noexcept = 0;
        virtual bool trySend(void const* data, std::size_t size) = 0;
        virtual int tryReceive(void* data, std::size_t capacity) = 0; // 0 = no packet
        // Optional bounded receive notification; default supports polling test transports.
        virtual void waitForReceive(std::uint32_t timeoutMs) { (void)timeoutMs; }
        virtual void observeReadResult(int result, double elapsedMs) noexcept
        {
            (void)result;
            (void)elapsedMs;
        }
        virtual TransportStats stats() = 0;
    };
    class Transport final : public RawTransport
    {
    public:
        Transport();
        ~Transport() override;
        Transport(const Transport&) = delete;
        Transport& operator=(const Transport&) = delete;
        static std::vector<Adapter> adapters();
        void open(std::string const& guid) override;
        void close() noexcept override;
        bool trySend(void const* data, std::size_t size) override;
        int tryReceive(void* data, std::size_t capacity) override;
        void waitForReceive(std::uint32_t timeoutMs) override;
        TransportStats stats() override;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
} // namespace kickcat::xdp

#endif
