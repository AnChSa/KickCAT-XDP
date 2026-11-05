// AnChSa, 2026-10-30. CeCILL-C; see LICENSE.
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <deque>
#include "kickcat/OS/Windows/XdpTransport.h"
#include "kickcat/OS/Windows/XdpSocket.h"
#include "TxSlots.h"
namespace
{
    void require(bool ok, char const* message)
    {
        if (!ok)
            throw std::runtime_error(message);
    }
    class Fake final : public kickcat::xdp::RawTransport
    {
    public:
        bool opened = false, full = false, fail = false, failOpen = false;
        std::vector<uint8_t> outgoing, incoming;
        std::deque<std::vector<uint8_t>> queued;
        int closes = 0;
        int foreign = 0;
        unsigned notifications = 0;
        std::vector<uint8_t> notificationIncoming;
        int lastReadResult = 0;
        void observeReadResult(int result, double) noexcept override { lastReadResult = result; }
        void waitForReceive(std::uint32_t) override
        {
            ++notifications;
            if (!notificationIncoming.empty())
                incoming = std::move(notificationIncoming);
        }
        void open(std::string const&) override
        {
            opened = true;
            if (failOpen)
                throw std::runtime_error("injected open failure");
        }
        void close() noexcept override
        {
            opened = false;
            ++closes;
        }
        bool trySend(void const* data, std::size_t size) override
        {
            if (fail)
                throw std::runtime_error("injected send failure");
            if (full)
                return false;
            auto p = static_cast<uint8_t const*>(data);
            outgoing.assign(p, p + size);
            return true;
        }
        int tryReceive(void* data, std::size_t size) override
        {
            if (!queued.empty())
            {
                incoming = std::move(queued.front());
                queued.pop_front();
            }
            if (foreign)
            {
                if (foreign > 0)
                    --foreign;
                std::memset(data, 0, 14);
                return 14;
            }
            if (incoming.empty())
                return 0;
            if (size < incoming.size())
                throw std::runtime_error("oversize");
            auto n = static_cast<int>(incoming.size());
            std::memcpy(data, incoming.data(), incoming.size());
            incoming.clear();
            return n;
        }
        kickcat::xdp::TransportStats stats() override { return {}; }
    };
} // namespace
int main()
{
    try
    {
        kickcat::xdp::TxSlots<2> pool;
        require(pool.acquire() == 0 && pool.acquire() == 1 && pool.acquire() == pool.None, "Do not reuse outstanding TX slots");
        pool.release(0);
        require(pool.acquire() == 0, "Reuse completed slot");
        pool.release(0);
        bool rejected = false;
        try
        {
            pool.release(0);
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected, "Reject duplicate completion");
        rejected = false;
        try
        {
            pool.release(9);
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected, "Reject foreign completion");
        auto fake = std::make_unique<Fake>();
        auto ptr = fake.get();
        kickcat::WindowsXdpSocket socket(std::move(fake));
        uint8_t frame[60]{};
        frame[12] = 0x88;
        frame[13] = 0xa4;
        uint8_t received[60]{};
        require(socket.write(frame, 60) == -ENOTCONN, "Closed socket");
        socket.open("test");
        socket.setTimeout(std::chrono::nanoseconds::zero());
        require(socket.write(frame, 60) == 60, "Write complete frame");
        frame[17] = 1;
        require(ptr->outgoing[17] == 0, "Caller can reuse TX input after write");
        ptr->incoming = ptr->outgoing;
        ptr->foreign = 1;
        require(socket.read(received, 60) == 60 && received[17] == 0, "Receive complete frame");
        ptr->incoming.assign(60, 0);
        require(socket.read(received, 60) == -ETIMEDOUT, "Filter non-EtherCAT");
        require(socket.read(received, 60) == -ETIMEDOUT, "Zero timeout");
        frame[12] = 0x88;
        frame[13] = 0xa4;
        ptr->full = true;
        require(socket.write(frame, 60) == -ETIMEDOUT, "Backpressure zero timeout");
        ptr->full = false;
        socket.requestStop();
        require(socket.read(received, 60) == -ECANCELED, "Cancel receive");
        socket.close();
        socket.close();
        require(!ptr->opened, "Idempotent close");
        socket.open("again");
        socket.setTimeout(std::chrono::milliseconds(1));
        ptr->foreign = -1;
        require(socket.read(received, 60) == -ETIMEDOUT, "Foreign traffic must not reset finite deadline");
        ptr->foreign = 0;
        ptr->full = true;
        require(socket.write(frame, 60) == -ETIMEDOUT, "Finite write timeout");
        ptr->full = false;
        socket.useReceiveNotifications(true);
        socket.setTimeout(std::chrono::milliseconds(20));
        ptr->notificationIncoming = ptr->outgoing;
        require(socket.read(received, 60) == 60 && ptr->notifications > 0, "XDP receive notification followed by normal frame validation");
        socket.setTimeout(std::chrono::milliseconds(1));
        require(socket.read(received, 60) == -ETIMEDOUT && ptr->lastReadResult == -ETIMEDOUT, "Read diagnostics retain finite-deadline timeout result");
        socket.setTimeout(std::chrono::nanoseconds(-1));
        int cancelled = 0;
        std::thread receiver([&]
        {
            cancelled = socket.read(received, 60);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        socket.requestStop();
        receiver.join();
        require(cancelled == -ECANCELED, "Interrupt an unlimited receive");
        socket.close();
        socket.open("after-cancel");
        ptr->fail = true;
        require(socket.write(frame, 60) == -EIO && !socket.lastError().empty(), "Transport failure diagnosis");
        socket.close();
        ptr->fail = false;
        ptr->failOpen = true;
        rejected = false;
        try
        {
            socket.open("failure");
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected && !ptr->opened, "Cleanup failed open");
        // Construct/close real backend without opening a driver or adapter.
        kickcat::xdp::Transport real;
        real.close();
        real.close();
        std::cout << "XDP adapter checks: OK (no driver or hardware opened)\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
