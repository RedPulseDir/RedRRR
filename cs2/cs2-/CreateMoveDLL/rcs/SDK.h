#pragma once
#include <cstdint>
#include <Windows.h>

// CS2 offsets.
// Источник: a2x/cs2-dumper, дамп от 2026-08-20 07:13:18 UTC (game build актуальный на 2026-08-21)
// https://github.com/a2x/cs2-dumper/blob/main/output/client_dll.hpp
// https://github.com/a2x/cs2-dumper/blob/main/output/offsets.hpp
// https://github.com/a2x/cs2-dumper/blob/main/output/buttons.hpp
namespace offsets {
    // client.dll  (offsets.hpp -> client_dll)
    constexpr uintptr_t dwEntityList            = 0x2555050; // CGameEntitySystem*   (без изменений)
    constexpr uintptr_t dwLocalPlayerController = 0x2384DB0; // CBasePlayerController* (без изменений)
    constexpr uintptr_t dwLocalPlayerPawn       = 0x23AA118; // C_CSPlayerPawn* (опционально, из дампа)

    // entity page-table base.
    // ВНИМАНИЕ: отдельного dwEntityPageTable в дампе a2x нет — таблица страниц лежит
    // внутри CGameEntitySystem: pageTable = *(client.dll + dwEntityList) + ENT_PAGE_TABLE_OFF
    //   listEntry = *(pageTable + ENT_PAGE_STRIDE * ((idx & 0x7FFF) >> 9))
    //   entity    = *(listEntry + ENT_SLOT_STRIDE * (idx & 0x1FF))
    constexpr uintptr_t dwEntityPageTable  = dwEntityList; // было 0x21E3AB0 (устарело/не существует в дампе)
    constexpr size_t    ENT_PAGE_TABLE_OFF = 0x10;
    constexpr size_t    ENT_PAGE_STRIDE    = 8;
    constexpr size_t    ENT_SLOT_STRIDE    = 112;  // 0x70. подтверждено автодетектом в игре:
                                                   // 120 не давал пешку, 112 даёт
    constexpr size_t    ENT_SLOT_HANDLE    = 16;   // CEntityIdentity::m_EHandle = 0x10

    // CBasePlayerController netvar - pawn handle
    constexpr uintptr_t m_hPawn = 0x6BC; // было 0x6D4

    // C_BaseEntity netvars
    constexpr uintptr_t m_fFlags           = 0x3F4; // было 0x3F8
    constexpr uintptr_t m_vecVelocity      = 0x430;
    constexpr uintptr_t m_hGroundEntity    = 0x530;
    constexpr uint32_t  FL_ONGROUND        = 1 << 0;

    // C_CSPlayerPawn
    constexpr uintptr_t m_angEyeAngles         = 0x3350; // было 0x3360
    constexpr uintptr_t m_angEyeAnglesVelocity = 0x3420; // было 0x3430

    // C_BasePlayerPawn -> CPlayer_MovementServices
    constexpr uintptr_t m_pMovementServices = 0x1248; // было 0x1220

    // CPlayer_MovementServices
    constexpr uintptr_t m_arrForceSubtickMoveWhen = 0x1B0;
    constexpr uintptr_t m_flMaxspeed              = 0x1AC;
    constexpr uintptr_t m_flCmdForwardMove        = 0x1A0;
    constexpr uintptr_t m_flCmdLeftMove           = 0x1A4;
    constexpr uintptr_t m_flForwardMove           = 0x1C0;
    constexpr uintptr_t m_flLeftMove              = 0x1C4;

    // CCSPlayer_MovementServices (subclass)
    constexpr uintptr_t m_flStamina            = 0x694; // было 0x6A4
    constexpr uintptr_t m_nLastJumpTick        = 0x700; // было 0x710
    constexpr uintptr_t m_flVelMulAtJumpStart  = 0x6A8; // было 0x6B8
    constexpr uintptr_t m_flStaminaAtJumpStart = 0x6A4; // бонус из дампа

    // force jump (buttons.hpp -> jump, client.dll)
    constexpr uintptr_t dwForceJump        = 0x209A510; // было 0x2058BE0
    // buttons.hpp -> attack / attack2. те же kbutton_t, что и jump.
    constexpr uintptr_t dwForceAttack      = 0x209A000;
    constexpr uintptr_t dwForceAttack2     = 0x209A090;

    // engine2.dll
    constexpr uintptr_t dwNetworkGameClient            = 0x90D4B0; // было 0x90B390
    constexpr uintptr_t dwNetworkGameClient_clientTick = 0x378;    // dwNetworkGameClient_clientTickCount

    // ---------------------------------------------------------------------
    // silent aim
    // ---------------------------------------------------------------------
    // client.dll
    // dwViewAngles — тот самый QAngle-источник, который CreateMove копирует в
    // исходящий usercmd (в разборе TKazer это r14: r14+0x10 pitch, +0x14 yaw,
    // +0x18 roll -> [rcx+0x18], [rcx+0x1C], [rcx+0x20]).
    constexpr uintptr_t dwViewAngles = 0x23C01A8; // QAngle {pitch, yaw, roll}
    constexpr uintptr_t dwCSGOInput  = 0x23BFB20; // CCSGOInput* (dwViewAngles лежит внутри, +0x688)
    constexpr uintptr_t dwGameEntitySystem_highestEntityIndex = 0x2090;

    // C_BaseEntity
    constexpr uintptr_t m_pGameSceneNode = 0x330; // CGameSceneNode*
    constexpr uintptr_t m_iHealth        = 0x34C; // int32
    constexpr uintptr_t m_lifeState      = 0x354; // uint8 (0 = ALIVE)
    constexpr uintptr_t m_iTeamNum       = 0x3E7; // uint8 (2 = T, 3 = CT)

    // C_BaseModelEntity
    constexpr uintptr_t m_vecViewOffset = 0xE78; // Vector, глаза = origin + viewOffset

    // CCSPlayerController
    constexpr uintptr_t m_hPlayerPawn  = 0x914; // CHandle<C_CSPlayerPawn>
    constexpr uintptr_t m_bPawnIsAlive = 0x91C; // bool
    constexpr uintptr_t m_iPawnHealth  = 0x920; // uint32

    // C_CSPlayerPawn::m_entitySpottedState -> EntitySpottedState_t
    constexpr uintptr_t m_entitySpottedState = 0x1C60;
    constexpr uintptr_t m_bSpotted           = 0x8;  // bool
    constexpr uintptr_t m_bSpottedByMask     = 0xC;  // uint32[2]

    // CGameSceneNode / CSkeletonInstance
    constexpr uintptr_t m_vecAbsOrigin = 0xC8;  // Vector
    constexpr uintptr_t m_bDormant     = 0x103; // bool
    constexpr uintptr_t m_modelState   = 0x140; // CSkeletonInstance::m_modelState
    constexpr uintptr_t BONE_ARRAY_OFF = m_modelState + 0x80; // = 0x1C0, CModelState::m_pBoneArray
    constexpr size_t    BONE_STRIDE    = 32;    // CTransform: Vector pos + pad + Quaternion
    constexpr int       BONE_HEAD      = 6;     // head_0 у playermodel'ей CS2
}
