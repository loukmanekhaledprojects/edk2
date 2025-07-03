#pragma once
#include <Uefi.h>


#define NV_VENDOR_ID        0x10DE
#define NV_DEVICE_GF106     0x0DD8
#define GF106_BAR0          0
#define GF106_BAR1          1

// PFB MMIO window is at BAR0 + 0x100000
#define PFB_BASE            0x100000
#define NV_PFB_CONFIG       0x200    // PFB.CONFIG offset
#define NV_PFB_START        0x400    // PFB.START (vram address)
#define NV_PFB_HOR_DISP     0x530
#define NV_PFB_VER_DISP     0x570

// PGRAPH CANVAS
#define NV_PGRAPH_CANVAS_CONFIG  0x1E0000

// GDI register offsets (object + offsets)
#define NV03_GDI                                  0x004B00
#define NV03_GDI_COLOR                            0x0084
#define NV03_GDI_XY                               0x0088
#define NV03_GDI_WH                               0x008C
#define NV03_GDI_OPERATION                        0x0090
#define NV03_GDI_OP_RECT                          0x1
#define NV03_GDI_OP_EXECUTE                       0x80000000



// CRTC registers (enable/display timing) - use 0x2000xx offsets
#define NV_CRTC_HDISPLAY        0x0020500
#define NV_CRTC_VDISPLAY        0x0020510
#define NV_CRTC_HSYNC_START     0x0020520
#define NV_CRTC_HSYNC_END       0x0020530
#define NV_CRTC_VSYNC_START     0x0020540
#define NV_CRTC_VSYNC_END       0x0020550
#define NV_CRTC_CONTROL         0x0020400  // Bit0 = enable


// VRAM base address (BAR1) - to be mapped separately
#define BAR1_INDEX              1

enum {
    NVFIFO_PUT = 0x2538,
    NVFIFO_GET = 0x253C,
    NVFIFO_START = 0x3210,
    NVFIFO_CONTEXT = 0x2640,
    NVPGRAPH_STATUS = 0x400700,
    NVPMC_STATUS = 0x100
};

typedef struct {
    EFI_PCI_IO_PROTOCOL* Pci;
} NVGPU;

static inline UINT32 NvMemRead(NVGPU* Gpu, UINT32 Reg) {
    UINT32 Val = 0;
    Gpu->Pci->Mem.Read(
        Gpu->Pci,
        EfiPciIoWidthUint32,
        0,       // BAR 0 = MMIO
        (UINT64)Reg,
        1,
        &Val
    );
    return Val;
}

static inline void NvMemWrite(NVGPU* Gpu, UINT32 Reg, UINT32 Val) {
    Gpu->Pci->Mem.Write(
        Gpu->Pci,
        EfiPciIoWidthUint32,
        0,       // BAR 0 = MMIO
        (UINT64)Reg,
        1,
        &Val
    );
}