#include <Uefi.h>
#include <svga3d_reg.h>
VMSVGA _Svga = {0};
VMSVGA* Svga = &_Svga;

#define PCI_BAR(__x) (0x10 + (__x * 4))
// int _fltused = 1;
void USERAPI SvgaGetMode(
    UINT32 *HorizontalResolution,
    UINT32 *VerticalResolution,
    UINT32 *Pitch,
    UINT32 *BitsPerPixel)
{
    *HorizontalResolution = SvgaRead(Svga, SVGA_REG_WIDTH);
    *VerticalResolution = SvgaRead(Svga, SVGA_REG_HEIGHT);
    *Pitch = SvgaRead(Svga, SVGA_REG_BYTES_PER_LINE);
    *BitsPerPixel = SvgaRead(Svga, SVGA_REG_BITS_PER_PIXEL);



    UINT32* Old = OsKernelData->BootFb.BaseAddress;
    OsKernelData->BootFb.BaseAddress = (UINT32*)(UINT64)SvgaRead(Svga, SVGA_REG_FB_START);
    OsKernelData->BootFb.Size = SvgaRead(Svga, SVGA_REG_FB_SIZE);
    if(!OsKernelData->BootFb.BaseAddress) OsKernelData->BootFb.BaseAddress = Old;
    
    
    
    if(Old != OsKernelData->BootFb.BaseAddress){
        kvMap((UINT64)OsKernelData->BootFb.BaseAddress, (UINT64)OsKernelData->BootFb.BaseAddress, MAJOR(OsKernelData->BootFb.Size, 0x1000) >> 12, PAGE_PRESENT | PAGE_RW);
    } 


    OsKernelData->BootFb.hRes = *HorizontalResolution;
    OsKernelData->BootFb.vRes = *VerticalResolution;
    OsKernelData->BootFb.Pitch = *Pitch;


}


typedef struct {
    UINT32 type;
    UINT32 format;
    UINT32 usage;
    UINT32 width;
    UINT32 height;
    UINT32 depth;
    UINT32 mipLevels;
    UINT32 multisample;
    UINT32 padding;
} SVGA3dSurfaceDefineV2Payload;

typedef struct {
    UINT32 cmd;
    UINT32 surfaceId;
    UINT64 guestAddress;
    UINT32 size;
    UINT32 padding;
} SVGA3dSurfaceDMA;

typedef struct {
    UINT32 x, y, width, height;
    UINT32 frontBufferMask;
    UINT32 backBufferMask;
} SVGA3dPresentPayload;


__declspec(align(0x1000)) UINT32 surfaceMem[1920*1080]={0};
#define fifo(__x) SvgaFifoPush(Svga, __x)
#define StartCmd(name, cmd) {fifo(name);fifo(sizeof(cmd) + 8);}
void SvgaDrawRect(UINT32 color, UINT32 x, UINT32 y, UINT32 width, UINT32 height) {
    SVGAWAIT(Svga);
    fifo(SVGA_CMD_RECT_FILL);
    fifo(color);
    fifo(x);
    fifo(y);
    fifo(width);
    fifo(height);
    fifo(SVGA_CMD_UPDATE);
    fifo(x);
    fifo(y);
    fifo(width);
    fifo(height);
    SvgaFifoSync(Svga);
}

