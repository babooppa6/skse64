#include "Inject.h"
#include <Winternl.h>

#pragma pack (push, 1)

struct HookLayout
{
	enum
	{
		kNumLibs = 16,
		kMaxLibNameLen = MAX_PATH
	};

	struct DoLoadLibrary
	{
		UInt8		pushRbx;	// 53
		UInt8		movRbxRsp1; // 48
		UInt8		movRbxRsp2; // 89
		UInt8		movRbxRsp3; // E3
		UInt8		subRsp1;	// 48
		UInt8		subRsp2;	// 83
		UInt8		subRsp3;	// EC
		UInt8		subRsp4;	// 20
		UInt8		movRcx1;	// 48
		UInt8		movRcx2;	// B9
		uintptr_t	strAddr;	// address
		UInt8		movRdx1;	// 48
		UInt8		movRdx2;	// BA
		uintptr_t	callAddr;	// address
		UInt8		callRdx1;	// FF
		UInt8		callRdx2;	// 12
		UInt8		movRsp1;    // 48
		UInt8		movRsp2;    // 89
		UInt8		movRsp3;    // DC
		UInt8		popRbx;		// 5B

		void	Clear(void)
		{
			// nops
			std::memset(this, 0x90, sizeof(DoLoadLibrary));
		}

		void	Setup(uintptr_t _strAddr, uintptr_t _callAddr)
		{
			pushRbx = 0x53;

			movRbxRsp1 = 0x48;
			movRbxRsp2 = 0x89;
			movRbxRsp3 = 0xE3;

			subRsp1 = 0x48;
			subRsp2 = 0x83;
			subRsp3 = 0xEC;
			subRsp4 = 0x20;

			movRcx1 = 0x48;
			movRcx2 = 0xB9;
			strAddr = _strAddr;
			movRdx1 = 0x48;
			movRdx2 = 0xBA;
			callAddr = _callAddr;
			callRdx1 = 0xFF;
			callRdx2 = 0x12;

			movRsp1 = 0x48;
			movRsp2 = 0x89;
			movRsp3 = 0xDC;

			popRbx = 0x5B;
		}
	};

	// code (entry point)
	UInt8			infLoop1;		// EB
	UInt8			infLoop2;		// FF
	DoLoadLibrary	loadLib[kNumLibs];
	UInt8			movMainRax1;	// 48
	UInt8			movMainRax2;	// B8
	uintptr_t		callMainAddr;	// address
	UInt8			jmpRax1;		// FF
	UInt8			jmpRax2;		// E0

	// data
	char			libNames[kMaxLibNameLen * kNumLibs];
	uintptr_t		mainAddr;

	void	Init(ProcHookInfo * hookInfo)
	{
#if 0
		infLoop1 = 0xEB;
		infLoop2 = 0xFE;
#else
		infLoop1 = 0x90;
		infLoop2 = 0x90;
#endif

		for(UInt32 i = 0; i < kNumLibs; i++)
			loadLib[i].Clear();

		movMainRax1 = 0x48;
		movMainRax2 = 0xB8;
		callMainAddr = 0;
		jmpRax1 = 0xFF;
		jmpRax2 = 0xE0;

		memset(libNames, 0, sizeof(libNames));

		mainAddr = 0;
	}
};

#pragma pack (pop, 1)

struct PEBPartial
{
	UCHAR InheritedAddressSpace;
	UCHAR ReadImageFileExecOptions;
	UCHAR BeingDebugged;
	UCHAR BitField;
	ULONG ImageUsesLargePages : 1;
	ULONG IsProtectedProcess : 1;
	ULONG IsLegacyProcess : 1;
	ULONG IsImageDynamicallyRelocated : 1;
	ULONG SpareBits : 4;
	PVOID Mutant;
	PVOID ImageBaseAddress;
	PPEB_LDR_DATA Ldr;
};

struct HookSetup
{
	HookLayout	m_data;

	HANDLE	m_proc;
	uintptr_t	m_base;
	uintptr_t	m_loadLib;

	UInt32	m_libIdx;
	UInt32	m_strOffset;

	bool	m_isInit;

	HookSetup()
	{
		m_proc = NULL;
		m_base = 0;
		m_loadLib = 0;

		m_libIdx = 0;
		m_strOffset = 0;

		m_isInit = false;
	}

