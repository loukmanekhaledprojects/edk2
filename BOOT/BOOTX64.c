// بسم الله الرحمان الرحيم
#include <bootx64.h>
#include <intrin.h>
__declspec(align(64)) OSKERNELDATA* OsKernelData = NULL;
__declspec(align(64)) EFI_STATUS Status = EFI_SUCCESS;
// __declspec(align(64)) FONT StartupFont = {0};
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
    EnhancedMemClr(OsKernelData, PSZ_OSKERNELDATA << 12);
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

    OsKernelData->StartupFont.Font = bAllocatePool(sizeof(FONT));
    OsKernelData->StartupFont.File = FontFile;
    OsKernelData->StartupFont.Size = FontFileSize;

    for(int i =0;i<OsKernelData->BootFb.Size / 4;i++) {
        OsKernelData->BootFb.BaseAddress[i] = 0;
    }
    
    void* KernelFile = ReadFile(L"\\System\\OSKERNEL.EXE", &FileSize);
    LargePgMap = 0x200;
    OsKernelData->Image.Base = (void*)0xFFFF800000000000;
    if(LoadImage(&OsKernelData->Image, KernelFile, FileSize, NULL)) {
        Print(L"Failed to load the kernel\n");
        while(1);
        // return EFI_UNSUPPORTED;
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



// Get the memory map
UINTN MapSize = 0, DescriptorSize = 0, MapKey = 0;
UINT32 DescriptorVersion = 0;
EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
Status = gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
if(Status != EFI_BUFFER_TOO_SMALL) {
	Print(L"Failed to get memory map (0)\n");
    while(1);
	// return EFI_UNSUPPORTED;
}

MapSize += 3 * DescriptorSize;
gBS->AllocatePool(EfiLoaderData, MapSize, (void**)&MemoryMap);

SetMem(MemoryMap, MapSize, 0);

MEMORYMGRTBL* Mmt = bAllocatePages(MAJOR(sizeof(MEMORYMGRTBL) + ((MapSize/DescriptorSize) * sizeof(BLOCK)), 0x1000) >> 12);
if(EFI_ERROR(gBS->GetMemoryMap(&MapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion))) {
	Print(L"Failed to get memory map (1)\n");
    while(1);
	// return EFI_UNSUPPORTED;
}
OsKernelData->PhysicalAllocator = Mmt;


MEMORY_HEADER* KBlock = (MEMORY_HEADER*)(Mmt + 1);
MMTInit(Mmt, 0x1000);



UINT64 Total = 0;
for(UINTN i = 0;i<MapSize/DescriptorSize;i++) {

EFI_MEMORY_DESCRIPTOR* Block = (EFI_MEMORY_DESCRIPTOR*)((char*)MemoryMap + i*DescriptorSize);
if(Block->PhysicalStart == 0) {Block->PhysicalStart = 0x1000;Block->NumberOfPages--;}
if(!Block->NumberOfPages) continue;  
if(Block->Type == EfiConventionalMemory) {

        
        MMTCreateRegion(Mmt, KBlock, (void*)Block->PhysicalStart, Block->NumberOfPages);
        Total+=KBlock->Size;
        KBlock++;
    }
}


    OSKERNELENTRY OsKernelEntry = (OSKERNELENTRY)OsKernelData->Image.EntryPoint;


// Blocks passed to the kernel

OsKernelData->MemoryMap.Memory = MemoryMap;
OsKernelData->MemoryMap.MapSize = MapSize / DescriptorSize;
OsKernelData->MemoryMap.DescriptorSize = DescriptorSize;
OsKernelData->VsPrint16 = UnicodeVSPrint;
OsKernelData->VsPrint8 = AsciiVSPrint;


// // Disable Watchdog Timer
gBS->SetWatchdogTimer(0, 0, 0, NULL);
// Exit boot services
if(EFI_ERROR(gBS->ExitBootServices(ImageHandle, MapKey))) {
	Print(L"Failed to exit boot services\n");
    while(1);
	// return EFI_UNSUPPORTED;
}

// Setup page table
kvMap((UINT64)OsKernelData->Image.VaBase, 0xFFFF800000000000, (MAJOR(((UINT64)OsKernelData->Image.VaLimit - (UINT64)OsKernelData->Image.VaBase), 0x200000) >> 21) | PAGESZ_2MB, PAGE_PRESENT | PAGE_RW | PAGE_LARGE);
kvMap((UINT64)OsKernelData->BootFb.BaseAddress, (UINT64)OsKernelData->BootFb.BaseAddress, MAJOR(OsKernelData->BootFb.Size, 0x1000) >> 12, PAGE_PRESENT | PAGE_RW | PAGE4KB_WRITECOMBINE);
for(UINTN i = 0;i<MapSize/DescriptorSize;i++) {
EFI_MEMORY_DESCRIPTOR* Block = (EFI_MEMORY_DESCRIPTOR*)((char*)MemoryMap + i*DescriptorSize);
    kvMap(Block->PhysicalStart, Block->PhysicalStart, Block->NumberOfPages, PAGE_PRESENT | PAGE_RW);
}

__writecr4(__readcr4() | (1 << 4)); // Enable PSE to support 1GB Pages
__writecr3((UINT64)PML4);



// بسم الله الرحمان الرحيم
    OsKernelEntry(OsKernelData);
    return EFI_SUCCESS;
}