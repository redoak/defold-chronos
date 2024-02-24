/**
The MIT License (MIT)

Copyright (c) ldrumm 2014

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

//BASED ON https://github.com/ldrumm/chronos



#define EXTENSION_NAME chronos
#define LIB_NAME "chronos"
#define MODULE_NAME "chronos"

#include <dmsdk/sdk.h>

#define CHRONOS_CLOCK_SUPPORTED

#if defined(__APPLE__) && defined(__MACH__)
 #include <mach/mach_time.h>
 #ifdef CHRONOS_USE_COREAUDIO
  #include <CoreAudio/HostTime.h>
 #endif
#elif defined(_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <intrin.h>
 #pragma intrinsic(__rdtsc)
#elif defined(__unix__) || defined(__linux__) && !defined(__APPLE__)
 #include <unistd.h>
 #if defined (_POSIX_TIMERS) && _POSIX_TIMERS > 0
  #ifdef _POSIX_MONOTONIC_CLOCK
   #define HAVE_CLOCK_GETTIME
   #include <time.h>
  #else
   #warning "A nanosecond resolution monotonic clock is not available;"
   #warning "falling back to microsecond gettimeofday()"
   #include <sys/time.h>
  #endif
 #endif
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
static int chronos_time(lua_State *L){
    lua_pushnumber(L,emscripten_get_now()/1000.0);
    return 1;
}

static int chronos_interval(lua_State * L)
{
    lua_pushnumber(L, static_cast<lua_Number>(0.0));
    return 1;
}

#elif !defined(__APPLE__) && (defined(__unix__) || defined(__linux__))
static int chronos_time(lua_State *L)
{
#ifdef HAVE_CLOCK_GETTIME
    /** From man clock_gettime(2)
       CLOCK_MONOTONIC
          Clock  that  cannot  be  set and represents monotonic time since
          some unspecified starting point.  This clock is not affected by
          discontinuous jumps in the system time (e.g., if the system
          administrator manually changes the clock), but is affected by the
          incremental  adjustments  performed by adjtime(3) and NTP.

       CLOCK_MONOTONIC_COARSE (since Linux 2.6.32; Linux-specific)
              A faster but less precise version of CLOCK_MONOTONIC.
              Use when you need very fast, but not fine-grained timestamps.

       CLOCK_MONOTONIC_RAW (since Linux 2.6.28; Linux-specific)
              Similar to CLOCK_MONOTONIC, but provides access to a raw
              hardware-based time that is not subject to NTP adjustments or the
              incremental adjustments performed by adjtime(3).
    */
    struct timespec t_info;
    const double multiplier = 1.0 / 1e9;

    if (clock_gettime(CLOCK_MONOTONIC, &t_info) != 0) {
        return luaL_error(L, "clock_gettime() failed:%s", strerror(errno));
    }
    lua_pushnumber(
        L,
        (lua_Number)t_info.tv_sec + (t_info.tv_nsec * multiplier)
    );
    return 1;

#else
    struct timeval t_info;
    if (gettimeofday(&t_info, NULL) < 0) {
        return luaL_error(L, "gettimeofday() failed!:%s", strerror(errno));
    };
    lua_pushnumber(L, (lua_Number)t_info.tv_sec + t_info.tv_usec / 1.e6);
    return 1;
#endif
}

static int chronos_interval(lua_State * L)
{
    lua_pushnumber(L, static_cast<lua_Number>(0.0));
    return 1;
}

#elif defined(_WIN32)
// qpc: https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
// tsc: https://github.com/cmuratori/computer_enhance

static int64_t get_qpc_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

static const int64_t qpc_frequency = get_qpc_frequency();
static const double qpc_period = 1.0 / get_qpc_frequency();

static int chronos_time(lua_State* L)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    lua_pushnumber(L, static_cast<lua_Number>(counter.QuadPart * qpc_period));
    return 1;
}

static int chronos_frequency(lua_State* L)
{
    lua_pushinteger(L, static_cast<lua_Integer>(qpc_frequency));
    return 1;
}

static int chronos_decimals(lua_State* L)
{
    int decimals = 9;
    int a = static_cast<int>((1000000000.0 / qpc_frequency) + .5);
    int b = a / 10;
    while (a == b * 10)
    {
        a = b;
        b = a / 10;
        --decimals;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(decimals));
    return 1;
}