	bool	Init(PROCESS_INFORMATION * info, ProcHookInfo * hookInfo)
	{
		bool	result = false;

		if(m_isInit) return true;

		m_loadLib =				hookInfo->loadLibAddr;
		uintptr_t hookBaseAddr =	hookInfo->hookCallAddr;

		m_data.Init(hookInfo);

		m_proc = OpenProcess(
			PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, info->dwProcessId);
		if(m_proc)
		{
			m_base = (uintptr_t)VirtualAllocEx(m_proc, NULL, sizeof(m_data), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if(m_base)
			{
				
				size_t	bytesTransferred = 0;

				_MESSAGE("remote memory = %08X", m_base);

				PROCESS_BASIC_INFORMATION pbi;
				UInt32 ret = 0;
				if (NT_SUCCESS(NtQueryInformationProcess(m_proc, ProcessBasicInformation, &pbi, sizeof(pbi), &ret)))
				{
					size_t length = 0;
					PEBPartial peb;
					if (ReadProcessMemory(m_proc, pbi.PebBaseAddress, &peb, sizeof(peb), &length))
					{
						hookBaseAddr += (uintptr_t)peb.ImageBaseAddress;
						m_loadLib += (uintptr_t)peb.ImageBaseAddress;

						int32_t	hookBaseCallAddrLo;
						// update the call
						if (ReadProcessMemory(m_proc, (void *)(hookBaseAddr + 1), &hookBaseCallAddrLo, sizeof(hookBaseCallAddrLo), &bytesTransferred) &&
							(bytesTransferred == sizeof(hookBaseCallAddrLo)))
						{
							// adjust for relcall
							intptr_t hookBaseCallAddr = 5 + hookBaseAddr + hookBaseCallAddrLo;

							_MESSAGE("old winmain = %08X", hookBaseCallAddr);

							m_data.mainAddr = hookBaseCallAddr;
							m_data.callMainAddr = hookBaseCallAddr;

							// Look for a code cave - adding 0x30 for now
							uintptr_t caveAddress = hookBaseAddr + 0x30;

							BYTE cave[12] = { 0 };
							if (ReadProcessMemory(m_proc, (void*)caveAddress, cave, sizeof(cave), &bytesTransferred))
							{
								for (auto i = 0; i < 12; ++i)
								{
									if (cave[i] != 0xCC)
									{
										_ERROR("Codecave is overwriting code!");
										return false;
									}
								}

								// mov rax, val
								cave[0] = 0x48;
								cave[1] = 0xB8;
								*(uintptr_t*)&cave[2] = m_base;
								// jmp rax
								cave[10] = 0xFF;
								cave[11] = 0xE0;

								if (WriteProcessMemory(m_proc, (void *)(caveAddress), &cave, sizeof(cave), &bytesTransferred) &&
									(bytesTransferred == sizeof(cave)))
								{

									UInt32	newHookDst = 0x30 - 5;
									if (WriteProcessMemory(m_proc, (void *)(hookBaseAddr + 1), &newHookDst, sizeof(newHookDst), &bytesTransferred) &&
										(bytesTransferred == sizeof(newHookDst)))
									{
										m_isInit = true;
										result = true;
									}
									else
									{
										_ERROR("couldn't write memory (update winmain)");
									}
								}
								else
								{
									_ERROR("couldn't write memory (code cave)");
								}
							}
							else
							{
								_ERROR("couldn't read memory (code cave)");
							}
						}
						else
						{
							_ERROR("couldn't read memory (original winmain)");
						}
					}
					else
					{
						_ERROR("couldn't read PEB");
					}
				}
				else
				{
					_ERROR("couldn't read PIB");
				}
			}
			else
			{
				_ERROR("couldn't allocate memory in remote process");
			}
		}
		else
		{
			_ERROR("couldn't open process");
		}

		return result;
	}

	bool	AddLoadLibrary(const char * dllPath)
	{
		bool	result = false;

		if(m_libIdx < HookLayout::kNumLibs)
		{
			HookLayout::DoLoadLibrary	* lib = &m_data.loadLib[m_libIdx];
			char						* strDst = &m_data.libNames[m_strOffset];
			m_libIdx++;

#pragma warning (push)
#pragma warning (disable : 4996)
			strcpy(strDst, dllPath);
#pragma warning (pop)

			m_strOffset += strlen(dllPath) + 1;

			lib->Setup(
				GetRemoteOffset(strDst),
				m_loadLib);

			if(UpdateRemoteProc())
			{
				result = true;
			}
		}

		return result;
	}

	uintptr_t	GetRemoteOffset(void * data)
	{
		return m_base + ((uintptr_t)data) - ((uintptr_t)&m_data);
	}

	bool	UpdateRemoteProc(void)
	{
		size_t	bytesTransferred;
		return	WriteProcessMemory(m_proc, (void *)m_base, &m_data, sizeof(m_data), &bytesTransferred) && (bytesTransferred == sizeof(m_data));
	}
};

HookSetup	g_hookData;

bool DoInjectDLL(PROCESS_INFORMATION * info, const char * dllPath, ProcHookInfo * hookInfo)
{
	bool	result = false;

	if(g_hookData.Init(info, hookInfo))
	{
		if(g_hookData.AddLoadLibrary(dllPath))
		{
			result = true;
		}
		else
		{
			_ERROR("couldn't add library to list");
		}
	}
	else
	{
		_ERROR("couldn't init hook");
	}

	return result;
}
