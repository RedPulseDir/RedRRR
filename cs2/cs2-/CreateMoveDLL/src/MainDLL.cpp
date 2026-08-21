#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "../rcs/MinHook/MinHook.h"
#include "../rcs/Config.h"
#include "../rcs/Menu.h"
#include "../rcs/Patterns.h"
#include "../rcs/SDK.h"
#include "../rcs/SilentAim.h"

// Всё, что раньше было рубильниками времени компиляции, живёт в cfg::g и
// крутится из меню по INSERT. Тут остались только константы протокола.
constexpr uint32_t FJ_PRESS   = 65537; // 0x10001
constexpr uint32_t FJ_RELEASE = 256;   // 0x100

FILE*   g_console = nullptr;
FILE*   g_logFile = nullptr; // duplicate of console output, survives fullscreen hiding it
HMODULE g_hMod    = nullptr;

void Log(const char* fmt, ...) {
    va_list a;
    if (g_console) {
        va_start(a, fmt);
        vfprintf(g_console, fmt, a);
        va_end(a);
        fflush(g_console);
    }
    if (g_logFile) {
        va_start(a, fmt);
        vfprintf(g_logFile, fmt, a);
        va_end(a);
        fflush(g_logFile);
    }
}

// opens bhop.log next to the dll. called once during init. lets us see logs
// even when cs2 fullscreen has the console window buried.
// DLL приходит manual map'ом, её нет в списке модулей процесса, поэтому
// GetModuleFileNameA(g_hMod) возвращает 0 и лог раньше падал в текущую
// директорию cs2 (game\\bin\\win64), где его никто не искал. Кладём в %TEMP%.
static void BuildLogPath(char* path, const size_t cap, const char* logName) {
    path[0] = '\0';
    if (g_hMod && GetModuleFileNameA(g_hMod, path, static_cast<DWORD>(cap))) {
        if (char* slash = strrchr(path, '\\'); slash && (slash + 1 - path) + strlen(logName) < cap) {
            strcpy_s(slash + 1, cap - (slash + 1 - path), logName);
            return;
        }
    }
    char tmp[MAX_PATH] = {};
    if (const DWORD n = GetTempPathA(MAX_PATH, tmp); n > 0 && n + strlen(logName) < cap) {
        strcpy_s(path, cap, tmp);
        strcat_s(path, cap, logName);
        return;
    }
    strcpy_s(path, cap, logName);
}

static void OpenLogFile() {
    char path[MAX_PATH] = {};
    BuildLogPath(path, MAX_PATH, "bhop.log");
    fopen_s(&g_logFile, path, "w");
    if (g_logFile) {
        fprintf(g_logFile, "[bhop] log: %s\n", path);
        fflush(g_logFile);
    }
    if (g_console)
        fprintf(g_console, "[bhop] log: %s\n", path);
}

// dumps the last error to a file next to the dll. overwrites every call,
// only keep the most recent one. still there after the console closes.
static void LogError(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    if (g_console) {
        vfprintf(g_console, fmt, a);
        fflush(g_console);
    }
    va_end(a);

    char path[MAX_PATH] = {};
    BuildLogPath(path, MAX_PATH, "bhop_error.log");

    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (!f)
        return;
    va_start(a, fmt);
    vfprintf(f, fmt, a);
    va_end(a);
    fclose(f);
}

size_t ModuleSize(const uintptr_t base) {
    if (!base) return 0;
    const auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    return nt->OptionalHeader.SizeOfImage;
}

