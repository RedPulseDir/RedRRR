#pragma once
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "Config.h"
#include "MinHook/MinHook.h"
#include "Patterns.h"
#include "SDK.h"

// определены в MainDLL.cpp
extern uintptr_t g_clientBase;
void Log(const char* fmt, ...);

// ---------------------------------------------------------------------------
// Silent aim.
//
// Два пути, primary и fallback.
//
// PRIMARY — хук WriteSubtickFromEntry (в чужих проектах её зовут
// populate_history_entry). Это функция, которая заполняет запись input history:
// углы взгляда, позицию выстрела и то, куда по мнению клиента целились. Её
// пролог:
//     8B 01                mov eax,[rcx]        ; rcx = input history entry
//     48 8B F9             mov rdi,rcx
//     81 4A 10 00 02 00 00 or  [rdx+10],200h    ; rdx = usercmd pb
// а дальше углы из entry уезжают в [rdx+0x18] / [rdx+0x1C] — ровно те адреса,
// которые в разборе TKazer'а меняли мид-функциональным хуком через Cheat Engine.
// Подменяем углы в entry ДО вызова оригинала и возвращаем сразу после: в cmd
// улетает наш угол, камера не двигается. Заодно правим shoot_position и
// target_* поля, по которым сервер сверяет hit-reg.
//
// FALLBACK — если сигнатура не нашлась, работает старый способ: своп
// client.dll + dwViewAngles вокруг вызова оригинального CreateMove. Грубее,
// hit-reg поля остаются настоящими, но лучше чем ничего.
// ---------------------------------------------------------------------------

namespace silent {

// --- статика, что не крутится из меню -------------------------------------
constexpr int   KEY_TOGGLE   = VK_INSERT;  // теперь открывает меню
constexpr int   KEY_AIM      = VK_LBUTTON; // удерживается -> подменяем угол
constexpr float PI_F         = 3.14159265358979f;
constexpr int   MAX_PLAYERS  = 64;
// компенсация aim punch. m_aimPunchAngle в текущем дампе a2x отсутствует,
// поэтому галка в меню заблокирована: поставь реальный оффсет -> заработает.
constexpr uintptr_t OFF_AIM_PUNCH = 0;
constexpr float     PUNCH_SCALE   = 2.0f;
// --------------------------------------------------------------------------

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};
struct QAng {
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;
};

// CCSGOInputHistoryEntry, как её видит WriteSubtickFromEntry (rcx).
struct input_history_entry_t {
    int32_t frame_tick_count;      // 0x00
    float   frame_tick_fraction;   // 0x04
    int32_t player_tick_count;     // 0x08
    float   player_tick_fraction;  // 0x0C
    float   view_angles[3];        // 0x10  <- pitch/yaw/roll
    float   shoot_position[3];     // 0x1C
    int32_t target_ent_index;      // 0x28
    float   target_head_position[3]; // 0x2C
    float   target_abs_origin[3];  // 0x38
    float   target_angle[3];       // 0x44
    int32_t sv_show_hit_reg;       // 0x50
    int32_t entry_index_max;       // 0x54
    int32_t index;                 // 0x58
};

using write_subtick_fn = int64_t(__fastcall*)(input_history_entry_t* entry,
                                             int64_t                subtick,
                                             char                   verify,
                                             double                 a4,
                                             int                    a5,
                                             int64_t                player_pawn);

inline bool g_applied    = false;
inline bool g_hooked     = false;
inline bool g_hasTarget  = false; // для оверлея: цель найдена в этом кадре
inline QAng g_backup{};

inline write_subtick_fn oWriteSubtick = nullptr;

