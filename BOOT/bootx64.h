#pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Pci.h>
#include <Guid/EventGroup.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <typedef.h>
#include <user.h>
#include <Library/PrintLib.h>

void* USERAPI TempPAlloc(UINT64 l);
#define PageMapAlloc(__x) TempPAlloc(__x)
#include <kernel.h>
#include <user/mem.h>
#include <kernel/kv.h>

#include <Library/debuglib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Library/BaseMemoryLib.h>
OSKERNELDATA* OsKernelData;
#define EASSERT(__f, errmsg) {static EFI_STATUS __Status; if(EFI_ERROR(__Status = __f)) {Print(L"Cannot continue the boot process.\n%s STATUS : %lx\n", errmsg, __Status); while(1);}}
void TestUi();
void Svga3dSetup();
void SvgaInit();
void* EFIAPI OsAllocatePool(UINTN Size);
void __fastcall UlibFree(void* Address);
#define OsFreePool(__ptr) UlibFree(__ptr)
void NvGpuTest();
extern void KCALL SimpleConOut(const UINT16* Str, ...);
EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;
void EFIAPI LocateBootFb(void);
void LoadStartupDrivers(void);

BOOLEAN efiexited;
EFI_STATUS Status;
int LargePgMap;

void* EFIAPI ReadFile(IN CHAR16* Path, OUT UINT64* FileSize);
void* EFIAPI bAllocatePages(UINTN NumPages);
void* EFIAPI bAllocatePool(UINTN Size);
void* __fastcall UlibAlloc(UINT64 Size);
void* __fastcall UlibAllocPages(UINT64 NumPages);
void __fastcall UlibFree(void* Address) ;
void __fastcall UlibFreePages(void* Address, UINT64 NumPages);
void* USERAPI TempPAlloc(UINT64 l);
void* USERAPI TempAlloc(UINT64 l);
void USERAPI TempFree(void* a);
void USERAPI TempPFree(void* a, UINT64 c);