// Прототип. В дампе сигнатур IDA показывает
//   bool CreateMove(void*, int nSlot, float flInputSampleTime, bool bActive)
// но это враньё: пролог функции
//   48 8B C4        mov rax, rsp
//   4C 89 40 18     mov [rax+18h], r8     <- хоумит 3-й аргумент из R8
//   48 89 48 08     mov [rax+08h], rcx
// хоумит именно R8, а не xmm2. Будь третий аргумент float, там был бы
// movss [rax+18h], xmm2. Значит все аргументы целочисленные, и старый
// прототип (pThis, slot, cmd) был прав — это я его зря "починил".
//
// Объявляем 4 целочисленных аргумента (rcx/rdx/r8/r9) и прокидываем их как
// есть: так forward корректен независимо от того, 3 их на самом деле или 4.
// Возврат uint64_t, а не bool: в логе видно "original returned 16", то есть
// функция кладёт в rax не булеву единицу, а флаги. Через bool это значение
// схлопывалось в 1, и вызывающий код получал не то, что ждал.
using fnCreateMove = uint64_t(__fastcall*)(void*, uintptr_t, uintptr_t, uintptr_t);

uintptr_t    g_clientBase = 0;
uintptr_t    g_engineBase = 0;
fnCreateMove oCreateMove  = nullptr;
uint32_t*    g_pForceJump = nullptr;
uint32_t*    g_pForceAttack = nullptr;
void**       g_pNGC       = nullptr; // &CNetworkGameClient*

// dwForceJump — статический оффсет, сигнатуры на него нет. Если он от чужого
// билда, запись 4 байт уходит в случайное место client.dll и игра падает.
// Поэтому пишем только через эти обёртки: адрес проверяется один раз при
// инициализации, дальше nullptr = молча ничего не делаем.
static void SetForceJump(const uint32_t value) {
    if (!cfg::g.bhop || !g_pForceJump)
        return;
    *g_pForceJump = value;
}

static uint32_t GetForceJump() {
    return g_pForceJump ? *g_pForceJump : 0u;
}

// Триггербот. Жмём kbutton атаки сами, но ТОЛЬКО если жали именно мы:
// иначе безусловный RELEASE каждый тик отменял бы обычную стрельбу игрока.
static bool      g_trigPressed = false;
static ULONGLONG g_trigNext    = 0;
static ULONGLONG g_trigRelease = 0;

static void UpdateTrigger(const bool wantFire) {
    if (!g_pForceAttack)
        return;

    const ULONGLONG now = GetTickCount64();

    if (wantFire) {
        if (!g_trigPressed && now >= g_trigNext) {
            *g_pForceAttack = FJ_PRESS;
            g_trigPressed   = true;
            g_trigRelease   = now + 15;
        } else if (g_trigPressed && now >= g_trigRelease) {
            *g_pForceAttack = FJ_RELEASE;
            g_trigPressed   = false;
            g_trigNext      = now + static_cast<ULONGLONG>(cfg::g.triggerMs);
        }
    } else if (g_trigPressed) {
        *g_pForceAttack = FJ_RELEASE;
        g_trigPressed   = false;
    }
}

// two ways we can unload: user hit END, or DllMain got DETACH'd.
// DETACH runs under the loader lock so FreeConsole/fclose/FreeLibrary
// will deadlock. gotta skip cleanup in that case.
enum UnloadReason : int {
    UNLOAD_NONE   = 0,
    UNLOAD_USER   = 1,
    UNLOAD_DETACH = 2
};

volatile long g_unloadReason = UNLOAD_NONE;

// one state change per server tick, no matter how many CreateMove calls.
static int  g_lastBhopTick = -1;
// fallback toggle when pawn/ground can't be resolved (warmup, respawn, etc).
// keeps us at old-tick-flip behavior instead of going silent.
static bool g_tickFlip = false;

// Остатки старых тюнаблов. Всё настраиваемое переехало в cfg::g (меню),
// здесь только то, что менять на лету смысла не имеет.
static constexpr float AS_MIN_SPEED = 60.0f; // не стрейфим, если почти стоим
static constexpr float AS_STEP_DEG  = 1.6f;  // макс. поворот вида за вызов, град
static constexpr float AS_PI        = 3.14159265358979f;

// diagnostic state. heartbeat prints a few times at startup regardless of
// anything else so we know the hook is alive; after that, throttled to every
// ~10 ticks while bhop is active. state-edges (space down/up, ground/air)
// log immediately.
// пошаговая трассировка первых вызовов хука с живой пешкой: если игра падает
// внутри хука, последняя строка в логе покажет, на каком шаге.
static constexpr bool TRACE_HOOK  = true;
static int            g_traceLeft = 10;