static double estimate_tsc_frequency(const int64_t wait_ms)
{
    const int64_t qpc_wait = (qpc_frequency * wait_ms) / 1000;
    const int64_t tsc_begin = __rdtsc();
    LARGE_INTEGER qpc_begin;
    LARGE_INTEGER qpc_end;
    QueryPerformanceCounter(&qpc_begin);
    int64_t qpc_elapsed = 0;
    while (qpc_elapsed < qpc_wait)
    {
        QueryPerformanceCounter(&qpc_end);
        qpc_elapsed = qpc_end.QuadPart - qpc_begin.QuadPart;
    }
    const int64_t tsc_end = __rdtsc();
    const int64_t tsc_elapsed = tsc_end - tsc_begin;
    return qpc_elapsed ? (qpc_frequency * tsc_elapsed) / static_cast<double>(qpc_elapsed) : 1.0;
}

static int chronos_tsc(lua_State* L)
{
    lua_pushinteger(L, static_cast<lua_Integer>(__rdtsc()));
    return 1;
}

static int chronos_tsc_time(lua_State* L)
{
    const lua_Integer end = static_cast<lua_Integer>(__rdtsc());
    const lua_Integer begin = luaL_checkinteger(L, 1);
    const lua_Integer elapsed = end - begin;
    const int64_t wait_ms = static_cast<int64_t>(luaL_optinteger(L, 2, 100));
    const double frequency = estimate_tsc_frequency(wait_ms);
    lua_pushnumber(L, static_cast<lua_Number>(elapsed / frequency));
    lua_pushnumber(L, static_cast<lua_Number>(frequency));
    return 2;
}

#elif defined(__APPLE__) && defined(__MACH__)
static int chronos_time(lua_State * L)
{
    //TODO All the apple stuff is untested because, like, I don't even got Apple.
    //If like, you have a mac, let me know whether this works.
    //see https://stackoverflow.com/questions/675626/coreaudio-audiotimestamp-mhosttime-clock-frequency
    //for info.
#ifdef CHRONOS_USE_COREAUDIO
    //Apparently this is just a wrapper around mach_absolute_time() anyway.
    lua_pushnumber(
        L,
        (lua_Number)AudioConvertHostTimeToNanos(AudioGetCurrentHostTime()) * 1.e9
    );
    return 1;
#else
    static int init = 1;
    static double resolution;
    static double multiplier;
    mach_timebase_info_data_t res_info;
    if(init){
        mach_timebase_info(&res_info);
        resolution = (double)res_info.numer / res_info.denom;
        multiplier = 1. / 1.e9;
        init = 0;
    }

    lua_pushnumber(L, (lua_Number)(mach_absolute_time() * resolution) * multiplier);
    return 1;
#endif
}

static int chronos_interval(lua_State * L)
{
    lua_pushnumber(L, static_cast<lua_Number>(0.0));
    return 1;
}
#else
#undef CHRONOS_CLOCK_SUPPORTED
#endif

#ifdef CHRONOS_CLOCK_SUPPORTED
static const struct luaL_reg Module_methods[] = {
    {"time", chronos_time},
    {"frequency", chronos_frequency},
    {"decimals", chronos_decimals},
    {"tsc", chronos_tsc},
    {"tsc_time", chronos_tsc_time},
    {NULL, NULL}
};

static void LuaInit(lua_State *L) {
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

#endif

static dmExtension::Result AppInitializeMyExtension(dmExtension::AppParams *params) { return dmExtension::RESULT_OK; }
static dmExtension::Result InitializeMyExtension(dmExtension::Params *params) {
    #ifdef CHRONOS_CLOCK_SUPPORTED
        // Init Lua
        LuaInit(params->m_L);
        dmLogInfo("Registered %s Extension\n", MODULE_NAME);
    #else
        dmLogInfo("chronos not supported")
    #endif

    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeMyExtension(dmExtension::AppParams *params) { return dmExtension::RESULT_OK; }

static dmExtension::Result FinalizeMyExtension(dmExtension::Params *params) { return dmExtension::RESULT_OK; }

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeMyExtension, AppFinalizeMyExtension, InitializeMyExtension, 0, 0, FinalizeMyExtension)