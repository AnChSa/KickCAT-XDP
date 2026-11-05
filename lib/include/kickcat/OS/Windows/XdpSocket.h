// AnChSa, 2026-10-30; Windows XDP fork preparation, 2026-11-05. CeCILL-C; see LICENSE.
#ifndef KICKCAT_WINDOWS_XDP_SOCKET_H
#define KICKCAT_WINDOWS_XDP_SOCKET_H
#include <atomic>
#include <memory>
#include <string>
#include "kickcat/AbstractSocket.h"
#include "kickcat/OS/Windows/XdpTransport.h"
namespace kickcat
{
    // One master thread owns open/read/write/close. requestStop may be called elsewhere;
    // join that thread before closing or destroying the object.
    class WindowsXdpSocket final : public AbstractSocket
    {
    public:
        WindowsXdpSocket();
        explicit WindowsXdpSocket(std::unique_ptr<xdp::RawTransport> transport);
        ~WindowsXdpSocket() override;
        void open(std::string const& guid) override;
        void close() noexcept override;
        void setTimeout(nanoseconds timeout) override;
        void useReceiveNotifications(bool enabled) noexcept { notifications_ = enabled; }
        int32_t read(void* frame, int32_t capacity) override;
        int32_t write(void const* frame, int32_t length) override;
        void requestStop() noexcept { stopping_.store(true); }
        std::string const& lastError() const noexcept { return error_; }
        xdp::TransportStats stats() { return transport_->stats(); }

    private:
        std::unique_ptr<xdp::RawTransport> transport_;
        nanoseconds timeout_{std::chrono::milliseconds(100)};
        std::atomic<bool> stopping_{false};
        bool opened_ = false;
        bool notifications_ = false;
        std::string error_;
    };
} // namespace kickcat

#endif