// чтения идут по сырым указателям из чужих структур: dormant-энтити и гонки с
// сервером легко дают битый указатель. SEH дешевле, чем VirtualQuery на каждое
// поле, и не роняет игру.
template <typename T>
inline bool SafeRead(uintptr_t addr, T& out) {
    if (addr < 0x10000)
        return false;
    // memcpy, а не присваивание: T может быть массивом (float[3] для QAngle).
#ifdef _MSC_VER
    __try {
        std::memcpy(&out, reinterpret_cast<const void*>(addr), sizeof(T));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(&out, reinterpret_cast<const void*>(addr), sizeof(T));
    return true;
#endif
}

// адреса глобалов: сначала то, что нашёл сигнатурный резолвер, иначе статика.
// Проверка, что SEH вообще работает. DLL приходит manual map'ом, и если лоадер
// не зарегистрировал .pdata через RtlAddFunctionTable, __except не поймает
// ничего: любое кривое чтение убьёт игру, несмотря на SafeRead. Читаем заведомо
// незакоммиченный адрес — если функция вернулась, защита живая; если игра
// умерла прямо здесь, в логе останется строка "probing" и всё станет ясно.
inline bool SehSelfTest() {
    uint32_t tmp = 0;
    return !SafeRead(0x0000700000000000ull, tmp);
}

inline uintptr_t EntityListAddr() {
    return sigs::pEntityList ? sigs::pEntityList : g_clientBase + offsets::dwEntityList;
}
inline uintptr_t LocalControllerAddr() {
    return sigs::pLocalPlayerController ? sigs::pLocalPlayerController
                                        : g_clientBase + offsets::dwLocalPlayerController;
}
inline float* ViewAngles() {
    return reinterpret_cast<float*>(g_clientBase + offsets::dwViewAngles);
}

inline float Norm180(float a) {
    while (a > 180.0f)
        a -= 360.0f;
    while (a < -180.0f)
        a += 360.0f;
    return a;
}

// постраничный обход энтити-листа по индексу.
inline void* EntityByIndex(const int index) {
    uintptr_t entitySystem = 0;
    if (!SafeRead(EntityListAddr(), entitySystem) || !entitySystem)
        return nullptr;

    const uint32_t idx  = static_cast<uint32_t>(index) & 0x7FFF;
    uintptr_t      page = 0;
    if (!SafeRead(entitySystem + offsets::ENT_PAGE_TABLE_OFF + offsets::ENT_PAGE_STRIDE * (idx >> 9), page) || !page)
        return nullptr;

    void* ent = nullptr;
    if (!SafeRead(page + offsets::ENT_SLOT_STRIDE * (idx & 0x1FF), ent))
        return nullptr;
    return ent;
}

inline void* EntityByHandle(const uint32_t handle) {
    if (!handle || handle == UINT32_MAX)
        return nullptr;
    return EntityByIndex(static_cast<int>(handle & 0x7FFF));
}

inline bool SceneNode(void* ent, uintptr_t& node) {
    return SafeRead(reinterpret_cast<uintptr_t>(ent) + offsets::m_pGameSceneNode, node) && node >= 0x10000;
}

inline bool AbsOrigin(void* ent, Vec3& out) {
    uintptr_t node = 0;
    return SceneNode(ent, node) && SafeRead(node + offsets::m_vecAbsOrigin, out);
}

inline bool IsDormant(void* ent) {
    uintptr_t node = 0;
    if (!SceneNode(ent, node))
        return true;
    bool dormant = true;
    return !SafeRead(node + offsets::m_bDormant, dormant) || dormant;
}

// позиция кости из bone matrix. если аниматор ещё не заполнил массив (только
// заспавнился / dormant), вернём false и цель просто пропустим.
inline bool BonePos(void* ent, const int bone, Vec3& out) {
    uintptr_t node = 0;
    if (!SceneNode(ent, node))
        return false;

    uintptr_t bones = 0;
    if (!SafeRead(node + offsets::BONE_ARRAY_OFF, bones) || bones < 0x10000)
        return false;

    if (!SafeRead(bones + offsets::BONE_STRIDE * static_cast<size_t>(bone), out))
        return false;

    return !(out.x == 0.0f && out.y == 0.0f && out.z == 0.0f);
}

inline bool EyePos(void* pawn, Vec3& out) {
    Vec3 origin{};
    Vec3 offset{};
    if (!AbsOrigin(pawn, origin))
        return false;
    if (!SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_vecViewOffset, offset))
        return false;
    out = {origin.x + offset.x, origin.y + offset.y, origin.z + offset.z};
    return true;
}

