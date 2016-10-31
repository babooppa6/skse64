#pragma once

void SafeWrite8(uintptr_t addr, UInt32 data);
void SafeWrite16(uintptr_t addr, UInt32 data);
void SafeWrite32(uintptr_t addr, UInt32 data);
void SafeWrite64(uintptr_t addr, UInt64 data);
void SafeWriteBuf(uintptr_t addr, void * data, UInt32 len);

// 5 bytes
void WriteRelJump(uintptr_t jumpSrc, uintptr_t jumpTgt);
void WriteRelCall(uintptr_t jumpSrc, uintptr_t jumpTgt);

// 6 bytes
void WriteRelJnz(uintptr_t jumpSrc, uintptr_t jumpTgt);
void WriteRelJle(uintptr_t jumpSrc, uintptr_t jumpTgt);