void USERAPI SvgaSetMode(
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

#define SVGA_CMD_UPDATE_CURSOR           (SVGA_CMD_DEFINE_ALPHA_CURSOR + 1)
#define SVGA_CMD_MOVE_CURSOR             (SVGA_CMD_DEFINE_ALPHA_CURSOR + 2)
#define SVGA_CMD_SHOW_CURSOR             (SVGA_CMD_DEFINE_ALPHA_CURSOR + 3)

void SvgaEnable3d() {
SvgaDrawRect(0xFFFFFFFF, 20, 20, 100, 100);
SvgaDrawRect(0xFF00FF00, 400, 400, 200, 80);
}



void SvgaInit() {

    if(!Svga->FifoMem) return;
    kvMap((UINT64)Svga->FifoMem, (UINT64)Svga->FifoMem, MAJOR(Svga->FifoSize, 0x1000) >> 12, PAGE_PRESENT | PAGE_RW);
    kvMap((UINT64)Svga->FbMem, (UINT64)Svga->FbMem, MAJOR(Svga->FbSize, 0x1000) >> 12, PAGE_PRESENT | PAGE_RW);
    OsKernelData->BootFb.BaseAddress = Svga->FbMem;
    OsKernelData->BootFb.Size = Svga->FbSize;

        SimpleConOut(L"VMSVGA Size VRAM %x FB %x FIFO %x FIFOMEM %x", Svga->VramSize, Svga->FbSize, Svga->FifoSize, Svga->FifoMem);
    // Pci->Pci.Read(Pci, EfiPciIoWidthUint32, PCI_BAR(2), 1, &Svga->FifoMem);

    UINT32 id = SvgaRead(Svga, SVGA_REG_ID);
    SimpleConOut(L"SVGA ID %x", id);
SimpleConOut(L"Found SVGA%d Device, IO %x", id & 0xFF, Svga->IoBase);



Svga->Capabilities = SvgaRead(Svga, SVGA_REG_CAPABILITIES);


SVGAWAIT(Svga);
        if (SvgaFifoCap(Svga, SVGA_CAP_EXTENDED_FIFO) &&
        SvgaFifoRegValid(Svga, SVGA_FIFO_GUEST_3D_HWVERSION))
        {
                    SvgaFifoWrite(Svga, SVGA_FIFO_GUEST_3D_HWVERSION, SVGA3D_HWVERSION_CURRENT);
                }
    
        SimpleConOut(L"SVGA Caps: 0x%x", SvgaRead(Svga, SVGA_REG_CAPABILITIES));
    
        if(!(Svga->Capabilities & SVGA_CAP_3D)) {
                SimpleConOut(L"SVGA Device does not support 3D Acceleration.");
            }

            Svga->FifoNextCmd = (SVGA_FIFO_NUM_REGS ) * 4;
            SvgaFifoWrite(Svga, SVGA_FIFO_MIN, Svga->FifoNextCmd);

            SvgaFifoWrite(Svga, SVGA_FIFO_MAX, Svga->FifoSize);
            SvgaFifoWrite(Svga, SVGA_FIFO_NEXT_CMD, Svga->FifoNextCmd);
            SvgaFifoWrite(Svga, SVGA_FIFO_STOP, Svga->FifoNextCmd);
        SvgaWrite(Svga, SVGA_REG_ENABLE, 1);
        SvgaWrite(Svga, SVGA_REG_CONFIG_DONE, 1);
        SvgaGetMode(&Svga->Width, &Svga->Height, &Svga->Pitch, &Svga->Bpp);
        SvgaSetMode(1280, 720, 32);
        SvgaGetMode(&Svga->Width, &Svga->Height, &Svga->Pitch, &Svga->Bpp);
   SimpleConOut(L"New mode %dx%d (%d BPP) Pitch : %d",
    Svga->Width, Svga->Height, Svga->Bpp, Svga->Pitch);
    
    // Simple test
        SvgaEnable3d();
}
void Svga3dSetup() {

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
            pciIo->Pci.Read(pciIo, EfiPciIoWidthUint32, PCI_BAR(0), 1, &Svga->IoBase);
            Svga->IoBase = MASKPCIBASE(Svga->IoBase);
            Svga->VramSize = SvgaRead(Svga, SVGA_REG_VRAM_SIZE);
            Svga->FbSize = SvgaRead(Svga, SVGA_REG_FB_SIZE);
            Svga->FifoSize = SvgaRead(Svga, SVGA_REG_MEM_SIZE);    
        EFI_PHYSICAL_ADDRESS Phys;
        pciIo->Pci.Read(pciIo, EfiPciIoWidthUint32, PCI_BAR(1), 1, &Phys);
        Phys = MASKPCIBASE(Phys);
        Svga->FbMem = (UINT32*)Phys;
        pciIo->Pci.Read(pciIo, EfiPciIoWidthUint32, PCI_BAR(2), 1, &Phys);
        Phys = MASKPCIBASE(Phys);
        Svga->FifoMem = (UINT32*)Phys;
        Print(L"Found SVGA Virtual GPU\n");
        break;
    }
}
}