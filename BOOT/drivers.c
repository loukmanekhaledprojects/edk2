#include <bootx64.h>

static UINT64 DrvBaseAddr = 0xFFFF800001000000;


static CHAR16 Path[256] = {0};

STS USERAPI LoadDll(UINT16* DllName, void* Custum) {
    (void)Custum;
    Status = StrCpyS(Path, 256, L"\\System\\");
    if(EFI_ERROR(Status)) return 2;    
    Status = StrCatS(Path, 256, DllName);
    if(EFI_ERROR(Status)) return 2;
    UINT64 FileSize;
    void* File = ReadFile(Path, &FileSize);
    STARTUPDLL* Dll = bAllocatePool(sizeof(STARTUPDLL));
    StrCpyS(Dll->Name, 256, DllName);
    Dll->File = File;
    Dll->Next = OsKernelData->StartupDlls;
    OsKernelData->StartupDlls = Dll;
    return 0;
}
static CHAR16 __Name[256];
BOOLEAN USERAPI DllRead(char* DllName, void** DllCopy, UINT64* DllFileSize, void** LoadAddress) {
    AsciiStrToUnicodeStrS(DllName, __Name, 256);
    Print(L"Dll read request : %s\n", __Name);
    if(AsciiStrCmp(DllName, "OSKERNEL.exe") == 0) {
        Print(L"OSKERNEL Linking...\n");
        *DllCopy = &OsKernelData->Image;
        *DllFileSize = 0; // For the kernel (no need to load image again)
        *LoadAddress = OsKernelData->Image.Base;
    } else while(1);

    return TRUE;
}

STS USERAPI LoadStartupDriver(UINT16* DriverName, void* Custum) {
    (void)Custum;

    // Build the path correctly
    
    Status = StrCpyS(Path, 256, L"\\System\\");
    if(EFI_ERROR(Status)) return 2;
    Status = StrCatS(Path, 256, DriverName);
    if(EFI_ERROR(Status)) return 2;

    // Print with clear formatting
    UINT64 FileSize;
    void* DriverImage = ReadFile(Path, &FileSize);
    STARTUPDRIVER* Driver = bAllocatePool(sizeof(STARTUPDRIVER));
    StrCpyS(Driver->Name, 256, DriverName);
    Driver->Next = OsKernelData->StartupDrivers;
    OsKernelData->StartupDrivers = Driver;
    
    Driver->Image.Base = (void*)DrvBaseAddr;
    DrvBaseAddr+=MAJOR(((UINT64)Driver->Image.VaLimit - (UINT64)Driver->Image.VaBase), 0x200000);
    if((Status = LoadImage(&Driver->Image, DriverImage, FileSize, DllRead))) return Status;
 
    return 0;
}

void LoadStartupDrivers(void) {
    UINT64 FileSize, DllListSize;
    char* DllList = ReadFile(L"\\System\\STARTUPDLLS", &DllListSize);
    char* DriverList = ReadFile(L"\\System\\STARTUPDRIVERS", &FileSize);
    STS Code;
    if((Code = IterateByLine(DllList, DllListSize, LoadDll, NULL)) || (Code = IterateByLine(DriverList, FileSize, LoadStartupDriver, NULL))) {
        Print(L"Failed to load %s with code %d\n", Path, Code);
        gBS->Exit(gImageHandle, EFI_UNSUPPORTED, 0, NULL);
    }
}