// AnChSa, 2026-10-30. CeCILL-C, see project LICENSE.
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#include "kickcat/AbstractSocket.h"
#include "kickcat/Bus.h"
#include "kickcat/Link.h"
#include "kickcat/SocketNull.h"
#include "kickcat/CoE/protocol.h"
#include "kickcat/FoE/protocol.h"
#include "kickcat/EoE/protocol.h"
#include "kickcat/CoE/CiA/DS402/Drive.h"

static_assert(sizeof(kickcat::EthernetHeader) == 14, "Ethernet wire size");
static_assert(sizeof(kickcat::EthercatHeader) == 2, "EtherCAT wire size");
static_assert(sizeof(kickcat::DatagramHeader) == 10, "Datagram wire size");
static_assert(offsetof(kickcat::DatagramHeader, address) == 2, "Datagram address offset");
static_assert(offsetof(kickcat::DatagramHeader, irq) == 8, "Datagram IRQ offset");
static_assert(sizeof(kickcat::mailbox::Header) == 6, "Mailbox wire size");
static_assert(sizeof(kickcat::CoE::Header) == 2, "CoE wire size");
static_assert(sizeof(kickcat::ESC::Description) == 10, "ESC description size");
static_assert(sizeof(kickcat::DLStatus) == 2, "Data link status size");
static_assert(sizeof(kickcat::ErrorCounters) == 20, "ESC error counter block size");
static_assert(sizeof(kickcat::SyncManager::Register) == 8, "SyncManager register size");
static_assert(sizeof(kickcat::fmmu::Register) == 16, "FMMU register size");
static_assert(sizeof(kickcat::eeprom::Request) == 6, "EEPROM request size");
static_assert(sizeof(kickcat::eeprom::InfoEntry) == 128, "SII fixed header size");
static_assert(sizeof(kickcat::eeprom::GeneralEntry) == 32, "SII general category size");
static_assert(sizeof(kickcat::eeprom::SyncManagerEntry) == 8, "SII SyncManager entry size");
static_assert(sizeof(kickcat::eeprom::PDOEntry) == 8, "SII PDO entry size");
static_assert(sizeof(kickcat::mailbox::Error::ServiceData) == 4, "Mailbox error size");
static_assert(sizeof(kickcat::CoE::ServiceData) == 4, "SDO header size");
static_assert(sizeof(kickcat::CoE::ServiceDataInfo) == 4, "SDO info size");
static_assert(sizeof(kickcat::CoE::Emergency) == 8, "CoE emergency size");
static_assert(sizeof(kickcat::CoE::SDO::information::EntryDescriptionRequest) == 4, "SDO entry request size");
static_assert(sizeof(kickcat::CoE::SDO::information::ObjectDescription) == 6, "SDO object description size");
static_assert(sizeof(kickcat::CoE::SDO::information::EntryDescription) == 10, "SDO entry description size");
static_assert(sizeof(kickcat::FoE::Header) == 6, "FoE header size");
static_assert(sizeof(kickcat::EoE::Header) == 4, "EoE header size");
static_assert(sizeof(kickcat::EoE::Parameter) == 4, "EoE parameter flags size");
static_assert(sizeof(kickcat::CoE::CiA::DS402::Drive::Input) == 16, "Drive input size");
static_assert(sizeof(kickcat::CoE::CiA::DS402::Drive::Output) == 14, "Drive output size");
struct HostPacking
{
    uint8_t tag;
    uint64_t value;
};
static_assert(offsetof(HostPacking, value) == 8 && sizeof(HostPacking) == 16,
              "Protocol headers must restore normal host packing");

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void put16(uint8_t* p, uint16_t value)
    {
        p[0] = static_cast<uint8_t>(value);
        p[1] = static_cast<uint8_t>(value >> 8);
    }
    class ReplySocket final : public kickcat::AbstractSocket
    {
        std::vector<uint8_t> packet;

    public:
        int writes = 0;
        std::array<uint8_t, 6> source{};
        void open(const std::string&) override {}
        void close() noexcept override { packet.clear(); }
        void setTimeout(kickcat::nanoseconds) override {}
        int32_t write(const void* data, int32_t size) override
        {
            require(size >= 29, "Short EtherCAT write");
            auto p = static_cast<const uint8_t*>(data);
            std::copy_n(p + 6, 6, source.begin());
            require(p[16] == 7, "Detection must use BRD, never a write command");
            packet.assign(p, p + size);
            auto length = static_cast<uint16_t>((packet[22] | (packet[23] << 8)) & 0x7ff);
            require(26u + length + 2u <= packet.size(), "Datagram bounds");
            put16(packet.data() + 18, 2);
            put16(packet.data() + 26 + length, 2);
            ++writes;
            return size;
        }
        int32_t read(void* data, int32_t size) override
        {
            if (packet.empty())
                return -ETIMEDOUT;
            require(size >= static_cast<int32_t>(packet.size()), "Receive buffer too small");
            auto length = static_cast<int32_t>(packet.size());
            std::memcpy(data, packet.data(), packet.size());
            packet.clear();
            return length;
        }
    };
    void checkFrame()
    {
        kickcat::Frame frame;
        const kickcat::MAC source = {2, 1, 2, 3, 4, 5};
        frame.setSourceMAC(source);
        frame.addDatagram(0x2a, kickcat::Command::BRD, 0, nullptr, 2);
        require(frame.finalize() == 60, "Minimum Ethernet frame size");
        const uint8_t expected[] = {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 2, 1, 2, 3, 4, 5, 0x88, 0xa4,
            0x0e, 0x10, 7, 0x2a, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0};
        require(std::memcmp(frame.data(), expected, sizeof(expected)) == 0, "BRD wire bytes differ");
        kickcat::Frame reply(frame.data(), 60);
        auto [header, data, wkc] = reply.nextDatagram();
        require(header->index == 0x2a && header->len == 2 && wkc == 0 && data != nullptr,
                "Datagram parser roundtrip");
        kickcat::CoE::Header coe{};
        coe.service = kickcat::CoE::Service::SDO_REQUEST;
        uint8_t coeBytes[2]{};
        std::memcpy(coeBytes, &coe, 2);
        require(coeBytes[0] == 0 && coeBytes[1] == 0x20, "CoE service bit layout");
        kickcat::mailbox::Header mailbox{};
        mailbox.len = 0x1234;
        mailbox.address = 0x5678;
        mailbox.channel = 5;
        mailbox.priority = 2;
        mailbox.type = kickcat::mailbox::CoE;
        mailbox.count = 7;
        const uint8_t expectedMailbox[] = {0x34, 0x12, 0x78, 0x56, 0x85, 0x73};
        require(std::memcmp(&mailbox, expectedMailbox, sizeof(expectedMailbox)) == 0,
                "Mailbox bitfield wire layout");
    }
    void checkMaster()
    {
        auto socket = std::make_shared<ReplySocket>();
        auto null = std::make_shared<kickcat::SocketNull>();
        auto link = std::make_shared<kickcat::Link>(socket, null, [] {});
        kickcat::Bus bus(link);
        require(bus.detectSlaves() == 2 && socket->writes == 1, "Master detection through injected socket");
        require(bus.detectedSlaves() == 2, "Master slave count retained");
        require(std::memcmp(socket->source.data(), kickcat::PRIMARY_IF_MAC, 6) == 0, "Default source MAC preserved");
        const kickcat::MAC custom = {2, 3, 4, 5, 6, 7};
        auto customLink = std::make_shared<kickcat::Link>(
            socket, null, [] {}, custom);
        kickcat::Bus customBus(customLink);
        require(customBus.detectSlaves() == 2 && std::memcmp(socket->source.data(), custom, 6) == 0,
                "Configured MAC must reach writeThenRead");
        bool processed = false;
        customLink->addDatagram(
            kickcat::Command::BRD, 0, nullptr, 2,
            [&](const kickcat::DatagramHeader*, const uint8_t*, uint16_t counter)
            {
                processed = counter == 2;
                return kickcat::DatagramState::OK;
            },
            [](const kickcat::DatagramState&)
            { throw std::runtime_error("Cyclic test failed"); });
        customLink->processDatagrams();
        require(processed && std::memcmp(socket->source.data(), custom, 6) == 0,
                "Configured MAC must reach cyclic sendFrame");
    }
} // namespace
int main()
{
    try
    {
        checkFrame();
        checkMaster();
        auto before = kickcat::now();
        kickcat::sleep(kickcat::nanoseconds::zero());
        require(kickcat::now() >= before, "Monotonic clock");
        std::cout << "KickCAT MSVC core checks: OK (no hardware access)\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
}
