/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "cmdlib.h"
#include "threads.h"
#include <windows.h>

qboolean threaded;
CRITICAL_SECTION crit;
static int enter;

#define MAX_THREADS 64

int numthreads = -1;
int dispatch;
int workcount;
int oldf;
qboolean pacifier;
void (*workfunction)(int);

/*
=============
ThreadSetDefault
=============
*/
void ThreadSetDefault(void)
{
    SYSTEM_INFO info;

    if (numthreads == -1)
    {
        GetSystemInfo(&info);
        numthreads = info.dwNumberOfProcessors;
        if (numthreads < 1 || numthreads > 32)
            numthreads = 1;
    }
    qprintf("%i threads\n", numthreads);
}

/*
=============
ThreadLock
=============
*/
void ThreadLock(void)
{
    if (!threaded)
        return;
    EnterCriticalSection(&crit);
    if (enter)
        Error("Recursive ThreadLock\n");
    enter = 1;
}

/*
=============
ThreadUnlock
=============
*/
void ThreadUnlock(void)
{
    if (!threaded)
        return;
    if (!enter)
        Error("ThreadUnlock without lock\n");
    enter = 0;
    LeaveCriticalSection(&crit);
}

/*
=============
RunThreadsOn
=============
*/
void RunThreadsOn(int workcnt, qboolean showpacifier, void (*func)(int))
{
    DWORD threadid[MAX_THREADS];
    HANDLE threadhandle[MAX_THREADS];
    int i;
    int start, end;

    start = I_FloatTime();
    dispatch = 0;
    workcount = workcnt;
    oldf = -1;
    pacifier = showpacifier;
    threaded = qtrue;
    InitializeCriticalSection(&crit);
    if (numthreads == 1)
    {
        func(0);
    }
    else
    {
        for (i = 0; i < numthreads; i++)
        {
            threadhandle[i] = CreateThread(NULL, 0,
                (LPTHREAD_START_ROUTINE)func, (LPVOID)i, 0, &threadid[i]);
        }
        for (i = 0; i < numthreads; i++)
            WaitForSingleObject(threadhandle[i], INFINITE);
    }
    DeleteCriticalSection(&crit);
    threaded = qfalse;
    end = I_FloatTime();
    if (pacifier)
        _printf(" (%i)\n", end - start);
}

/*
=============
GetThreadWork

=============
*/
int GetThreadWork(void)
{
    int r;
    int f;

    ThreadLock();
    if (dispatch == workcount)
    {
        ThreadUnlock();
        return -1;
    }
    f = 10 * dispatch / workcount;
    if (f != oldf)
    {
        oldf = f;
        if (pacifier)
            _printf("%i...", f);
    }
    r = dispatch;
    dispatch++;
    ThreadUnlock();
    return r;
}

/*
=============
ThreadWorkerFunction
=============
*/
void ThreadWorkerFunction(int threadnum)
{
    int work;

    while (1)
    {
        work = GetThreadWork();
        if (work == -1)
            break;
        workfunction(work);
    }
}

/*
=============
RunThreadsOnIndividual
=============
*/
void RunThreadsOnIndividual(int workcnt, qboolean showpacifier, void (*func)(int))
{
    if (numthreads == -1)
        ThreadSetDefault();
    workfunction = func;
    RunThreadsOn(workcnt, showpacifier, ThreadWorkerFunction);
}

