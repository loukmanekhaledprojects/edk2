#include <bootx64.h>
void* EFIAPI ReadFile(IN CHAR16* Path, OUT UINT64* FileSize) {
    Print(L"Reading file %s\n", Path);
    EFI_HANDLE *handles;
    UINTN count;
    EASSERT(gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &count, &handles), L"Failed to read file (LocateHandleBuffer)");
    ASSERT(count > 0);
    
    EFI_FILE_PROTOCOL* File;

    EFI_FILE_PROTOCOL* Root;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Fs;

    for (UINTN i = 0; i < count; ++i) {
    if (EFI_ERROR(gBS->HandleProtocol(handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Fs)))
        continue;

    if (EFI_ERROR(Fs->OpenVolume(Fs, &Root)))
        continue;

    if (!EFI_ERROR(Root->Open(Root, &File, Path, EFI_FILE_MODE_READ, 0))) {
        // Found the file!
        break;
    } else {
        // Clean up Root if file open failed
        Root->Close(Root);
        Root = NULL;
    }
}
    if(!Root) {
        Print(L"Cannot find file %s\n");
        while(1);
    }
    // EASSERT(gBS->HandleProtocol(handles[0], &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Fs),  L"Failed to read file (HandleProtocol)");
    // EASSERT(Fs->OpenVolume(Fs, &Root), L"Failed to read file (OpenVolume)");

    // EASSERT(Root->Open(Root, &File, Path, EFI_FILE_MODE_READ, 0), L"Failed to read file (Open File)");

    // Get file size
    UINTN infoSize = sizeof(EFI_FILE_INFO) + 200;
    EFI_FILE_INFO* fi = bAllocatePool(infoSize);
    EASSERT(File->GetInfo(File, &gEfiFileInfoGuid, &infoSize, fi), L"Failed to read file (Get File Info)");

    UINT64 size = fi->FileSize;
    UINTN pages = (UINTN)MAJOR(size, EFI_PAGE_SIZE);
    void* buf = bAllocatePages(pages >> 12);

    // Print(L"Reading %llu bytes into buffer %p\n", size, buf);
    EASSERT(File->Read(File, &size, buf), L"Failed to read file (File->Read)");

    File->Close(File);
    Root->Close(Root);
    gBS->FreePool(fi);

    *FileSize = size;
    return buf;
}