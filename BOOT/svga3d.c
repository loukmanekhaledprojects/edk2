#include <Uefi.h>
#include <svga3d_reg.h>


#define PCI_BAR(__x) (0x10 + (__x * 4))
// int _fltused = 1;
void USERAPI SvgaGetMode(
    VMSVGA *Svga,
    UINT32 *HorizontalResolution,
    UINT32 *VerticalResolution,
    UINT32 *Pitch,
    UINT32 *BitsPerPixel)
{
    *HorizontalResolution = SvgaRead(Svga, SVGA_REG_WIDTH);
    *VerticalResolution = SvgaRead(Svga, SVGA_REG_HEIGHT);
    *Pitch = SvgaRead(Svga, SVGA_REG_BYTES_PER_LINE);
    *BitsPerPixel = SvgaRead(Svga, SVGA_REG_BITS_PER_PIXEL);
}

void USERAPI SvgaSetMode(
    VMSVGA *Svga,
    UINT32 HorizontalResolution,
    UINT32 VerticalResolution,
    UINT32 BitsPerPixel)
{
    Svga->Width = HorizontalResolution;
    Svga->Height = VerticalResolution;
    Svga->Bpp = BitsPerPixel;

    SvgaWrite(Svga, SVGA_REG_WIDTH, HorizontalResolution);
    SvgaWrite(Svga, SVGA_REG_HEIGHT, VerticalResolution);
    SvgaWrite(Svga, SVGA_REG_BITS_PER_PIXEL, BitsPerPixel);
    Svga->Pitch = SvgaRead(Svga, SVGA_REG_BYTES_PER_LINE);
    SVGAWAIT(Svga);
}

void SvgaDrawRect(VMSVGA* Svga) {
    SVGAWAIT(Svga);
    volatile UINT32 NextCmd = Svga->FifoMem[SVGA_FIFO_NEXT_CMD] >> 2;
    volatile UINT32* Cmd = Svga->FifoMem + NextCmd;
    Cmd[0] = SVGA_CMD_RECT_FILL;
    Cmd[1] = 0xFFFFFFFF;
    Cmd[2] = 50;
    Cmd[3] = 50;
    Cmd[4] = 150;
    Cmd[5] = 150;
    Cmd[6] = SVGA_CMD_UPDATE;
    SVGAFifoCmdUpdate* CmdUpdate = (SVGAFifoCmdUpdate*)(Svga->FifoMem + 7);
    CmdUpdate->x = 50;
    CmdUpdate->y = 50;
    CmdUpdate->width = 250;
    CmdUpdate->height = 250;
    Svga->FifoMem[SVGA_FIFO_NEXT_CMD] += 44;


    SvgaFifoSync(Svga);
}

void SvgaEnable3d(VMSVGA* Svga) {

    SvgaDrawRect(Svga);

}
VOID EFIAPI OnVirtualAddressChange(IN EFI_EVENT Event, IN VOID *Context) {
    VMSVGA *Svga = (VMSVGA*)Context;
    gST->RuntimeServices->ConvertPointer(0, (VOID**)&Svga->FifoMem);
}
EFI_STATUS MapFifoMemory(VMSVGA *Svga) {
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS fifoPhys;
    UINTN fifoSize;
    EFI_PHYSICAL_ADDRESS deviceAddr;

    // Read FIFO BAR physical address from PCI config
    Status = Svga->Pci->Pci.Read(
        Svga->Pci,
        EfiPciIoWidthUint32,
        PCI_BAR(2), // Usually BAR2 is FIFO
        1,
        &fifoPhys
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to read PCI BAR2: %r\n", Status);
        return Status;
    }

    fifoPhys = fifoPhys & ~0xF;  // Mask low bits to get base address
    fifoSize = Svga->FifoSize;

    // Map FIFO BAR into CPU address space
    Status = Svga->Pci->Map(
        Svga->Pci,
        EfiPciIoOperationBusMasterCommonBuffer,  // or BusMasterRead/Write depending on usage
        (VOID*)(UINTN)fifoPhys,                   // HostAddress (physical BAR base)
        &fifoSize,
        &deviceAddr,
        &Svga->FifoMapping
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to map FIFO BAR: %r\n", Status);
        return Status;
    }

    Svga->FifoMem = (VOID*)(UINTN)deviceAddr;
    Print(L"FIFO mapped at virtual address %p\n", Svga->FifoMem);

    // Register for virtual address change event to convert pointer after ExitBootServices
    Status = gBS->CreateEventEx(
        EVT_NOTIFY_SIGNAL,
        TPL_NOTIFY,
        OnVirtualAddressChange,
        Svga,
        &gEfiEventVirtualAddressChangeGuid,
        &Svga->VirtualAddressChangeEvent
    );
    if (EFI_ERROR(Status)) {
        Print(L"Failed to create virtual address change event: %r\n", Status);
        // Cleanup mapping on failure
        Svga->Pci->Unmap(Svga->Pci, Svga->FifoMapping);
        return Status;
    }

    return EFI_SUCCESS;
}

