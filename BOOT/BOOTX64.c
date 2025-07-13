// بسم الله الرحمان الرحيم
#include <bootx64.h>
#include <intrin.h>
OSKERNELDATA* OsKernelData = NULL;
EFI_STATUS Status = EFI_SUCCESS;
FONT StartupFont = {0};
BOOLEAN efiexited = 0;
int LargePgMap = 0;
EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop = NULL;



__declspec(align(0x1000)) PAGE_ENTRY __PML4[512];
PAGE_ENTRY* PML4 = __PML4;

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
    
    OsKernelData = bAllocatePages(PSZ_OSKERNELDATA);
    ZeroMem(OsKernelData, PSZ_OSKERNELDATA << 12);
    USERMEMORYIF ULibIf = {
        UlibAlloc,
        UlibAllocPages,
        UlibFree,
        UlibFreePages
    };
UlibSetMemoryInterface(&ULibIf);

    LocateBootFb();
    
    UINT64 FileSize;
    UINT64 FontFileSize;
    UINT32* FontFile = ReadFile(L"\\Fonts\\Inter.ttf", &FontFileSize);

    
    for(int i =0;i<OsKernelData->BootFb.Size / 4;i++) {
        OsKernelData->BootFb.BaseAddress[i] = 0;
    }
    
    void* KernelFile = ReadFile(L"\\System\\OSKERNEL.EXE", &FileSize);
    LargePgMap = 1;

    OsKernelData->Image = LoadImage(KernelFile, FileSize, NULL);

    if(!RelocateImage(OsKernelData->Image, (void*)0xFFFF800000000000)) {
        Print(L"Error loading kernel, relocation failed.\n");
        return EFI_UNSUPPORTED;
    }

    LoadStartupDrivers();

    LargePgMap = 0;

    OsKernelData->ConfigurationTable.Count = SystemTable->NumberOfTableEntries;
OsKernelData->ConfigurationTable.Tables = bAllocatePool(sizeof(EFI_CONFIGURATION_TABLE) * SystemTable->NumberOfTableEntries);

// Copy config table
for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; ++i) {
        OsKernelData->ConfigurationTable.Tables[i].VendorGuid = SystemTable->ConfigurationTable[i].VendorGuid;
        OsKernelData->ConfigurationTable.Tables[i].VendorTable = SystemTable->ConfigurationTable[i].VendorTable;
    }

    // Test
    Svga3dSetup();


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

MEMORYMGRTBL* Mmt = bAllocatePages(MAJOR(sizeof(MEMORYMGRTBL) + ((MapSize/DescriptorSize) * sizeof(BLOCK)), 0x1000) >> 12);
if(EFI_ERROR(gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion))) {
	Print(L"Failed to get memory map\n");
	return EFI_UNSUPPORTED;
}
OsKernelData->PhysicalAllocator = Mmt;


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
}


    OSKERNELENTRY OsKernelEntry = (OSKERNELENTRY)OsKernelData->Image->EntryPoint;


// Blocks passed to the kernel

OsKernelData->MemoryMap.Memory = MemoryMap;
OsKernelData->MemoryMap.MapSize = MapSize / DescriptorSize;
OsKernelData->MemoryMap.DescriptorSize = DescriptorSize;
OsKernelData->StartupFont = &StartupFont;



OsKernelData->SimpleConOut =SimpleConOut;
// // Disable Watchdog Timer
gBS->SetWatchdogTimer(0, 0, 0, NULL);
// Exit boot services
if(EFI_ERROR(gBS->ExitBootServices(ImageHandle, MapKey))) {
	Print(L"Failed to exit boot services\n");
	return EFI_UNSUPPORTED;
}

// Setup page table
kvMap((UINT64)OsKernelData->Image->VaBase, 0xFFFF800000000000, (MAJOR(((UINT64)OsKernelData->Image->VaLimit - (UINT64)OsKernelData->Image->VaBase), 0x200000) >> 21) | PAGESZ_2MB, PAGE_PRESENT | PAGE_RW | PAGE_LARGE);
kvMap((UINT64)OsKernelData->BootFb.BaseAddress, (UINT64)OsKernelData->BootFb.BaseAddress, MAJOR(OsKernelData->BootFb.Size, 0x1000) >> 12, PAGE_PRESENT | PAGE_RW);
for(UINTN i = 0;i<MapSize/DescriptorSize;i++) {
EFI_MEMORY_DESCRIPTOR* Block = (EFI_MEMORY_DESCRIPTOR*)((char*)MemoryMap + i*DescriptorSize);
    kvMap(Block->PhysicalStart, Block->PhysicalStart, Block->NumberOfPages, PAGE_PRESENT | PAGE_RW);
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
SimpleConOut(L"BISMILLAH");

SimpleConOut(L"Starting Kernel... Entry Point %lx", OsKernelEntry);
OsKernelData->SimpleConOut(L"Test");
SvgaInit();
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