// «меня видят» через EntitySpottedState_t::m_bSpottedByMask. это не настоящая
// трассировка: маска говорит, кто спотнул цель, а не есть ли прямая линия. для
// честной видимости нужен TraceShape + CTraceFilter.
inline bool SpottedBy(void* ent, const int localIndex) {
    if (localIndex <= 0 || localIndex > 64)
        return true;
    const uintptr_t base = reinterpret_cast<uintptr_t>(ent) + offsets::m_entitySpottedState +
                           offsets::m_bSpottedByMask;
    uint32_t lo = 0, hi = 0;
    if (!SafeRead(base, lo) || !SafeRead(base + 4, hi))
        return false;
    const uint64_t mask = static_cast<uint64_t>(hi) << 32 | lo;
    return (mask & (1ULL << (localIndex - 1))) != 0;
}

inline QAng AngleTo(const Vec3& from, const Vec3& to) {
    const float dx   = to.x - from.x;
    const float dy   = to.y - from.y;
    const float dz   = to.z - from.z;
    const float flat = std::sqrt(dx * dx + dy * dy);

    QAng a{};
    a.pitch = -std::atan2(dz, flat) * (180.0f / PI_F);
    a.yaw   = std::atan2(dy, dx) * (180.0f / PI_F);
    a.roll  = 0.0f;

    if (a.pitch > 89.0f)
        a.pitch = 89.0f;
    else if (a.pitch < -89.0f)
        a.pitch = -89.0f;
    a.yaw = Norm180(a.yaw);
    return a;
}

inline float Dist3(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline float FovDelta(const QAng& view, const QAng& target) {
    const float dp = Norm180(target.pitch - view.pitch);
    const float dy = Norm180(target.yaw - view.yaw);
    return std::sqrt(dp * dp + dy * dy);
}

// локальный контроллер и его индекс в энтити-листе (нужен для spotted-маски).
inline void* LocalController(int& outIndex) {
    outIndex = 0;
    void* local = nullptr;
    if (!SafeRead(LocalControllerAddr(), local) || !local)
        return nullptr;

    for (int i = 1; i <= MAX_PLAYERS; ++i) {
        if (EntityByIndex(i) == local) {
            outIndex = i;
            break;
        }
    }
    return local;
}

struct target_t {
    bool  valid = false;
    int   index = 0;
    Vec3  head{};
    Vec3  origin{};
    QAng  aim{};
    float eyeAngles[3]{};
};

// ближайший к прицелу живой враг в пределах FOV_DEG.
// перебираем контроллеров и берём их пешки — надёжнее, чем гадать, что лежит
// по индексам 1..64 в энтити-листе.
inline bool FindTarget(void* localPawn, const QAng& view, target_t& out) {
    Vec3 eye{};
    if (!EyePos(localPawn, eye))
        return false;

    uint8_t myTeam = 0;
    if (!SafeRead(reinterpret_cast<uintptr_t>(localPawn) + offsets::m_iTeamNum, myTeam))
        return false;

    int   localIndex = 0;
    void* localCtrl  = LocalController(localIndex);

    float bestFov = cfg::g.fov;
    bool  found   = false;

    for (int i = 1; i <= MAX_PLAYERS; ++i) {
        void* ctrl = EntityByIndex(i);
        if (!ctrl || ctrl == localCtrl)
            continue;

        bool alive = false;
        if (!SafeRead(reinterpret_cast<uintptr_t>(ctrl) + offsets::m_bPawnIsAlive, alive) || !alive)
            continue;

        uint32_t hPawn = 0;
        if (!SafeRead(reinterpret_cast<uintptr_t>(ctrl) + offsets::m_hPlayerPawn, hPawn))
            continue;

        void* pawn = EntityByHandle(hPawn);
        if (!pawn || pawn == localPawn)
            continue;

        int hp = 0;
        if (!SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_iHealth, hp) || hp <= 0 || hp > 100)
            continue;

        uint8_t team = 0;
        if (!SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_iTeamNum, team))
            continue;
        if (team == myTeam || team < 2 || team > 3)
            continue;

        if (IsDormant(pawn))
            continue;

        if (cfg::g.visibleOnly && !SpottedBy(pawn, localIndex))
            continue;

        Vec3 origin{};
        if (!AbsOrigin(pawn, origin))
            continue;

        // санити: голова должна быть выше ног и в пределах роста модели,
        // иначе это мусорная кость из недозаполненной матрицы. Диапазон шире
        // прежнего: в приседе и на лестницах голова ниже, и цель терялась.
        Vec3        head{};
        bool        haveBone = BonePos(pawn, cfg::g.aimBone, head);
        const float dz       = haveBone ? head.z - origin.z : 0.0f;
        if (haveBone && (dz < 25.0f || dz > 100.0f))
            haveBone = false;

        if (!haveBone) {
            // матрица костей не готова (только заспавнился, был dormant) —
            // целимся по глазам от origin, это лучше чем пропустить врага
            Vec3 vo{};
            if (!SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_vecViewOffset, vo))
                continue;
            if (vo.z < 10.0f || vo.z > 100.0f)
                continue;
            head = {origin.x + vo.x, origin.y + vo.y, origin.z + vo.z};
        }

        if (Dist3(eye, head) > cfg::g.maxDist)
            continue;

        const QAng  aim = AngleTo(eye, head);
        const float fov = FovDelta(view, aim);
        if (fov >= bestFov)
            continue;

        bestFov      = fov;
        found        = true;
        out.valid    = true;
        out.index    = static_cast<int>(hPawn & 0x7FFF);
        out.head     = head;
        out.origin   = origin;
        out.aim      = aim;
        SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_angEyeAngles, out.eyeAngles[0]);
        SafeRead(reinterpret_cast<uintptr_t>(pawn) + offsets::m_angEyeAngles + 4, out.eyeAngles[1]);
        out.eyeAngles[2] = 0.0f;
    }
    return found;
}

