#pragma once
enum MH_STATUS { MH_OK = 0, MH_ERROR = 1 };
#define MH_ALL_HOOKS nullptr
MH_STATUS MH_Initialize();
MH_STATUS MH_Uninitialize();
MH_STATUS MH_CreateHook(LPVOID, LPVOID, LPVOID*);
MH_STATUS MH_EnableHook(LPVOID);
MH_STATUS MH_DisableHook(LPVOID);
