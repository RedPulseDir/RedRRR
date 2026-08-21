#pragma once
#include <Windows.h>
#include <cstdint>

// ---------------------------------------------------------------------------
// Рантайм-конфиг. Раньше всё было constexpr, и чтобы что-то выключить, надо
// было пересобирать DLL. Теперь это обычные переменные, которые правит меню.
//
// Значения по умолчанию подобраны так, чтобы сборка стартовала в заведомо
// рабочем состоянии: бхоп включён, всё экспериментальное выключено.
// ---------------------------------------------------------------------------

namespace cfg {

struct Config {
    // --- bhop ---
    bool  bhop        = true;  // сам автопрыжок
    bool  subtick     = true;  // сброс m_arrForceSubtickMoveWhen на 0.0
    float staminaCap  = 200.0f;

    // --- strafe ---
    // move-версия пишет в m_flCmdLeftMove/m_flLeftMove (реальный ввод),
    // view-версия крутит m_angEyeAngles. Обе по умолчанию выключены: они
    // дерутся с собственным управлением игрока и сильно меняют ощущение.
    bool  moveStrafe  = false;
    bool  viewStrafe  = false;
    float strafeMove  = 450.0f; // сколько писать в leftmove
    float yawDeadzone = 0.25f;

    // --- silent aim ---
    bool  silent      = true;
    float fov         = 30.0f; // градусы, он же радиус круга на экране
    bool  visibleOnly = false; // фильтр по m_bSpottedByMask (ненадёжный, см. README)
    bool  triggerbot  = false; // авто-огонь при захвате цели
    int   triggerMs   = 60;    // пауза между выстрелами, мс
    bool  aimPunch    = false; // компенсация отдачи (нужен оффсет, см. SilentAim.h)
    float maxDist     = 8192.0f;
    int   aimBone     = 6;     // head_0

    // --- визуал ---
    bool  showFovCircle = true;
    bool  showWatermark = true;
    bool  showTargetRing = true;
    float circleThick   = 1.5f;

    // FOV игры из настроек CS2 (fov_cs_debug / ползунок). Это горизонтальный
    // угол при 4:3; на широких мониторах движок работает по Hor+, поэтому
    // реальный угол считается из вертикального — так круг совпадает с
    // прицелом на любом соотношении сторон.
    float gameFov = 90.0f;

    // --- масштаб интерфейса ---
    // 0 = авто по высоте экрана (1080p принят за единицу). Иначе множитель.
    bool  autoScale = true;
    float uiScale   = 1.0f;

    // --- меню ---
    bool menuOpen = true;
};

inline Config g{};

} // namespace cfg
