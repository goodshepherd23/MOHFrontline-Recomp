/* Wrapper for msvcp140_atomic_wait.dll. Windows zones newer than the system ICU's data (for
 * example "Alberta Standard Time") make the STL's current zone come back "Etc/Unknown", and
 * std::chrono::current_zone() then throws, aborting MSVC apps such as the runtime (0xc0000409).
 * Rename it (America/Edmonton for the Alberta zone, otherwise Etc/UTC; the game only uses it for
 * its clock) and forward everything else to the original DLL. GPL-3.0, see LICENSE. */
#include <windows.h>
#include <string.h>
#include <wchar.h>

typedef struct { int err; const char *name; } zone_info;
typedef zone_info *(__stdcall *get_fn)(void);
typedef void (__stdcall *del_fn)(zone_info *);

static const char *fixed_name(void)
{
    DYNAMIC_TIME_ZONE_INFORMATION tz;
    if (GetDynamicTimeZoneInformation(&tz) != TIME_ZONE_ID_INVALID &&
        wcscmp(tz.TimeZoneKeyName, L"Alberta Standard Time") == 0)
        return "America/Edmonton";
    return "Etc/UTC";
}
static zone_info *volatile infos[16];
static const char *volatile originals[16];

static FARPROC orig(const char *name)
{
    static HMODULE mod;
    if (!mod) mod = LoadLibraryA("msvcp140_atomic_wait_orig.dll");
    return mod ? GetProcAddress(mod, name) : NULL;
}

__declspec(dllexport) zone_info *__stdcall __std_tzdb_get_current_zone(void)
{
    zone_info *info = ((get_fn)orig("__std_tzdb_get_current_zone"))();
    if (info && info->err == 0 && info->name && strcmp(info->name, "Etc/Unknown") == 0) {
        for (int i = 0; i < 16; i++) {
            if (InterlockedCompareExchangePointer((PVOID volatile *)&infos[i], info, NULL) == NULL) {
                originals[i] = info->name;
                info->name = fixed_name();
                break;
            }
        }
    }
    return info;
}

__declspec(dllexport) void __stdcall __std_tzdb_delete_current_zone(zone_info *info)
{
    for (int i = 0; info && i < 16; i++) {
        if (infos[i] == info) {
            info->name = originals[i];
            InterlockedExchangePointer((PVOID volatile *)&infos[i], NULL);
            break;
        }
    }
    ((del_fn)orig("__std_tzdb_delete_current_zone"))(info);
}
