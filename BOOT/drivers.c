#include <bootx64.h>


// TODO : Make pages closer to the kernel for more efficiency
UINT64 DrvBaseAddr = 0xFFFF810000000000;


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
    UINTN Len = StrLen(DllName);
    for(UINTN i=0;i<Len+1;i++){
        Dll->Name[i] = (UINT8)DllName[i];
    }
    Dll->File = File;
    Dll->FileSize = FileSize;
    Dll->Loaded = FALSE;
    Dll->Next = OsKernelData->StartupDlls;
    OsKernelData->StartupDlls = Dll;


    
    return 0;
}
BOOLEAN USERAPI DllRead(char* DllName, void** DllCopy, UINT64* DllFileSize, void** LoadAddress) {
    AsciiPrint("Dll read request : %a\n", DllName);
    if(AsciiStrCmp(DllName, "OSKERNEL.exe") == 0) {
        Print(L"OSKERNEL Linking...\n");
        *DllCopy = &OsKernelData->Image;
        *DllFileSize = 0; // For the kernel (no need to load image again)
        *LoadAddress = OsKernelData->Image.Base;
        return TRUE;
    } else {
        while(1);
        STARTUPDLL* Dll = OsKernelData->StartupDlls;
        while(Dll) {
            if(AsciiStriCmp(Dll->Name, DllName) == 0) {
                Print(L"Linking DLL : %a", Dll->Name);
                *DllCopy = Dll->File;
                *DllFileSize = Dll->FileSize;
                *LoadAddress = (void*)DrvBaseAddr;
                return TRUE;
            }
            Dll = Dll->Next;
        }
        return FALSE;
    }

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
    if((Status = LoadImage(&Driver->Image, DriverImage, FileSize, DllRead))) return Status;
    DrvBaseAddr+=MAJOR(((UINT64)Driver->Image.VaLimit - (UINT64)Driver->Image.VaBase), 0x200000);
 
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