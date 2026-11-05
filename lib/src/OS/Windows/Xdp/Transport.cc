// AnChSa, 2026-10-30; Windows XDP fork preparation, 2026-11-05. CeCILL-C; see LICENSE.
#define XDP_INCLUDE_WINCOMMON
#include <xdpapi.h>
#include <afxdp_helper.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <objbase.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include "kickcat/OS/Windows/XdpTransport.h"
#include "TxSlots.h"

namespace kickcat::xdp
{
    namespace
    {
        constexpr UINT32 Count = 64, Chunk = 2048;
        void check(HRESULT hr, char const* operation)
        {
            if (FAILED(hr))
            {
                std::ostringstream out;
                out << operation << " HRESULT=0x" << std::hex << static_cast<unsigned long>(hr);
                throw std::runtime_error(out.str());
            }
        }
        struct Handle
        {
            HANDLE value = nullptr;
            ~Handle()
            {
                if (value && value != INVALID_HANDLE_VALUE)
                    CloseHandle(value);
            }
        };
        std::string utf8(wchar_t const* text)
        {
            if (!text || !*text)
                return {};
            const int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (!n)
                throw std::runtime_error("Adapter name conversion failed");
            std::string out(static_cast<std::size_t>(n), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), n, nullptr, nullptr);
            out.pop_back();
            return out;
        }
        NET_LUID luid(std::string const& text)
        {
            if (text.size() > 80)
                throw std::runtime_error("Invalid interface GUID");
            std::wstring wide(text.begin(), text.end());
            GUID guid{};
            check(CLSIDFromString(wide.c_str(), &guid), "Interface GUID");
            NET_LUID value{};
            auto error = ConvertInterfaceGuidToLuid(&guid, &value);
            if (error)
                throw std::runtime_error("Interface GUID not present: " + std::to_string(error));
            return value;
        }
    } // namespace
    std::vector<Adapter> Transport::adapters()
    {
        ULONG size = 16384;
        std::vector<BYTE> storage(size);
        auto error = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr,
                                          reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()), &size);
        if (error == ERROR_BUFFER_OVERFLOW)
        {
            storage.resize(size);
            error = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr,
                                         reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()), &size);
        }
        if (error)
            throw std::runtime_error("GetAdaptersAddresses: " + std::to_string(error));
        std::vector<Adapter> result;
        for (auto a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data()); a; a = a->Next)
        {
            if (a->IfType != IF_TYPE_ETHERNET_CSMACD || a->PhysicalAddressLength != 6)
                continue;
            Adapter value;
            value.guid = a->AdapterName;
            value.name = utf8(a->FriendlyName);
            value.index = a->IfIndex;
            value.up = a->OperStatus == IfOperStatusUp;
            std::copy_n(a->PhysicalAddress, 6, value.mac.begin());
            result.push_back(value);
        }
        return result;
    }
    struct Transport::Impl
    {
        // Socket is destroyed before registered memory, including on partial setup failure.
        std::unique_ptr<BYTE[]> memory{new BYTE[2 * Count * Chunk]{}};
        TxSlots<Count> slots;
        Handle socket, program;
        XSK_RING rx{}, fill{}, tx{}, completion{};
        TransportStats counters;
        void recycle(UINT64 address)
        {
            UINT32 i = 0;
            if (XskRingProducerReserve(&fill, 1, &i) != 1)
                throw std::runtime_error("RX fill ring full");
            *static_cast<UINT64*>(XskRingGetElement(&fill, i)) = address;
            XskRingProducerSubmit(&fill, 1);
        }
        void pump()
        {
            if (XskRingError(&rx) || XskRingError(&fill) || XskRingError(&tx) || XskRingError(&completion))
                throw std::runtime_error("XDP ring error / interface detached");
            XSK_NOTIFY_RESULT_FLAGS result{};
            check(XskNotifySocket(socket.value, XSK_NOTIFY_FLAG_POKE_RX | XSK_NOTIFY_FLAG_POKE_TX, 0, &result), "XskNotifySocket");
            UINT32 i = 0;
            while (XskRingConsumerReserve(&completion, 1, &i) == 1)
            {
                auto address = *static_cast<UINT64*>(XskRingGetElement(&completion, i));
                XskRingConsumerRelease(&completion, 1);
                if (address < static_cast<UINT64>(Count) * Chunk || address >= static_cast<UINT64>(2 * Count) * Chunk || address % Chunk)
                    throw std::runtime_error("Invalid TX completion address");
                slots.release(static_cast<std::size_t>(address / Chunk - Count));
                ++counters.completed;
            }
        }
        void initialize(UINT32 index)
        {
            check(XskCreate(&socket.value), "XskCreate (runtime installed, elevated process?)");
            XSK_UMEM_REG reg{};
            reg.Address = memory.get();
            reg.TotalSize = 2 * Count * Chunk;
            reg.ChunkSize = Chunk;
            check(XskSetSockopt(socket.value, XSK_SOCKOPT_UMEM_REG, &reg, sizeof(reg)), "UMEM");
            check(XskBind(socket.value, index, 0, XSK_BIND_FLAG_GENERIC | XSK_BIND_FLAG_RX | XSK_BIND_FLAG_TX), "XskBind Generic queue 0");
            for (UINT32 option : {XSK_SOCKOPT_RX_RING_SIZE, XSK_SOCKOPT_RX_FILL_RING_SIZE,
                                  XSK_SOCKOPT_TX_RING_SIZE, XSK_SOCKOPT_TX_COMPLETION_RING_SIZE})
                check(XskSetSockopt(socket.value, option, &Count, sizeof(Count)), "Ring size");
            check(XskActivate(socket.value, XSK_ACTIVATE_FLAG_NONE), "XskActivate");
            XSK_RING_INFO_SET info{};
            UINT32 length = sizeof(info);
            check(XskGetSockopt(socket.value, XSK_SOCKOPT_RING_INFO, &info, &length), "Ring info");
            XskRingInitialize(&rx, &info.Rx);
            XskRingInitialize(&fill, &info.Fill);
            XskRingInitialize(&tx, &info.Tx);
            XskRingInitialize(&completion, &info.Completion);
            for (UINT32 i = 0; i < Count; ++i)
                recycle(static_cast<UINT64>(i) * Chunk);
            XDP_HOOK_ID hook{};
            hook.Layer = XDP_HOOK_L2;
            hook.Direction = XDP_HOOK_RX;
            hook.SubLayer = XDP_HOOK_INSPECT;
            XDP_RULE rule{};
            rule.Match = XDP_MATCH_ALL;
            rule.Action = XDP_PROGRAM_ACTION_REDIRECT;
            rule.Redirect.TargetType = XDP_REDIRECT_TARGET_TYPE_XSK;
            rule.Redirect.Target = socket.value;
            check(XdpCreateProgram(index, &hook, 0, XDP_CREATE_PROGRAM_FLAG_GENERIC, &rule, 1, &program.value), "XdpCreateProgram Generic RX");
        }
    };
    Transport::Transport() = default;
    Transport::~Transport() = default;
    void Transport::open(std::string const& guid)
    {
        if (impl_)
            throw std::runtime_error("XDP transport already open");
        auto id = luid(guid);
        NET_IFINDEX index = 0;
        auto error = ConvertInterfaceLuidToIndex(&id, &index);
        if (error)
            throw std::runtime_error("Interface index: " + std::to_string(error));
        bool present = false;
        for (const auto& a : adapters())
            if (a.index == index && a.up)
                present = true;
        if (!present)
            throw std::runtime_error("Selected interface is not an active Ethernet adapter");
        auto candidate = std::make_unique<Impl>();
        candidate->initialize(index);
        impl_ = std::move(candidate);
    }
    void Transport::close() noexcept { impl_.reset(); }
    bool Transport::trySend(void const* data, std::size_t size)
    {
        if (!impl_)
            throw std::runtime_error("XDP transport closed");
        if (!data || size < 14 || size > 1518)
            throw std::runtime_error("Invalid Ethernet frame length");
        auto& p = *impl_;
        p.pump();
        auto slot = p.slots.acquire();
        if (slot == TxSlots<Count>::None)
            return false;
        UINT32 i = 0;
        if (XskRingProducerReserve(&p.tx, 1, &i) != 1)
        {
            p.slots.release(slot);
            return false;
        }
        auto address = static_cast<UINT64>(Count + slot) * Chunk;
        const auto padded = std::max<std::size_t>(60, size);
        std::memcpy(p.memory.get() + address, data, size);
        std::memset(p.memory.get() + address + size, 0, padded - size);
        auto descriptor = static_cast<XSK_BUFFER_DESCRIPTOR*>(XskRingGetElement(&p.tx, i));
        *descriptor = {};
        descriptor->Address.BaseAddress = address;
        descriptor->Length = static_cast<UINT32>(padded);
        XskRingProducerSubmit(&p.tx, 1);
        ++p.counters.submitted;
        p.pump();
        return true;
    }
    int Transport::tryReceive(void* data, std::size_t capacity)
    {
        if (!impl_)
            throw std::runtime_error("XDP transport closed");
        if (!data || !capacity)
            throw std::runtime_error("Invalid RX destination");
        auto& p = *impl_;
        p.pump();
        UINT32 i = 0;
        if (XskRingConsumerReserve(&p.rx, 1, &i) != 1)
            return 0;
        const auto d = *static_cast<XSK_BUFFER_DESCRIPTOR*>(XskRingGetElement(&p.rx, i));
        const UINT64 base = d.Address.BaseAddress, offset = d.Address.Offset;
        if (base >= static_cast<UINT64>(Count) * Chunk || base % Chunk || offset >= Chunk || d.Length > Chunk - offset)
            throw std::runtime_error("Invalid RX buffer descriptor");
        const bool fits = d.Length <= capacity;
        if (fits)
            std::memcpy(data, p.memory.get() + base + offset, d.Length);
        XskRingConsumerRelease(&p.rx, 1);
        p.recycle(base);
        ++p.counters.received;
        if (!fits)
            throw std::runtime_error("RX frame exceeds destination capacity");
        return static_cast<int>(d.Length);
    }
    void Transport::waitForReceive(std::uint32_t timeoutMs)
    {
        if (!impl_)
            throw std::runtime_error("XDP transport closed");
        XSK_NOTIFY_RESULT_FLAGS result{};
        const auto hr = XskNotifySocket(impl_->socket.value,
                                        XSK_NOTIFY_FLAG_POKE_RX | XSK_NOTIFY_FLAG_WAIT_RX, timeoutMs, &result);
        if (hr != XDP_STATUS_TIMEOUT)
            check(hr, "XskNotifySocket WAIT_RX");
    }
    TransportStats Transport::stats()
    {
        if (!impl_)
            return {};
        auto& p = *impl_;
        p.pump();
        XSK_STATISTICS stats{};
        UINT32 size = sizeof(stats);
        check(XskGetSockopt(p.socket.value, XSK_SOCKOPT_STATISTICS, &stats, &size), "XDP statistics");
        auto result = p.counters;
        result.dropped = stats.RxDropped;
        result.truncated = stats.RxTruncated;
        result.invalid = stats.RxInvalidDescriptors + stats.TxInvalidDescriptors;
        return result;
    }
} // namespace kickcat::xdp
