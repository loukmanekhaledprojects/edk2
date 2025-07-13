#include <bootx64.h>


void* EFIAPI bAllocatePages(UINTN NumPages) {
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
void* EFIAPI bAllocatePool(UINTN Size) {
    if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }
    void* Addr;
    void** Pt = &Addr;
    EASSERT(gBS->AllocatePool(EfiLoaderData, Size, Pt), L"Failed to allocate pool");
    return Addr;
}

void* __fastcall UlibAlloc(UINT64 Size) {return bAllocatePool(Size);}
void __fastcall UlibFree(void* Address) {if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }gBS->FreePool(Address);}
void* __fastcall UlibAllocPages(UINT64 NumPages) {return bAllocatePages(NumPages);}
void __fastcall UlibFreePages(void* Address, UINT64 NumPages) {if(efiexited) {
        *(UINT64*)(-1LL) = 0;
        while(1);
    }gBS->FreePages((EFI_PHYSICAL_ADDRESS)Address, NumPages);}


MEMORY_HEADER Hdr = {0};
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