// Атака определяется двумя способами сразу: асинхронное состояние ЛКМ и
// собственный kbutton_t игры. Второе важно, потому что ловит перебинженную
// кнопку огня и не зависит от того, какое окно в фокусе — раньше из-за этого
// silent aim иногда просто не срабатывал.
inline uintptr_t g_clientForAttack = 0;

inline bool AttackDown() {
    if ((GetAsyncKeyState(KEY_AIM) & 0x8000) != 0)
        return true;
    if (g_clientForAttack) {
        uint32_t state = 0;
        // бит 0 = кнопка нажата. Маска 0xFFFF была бы неверной: RELEASE мы
        // пишем как 0x100, и он бы читался как "нажато".
        if (SafeRead(g_clientForAttack + offsets::dwForceAttack, state) && (state & 1u) != 0)
            return true;
    }
    return false;
}

inline bool AimKeyHeld() {
    return AttackDown();
}

// ------------------------- PRIMARY: input history --------------------------

inline int64_t __fastcall hkWriteSubtick(input_history_entry_t* entry,
                                         int64_t                subtick,
                                         char                   verify,
                                         double                 a4,
                                         int                    a5,
                                         int64_t                player_pawn) {
    float saved[3]{};
    bool  modified = false;

    // WriteSubtickFromEntry зовётся на КАЖДУЮ сабтик-запись, то есть несколько
    // раз за тик. Полный перебор 64 игроков на каждый вызов — это заметный
    // расход на главном потоке, из-за которого плавает тайминг прыжка. Поэтому
    // при отпущенной кнопке ищем цель только для индикатора, 10 раз в секунду.
    const bool held = AimKeyHeld();
    static ULONGLONG lastIdleScan = 0;
    bool             doSearch     = held;
    if (!held) {
        const ULONGLONG now = GetTickCount64();
        if (now - lastIdleScan >= 100) {
            lastIdleScan = now;
            doSearch     = true;
        }
    }

    if (!doSearch)
        return oWriteSubtick(entry, subtick, verify, a4, a5, player_pawn);

    g_hasTarget = false;

    if (cfg::g.silent && entry && player_pawn) {
        void* localPawn = reinterpret_cast<void*>(static_cast<uintptr_t>(player_pawn));

        Vec3 eye{};
        if (EyePos(localPawn, eye)) {
            const QAng view{entry->view_angles[0], entry->view_angles[1], entry->view_angles[2]};

            target_t target{};
            if (FindTarget(localPawn, view, target)) {
                g_hasTarget = true;
            }

            if (target.valid && held) {
                saved[0] = entry->view_angles[0];
                saved[1] = entry->view_angles[1];
                saved[2] = entry->view_angles[2];

                QAng aim = AngleTo(eye, target.head);

                if (cfg::g.aimPunch && OFF_AIM_PUNCH) {
                    Vec3 punch{};
                    if (SafeRead(reinterpret_cast<uintptr_t>(localPawn) + OFF_AIM_PUNCH, punch)) {
                        aim.pitch -= punch.x * PUNCH_SCALE;
                        aim.yaw -= punch.y * PUNCH_SCALE;
                        if (aim.pitch > 89.0f)
                            aim.pitch = 89.0f;
                        else if (aim.pitch < -89.0f)
                            aim.pitch = -89.0f;
                        aim.yaw = Norm180(aim.yaw);
                    }
                }

                entry->view_angles[0] = aim.pitch;
                entry->view_angles[1] = aim.yaw;
                entry->view_angles[2] = 0.0f;

                // hit-reg поля: сервер сверяет их с тем, что реально прилетело.
                entry->shoot_position[0] = eye.x;
                entry->shoot_position[1] = eye.y;
                entry->shoot_position[2] = eye.z;

                entry->target_ent_index = target.index;

                entry->target_head_position[0] = target.head.x;
                entry->target_head_position[1] = target.head.y;
                entry->target_head_position[2] = target.head.z;

                entry->target_abs_origin[0] = target.origin.x;
                entry->target_abs_origin[1] = target.origin.y;
                entry->target_abs_origin[2] = target.origin.z;

                entry->target_angle[0] = target.eyeAngles[0];
                entry->target_angle[1] = target.eyeAngles[1];
                entry->target_angle[2] = target.eyeAngles[2];

                modified = true;
            }
        }
    }

    const int64_t result = oWriteSubtick(entry, subtick, verify, a4, a5, player_pawn);

    // вернуть настоящий угол: иначе подмена утечёт дальше по кадру.
    if (modified) {
        entry->view_angles[0] = saved[0];
        entry->view_angles[1] = saved[1];
        entry->view_angles[2] = saved[2];
    }

    return result;
}

