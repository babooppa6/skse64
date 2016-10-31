#include "Hooks_Gameplay.h"
#include "SafeWrite.h"
#include "Utilities.h"
#include "PapyrusEvents.h"
#include "GameReferences.h"
#include "skse_version.h"

static UInt32 g_forceContainerCategorization = 0;
static const UInt32 kHook_ContainerMode_Base = 0x0084B4C0;
static const UInt32 kHook_ContainerMode_Categories = kHook_ContainerMode_Base + 0x4E;
static const UInt32 kHook_ContainerMode_NoCategories = kHook_ContainerMode_Base + 0x63;
static UInt32 ** g_containerMode = (UInt32 **)0x01B3E6FC;

TESObjectREFR*	g_curCrosshairRef = NULL;

/*static void __declspec(naked) Hook_ContainerMode(void)
{
	__asm
	{
		// check override
		cmp		g_forceContainerCategorization, 0
		jnz		useCategories

		// original code (modified because msvc doesn't like immediates)
		// setting g_containerMode to 3 all the time causes other problems, don't bother

		push	eax
		mov		eax, [g_containerMode]
		mov		eax, [eax]
		cmp		eax, 3
		pop		eax

		jz		useCategories

		jmp		[kHook_ContainerMode_NoCategories]

useCategories:
		jmp		[kHook_ContainerMode_Categories]
	}
}*/

void Hooks_Gameplay_EnableForceContainerCategorization(bool enable)
{
	g_forceContainerCategorization = enable ? 1 : 0;
}

UInt32 g_invalidateKeywordCache = 0;
static UInt32 kHook_BGSKeyword_Base = 0x0054DDE0;
static UInt32 kHook_BGSKeyword_Create_Return = kHook_BGSKeyword_Base + 5;

/*static void __declspec(naked) Hook_BGSKeyword_Create(void)
{
	__asm
	{
		mov		g_invalidateKeywordCache, 1

		// original code
		push	esi
		push	0
		push	0

		jmp		[kHook_BGSKeyword_Create_Return]
	}
}*/


static char		kHook_ShowVersion_FormatString[] =
	"%s.%d (SKSE " __PREPRO_TOKEN_STR__(SKSE_VERSION_INTEGER) "."
	__PREPRO_TOKEN_STR__(SKSE_VERSION_INTEGER_MINOR) "."
	__PREPRO_TOKEN_STR__(SKSE_VERSION_INTEGER_BETA) " rel "
	__PREPRO_TOKEN_STR__(SKSE_VERSION_RELEASEIDX) ")";

static UInt8    kHook_ShowVersion_Stub[] = {
	0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov r8, kHook_ShowVersion_FormatString
	0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, kHook_ShowVersion_Return
	0xFF, 0xE0													// jmp rax
};

static const UInt32 kHook_Crosshair_LookupREFRByHandle_Base = 0x00739FD0;
static const UInt32 kHook_Crosshair_LookupREFRByHandle_Enter = kHook_Crosshair_LookupREFRByHandle_Base + 0x64;

/*bool __cdecl Hook_Crosshair_LookupREFRByHandle(UInt32 * refHandle, TESObjectREFR ** refrOut)
{
	bool result = LookupREFRByHandle(refHandle, refrOut);

	if (refrOut)
	{
		g_curCrosshairRef = *refrOut;

		SKSECrosshairRefEvent evn(*refrOut);
		g_crosshairRefEventDispatcher.SendEvent(&evn);
	}
	else
	{
		g_curCrosshairRef = NULL;
	}

	return result;
}*/

TESObjectREFR* Hooks_Gameplay_GetCrosshairRef()
{
	return g_curCrosshairRef;
}

static UInt8 s_disableMapMenuMouseWheel = 1;
static const UInt32 kHook_MapMenuMouseWheel_Base = 0x00899FD0;
static const UInt32 kHook_MapMenuMouseWheel_Enter = kHook_MapMenuMouseWheel_Base + 0x169;
static const UInt32 kHook_MapMenuMouseWheel_Return = kHook_MapMenuMouseWheel_Base + 0x16F;

/*static void __declspec(naked) Hook_MapMenuMouseWheel(void)
{
	__asm
	{
		mov		al, s_disableMapMenuMouseWheel

		// Original code
		pop     edi
		pop     esi
		pop     ebp
		pop     ebx

		jmp		[kHook_MapMenuMouseWheel_Return]
	}
}*/

void Hooks_Gameplay_EnableMapMenuMouseWheel(bool enable)
{
	s_disableMapMenuMouseWheel = enable ? 0 : 1;
}

void Hooks_Gameplay_Commit(void)
{
	// optionally force containers in to "npc" mode, showing categories
	/*WriteRelJump(kHook_ContainerMode_Base + 0x45, (UInt32)Hook_ContainerMode);

	// read config
	UInt32	enableContainerCategorization = 0;
	if(GetConfigOption_UInt32("Interface", "EnableContainerCategorization", &enableContainerCategorization))
	{
		if(enableContainerCategorization)
		{
			_MESSAGE(".ini enabled container categorization");
			g_forceContainerCategorization = 1;
		}
	}
	*/

	static uintptr_t	kHook_ShowVersion_Base = 0x140B8DDF0 - 0x140000000 + (uintptr_t)GetModuleHandleA(NULL);
	static uintptr_t	kHook_ShowVersion_Enter = kHook_ShowVersion_Base + 0x78;
	static uintptr_t	kHook_ShowVersion_Return = kHook_ShowVersion_Base + 0x7F;
	static uintptr_t	kHook_ShowVersion_End = kHook_ShowVersion_Base + 0x143;
	// show SKSE version in menu
	// JMP to cave
	WriteRelJump(kHook_ShowVersion_Enter, kHook_ShowVersion_End);
	// JMP to stub
	SafeWrite8  (kHook_ShowVersion_End,      0x48);
	SafeWrite8  (kHook_ShowVersion_End + 1,  0xB8);
	SafeWrite64 (kHook_ShowVersion_End + 2,  (UInt64)&kHook_ShowVersion_Stub);
	SafeWrite8  (kHook_ShowVersion_End + 10, 0xFF);
	SafeWrite8  (kHook_ShowVersion_End + 11, 0xE0);

	*(uintptr_t*)&kHook_ShowVersion_Stub[2] = (uintptr_t)kHook_ShowVersion_FormatString;
	*(uintptr_t*)&kHook_ShowVersion_Stub[12] = kHook_ShowVersion_Return;

	DWORD lel;
	VirtualProtect(kHook_ShowVersion_Stub, sizeof(kHook_ShowVersion_Stub), PAGE_EXECUTE_READWRITE, &lel);

	/*
	// hook BGSKeyword ctor so we can rebuild the lookup table
	WriteRelJump(kHook_BGSKeyword_Base, (UInt32)Hook_BGSKeyword_Create);

	// hook crosshair ref update
	WriteRelCall(kHook_Crosshair_LookupREFRByHandle_Enter, (UInt32) &Hook_Crosshair_LookupREFRByHandle);

	// change return value of LocalMapMenu::InputHandler::HandleMouseMoveEvent
	// for zoomIn/out case so it passes on the event to flash
	WriteRelJump(kHook_MapMenuMouseWheel_Enter, (UInt32)Hook_MapMenuMouseWheel);*/
}
