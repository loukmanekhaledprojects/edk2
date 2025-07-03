#include <bootx64.h>
#include <nvfermi.h>


// }

void NvidiaGpuInit(EFI_PCI_IO_PROTOCOL* Pci) {
    Print(L"Found NVIDIA GF106 Gpu\n");

    NVGPU Nvgpu ={ 0};
    Nvgpu.Pci = Pci;

UINT32 val = NvMemRead(&Nvgpu, 0x00000000); // NV_PMC_BOOT_0
Print(L"PMC_BOOT_0 = 0x%x\n", val);
    //     // 1. Set resolution (horizontal & vertical)
    NvMemWrite(&Nvgpu, NV_PFB_HOR_DISP, 1920);
    NvMemWrite(&Nvgpu, NV_PFB_VER_DISP, 1080);

    // 2. Configure framebuffer: canvas_sel=5 (usually 1920 width), bpp=4 bytes (32bpp)
    NvMemWrite(&Nvgpu, NV_PFB_CONFIG, (5 << 4) | (3 << 8));

    // 3. Enable 32bpp canvas config
    NvMemWrite(&Nvgpu, NV_PGRAPH_CANVAS_CONFIG, (1 << 0) | (1 << 16));

    // 4. Scanout start at VRAM offset 0
    NvMemWrite(&Nvgpu, NV_PFB_START, 0);

    // 5. Setup CRTC timings (approximate timings for 1080p60)
    NvMemWrite(&Nvgpu, NV_CRTC_HDISPLAY, 1920);
    NvMemWrite(&Nvgpu, NV_CRTC_VDISPLAY, 1080);
    NvMemWrite(&Nvgpu, NV_CRTC_HSYNC_START, 2008);
    NvMemWrite(&Nvgpu, NV_CRTC_HSYNC_END, 2052);
    NvMemWrite(&Nvgpu, NV_CRTC_VSYNC_START, 1084);
    NvMemWrite(&Nvgpu, NV_CRTC_VSYNC_END, 1089);

    // 6. Enable CRTC (bit 0 = enable)
    NvMemWrite(&Nvgpu, NV_CRTC_CONTROL, 1);

    Print(L"Resolution set to 1920x1080.\n");
}



void NvGpuTest() {
    Print(L"Looking for Nvidia Quadro 2000\n");

EFI_STATUS status;
UINTN handleCount = 0;
EFI_HANDLE *handles = NULL;

status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &handleCount, &handles);
if (EFI_ERROR(status) || handleCount == 0) {
    Print(L"No PCI handles found.\n");
}

for (UINTN i = 0; i < handleCount; i++) {
    EFI_PCI_IO_PROTOCOL *pciIo = NULL;
    status = gBS->HandleProtocol(handles[i], &gEfiPciIoProtocolGuid, (VOID**)&pciIo);
    if (EFI_ERROR(status)) {
        Print(L"Failed to get PCI IO Protocol for handle %u\n", i);
        continue;
    }

    UINT16 vendorId = 0, deviceId = 0;
    pciIo->Pci.Read(pciIo, EfiPciIoWidthUint16, PCI_VENDOR_ID_OFFSET, 1, &vendorId);
    pciIo->Pci.Read(pciIo, EfiPciIoWidthUint16, PCI_DEVICE_ID_OFFSET, 1, &deviceId);

    Print(L"Handle %u: VendorId = 0x%04x, DeviceId = 0x%04x\n", i, vendorId, deviceId);

    if (vendorId == NV_VENDOR_ID && deviceId == NV_DEVICE_GF106) {
        Print(L"NVIDIA GPU found! DEVICE_ID %x\n", deviceId);
        NvidiaGpuInit(pciIo);
        // If you want to detect multiple GPUs, avoid infinite loop here
        // Remove or move while(1) after loop
    }
}

// Optionally free handles if allocated by LocateHandleBuffer
// if (handles) {
//     FreePool(handles);
// }
for(;;);    

}