#include <bootx64.h>

static UINT64 DrvBaseAddr = 0xFFFF800001000000;


BOOLEAN LoadStartupDriver(UINT16* DriverName) {
    static CHAR16 Path[256] = {0};

    // Build the path correctly
    
    Status = StrCpyS(Path, 256, L"\\System\\");
    if (EFI_ERROR(Status)) {
        Print(L"StrCpyS failed: %r\n", Status);
        return FALSE;
    }

    Status = StrCatS(Path, 256, DriverName);
    if (EFI_ERROR(Status)) {
        Print(L"StrCatS failed: %r\n", Status);
        return FALSE;
    }

    // Print with clear formatting
    UINT64 FileSize;
    void* DriverImage = ReadFile(Path, &FileSize);
    LOADED_IMAGE_DATA* LoadedImage = LoadImage(DriverImage, FileSize, NULL);
    if(!RelocateImage(LoadedImage, (void*)DrvBaseAddr)) {
        
        return FALSE;
    }
 
    DrvBaseAddr+=MAJOR(((UINT64)LoadedImage->VaLimit - (UINT64)LoadedImage->VaBase), 0x200000);
    STARTUPDRIVER* Driver = bAllocatePool(sizeof(STARTUPDRIVER));
    StrCpyS(Driver->DriverName, 256, DriverName);
    Driver->LoadedImage = LoadedImage;
    Driver->Next = OsKernelData->StartupDrivers;
    OsKernelData->StartupDrivers = Driver;
    return TRUE;
}

void LoadStartupDrivers(void) {
    UINT64 FileSize;
    char* DriverList = ReadFile(L"\\System\\STARTUPDRIVERS", &FileSize);
    UINT16 DriverName[100];
    UINT16 c =0;
    for(UINT64 i=0;i<FileSize;i++) {
        if ( DriverList[i] == '\r') continue;
        if(DriverList[i] != '\n') {
            DriverName[c] = DriverList[i];
            c++;
        }
        else {
            DriverName[c]=0;
              if(!LoadStartupDriver(DriverName)){
                    Print(L"Failed to load driver %s\n", DriverName);
                    gBS->Exit(gImageHandle, EFI_UNSUPPORTED, 0, NULL);
                }

            c=0;
        }
    }
    // Last driver
    DriverName[c]=0;
    if(!c) return;
    if(!LoadStartupDriver(DriverName)){
        Print(L"Failed to load driver %s\n", DriverName);
        gBS->Exit(gImageHandle, EFI_UNSUPPORTED, 0, NULL);
    }
}