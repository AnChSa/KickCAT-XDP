// AnChSa, 2026-10-30; Windows XDP fork preparation, 2026-11-05. CeCILL-C; see LICENSE.
#include <cerrno>
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>
#include "kickcat/OS/Windows/XdpSocket.h"
namespace kickcat
{
    WindowsXdpSocket::WindowsXdpSocket()
        : WindowsXdpSocket(std::make_unique<xdp::Transport>()) {}
    WindowsXdpSocket::WindowsXdpSocket(std::unique_ptr<xdp::RawTransport> transport)
        : transport_(std::move(transport))
    {
        if (!transport_)
            throw std::invalid_argument("Null transport");
    }
    WindowsXdpSocket::~WindowsXdpSocket() { close(); }
    void WindowsXdpSocket::open(std::string const& guid)
    {
        if (opened_)
            throw std::runtime_error("Socket already open");
        stopping_.store(false);
        error_.clear();
        try
        {
            transport_->open(guid);
            opened_ = true;
        }
        catch (const std::exception& e)
        {
            transport_->close();
            error_ = e.what();
            throw;
        }
    }
    void WindowsXdpSocket::close() noexcept
    {
        stopping_.store(true);
        transport_->close();
        opened_ = false;
    }
    void WindowsXdpSocket::setTimeout(nanoseconds timeout) { timeout_ = timeout; }
    int32_t WindowsXdpSocket::read(void* frame, int32_t capacity)
    {
        if (!opened_)
            return -ENOTCONN;
        if (!frame || capacity < 14)
            return -EINVAL;
        const auto start = std::chrono::steady_clock::now();
        unsigned skipped = 0;
        const auto finish = [&](int result)
        {
            transport_->observeReadResult(result, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
            return result;
        };
        try
        {
            do
            {
                if (stopping_.load())
                    return finish(-ECANCELED);
                const int size = transport_->tryReceive(frame, static_cast<std::size_t>(capacity));
                if (size < 0 || size > capacity)
                    throw std::runtime_error("Invalid transport receive result");
                auto p = static_cast<uint8_t const*>(frame);
                if (size >= 14 && p[12] == 0x88 && p[13] == 0xa4)
                    return finish(size);
                // Drain already queued foreign frames without an unbounded zero-timeout loop.
                if (timeout_.count() == 0)
                {
                    if (!size || ++skipped >= 64)
                        break;
                    continue;
                }
                if (timeout_.count() >= 0 && std::chrono::steady_clock::now() - start >= timeout_)
                    break;
                if (!size && notifications_)
                {
                    const auto remaining = timeout_.count() < 0 ? std::chrono::milliseconds(10) : std::chrono::duration_cast<std::chrono::milliseconds>(timeout_ - (std::chrono::steady_clock::now() - start));
                    if (remaining.count() > 0)
                    {
                        transport_->waitForReceive(static_cast<std::uint32_t>(std::min<std::int64_t>(10, remaining.count())));
                        continue;
                    }
                }
                std::this_thread::yield();
            } while (true);
            return finish(-ETIMEDOUT);
        }
        catch (const std::exception& e)
        {
            error_ = e.what();
            stopping_.store(true);
            return finish(-EIO);
        }
    }
    int32_t WindowsXdpSocket::write(void const* frame, int32_t length)
    {
        if (!opened_)
            return -ENOTCONN;
        if (!frame || length < 14 || length > 1518)
            return -EINVAL;
        auto p = static_cast<uint8_t const*>(frame);
        if (p[12] != 0x88 || p[13] != 0xa4)
            return -EINVAL;
        const auto start = std::chrono::steady_clock::now();
        try
        {
            do
            {
                if (stopping_.load())
                    return -ECANCELED;
                if (transport_->trySend(frame, static_cast<std::size_t>(length)))
                    return length;
                if (timeout_.count() >= 0 && std::chrono::steady_clock::now() - start >= timeout_)
                    break;
                std::this_thread::yield();
            } while (true);
            return -ETIMEDOUT;
        }
        catch (const std::exception& e)
        {
            error_ = e.what();
            stopping_.store(true);
            return -EIO;
        }
    }
} // namespace kickcat
