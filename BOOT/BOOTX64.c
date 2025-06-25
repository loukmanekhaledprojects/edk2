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
    FONT StartupFont = {0};

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

void* __fastcall UlibAlloc(UINT64 Size) {return OsAllocatePool(Size);}
void __fastcall UlibFree(void* Address) {gBS->FreePool(Address);}
void* __fastcall UlibAllocPages(UINT64 NumPages) {return OsAllocatePages(NumPages);}
void __fastcall UlibFreePages(void* Address, UINT64 NumPages) {gBS->FreePages((EFI_PHYSICAL_ADDRESS)Address, NumPages);}

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

void BlitGrayBitmapToFramebuffer(
    OSKERNELDATA* osData,
    int dest_x,
    int dest_y,
    FONT_GLYPH_BITMAP* bitmap,
    int bitmap_left,
    int bitmap_top,
    UINT32 fg_color    // foreground color as 0x00RRGGBB
) {
    UINT32 width = osData->BootFb.hRes;
    UINT32 height = osData->BootFb.vRes;
    UINT32 pitch_pixels = osData->BootFb.Pitch / sizeof(UINT32);
    UINT32* framebuffer = osData->BootFb.BaseAddress;

    for (UINT32 row = 0; row < bitmap->rows; row++) {
        int y = dest_y + row - bitmap_top;
        if (y < 0 || y >= (int)height)
            continue;

        for (UINT32 col = 0; col < bitmap->width; col++) {
            int x = dest_x + col + bitmap_left;
            if (x < 0 || x >= (int)width)
                continue;

            UINT8 alpha = bitmap->buffer[row * bitmap->pitch + col];
            UINT8 inv_alpha = 255 - alpha;

            UINT32* pixel = &framebuffer[y * pitch_pixels + x];

            // Extract RGB components of fg_color and bg_color
            UINT8 fg_r = (fg_color >> 16) & 0xFF;
            UINT8 fg_g = (fg_color >> 8) & 0xFF;
            UINT8 fg_b = fg_color & 0xFF;


            // Existing framebuffer pixel RGB (assuming no alpha)
            UINT8 dst_r = (*pixel >> 16) & 0xFF;
            UINT8 dst_g = (*pixel >> 8) & 0xFF;
            UINT8 dst_b = *pixel & 0xFF;

            // Blend foreground glyph color with existing pixel based on alpha
            UINT8 out_r = (fg_r * alpha + dst_r * inv_alpha) / 255;
            UINT8 out_g = (fg_g * alpha + dst_g * inv_alpha) / 255;
            UINT8 out_b = (fg_b * alpha + dst_b * inv_alpha) / 255;

            *pixel = (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

void DrawText(UINT16* Text, UINT32 DestX, UINT32 DestY, UINT32 FontSize, UINT32 ForegroundColor) {
    UINTN Len = StrLen(Text);
    SetFontSize(&StartupFont, FontSize);
    signed long x = DestX;
    UINT16 PrevChar = 0;
    FONT_GLYPH_DATA GlyphData = {0};

    for (UINT32 i = 0; i < (UINT32)Len; i++) {
        if (!LoadGlyf(&StartupFont, &GlyphData, Text[i], PrevChar)) {
            Print(L"Failed to load glyph bitmap.\n");
            gBS->Exit(gImageHandle, EFI_LOAD_ERROR, 0, NULL);
        }

        // Add kerning between previous char and current char BEFORE drawing
        x += GlyphData.KerningX;

        // Draw glyph at current position x
        BlitGrayBitmapToFramebuffer(OsKernelData, x, DestY + GlyphData.Top, GlyphData.Bitmap, 0, 0, ForegroundColor);

        // Advance x by glyph's advance width for next glyph
        x += GlyphData.AdvanceX;

        PrevChar = Text[i];
    }
}


EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {

    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gImageHandle = ImageHandle;
    Print(L"UEFI Bootloader Startup\n");
    OsKernelData = OsAllocatePages(PSZ_OSKERNELDATA);
    
    Print(L"Os Kernel Data  :0x%lx \n", OsKernelData);
    LocateBootFb();
    
    
    UINT64 FileSize;
    UINT32* FontFile = ReadFile(L"\\Fonts\\Inter.ttf", &FileSize);
    Print(L"FileSize : %llu , First bytes : %x %x %x %x\n", FileSize, FontFile[0], FontFile[1], FontFile[2], FontFile[3]);
    USERMEMORYIF ULibIf = {
        UlibAlloc,
        UlibAllocPages,
        UlibFree,
        UlibFreePages
    };
    UlibSetMemoryInterface(&ULibIf);
    if(!LoadFont(&StartupFont, FontFile, FileSize)){
        
        Print(L"Failed to load startup font.\n");
        return EFI_LOAD_ERROR;
    }
    for(int i =0;i<OsKernelData->BootFb.Size / 4;i++) {
        OsKernelData->BootFb.BaseAddress[i] = 0;
    }
    DrawText(L"BISMILLAH", 120, 120, 99, 0xFFFFFF);

    DrawText(L"Operating System Booting...", 300, 400, 22, 0xFFFFFF);
    
    void* KernelFile = ReadFile(L"\\System\\OSKERNEL.EXE", &FileSize);
    OsKernelData->Image = LoadImage(KernelFile, FileSize, NULL);
    Print(L"KIMG %lx BASE %lx ENTRY_POINT %lx File %lx File Size %lx , First bytes %x %x %x %x\n", OsKernelData->Image, OsKernelData->Image->Base, OsKernelData->Image->EntryPoint, KernelFile, FileSize);
    OSKERNELENTRY OsKernelEntry = (OSKERNELENTRY)OsKernelData->Image->EntryPoint;

// Get the memory map
UINTN MapSize = 0, DescriptorSize = 0, MapKey = 0;
UINT32 DescriptorVersion = 0;
EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
Status = gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
if(Status != EFI_BUFFER_TOO_SMALL) {
	Print(L"Failed to get memory map\n");
	return EFI_UNSUPPORTED;
}
MapSize += 3 * DescriptorSize;
gBS->AllocatePool(EfiLoaderData, MapSize, (void**)&MemoryMap);

if(EFI_ERROR(gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion))) {
	Print(L"Failed to get memory map\n");
	return EFI_UNSUPPORTED;
}

OsKernelData->MemoryMap.Memory = MemoryMap;
OsKernelData->MemoryMap.MapSize = MapSize / DescriptorSize;
OsKernelData->MemoryMap.DescriptorSize = DescriptorSize;


// Disable Watchdog Timer
gBS->SetWatchdogTimer(0, 0, 0, NULL);
// Exit boot services
if(EFI_ERROR(gBS->ExitBootServices(ImageHandle, MapKey))) {
	Print(L"Failed to exit boot services\n");
	return EFI_UNSUPPORTED;
}
// بسم الله الرحمان الرحيم
    // DrawText(L"Entering kernel...", 300, 430, 22, 0xFFFFFF);
    OsKernelEntry(OsKernelData);
    return EFI_SUCCESS;
}