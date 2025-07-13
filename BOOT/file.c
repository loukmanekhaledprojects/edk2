#include <bootx64.h>
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