// CEntityIdentity stride: 0x78 (120) против 0x70 (112) — разные источники дают
// разное, поэтому определяем на живой памяти по совпадению хэндла.
static size_t g_entStride   = offsets::ENT_SLOT_STRIDE;
static bool   g_strideKnown = false;

static int  g_lastDiagTick     = -1;
static int  g_heartbeatCount   = 0;
static bool g_prevHeld         = false;
static bool g_prevOnGround     = false;
static bool g_prevSnapValid    = false;

// local pawn resolution, matching the engine's own GetLocalPlayerPawn.
// dwLocalPlayerPawn существует в дампе (client.dll+0x23AA118), но здесь
// используется путь через контроллер:
//   controller  = *(client + dwLocalPlayerController)
//   handle      = controller + m_hPawn (0x6BC)
//   pageTable   = *(client + dwEntityList) + ENT_PAGE_TABLE_OFF (0x10)
//   listEntry   = *(pageTable + ENT_PAGE_STRIDE * ((idx & 0x7FFF) >> 9))
//   identity    = listEntry + ENT_SLOT_STRIDE * (idx & 0x1FF)
//   pawn        = *(identity)   [serial check: *(identity + 0x10) == handle]
static void* ResolveLocalPawn() {
    // ВСЕ чтения через silent::SafeRead: dwLocalPlayerController — статический
    // оффсет, и если он от другого билда, тут лежит мусор. Разыменование мусора
    // без SEH = мгновенный AV и вылет игры (именно так оно и падало).
    const uintptr_t ctrlAddr = sigs::pLocalPlayerController ? sigs::pLocalPlayerController
                                                            : g_clientBase + offsets::dwLocalPlayerController;
    void* controller = nullptr;
    if (!silent::SafeRead(ctrlAddr, controller) || !controller)
        return nullptr;

    uint32_t handle = 0;
    if (!silent::SafeRead(reinterpret_cast<uintptr_t>(controller) + offsets::m_hPawn, handle))
        return nullptr;
    if (handle == UINT32_MAX || handle == 0)
        return nullptr;

    const uintptr_t listAddr = sigs::pEntityList ? sigs::pEntityList
                                                 : g_clientBase + offsets::dwEntityList;
    uintptr_t entitySystem = 0;
    if (!silent::SafeRead(listAddr, entitySystem) || !entitySystem)
        return nullptr;

    const uint32_t index    = handle & 0x7FFF;
    uintptr_t      pageBase = 0;
    if (!silent::SafeRead(entitySystem + offsets::ENT_PAGE_TABLE_OFF +
                              offsets::ENT_PAGE_STRIDE * (index >> 9),
                          pageBase) ||
        !pageBase)
        return nullptr;

    // пробуем stride: сначала текущий, потом альтернативный. критерий — в
    // slot+0x10 лежит наш же хэндл, а у пешки валидный scene node.
    const size_t strides[2] = {g_entStride, g_entStride == 120 ? 112u : 120u};
    for (int attempt = 0; attempt < (g_strideKnown ? 1 : 2); ++attempt) {
        const size_t    stride = strides[attempt];
        const uintptr_t slot   = pageBase + stride * (index & 0x1FF);

        // serial check: slot's cached handle must match ours, else the entity
        // was freed and the slot reused.
        uint32_t slotHandle = 0;
        if (!silent::SafeRead(slot + offsets::ENT_SLOT_HANDLE, slotHandle) || slotHandle != handle)
            continue;

        void* pawn = nullptr;
        if (!silent::SafeRead(slot, pawn) || !pawn)
            continue;

        uintptr_t node = 0;
        if (!silent::SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_pGameSceneNode, node) ||
            node < 0x10000)
            continue;

        if (!g_strideKnown) {
            g_strideKnown = true;
            g_entStride   = stride;
            Log("[bhop] entity stride = %zu (handle 0x%X -> pawn %p)\n", stride, handle, pawn);
        }
        return pawn;
    }
    return nullptr;
}

