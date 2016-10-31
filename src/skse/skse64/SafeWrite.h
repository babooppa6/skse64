#pragma once

void SafeWrite8(uintptr_t addr, UInt32 data);
void SafeWrite16(uintptr_t addr, UInt32 data);
void SafeWrite32(uintptr_t addr, UInt32 data);
void SafeWrite64(uintptr_t addr, UInt64 data);
void SafeWriteBuf(uintptr_t addr, void * data, UInt32 len);

// 5 bytes
void WriteRelJump(UInt32 jumpSrc, UInt32 jumpTgt);
void WriteRelCall(UInt32 jumpSrc, UInt32 jumpTgt);

// 6 bytes
void WriteRelJnz(UInt32 jumpSrc, UInt32 jumpTgt);
void WriteRelJle(UInt32 jumpSrc, UInt32 jumpTgt);
