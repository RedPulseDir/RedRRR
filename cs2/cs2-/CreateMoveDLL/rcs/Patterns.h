#pragma once
#include <Windows.h>
#include <cstdint>

// определены в MainDLL.cpp
void Log(const char* fmt, ...);

// ---------------------------------------------------------------------------
// Резолв адресов по сигнатурам вместо статических RVA из дампера.
// Оффсеты протухают каждое обновление игры, сигнатуры живут по несколько
// билдов, так что всё критичное ищем паттерном, а статику из SDK.h держим
// только как фолбэк.
//
// Паттерны из дампа patterns.json (503 записи, client/engine2/server/...).
// resolve:
//   raw    — адрес совпадения и есть адрес функции
//   riprel — совпадение указывает на инструкцию с RIP-relative смещением
//            (3 байта опкода + int32 disp, длина 7): target = addr + 7 + disp
//   rel32  — совпадение указывает на call rel32: target = addr + 5 + disp
// ---------------------------------------------------------------------------

namespace sigs {

enum class resolve_t {
    raw,
    riprel,
    rel32
};

// --- результаты (абсолютные VA, 0 = не найдено) ---
inline uintptr_t createMove            = 0; // bool CreateMove(void*, int slot, float sampleTime, bool active)
inline uintptr_t writeSubtickFromEntry = 0; // заполнение CCSGOInputHistoryEntry (цель silent aim)
inline uintptr_t pEntityList           = 0; // адрес глобала CGameEntitySystem*
inline uintptr_t pLocalPlayerController = 0; // адрес глобала CCSPlayerController*

inline int hexval(const char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

inline uintptr_t FindPattern(const uintptr_t base, const size_t size, const char* pattern) {
    constexpr size_t MAX_LEN = 256;
    uint8_t          bytes[MAX_LEN]{};
    bool             wild[MAX_LEN]{};
    size_t           len = 0;

    for (const char* p = pattern; *p && len < MAX_LEN;) {
        if (*p == ' ') {
            ++p;
            continue;
        }
        if (*p == '?') {
            wild[len] = true;
            bytes[len] = 0;
            ++len;
            ++p;
            if (*p == '?')
                ++p;
            continue;
        }
        const int hi = hexval(p[0]);
        const int lo = p[1] ? hexval(p[1]) : -1;
        if (hi < 0 || lo < 0)
            return 0;
        wild[len]  = false;
        bytes[len] = static_cast<uint8_t>(hi << 4 | lo);
        ++len;
        p += 2;
    }
    if (!len || len > size)
        return 0;

    const auto* mem = reinterpret_cast<const uint8_t*>(base);
    for (size_t i = 0; i + len <= size; ++i) {
        bool ok = true;
        for (size_t j = 0; j < len; ++j) {
            if (!wild[j] && mem[i + j] != bytes[j]) {
                ok = false;
                break;
            }
        }
        if (ok)
            return base + i;
    }
    return 0;
}

inline uintptr_t Resolve(const uintptr_t addr, const resolve_t how) {
    if (!addr)
        return 0;
    switch (how) {
    case resolve_t::riprel: {
        const auto disp = *reinterpret_cast<int32_t*>(addr + 3);
        return addr + 7 + disp;
    }
    case resolve_t::rel32: {
        const auto disp = *reinterpret_cast<int32_t*>(addr + 1);
        return addr + 5 + disp;
    }
    case resolve_t::raw:
    default:
        return addr;
    }
}

// адрес закоммичен и доступен на запись? нужно перед любой записью по
// статическому оффсету: если оффсет от чужого билда, мы иначе гадим в случайную
// страницу client.dll и игра падает не там, где виновата.
inline bool IsWritable(const uintptr_t addr, const size_t size) {
    if (addr < 0x10000)
        return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi)))
        return false;
    if (mbi.State != MEM_COMMIT)
        return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return false;
    const DWORD ok = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (!(mbi.Protect & ok))
        return false;
    const uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return addr + size <= end;
}

struct entry_t {
    const char* name;
    const char* pattern;
    resolve_t   how;
    uintptr_t*  out;
    bool        critical;
};

// критичные — без них модуль не поднимается; остальные просто логируются.
inline const entry_t k_client[] = {
    {"CreateMove",
     "48 8B C4 4C 89 40 18 48 89 48 08 55 53 41 54 41 55",
     resolve_t::raw, &createMove, true},

    // та самая функция, которую в research-репозиториях зовут
    // populate_history_entry: пишет углы/позицию выстрела в input history entry.
    {"WriteSubtickFromEntry",
     "48 89 5C 24 ? 55 57 41 56 48 8D 6C 24 ? 48 81 EC B0 00 00 00 8B 01 48 8B F9 81 4A 10 00 02",
     resolve_t::raw, &writeSubtickFromEntry, false},

    {"pEntityList",
     "48 89 0D ? ? ? ? E9 ? ? ? ? CC",
     resolve_t::riprel, &pEntityList, false},

    {"pLocalPlayerController",
     "48 8B 05 ? ? ? ? 41 89 BE",
     resolve_t::riprel, &pLocalPlayerController, false},
};

// возвращает false только если не нашлась критичная сигнатура.
inline bool ResolveClient(const uintptr_t client, const size_t size) {
    bool ok = true;
    for (const auto& e : k_client) {
        const uintptr_t hit  = FindPattern(client, size, e.pattern);
        const uintptr_t addr = Resolve(hit, e.how);
        *e.out               = addr;

        if (addr) {
            Log("[sig] %-24s client+0x%llX\n", e.name,
                static_cast<unsigned long long>(addr - client));
        } else {
            Log("[sig] %-24s NOT FOUND%s\n", e.name, e.critical ? " (critical)" : "");
            if (e.critical)
                ok = false;
        }
    }
    return ok;
}

// сверка найденного по сигнатуре с тем, что лежит в SDK.h. если дельта не ноль,
// статические оффсеты взяты от другого билда и всему, что не резолвится
// сигнатурой (dwForceJump, dwViewAngles, нетвары), верить нельзя.
inline int ReportDrift(const uintptr_t client, const char* name,
                       const uintptr_t resolved, const uintptr_t staticOff) {
    if (!resolved)
        return 0;
    const auto rva   = static_cast<long long>(resolved - client);
    const auto delta = rva - static_cast<long long>(staticOff);
    Log("[sig] drift %-22s sig=0x%llX sdk=0x%llX delta=%s0x%llX\n", name,
        static_cast<unsigned long long>(rva), static_cast<unsigned long long>(staticOff),
        delta < 0 ? "-" : "", static_cast<unsigned long long>(delta < 0 ? -delta : delta));
    return delta == 0 ? 0 : 1;
}

} // namespace sigs
