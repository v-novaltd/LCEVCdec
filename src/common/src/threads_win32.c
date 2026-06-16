/* Copyright (c) V-Nova International Limited 2024-2026. All rights reserved.
 * This software is licensed under the BSD-3-Clause-Clear License by V-Nova Limited.
 * No patent licenses are granted under this license. For enquiries about patent licenses,
 * please contact legal@v-nova.com.
 * The LCEVCdec software is a stand-alone project and is NOT A CONTRIBUTION to any other project.
 * If the software is incorporated into another project, THE TERMS OF THE BSD-3-CLAUSE-CLEAR LICENSE
 * AND THE ADDITIONAL LICENSING INFORMATION CONTAINED IN THIS FILE MUST BE MAINTAINED, AND THE
 * SOFTWARE DOES NOT AND MUST NOT ADOPT THE LICENSE OF THE INCORPORATING PROJECT. However, the
 * software may be incorporated into a project under a compatible license provided the requirements
 * of the BSD-3-Clause-Clear license are respected, and V-Nova Limited remains
 * licensor of the software ONLY UNDER the BSD-3-Clause-Clear license (not the compatible license).
 * ANY ONWARD DISTRIBUTION, WHETHER STAND-ALONE OR AS PART OF ANY OTHER PROJECT, REMAINS SUBJECT TO
 * THE EXCLUSION OF PATENT LICENSES PROVISION OF THE BSD-3-CLAUSE-CLEAR LICENSE. */

#include <LCEVC/common/clock.h>
#include <LCEVC/common/diagnostics.h>
#include <LCEVC/common/log.h>
#include <LCEVC/common/platform.h>
#include <LCEVC/common/threads.h>
#include <stdlib.h>

int32_t threadNumCores(void)
{
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return (int32_t)sysinfo.dwNumberOfProcessors;
}

static DWORD WINAPI threadWrapper(LPVOID arg)
{
    Thread* thread = (Thread*)arg;
    thread->result = 0;
    thread->result = thread->function(thread->argument);
    return 0;
};

int threadCreate(Thread* thread, ThreadFunction function, void* argument)
{
    thread->function = function;
    thread->argument = argument;
    thread->thread = CreateThread(NULL, 0, threadWrapper, thread, 0, NULL);
    return (thread->thread != 0) ? ThreadResultSuccess : ThreadResultError;
}

void threadExit(void) { ExitThread(0); }

int threadJoin(Thread* thread, intptr_t* resultPtr)
{
    if (WaitForSingleObject(thread->thread, INFINITE) != WAIT_OBJECT_0) {
        return ThreadResultError;
    }

    if (resultPtr) {
        *resultPtr = thread->result;
    }

    return ThreadResultSuccess;
}

int threadSleep(uint32_t milliseconds)
{
    Sleep(milliseconds);
    return ThreadResultSuccess;
}

void threadSetPriority(Thread* thread, ThreadPriority priority)
{
    int prio = 0;

    switch (priority) {
        case ThreadPriorityHigh: prio = THREAD_PRIORITY_ABOVE_NORMAL; break;
        case ThreadPriorityNormal: prio = THREAD_PRIORITY_NORMAL; break;
        case ThreadPriorityIdle: prio = THREAD_PRIORITY_IDLE; break;
    }

    SetThreadPriority(thread->thread, prio);
}

void threadSetName(const char* name)
{
    if (!name) {
        return;
    }

    wchar_t wname[256];
    mbstowcs(wname, name, sizeof(wname) / sizeof(wname[1]) - 1);

    // This might not be available on Windows prior to Windows10
    const HRESULT hr = SetThreadDescription(GetCurrentThread(), wname);
    if (FAILED(hr)) {
        VNLogWarning("Cannot set thread name");
    }
}

uint64_t threadTimeMicroseconds(int32_t offset) { return ldcClockDeadlineMicroseconds(offset); }

int threadCondVarWaitDeadline(ThreadCondVar* condVar, ThreadMutex* mutex, uint64_t deadline)
{
    if (deadline == UINT64_MAX) {
        SleepConditionVariableSRW(&condVar->condVar, &mutex->lock, INFINITE, 0);
        return ThreadResultSuccess;
    }

    const uint64_t usecs = ldcClockDeadlineMicroseconds(0);

    if (usecs >= deadline) {
        // Expired already
        return ThreadResultTimeout;
    }

    // Round up remaining time to deadline to milliseconds
    const DWORD millis = (DWORD)((deadline - usecs + 999) / 1000);

    if (!SleepConditionVariableSRW(&condVar->condVar, &mutex->lock, millis, 0)) {
        return ThreadResultTimeout;
    }

    return ThreadResultSuccess;
}