void SvgaInit(EFI_PCI_IO_PROTOCOL* Pci) {
    VMSVGA* Svga;
    gBS->AllocatePool(EfiLoaderData, sizeof(VMSVGA), &Svga);
    Pci->Pci.Read(Pci, EfiPciIoWidthUint32, PCI_BAR(0), 1, &Svga->IoBase);
    Svga->IoBase = MASKPCIBASE(Svga->IoBase);
    Svga->VramSize = SvgaRead(Svga, SVGA_REG_VRAM_SIZE);
        Svga->FbSize = SvgaRead(Svga, SVGA_REG_FB_SIZE);
        Svga->FifoSize = SvgaRead(Svga, SVGA_REG_MEM_SIZE);
    Svga->Pci = Pci;

    EFI_PHYSICAL_ADDRESS fifoPhys;

Pci->Pci.Read(Pci, EfiPciIoWidthUint32, PCI_BAR(2), 1, &fifoPhys);
fifoPhys = MASKPCIBASE(fifoPhys);
Svga->FifoMem = (UINT32*)fifoPhys;
if(EFI_ERROR(MapFifoMemory(Svga))) return;
        Print(L"VMSVGA Size VRAM %x FB %x FIFO %x FIFOMEM %x", Svga->VramSize, Svga->FbSize, Svga->FifoSize, Svga->FifoMem);
    // Pci->Pci.Read(Pci, EfiPciIoWidthUint32, PCI_BAR(2), 1, &Svga->FifoMem);

    UINT32 id = SvgaRead(Svga, SVGA_REG_ID);
    Print(L"SVGA ID %x\n", id);
Print(L"Found SVGA%d Device, IO %x\n", id & 0xFF, Svga->IoBase);
    
// Init FIFO

    SvgaFifoWrite(Svga, SVGA_FIFO_MIN, SvgaFifoRead(Svga, SVGA_FIFO_NUM_REGS) * 4);
    SvgaFifoWrite(Svga, SVGA_FIFO_MAX, Svga->FifoSize);
    SvgaFifoWrite(Svga, SVGA_FIFO_NEXT_CMD, SvgaFifoRead(Svga, SVGA_FIFO_MIN));
    SvgaFifoWrite(Svga, SVGA_FIFO_STOP, SvgaFifoRead(Svga, SVGA_FIFO_MIN));


    /*
     * Prep work for 3D version negotiation. See SVGA3D_Init for
     * details, but we have to give the host our 3D protocol version
     * before enabling the FIFO.
     */

    if (SvgaFifoCap(Svga, SVGA_CAP_EXTENDED_FIFO) &&
        SvgaFifoRegValid(Svga, SVGA_FIFO_GUEST_3D_HWVERSION))
    {
        SvgaFifoWrite(Svga, SVGA_FIFO_GUEST_3D_HWVERSION, SVGA3D_HWVERSION_CURRENT);
    }

    UINT32 caps = SvgaRead(Svga, SVGA_REG_CAPABILITIES);
Print(L"SVGA Caps: 0x%x\n", caps);

    if(!(caps & SVGA_CAP_3D)) {
        Print(L"SVGA Device does not support 3D Acceleration.\n");
    }

    /*
     * Enable the SVGA device and FIFO.
     */

    SvgaWrite(Svga, SVGA_REG_ENABLE, TRUE);
    
    SvgaWrite(Svga, SVGA_REG_CONFIG_DONE, TRUE);
    
    SvgaGetMode(Svga, &Svga->Width, &Svga->Height, &Svga->Pitch, &Svga->Bpp);

    Print(L"Current mode %dx%d (%d BPP) Pitch : %d",
                Svga->Width, Svga->Height, Svga->Bpp, Svga->Pitch);

    SvgaSetMode(Svga, 720, 720, 32);
    SvgaGetMode(Svga, &Svga->Width, &Svga->Height, &Svga->Pitch, &Svga->Bpp);

        Print(L"New mode %dx%d (%d BPP) Pitch : %d",
                Svga->Width, Svga->Height, Svga->Bpp, Svga->Pitch);

    // Simple test
    
        SvgaEnable3d(Svga);
}

void Svga3dSetup() {
    Print(L"Looking for SVGA3D\n");

    EFI_HANDLE *handles;
UINTN handleCount;
EFI_STATUS status;

status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiPciIoProtocolGuid,
    NULL,
    &handleCount,
    &handles
);

for (UINTN i = 0; i < handleCount; i++) {
    EFI_PCI_IO_PROTOCOL *pciIo;
    status = gBS->HandleProtocol(handles[i], &gEfiPciIoProtocolGuid, (void**)&pciIo);
    if (EFI_ERROR(status)) continue;

    UINT16 vendorId, deviceId;
    pciIo->Pci.Read(pciIo, EfiPciIoWidthUint16, PCI_VENDOR_ID_OFFSET, 1, &vendorId);
    pciIo->Pci.Read(pciIo, EfiPciIoWidthUint16, PCI_DEVICE_ID_OFFSET, 1, &deviceId);
    if (vendorId == PCI_VENDOR_ID_VMWARE && (deviceId == PCI_DEVICE_ID_VMWARE_SVGA2 || deviceId == 0x0407)) {
            SvgaInit(pciIo);
    }
}
}