// don't intercept while alt-tabbed or typing in the console / menu.
static bool GameHasFocus() {
    const HWND fg  = GetForegroundWindow();
    DWORD      pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

// gather everything we read off the pawn/mvs once per hook call so pre- and
// post-phase share the same snapshot instead of re-walking the entity table.
struct TickSnapshot {
    void*    pawn     = nullptr;
    void*    mvs      = nullptr;
    bool     onGround = false;
    bool     valid    = false;
    float    speed2d  = 0.0f;
    float    yawVel   = 0.0f;
    float    stamina  = 0.0f;
    uint32_t flags    = 0;
};

static TickSnapshot ReadTickSnapshot() {
    TickSnapshot s{};
    s.pawn = ResolveLocalPawn();
    if (!s.pawn)
        return s;

    const uintptr_t pawn = reinterpret_cast<uintptr_t>(s.pawn);

    uint32_t hGround = UINT32_MAX;
    if (!silent::SafeRead(pawn + offsets::m_fFlags, s.flags))
        return s;
    silent::SafeRead(pawn + offsets::m_hGroundEntity, hGround);
    s.onGround = (s.flags & offsets::FL_ONGROUND) != 0 || hGround != UINT32_MAX;

    float vel[3]{};
    if (silent::SafeRead(pawn + offsets::m_vecVelocity, vel))
        s.speed2d = std::sqrt(vel[0] * vel[0] + vel[1] * vel[1]);

    // QAngle layout: [0]=pitch, [1]=yaw, [2]=roll. yaw-velocity drives autostrafe.
    float angVel[3]{};
    if (silent::SafeRead(pawn + offsets::m_angEyeAnglesVelocity, angVel))
        s.yawVel = angVel[1];

    if (silent::SafeRead(pawn + offsets::m_pMovementServices, s.mvs) && s.mvs) {
        // stamina lives on CCSPlayer_MovementServices (subclass). offset is valid
        // for CS players - this DLL only targets CS2 so every local pawn is one.
        silent::SafeRead(reinterpret_cast<uintptr_t>(s.mvs) + offsets::m_flStamina, s.stamina);
    }
    s.valid = true;
    return s;
}

// autostrafe: while airborne and player is turning their view, write the
// matching strafe direction into the cmd's move fields so the engine applies
// optimal air-accel. perpendicular wishdir maxes air-acceleration since the
// dot(vel, wish) term goes to zero and add_speed hits the airaccel cap.
//
// driven by m_angEyeAnglesVelocity.y so it tracks actual mouse movement rather
// than auto-zigzagging (which would look obviously bot-like). no mouse input
// -> no override, original move values pass through.
//
// run BEFORE oCreateMove so our values are what the cmd-builder reads. the
// cmd ships to the server with our sidemove, server applies the same airaccel
// we predict client-side, so no prediction desync + no CRC issue (we're not
// mutating the cmd itself, just the pre-cmd inputs the builder pulls from).
static void ApplyAutostrafe(const TickSnapshot& s) {
    if (!s.valid || !s.mvs || s.onGround)
        return;

    float side = 0.0f;
    if (s.yawVel > cfg::g.yawDeadzone)
        side = +cfg::g.strafeMove; // turning right
    else if (s.yawVel < -cfg::g.yawDeadzone)
        side = -cfg::g.strafeMove; // turning left
    else
        return; // player isn't steering -> leave their input alone

    auto* mvsB = static_cast<uint8_t*>(s.mvs);
    // write BOTH the cmd-staging value (read by the builder for cmd.sidemove)
    // AND the physics-consumed value so any immediate prediction step lines up.
    *reinterpret_cast<float*>(mvsB + offsets::m_flCmdLeftMove) = side;
    *reinterpret_cast<float*>(mvsB + offsets::m_flLeftMove)    = side;
}

// normalize an angle to [-180, 180].
static float WrapAngle(float a) {
    while (a > 180.0f)
        a -= 360.0f;
    while (a < -180.0f)
        a += 360.0f;
    return a;
}

// experimental: rotate the pawn's view yaw toward the optimal airstrafe
// heading. if the cmd-builder sources its viewangle from m_angEyeAngles the
// cmd ships with our rotated yaw, CRC matches (we modified before CRC runs),
// and the engine naturally applies air-accel with forward-held-wishdir now
// perpendicular to velocity -> airspeed climbs past run cap -> even after
// CS2's ~0.65 jump clamp we launch well above 165 u/s.
//
// if the cmd-builder pulls viewangles from elsewhere (separate input cache),
// writing here changes nothing about the shipped cmd and we'll just see the
// view twitch with no speed gain. either way it's not a cmd mutation so no
// CRC kick risk.
static void ApplyViewAutostrafe(const TickSnapshot& s) {
    if (!cfg::g.viewStrafe || !s.valid || s.onGround)
        return;

    // velocity too low -> can't compute a stable direction, and nothing to
    // accelerate anyway. idle-air-strafing just looks like a drunk bot.
    if (s.speed2d < AS_MIN_SPEED)
        return;

    auto*        pawnB    = static_cast<uint8_t*>(s.pawn);
    const float* vel      = reinterpret_cast<const float*>(pawnB + offsets::m_vecVelocity);
    const float  velYaw   = std::atan2(vel[1], vel[0]) * (180.0f / AS_PI); // [-180, 180]
    float*       angles   = reinterpret_cast<float*>(pawnB + offsets::m_angEyeAngles);
    const float  curYaw   = angles[1];

    // optimal airstrafe heading: perpendicular to velocity. pick whichever side
    // is closer to where the player is currently looking, so the view only has
    // to drift a little per tick instead of snapping 180 deg.
    const float perpA  = velYaw + 90.0f;
    const float perpB  = velYaw - 90.0f;
    const float deltaA = fabsf(WrapAngle(perpA - curYaw));
    const float deltaB = fabsf(WrapAngle(perpB - curYaw));
    const float target = (deltaA < deltaB) ? perpA : perpB;

    // smooth step toward target, capped per tick so the view drifts rather
    // than snapping. AS_STEP_DEG ~= a tick worth at ~100 deg/sec which is
    // comparable to a human flicking slightly while bhopping.
    const float need      = WrapAngle(target - curYaw);
    const float stepMag   = fabsf(need) < AS_STEP_DEG ? fabsf(need) : AS_STEP_DEG;
    const float step      = (need > 0.0f ? 1.0f : -1.0f) * stepMag;
    angles[1]             = WrapAngle(curYaw + step);
}

static uint64_t __fastcall hkCreateMove(void* pThis, const uintptr_t slot, const uintptr_t cmd,
                                        const uintptr_t a4) {
    // Раньше тут стояло !cfg::g.menuOpen, и бхоп молча умирал, потому что меню
    // открыто по умолчанию. Прыжок работает всегда, когда включён в конфиге.
    const bool held = cfg::g.bhop && (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 && GameHasFocus();
    const TickSnapshot snap = ReadTickSnapshot();

    if (TRACE_HOOK && snap.valid && g_traceLeft > 0) {
        --g_traceLeft;
        Log("[trace] A snapshot ok pawn=%p mvs=%p flags=0x%X ground=%d spd=%.1f\n",
            snap.pawn, snap.mvs, snap.flags, (int)snap.onGround, snap.speed2d);
        if (g_traceLeft == 0)
            Log("[trace] --- trace budget spent, further calls are silent ---\n");
    }

    // heartbeat: first 5 calls print unconditionally so you can see in the log
    // that the hook is actually being invoked and what state it sees.
    if (g_heartbeatCount < 5) {
        g_heartbeatCount++;
        Log("[bhop] hook#%d pawn=%p mvs=%p valid=%d held=%d\n",
            g_heartbeatCount, snap.pawn, snap.mvs, (int)snap.valid, (int)held);
    }

    // edge log: space down / up (immediate, not throttled)
    if (held != g_prevHeld) {
        Log("[bhop] space %s  spd=%.1f stam=%.1f %s\n",
            held ? "DOWN" : "UP", snap.speed2d, snap.stamina,
            snap.valid ? (snap.onGround ? "GND" : "AIR") : "NO-PAWN");
        g_prevHeld = held;
    }
    // edge log: ground / air (immediate while bhop engaged)
    if (snap.valid && held && snap.onGround != g_prevOnGround) {
        Log("[bhop] %s  spd=%.1f stam=%.1f\n",
            snap.onGround ? "LAND" : "JUMP", snap.speed2d, snap.stamina);
        g_prevOnGround = snap.onGround;
    }

    // experimental view-yaw autostrafe. writes to pawn->m_angEyeAngles pre-
    // oCreateMove hoping the cmd-builder pulls viewangles from there. if it
    // does, the cmd ships with rotated yaw -> natural airstrafe kicks in.
    // toggle with AUTOSTRAFE_ON. visible view drift is expected.
    // move-автострейф: пишет в m_flCmdLeftMove/m_flLeftMove, то есть в реальный
    // ввод, а не в угол взгляда. view-версия оставлена под отдельным флагом,
    // она экспериментальная и мутит воду, когда включён silent aim.
    if (cfg::g.moveStrafe && held && snap.valid && !snap.onGround)
        ApplyAutostrafe(snap);
    if (cfg::g.viewStrafe && held && snap.valid && !snap.onGround)
        ApplyViewAutostrafe(snap);

    // silent aim: подменяем QAngle-источник только на время построения cmd,
    // Restore() сразу после оригинала возвращает угол камеры на место.
    const bool silentApplied = cfg::g.silent && silent::Apply(snap.pawn);

    if (TRACE_HOOK && snap.valid && g_traceLeft > 0)
        Log("[trace] B calling original\n");

    const uint64_t ret = oCreateMove(pThis, slot, cmd, a4);

    if (TRACE_HOOK && snap.valid && g_traceLeft > 0)
        Log("[trace] C original returned 0x%llX\n", static_cast<unsigned long long>(ret));

    if (silentApplied)
        silent::Restore();

    UpdateTrigger(cfg::g.triggerbot && silent::g_hasTarget && !cfg::g.menuOpen && GameHasFocus());

    if (!held) {
        SetForceJump(FJ_RELEASE);
        g_lastBhopTick = -1;
        g_tickFlip     = false;
        return ret;
    }

    void* ngc = g_pNGC ? *g_pNGC : nullptr;
    if (!ngc)
        return ret;
    const int tick = *reinterpret_cast<int*>(static_cast<uint8_t*>(ngc) + offsets::dwNetworkGameClient_clientTick);
    if (tick == g_lastBhopTick)
        return ret;
    g_lastBhopTick = tick;

    if (snap.valid) {
        if (snap.onGround) {
            // stamina gate (currently disabled by STAMINA_CAP = 200; we log
            // actual values so you can pick a real threshold from data).
            if (snap.stamina > cfg::g.staminaCap) {
                SetForceJump(FJ_RELEASE);
            } else {
                // subtick: emit jump edge at tick-fraction 0.0 so we don't
                // waste the fraction between ground contact and the default
                // (late) subtick slot.
                // m_arrForceSubtickMoveWhen — статический нетвар-оффсет; если он
                // от другого билда, это запись 16 байт в чужую память.
                if (cfg::g.subtick && snap.mvs) {
                    const uintptr_t arrAddr =
                        reinterpret_cast<uintptr_t>(snap.mvs) + offsets::m_arrForceSubtickMoveWhen;
                    if (sigs::IsWritable(arrAddr, sizeof(float) * 4)) {
                        float* arr = reinterpret_cast<float*>(arrAddr);
                        arr[0] = arr[1] = arr[2] = arr[3] = 0.0f;
                    }
                }
                SetForceJump(FJ_PRESS);
            }
        } else {
            SetForceJump(FJ_RELEASE);
        }
        g_tickFlip = false;

        // periodic diag while bhop active: every 10 ticks (~6x/sec at 64tr).
        // kept short so scroll stays readable in bhop.log.
        if (tick - g_lastDiagTick >= 10 || g_lastDiagTick < 0) {
            g_lastDiagTick = tick;
            Log("[bhop] t=%d spd=%5.1f stam=%5.1f yawv=%+6.2f %s fj=0x%X\n",
                tick, snap.speed2d, snap.stamina, snap.yawVel,
                snap.onGround ? "GND" : "AIR", GetForceJump());
        }
        return ret;
    }

    // fallback: pawn unreachable - old alternating flip so we still get
    // something instead of silence.
    g_tickFlip    = !g_tickFlip;
    SetForceJump(g_tickFlip ? FJ_PRESS : FJ_RELEASE);

    // warn once every ~2 sec that pawn is unreachable so we notice if this
    // state persists instead of bouncing out.
    if (tick - g_lastDiagTick >= 128 || g_lastDiagTick < 0) {
        g_lastDiagTick = tick;
        Log("[bhop] t=%d NO-PAWN fallback flip=%d\n", tick, (int)g_tickFlip);
    }

    return ret;
}

static DWORD WINAPI MainThread(const LPVOID hMod) {
    AllocConsole();
    freopen_s(&g_console, "CONOUT$", "w", stdout);
    OpenLogFile();
    Log("[bhop] loaded\n");

    HMODULE client;
    while (!((client = GetModuleHandleA("client.dll"))))
        Sleep(100);
    HMODULE engine;
    while (!((engine = GetModuleHandleA("engine2.dll"))))
        Sleep(100);

    g_clientBase      = reinterpret_cast<uintptr_t>(client);
    g_engineBase      = reinterpret_cast<uintptr_t>(engine);
    const size_t size = ModuleSize(g_clientBase);

    Log("[bhop] modules resolved\n");

    Log("[bhop] SEH self-test: probing...\n");
    if (silent::SehSelfTest()) {
        Log("[bhop] SEH self-test: OK, faults are caught\n");
    } else {
        LogError("[!] SEH self-test: probe did NOT fault, something is off\n");
    }

    const uintptr_t fjAddr = g_clientBase + offsets::dwForceJump;
    if (sigs::IsWritable(fjAddr, sizeof(uint32_t))) {
        g_pForceJump = reinterpret_cast<uint32_t*>(fjAddr);
        Log("[bhop] forceJump @ client+0x%llX initial=0x%X\n",
            static_cast<unsigned long long>(offsets::dwForceJump), *g_pForceJump);
        // kbutton_t держит маленькие целые (0 / 0x100 / 0x10001). если тут
        // указатель или мусор — оффсет не от этого билда, писать нельзя.
        if (*g_pForceJump > 0x00FFFFFFu) {
            LogError("[!] forceJump: value 0x%X doesn't look like kbutton_t, writes disabled\n",
                     *g_pForceJump);
            g_pForceJump = nullptr;
        }
    } else {
        LogError("[!] forceJump: client+0x%llX not writable, writes disabled\n",
                 static_cast<unsigned long long>(offsets::dwForceJump));
        g_pForceJump = nullptr;
    }

    const uintptr_t atkAddr = g_clientBase + offsets::dwForceAttack;
    if (sigs::IsWritable(atkAddr, sizeof(uint32_t)) && *reinterpret_cast<uint32_t*>(atkAddr) <= 0x00FFFFFFu) {
        g_pForceAttack = reinterpret_cast<uint32_t*>(atkAddr);
        Log("[bhop] forceAttack @ client+0x%llX initial=0x%X\n",
            static_cast<unsigned long long>(offsets::dwForceAttack), *g_pForceAttack);
    } else {
        LogError("[!] forceAttack: client+0x%llX unusable, triggerbot disabled\n",
                 static_cast<unsigned long long>(offsets::dwForceAttack));
    }

    // silent aim читает ту же кнопку, чтобы ловить перебинженный огонь
    silent::g_clientForAttack = g_clientBase;

    const uintptr_t ngcAddr = g_engineBase + offsets::dwNetworkGameClient;
    g_pNGC = sigs::IsWritable(ngcAddr, sizeof(void*)) ? reinterpret_cast<void**>(ngcAddr) : nullptr;
    if (!g_pNGC)
        LogError("[!] dwNetworkGameClient: engine2+0x%llX not readable\n",
                 static_cast<unsigned long long>(offsets::dwNetworkGameClient));

    if (!sigs::ResolveClient(g_clientBase, size)) {
        LogError("[!] critical signature missing (CreateMove)\n");
        return 1;
    }
    const uintptr_t cmAddr = sigs::createMove;

    // сверяем сигнатуры со статикой SDK.h: ненулевая дельта = оффсеты от
    // другого билда, и всё, что не резолвится сигнатурой, врёт.
    int drift = 0;
    drift += sigs::ReportDrift(g_clientBase, "dwEntityList", sigs::pEntityList, offsets::dwEntityList);
    drift += sigs::ReportDrift(g_clientBase, "dwLocalPlayerController", sigs::pLocalPlayerController,
                               offsets::dwLocalPlayerController);
    if (drift)
        LogError("[!] SDK.h offsets are from a different build (%d mismatch), "
                 "static offsets unreliable\n", drift);

    if (MH_Initialize() != MH_OK) {
        Log("[!] MH_Initialize\n");
        return 1;
    }
    if (MH_CreateHook(reinterpret_cast<LPVOID>(cmAddr), &hkCreateMove, reinterpret_cast<LPVOID*>(&oCreateMove)) !=
        MH_OK) {
        Log("[!] MH_CreateHook\n");
        return 1;
    }
    if (MH_EnableHook(reinterpret_cast<LPVOID>(cmAddr)) != MH_OK) {
        Log("[!] MH_EnableHook\n");
        return 1;
    }
    Log("[bhop] hook ON, SPACE=bhop, END=unload\n");

    silent::Install();

    if (!menu::Install())
        LogError("[!] menu: d3d11 hook failed, running without GUI\n");
    Log("[silent] armed: INSERT=menu, hold LMB=silent aim (fov %.0f)\n", cfg::g.fov);

    while (g_unloadReason == UNLOAD_NONE && !(GetAsyncKeyState(VK_END) & 0x8000))
        Sleep(50);

    // if END got us out, mark it USER. if DllMain already flagged DETACH,
    // don't touch it.
    InterlockedCompareExchange(&g_unloadReason, UNLOAD_USER, UNLOAD_NONE);

    menu::Remove();
    silent::Remove();
    MH_DisableHook(reinterpret_cast<LPVOID>(cmAddr));
    // give any in-flight hkCreateMove a moment to finish before we nuke
    // the trampoline pages
    Sleep(250);
    MH_Uninitialize();

    if (g_unloadReason == UNLOAD_DETACH) {
        // loader lock is held, anything fancy here deadlocks.
        // FreeLibraryAndExitThread is pointless too since we're already going away.
        // client.dll might be gone already so don't touch g_pForceJump either.
        return 0;
    }

    SetForceJump(FJ_RELEASE);
    if (g_pForceAttack && g_trigPressed)
        *g_pForceAttack = FJ_RELEASE;
    Log("[bhop] unloaded\n");
    if (g_console)
        fclose(g_console);
    if (g_logFile) {
        fclose(g_logFile);
        g_logFile = nullptr;
    }
    FreeConsole();
    FreeLibraryAndExitThread(static_cast<HMODULE>(hMod), 0);
}

BOOL APIENTRY DllMain(const HMODULE hMod, const DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hMod = hMod;
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, MainThread, hMod, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        // loader lock is held here. bare minimum, get out.
        // MainThread checks this flag and skips the stuff that would hang.
        InterlockedExchange(&g_unloadReason, UNLOAD_DETACH);
    }
    return TRUE;
}