inline bool Install() {
    if (!sigs::writeSubtickFromEntry) {
        Log("[silent] WriteSubtickFromEntry not resolved, falling back to dwViewAngles swap\n");
        return false;
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(sigs::writeSubtickFromEntry),
                      reinterpret_cast<LPVOID>(&hkWriteSubtick),
                      reinterpret_cast<LPVOID*>(&oWriteSubtick)) != MH_OK) {
        Log("[silent] MH_CreateHook(WriteSubtickFromEntry) failed\n");
        return false;
    }
    if (MH_EnableHook(reinterpret_cast<LPVOID>(sigs::writeSubtickFromEntry)) != MH_OK) {
        Log("[silent] MH_EnableHook(WriteSubtickFromEntry) failed\n");
        return false;
    }

    g_hooked = true;
    Log("[silent] input-history hook ON @ client+0x%llX\n",
        static_cast<unsigned long long>(sigs::writeSubtickFromEntry - g_clientBase));
    return true;
}

inline void Remove() {
    if (!g_hooked)
        return;
    MH_DisableHook(reinterpret_cast<LPVOID>(sigs::writeSubtickFromEntry));
    g_hooked = false;
}

// ------------------------- FALLBACK: dwViewAngles --------------------------

// вызывать ПЕРЕД оригинальным CreateMove. работает только когда основной хук
// не встал. вернёт true, если угол подменён — тогда нужен Restore().
inline bool Apply(void* localPawn) {
    g_applied = false;
    if (g_hooked || !cfg::g.silent || !localPawn)
        return false;
    if (!AimKeyHeld())
        return false;

    float* va = ViewAngles();
    QAng   view{};
    if (!SafeRead(reinterpret_cast<uintptr_t>(va), view))
        return false;

    target_t target{};
    if (!FindTarget(localPawn, view, target))
        return false;

    g_backup  = view;
    va[0]     = target.aim.pitch;
    va[1]     = target.aim.yaw;
    g_applied = true;
    return true;
}

// вызывать СРАЗУ после оригинального CreateMove, иначе подменённый угол утечёт
// в рендер и камеру развернёт на цель (это уже не silent, а обычный aimbot).
inline void Restore() {
    if (!g_applied)
        return;
    float* va = ViewAngles();
    va[0]     = g_backup.pitch;
    va[1]     = g_backup.yaw;
    va[2]     = g_backup.roll;
    g_applied = false;
}

} // namespace silent
