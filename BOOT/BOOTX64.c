// بسم الله الرحمان الرحيم
#include <bootx64.h>
#include <intrin.h>
OSKERNELDATA* OsKernelData = NULL;
EFI_STATUS Status = EFI_SUCCESS;
FONT StartupFont = {0};
BOOLEAN efiexited = 0;
int LargePgMap = 0;
#define EASSERT(__f, errmsg) if(EFI_ERROR(Status = __f)) {Print(L"Cannot continue the boot process.\n%s STATUS : %lx\n", errmsg, Status); gBS->Exit(gImageHandle, 1, 0, NULL);}
EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;
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
    Gop = GraphicsProtocol;
}

void* EFIAPI OsAllocatePages(UINTN NumPages) {
    if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }
    EFI_PHYSICAL_ADDRESS Addr;
    EFI_PHYSICAL_ADDRESS* Pt = &Addr;
    
    EASSERT(gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, NumPages << ((LargePgMap ? (10) : 0)), Pt), L"Failed to allocate pages");
    if(LargePgMap) {
        EFI_PHYSICAL_ADDRESS NewAddr = MAJOR(Addr, 0x200000);
        if(NewAddr != Addr) EASSERT(gBS->FreePages(Addr, (NewAddr - Addr) >> 12), "Large Page Allocation error");
        Addr = NewAddr;
    }
    return (void*)((UINTN)Addr);
}
void* EFIAPI OsAllocatePool(UINTN Size) {
    if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }
    void* Addr;
    void** Pt = &Addr;
    EASSERT(gBS->AllocatePool(EfiLoaderData, Size, Pt), L"Failed to allocate pool");
    return Addr;
}

void* __fastcall UlibAlloc(UINT64 Size) {return OsAllocatePool(Size);}
void __fastcall UlibFree(void* Address) {if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }gBS->FreePool(Address);}
void* __fastcall UlibAllocPages(UINT64 NumPages) {return OsAllocatePages(NumPages);}
void __fastcall UlibFreePages(void* Address, UINT64 NumPages) {if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }gBS->FreePages((EFI_PHYSICAL_ADDRESS)Address, NumPages);}

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

// Print(L"Reading %llu bytes into buffer %p\n", size, buf);
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

