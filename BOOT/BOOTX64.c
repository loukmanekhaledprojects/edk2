// بسم الله الرحمان الرحيم


#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <user.h>
#include <kernel.h>
#include <Library/debuglib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
OSKERNELDATA* OsKernelData;
EFI_STATUS Status;

#define EASSERT(__f, errmsg) if(EFI_ERROR(Status = __f)) {Print(L"Cannot continue the boot process.\n%s STATUS : %lx\n", errmsg, Status); gBS->Exit(gImageHandle, 1, 0, NULL);}

void EFIAPI LocateBootFb() {
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsProtocol;
	// Check for G.O.P Support
	Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&GraphicsProtocol);
	if(EFI_ERROR(Status)) {
		Print(L"Graphics Output Protocol is not supported.\n");
		gBS->Exit(gImageHandle, EFI_UNSUPPORTED, 0, NULL);
	}
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* ModeInfo;
	UINTN szInfo;
	Status = GraphicsProtocol->QueryMode(GraphicsProtocol, GraphicsProtocol->Mode == NULL ? 0 : GraphicsProtocol->Mode->Mode, &szInfo, &ModeInfo);
	if(Status == EFI_NOT_STARTED) {
		if(EFI_ERROR(GraphicsProtocol->SetMode(GraphicsProtocol, 0)) || !GraphicsProtocol->Mode || !GraphicsProtocol->Mode->FrameBufferBase) {
			Print(L"Failed to set video mode.\n");
			gBS->Exit(gImageHandle, EFI_UNSUPPORTED, 0, NULL);
		}
	}

	OsKernelData->BootFb.BaseAddress = (void*)GraphicsProtocol->Mode->FrameBufferBase;
	OsKernelData->BootFb.Size = GraphicsProtocol->Mode->FrameBufferSize;
	OsKernelData->BootFb.hRes = ModeInfo->HorizontalResolution;
	OsKernelData->BootFb.vRes = ModeInfo->VerticalResolution;
	OsKernelData->BootFb.Pitch = ModeInfo->PixelsPerScanLine * 4;
}

void* EFIAPI OsAllocatePages(UINTN NumPages) {
    EFI_PHYSICAL_ADDRESS Addr;
    EFI_PHYSICAL_ADDRESS* Pt = &Addr;
    EASSERT(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, NumPages, Pt), L"Failed to allocate pages");
    return (void*)((UINTN)Addr);
}
void* EFIAPI OsAllocatePool(UINTN Size) {
    void* Addr;
    void** Pt = &Addr;
    EASSERT(gBS->AllocatePool(EfiLoaderData, Size, Pt), L"Failed to allocate pool");
    return Addr;
}
void* EFIAPI ReadFile(IN CHAR16* Path, OUT UINT64* FileSize) {
    EFI_HANDLE *handles;
UINTN count;
EASSERT(gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &count, &handles), L"Failed to read file (LocateHandleBuffer)");
ASSERT(count > 0);

EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs;
EASSERT(gBS->HandleProtocol(handles[0], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Fs),  L"Failed to read file (HandleProtocol)");

EFI_FILE_PROTOCOL* Root;
EASSERT(Fs->OpenVolume(Fs, &Root), L"Failed to read file (OpenVolume)");

EFI_FILE_PROTOCOL* File;
EASSERT(Root->Open(Root, &File, Path, EFI_FILE_MODE_READ, 0), L"Failed to read file (Open File)");

// Get file size
UINTN infoSize = sizeof(EFI_FILE_INFO) + 200;
EFI_FILE_INFO* fi = OsAllocatePool(infoSize);
EASSERT(File->GetInfo(File, &gEfiFileInfoGuid, &infoSize, fi), L"Failed to read file (Get File Info)");

UINT64 size = fi->FileSize;
UINTN pages = (UINTN)((size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
void* buf = OsAllocatePages(pages);

Print(L"Reading %llu bytes into buffer %p\n", size, buf);
EASSERT(File->Read(File, &size, buf), L"Failed to read file (File->Read)");

File->Close(File);
gBS->FreePool(fi);

*FileSize = size;
return buf;
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {

    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gImageHandle = ImageHandle;
    Print(L"UEFI Bootloader Startup\n");
    OsKernelData = OsAllocatePages(PSZ_OSKERNELDATA);
    
    Print(L"Os Kernel Data  :0x%lx \n", OsKernelData);
    LocateBootFb();
    for(int i =0;i<0x10000;i++) {
        OsKernelData->BootFb.BaseAddress[i] = 0xFFFFFF;
    }

    
    UINT64 FileSize;
    UINT32* FontFile = ReadFile(L"\\Fonts\\Inter-VariableFont_opsz,wght.ttf", &FileSize);
    Print(L"FileSize : %llu , First bytes : %x %x %x %x\n", FileSize, FontFile[0], FontFile[1], FontFile[2], FontFile[3]);
    LoadFont(FontFile, FileSize);
    return EFI_SUCCESS;
}