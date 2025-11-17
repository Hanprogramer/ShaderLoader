#pragma once
#include <Windows.h>
#include <amethyst/runtime/mod/Mod.hpp>
#include <amethyst/runtime/ModContext.hpp>
#include <amethyst/runtime/events/GameEvents.hpp> // OnStartJoinGameEvent
#include "ShaderManager.hpp"

#define ModFunction extern "C" __declspec(dllexport)

ShaderManager* shaderManager = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return TRUE;
}

void OnStartJoinGame(OnStartJoinGameEvent& event);