void DrawText(const UINT16* Text, UINT32 DestX, UINT32 DestY, UINT32 FontSize, UINT32 FontWeight, UINT32 ForegroundColor) {
    UINTN Len = StrLen(Text);
    if(!FontSetSize(&StartupFont, FontSize) || !FontSetWeight(&StartupFont, FontWeight)) {
        Print(L"Could not use the startup font.\n");
            gBS->Exit(gImageHandle, EFI_LOAD_ERROR, 0, NULL);
    }
    signed long x = DestX;
    UINT16 PrevChar = 0;
    FONT_GLYPH_DATA GlyphData = {0};

    for (UINT32 i = 0; i < (UINT32)Len; i++) {
        if (!FontRenderGlyf(&StartupFont, &GlyphData, Text[i], PrevChar)) {
            continue;
            // Print(L"Failed to load glyph bitmap.\n");
            // gBS->Exit(gImageHandle, EFI_LOAD_ERROR, 0, NULL);
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

void ImageResize(
    IMAGERENDER* Imgr,
    UINT32 dst_w, UINT32 dst_h)
{
    UINT8* src_pixels =Imgr->PixelBuffer;
    UINT32 src_w = Imgr->Width;
    UINT32 src_h = Imgr->Height;
    UINT8* dst_pixels = OsAllocatePool(dst_w * dst_h * 4);
    
    for (UINT32 y = 0; y < dst_h; y++) {
        UINT32 src_y = y * src_h / dst_h;
        for (UINT32 x = 0; x < dst_w; x++) {
            UINT32 src_x = x * src_w / dst_w;
            // Copy pixel (4 bytes BGRA)
            UINT8* src_px = &src_pixels[(src_y * src_w + src_x) * 4];
            UINT8* dst_px = &dst_pixels[(y * dst_w + x) * 4];
            dst_px[0] = src_px[0];
            dst_px[1] = src_px[1];
            dst_px[2] = src_px[2];
            dst_px[3] = src_px[3];
        }
    }
    UlibFree(src_pixels);
    Imgr->PixelBuffer = dst_pixels;
    Imgr->Width = dst_w;
    Imgr->Height = dst_h;
}

UINT16 __debug_fmt[0x100];

#include <Library/PrintLib.h>
#include <stdarg.h>
UINT32 ConOff = 0;
void SimpleConOut(const UINT16* Str, ...) {
    va_list args;
    va_start(args, Str);
    UnicodeVSPrint(__debug_fmt, 0xFF0, Str, args);
    va_end(args);
    DrawText(__debug_fmt, 20, ConOff+=20, 16, 500, 0xAAAAAA);
}

MEMORY_HEADER Hdr;
void* USERAPI TempPAlloc(UINT64 l) {
    return MMTRequestMemory(OsKernelData->PhysicalAllocator, &Hdr, l);
}
void* USERAPI TempAlloc(UINT64 l) {
    return TempPAlloc(MAJOR(l, 0x1000) >> 12);
}
void USERAPI TempFree(void* a) {
}
void USERAPI TempPFree(void* a, UINT64 c) {
}
typedef UINT64 PAGE_ENTRY;
static PAGE_ENTRY* __fastcall GetOrCreateTable(PAGE_ENTRY* Table, UINTN Index) {
    if (!(Table[Index] & 1)) {
        VOID* NewPage = TempPAlloc(1);
        if (!NewPage) return NULL;
        EnhancedMemClr(NewPage, 0x1000);
        Table[Index] = ((UINT64)NewPage & ~0xFFFULL) | 3;
    }
    return (PAGE_ENTRY*)(UINTN)(Table[Index] & ~0xFFFULL);
}
__declspec(align(0x1000)) PAGE_ENTRY __PML4[512] = {0};
PAGE_ENTRY* PML4 = __PML4;
INTN __fastcall MapPages(UINT64 PhysAddr, UINT64 VirtualAddr, UINT64 Count, UINT64 Attr) {
    VirtualAddr = ((VirtualAddr >= 0xffff800000000000)) ? (1ULL << 47) + (VirtualAddr - 0xffff800000000000) : (VirtualAddr);
    UINT8 MaxLvl = 4 - (Count >> 62);
    UINT64 Step = 1ULL << (12 + ((Count >> 62) * 9));
    Count &= ((1ULL << 62) - 1);
    while(Count--) {
        PAGE_ENTRY* Pte = PML4;
        for(UINT8 i =0;i<MaxLvl - 1;i++) {
            Pte = GetOrCreateTable(Pte, (VirtualAddr >> (39 - 9*i)) & 0x1FF);
        }
        Pte[(VirtualAddr >> (39 - 9*(MaxLvl - 1))) & 0x1FF] = (PhysAddr & ~0xFFFULL) | Attr;
        PhysAddr+=Step;
        VirtualAddr+=Step;
    }
    return 0;
}


EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable) {

    gST = SystemTable;
    gBS = SystemTable->BootServices;
    gImageHandle = ImageHandle;
    Print(L"UEFI Bootloader Startup\n");
    // Allocate Null page to prevent errors
    EFI_PHYSICAL_ADDRESS NullPg = 0;
    if(!EFI_ERROR(gBS->AllocatePages(AllocateAddress, EfiLoaderData, 1, &NullPg))) {
        // Print(L"NULL Page Reserved successfully\n");
    }
    
    OsKernelData = OsAllocatePages(PSZ_OSKERNELDATA);
    USERMEMORYIF ULibIf = {
        UlibAlloc,
        UlibAllocPages,
        UlibFree,
        UlibFreePages
    };
UlibSetMemoryInterface(&ULibIf);

    // UlibSetMemoryInterface(&ULibIf);
    
    LocateBootFb();
    
    UINT64 FileSize;
    UINT64 FontFileSize;
    UINT32* FontFile = ReadFile(L"\\Fonts\\Inter.ttf", &FontFileSize);
    // if(!FontLoad(&StartupFont, FontFile, FontFileSize)){
        
    //     Print(L"Failed to load startup font.\n");
    //     return EFI_LOAD_ERROR;
    // }
    for(int i =0;i<OsKernelData->BootFb.Size / 4;i++) {
        OsKernelData->BootFb.BaseAddress[i] = 0;
    }
    
        // IMAGERENDER Imgr;
// void* ImageFile = ReadFile(L"\\System\\wallpaper.jpg", &FileSize);


// if(!ImageRender(ImageFile, FileSize, &Imgr)) {
//     Print(L"Failed to render wallpaper\n");
//     return EFI_LOAD_ERROR;
// }
// ImageResize(&Imgr, OsKernelData->BootFb.hRes, OsKernelData->BootFb.vRes);
// Gop->Blt(Gop, Imgr.PixelBuffer, EfiBltBufferToVideo, 0, 0, 0, 0, Imgr.Width, Imgr.Height, 0);

    
    // DrawText(L"BISMILLAH", 20, 20, 99, 900, 0xFFFFFF);

    // DrawText(L"Operating System Booting...", 300, 400, 22, 500, 0xFFFFFF);
    // // TestUi();
    // DrawText(L"Welcome To the Operating System", 100, 300, 40, 200, 0xFFFFFF);
    void* KernelFile = ReadFile(L"\\System\\OSKERNEL.EXE", &FileSize);
    LargePgMap = 1;

    OsKernelData->Image = LoadImage(KernelFile, FileSize, NULL);
    LargePgMap = 0;



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
SetMem(MemoryMap, MapSize, 0);


MEMORYMGRTBL* Mmt = OsAllocatePages(MAJOR(sizeof(MEMORYMGRTBL) + (MapSize/DescriptorSize) * sizeof(BLOCK), 0x1000) >> 12);
if(EFI_ERROR(gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion))) {
	Print(L"Failed to get memory map\n");
	return EFI_UNSUPPORTED;
}
OsKernelData->PhysicalAllocator = Mmt;


// Print(L"MMTInit...\n");
MEMORY_HEADER* KBlock = (MEMORY_HEADER*)(Mmt + 1);
MMTInit(Mmt, 0x1000);

UINT64 Total = 0;
for(UINTN i = 0;i<MapSize/DescriptorSize;i++) {
EFI_MEMORY_DESCRIPTOR* Block = (EFI_MEMORY_DESCRIPTOR*)((char*)MemoryMap + i*DescriptorSize);
    if(Block->Type == EfiConventionalMemory) {


        MMTCreateRegion(Mmt, KBlock, (void*)Block->PhysicalStart, Block->NumberOfPages);
        Total+=KBlock->Size;
        KBlock++;
    }
    MapPages(Block->PhysicalStart,Block->PhysicalStart, Block->NumberOfPages, PAGE_PRESENT | PAGE_RW);
}
    MapPages((UINT64)OsKernelData->BootFb.BaseAddress, (UINT64)OsKernelData->BootFb.BaseAddress, MAJOR(OsKernelData->BootFb.Size, 0x1000) >> 12, PAGE_PRESENT | PAGE_RW);


    // Map Kernel
    MapPages((UINT64)OsKernelData->Image->VaBase, 0xFFFF800000000000, (MAJOR(((UINT64)OsKernelData->Image->VaLimit - (UINT64)OsKernelData->Image->VaBase), 0x200000) >> 21) | PAGESZ_2MB, PAGE_PRESENT | PAGE_RW | PAGE_LARGE);
    if(!RelocateImage(OsKernelData->Image, (void*)0xFFFF800000000000)) {
        Print(L"Error loading kernel, relocation failed.\n");
        return EFI_UNSUPPORTED;
    }
    OSKERNELENTRY OsKernelEntry = (OSKERNELENTRY)OsKernelData->Image->EntryPoint;


// Blocks passed to the kernel

OsKernelData->MemoryMap.Memory = MemoryMap;
OsKernelData->MemoryMap.MapSize = MapSize / DescriptorSize;
OsKernelData->MemoryMap.DescriptorSize = DescriptorSize;
OsKernelData->StartupFont = &StartupFont;

// // Disable Watchdog Timer
gBS->SetWatchdogTimer(0, 0, 0, NULL);
// Exit boot services

if(EFI_ERROR(gBS->ExitBootServices(ImageHandle, MapKey))) {
	Print(L"Failed to exit boot services\n");
	return EFI_UNSUPPORTED;
}

__writecr4(__readcr4() | (1 << 4)); // Enable PSE to support 1GB Pages
__writecr3((UINT64)PML4);

efiexited=1;
ULibIf.Allocate = TempAlloc;
ULibIf.AllocatePages = TempPAlloc;
ULibIf.Free = TempFree;
ULibIf.FreePages = TempPFree;
UlibSetMemoryInterface(&ULibIf);

FontLoad(&StartupFont, FontFile, FontFileSize);
DrawText(L"BISMILLAH", 400, 20, 99, 900, 0xFFFFFF);

SimpleConOut(L"Starting Kernel... Entry Point %lx", OsKernelEntry);
// SimpleConOut(L"Enabling VMX");
// // VMX Is required
// Status = VmxEnable();
// if(Status == 2) SimpleConOut(L"VMX Is not supported by the host CPU.");
// else if(Status == 3) SimpleConOut(L"VMX Is disabled in BIOS");
// else if(Status == 0) SimpleConOut(L"VMX Enabled");
// MEMORY_HEADER _mhdr;
// void* VmxMem = MMTRequestMemory(Mmt, &_mhdr, 0x10000);
// VmStart(VmxMem, SimpleConOut);
// بسم الله الرحمان الرحيم
    // DrawText(L"Entering kernel...", 300, 430, 22, 0xFFFFFF);
    OsKernelEntry(OsKernelData);
    return EFI_SUCCESS;
}