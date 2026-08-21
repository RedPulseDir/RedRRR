#pragma once

#include <cstddef>
#include <cstdint>

namespace netvars {

	namespace CBasePlayerController {
		constexpr std::ptrdiff_t m_hPawn = 0x6C4;
	}

	namespace CCSPlayerController {
		constexpr std::ptrdiff_t m_hPlayerPawn = 0x90C;
		constexpr std::ptrdiff_t m_sSanitizedPlayerName = 0x860;
		constexpr std::ptrdiff_t m_bIsLocalPlayerController = 0x788;
		constexpr std::ptrdiff_t m_bPawnIsAlive = 0x854;
		constexpr std::ptrdiff_t m_iPawnHealth = 0x858;
		constexpr std::ptrdiff_t m_iPawnArmor = 0x85C;
		constexpr std::ptrdiff_t m_bPawnHasDefuser = 0x860;
		constexpr std::ptrdiff_t m_bPawnHasHelmet = 0x861;
	}

	namespace C_BaseEntity {
		constexpr std::ptrdiff_t m_iHealth = 0x354;
		constexpr std::ptrdiff_t m_lifeState = 0x35C;
		constexpr std::ptrdiff_t m_pGameSceneNode = 0x338;
		constexpr std::ptrdiff_t m_iTeamNum = 0x3F3;
		constexpr std::ptrdiff_t m_fFlags = 0x400;
		constexpr std::ptrdiff_t m_MoveType = 0x52D;
		constexpr std::ptrdiff_t m_flSimulationTime = 0x3C0;
		constexpr std::ptrdiff_t m_flOldSimulationTime = 0x3C4;
	}

	namespace CGameSceneNode {
		constexpr std::ptrdiff_t m_vecAbsOrigin = 0xD0;
	}

	namespace C_BaseModelEntity {
		constexpr std::ptrdiff_t m_vecViewOffset = 0xD58;
		constexpr std::ptrdiff_t m_Glow = 0xCE0;
	}

	namespace CGlowProperty {
		constexpr std::ptrdiff_t m_fGlowColor = 0x8;
		constexpr std::ptrdiff_t m_iGlowType = 0x30;
		constexpr std::ptrdiff_t m_iGlowTeam = 0x34;
		constexpr std::ptrdiff_t m_nGlowRange = 0x38;
		constexpr std::ptrdiff_t m_nGlowRangeMin = 0x3C;
		constexpr std::ptrdiff_t m_glowColorOverride = 0x40;
		constexpr std::ptrdiff_t m_bFlashing = 0x44;
		constexpr std::ptrdiff_t m_flGlowTime = 0x48;
		constexpr std::ptrdiff_t m_flGlowStartTime = 0x4C;
		constexpr std::ptrdiff_t m_bEligibleForScreenHighlight = 0x50;
	}

	namespace EntitySpottedState_t {
		constexpr std::ptrdiff_t m_bSpotted = 0x8;
		constexpr std::ptrdiff_t m_bSpottedByMask = 0xC;
	}

	namespace C_BasePlayerPawn {
		constexpr std::ptrdiff_t m_vOldOrigin = 0x1588;
		constexpr std::ptrdiff_t m_pWeaponServices = 0x13D8;
		constexpr std::ptrdiff_t m_pObserverServices = 0x13F0;
		constexpr std::ptrdiff_t m_pCameraServices = 0x1410;
	}

	namespace CPlayer_ObserverServices {
		constexpr std::ptrdiff_t m_iObserverMode = 0x48;
		constexpr std::ptrdiff_t m_hObserverTarget = 0x4C;
	}

	namespace CPlayer_WeaponServices {
		constexpr std::ptrdiff_t m_hActiveWeapon = 0x60;
		constexpr std::ptrdiff_t m_hMyWeapons = 0x0;
	}

	namespace CCSPlayerBase_CameraServices {
		constexpr std::ptrdiff_t m_iFOV = 0x290;
		constexpr std::ptrdiff_t m_iFOVStart = 0x294;
		constexpr std::ptrdiff_t m_flFOVTime = 0x298;
		constexpr std::ptrdiff_t m_flFOVRate = 0x29C;
	}

