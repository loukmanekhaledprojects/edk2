#pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Pci.h>
#include <Guid/EventGroup.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <kernel.h>
#include <user/mem.h>
#include <user.h>

#include <Library/debuglib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Library/BaseMemoryLib.h>

OSKERNELDATA* OsKernelData;

void TestUi();
void Svga3dSetup();
void* EFIAPI OsAllocatePool(UINTN Size);
void __fastcall UlibFree(void* Address);
#define OsFreePool(__ptr) UlibFree(__ptr)

void NvGpuTest();
