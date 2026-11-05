// AnChSa, 2026-10-30. CeCILL-C, see project LICENSE.
#include <thread>
#include <windows.h>
#include <algorithm>
#include "kickcat/OS/Time.h"

namespace kickcat
{
    nanoseconds now()
    {
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch());
    }

    nanoseconds since_unix_epoch()
    {
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
    }

    void sleep(nanoseconds ns)
    {
        if (ns <= nanoseconds::zero())
            return;
        // Mailbox polling asks for 200 us. sleep_for can round this to roughly
        // 15.6 ms on Windows, multiplying thousands of startup transactions.
        // A per-thread timer avoids handle allocation in the polling loop.
        // No process-wide timeBeginPeriod and no busy waiting. Not hard real-time.
        struct WaitTimer
        {
            HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS);
            ~WaitTimer()
            {
                if (handle)
                    CloseHandle(handle);
            }
        };
        thread_local WaitTimer timer;
        LARGE_INTEGER due{};
        due.QuadPart = -std::max<long long>(1, ns.count() / 100 + (ns.count() % 100 != 0));
        if (timer.handle && SetWaitableTimerEx(timer.handle, &due, 0, nullptr, nullptr, nullptr, 0) && WaitForSingleObject(timer.handle, INFINITE) == WAIT_OBJECT_0)
            return;
        // Compatibility fallback if high-resolution waitable timers are unavailable.
        std::this_thread::sleep_for(ns);
    }
} // namespace kickcat