	namespace C_CSPlayerPawnBase {
		constexpr std::ptrdiff_t m_iShotsFired = 0x270C;
		constexpr std::ptrdiff_t m_aimPunchAngle = 0x16CC;
		constexpr std::ptrdiff_t m_angEyeAngles = 0x3DD0;
		constexpr std::ptrdiff_t m_bIsScoped = 0x26F8;
		constexpr std::ptrdiff_t m_flFlashMaxAlpha = 0x15F4;
		constexpr std::ptrdiff_t m_flFlashDuration = 0x15F8;
		constexpr std::ptrdiff_t m_pClippingWeapon = 0x3DC0;
		constexpr std::ptrdiff_t m_entitySpottedState = 0x26E0;
		constexpr std::ptrdiff_t m_flVelocityModifier = 0x2714;
		constexpr std::ptrdiff_t m_ArmorValue = 0x272C;
	}

	namespace C_CSPlayerPawn {
		constexpr std::ptrdiff_t m_bLeftHanded = 0x2411;
		constexpr std::ptrdiff_t m_flViewmodelOffsetX = 0x2418;
		constexpr std::ptrdiff_t m_flViewmodelOffsetY = 0x241C;
		constexpr std::ptrdiff_t m_flViewmodelOffsetZ = 0x2420;
		constexpr std::ptrdiff_t m_flViewmodelFOV = 0x2424;
	}

	namespace C_BaseCSGrenade {
		constexpr std::ptrdiff_t m_bPinPulled = 0x1F43;
		constexpr std::ptrdiff_t m_bThrowAnimating = 0x1F45;
		constexpr std::ptrdiff_t m_flThrowStrength = 0x1F50;
	}

	namespace C_PlantedC4 {
		constexpr std::ptrdiff_t m_bBombTicking = 0x1170;
		constexpr std::ptrdiff_t m_nBombSite = 0x1174;
		constexpr std::ptrdiff_t m_flC4Blow = 0x11A0;
		constexpr std::ptrdiff_t m_bHasExploded = 0x11A5;
		constexpr std::ptrdiff_t m_flTimerLength = 0x11A8;
		constexpr std::ptrdiff_t m_bBeingDefused = 0x11AC;
		constexpr std::ptrdiff_t m_flDefuseLength = 0x11BC;
		constexpr std::ptrdiff_t m_flDefuseCountDown = 0x11C0;
		constexpr std::ptrdiff_t m_bBombDefused = 0x11C4;
	}

	namespace C_SmokeGrenadeProjectile {
		constexpr std::ptrdiff_t m_nSmokeEffectTickBegin = 0x1450;
		constexpr std::ptrdiff_t m_bDidSmokeEffect = 0x1454;
		constexpr std::ptrdiff_t m_bSmokeEffectSpawned = 0x1499;
	}

	namespace C_EconEntity {
		constexpr std::ptrdiff_t m_AttributeManager = 0x1378;
		constexpr std::ptrdiff_t m_OriginalOwnerXuidLow = 0x1848;
		constexpr std::ptrdiff_t m_OriginalOwnerXuidHigh = 0x184C;
		constexpr std::ptrdiff_t m_nFallbackPaintKit = 0x1850;
		constexpr std::ptrdiff_t m_nFallbackSeed = 0x1854;
		constexpr std::ptrdiff_t m_flFallbackWear = 0x1858;
		constexpr std::ptrdiff_t m_nFallbackStatTrak = 0x185C;
	}

	namespace C_EconItemView {
		constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
		constexpr std::ptrdiff_t m_iEntityQuality = 0x1BC;
		constexpr std::ptrdiff_t m_iEntityLevel = 0x1C0;
		constexpr std::ptrdiff_t m_iItemIDHigh = 0x1D0;
		constexpr std::ptrdiff_t m_iItemIDLow = 0x1D4;
		constexpr std::ptrdiff_t m_iAccountID = 0x1D8;
		constexpr std::ptrdiff_t m_bInitialized = 0x1E8;
	}

	namespace CAttributeManager {
		constexpr std::ptrdiff_t m_iReapplyProvisionParity = 0x20;
	}

	namespace CAttributeContainer {
		constexpr std::ptrdiff_t m_Item = 0x50;
	}

	namespace C_BasePlayerWeapon {
		constexpr std::ptrdiff_t m_nNextPrimaryAttackTick = 0x18C0;
		constexpr std::ptrdiff_t m_flNextPrimaryAttackTickRatio = 0x18C4;
		constexpr std::ptrdiff_t m_nNextSecondaryAttackTick = 0x18C8;
		constexpr std::ptrdiff_t m_iClip1 = 0x18D0;
		constexpr std::ptrdiff_t m_iClip2 = 0x18D4;
	}

}
