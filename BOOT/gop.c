#include <bootx64.h>

void EFIAPI LocateBootFb(void) {
	EFI_GRAPHICS_OUTPUT_PROTOCOL* GraphicsProtocol;
	// Check for G.O.P Support
	Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (void**)&GraphicsProtocol);
	if(EFI_ERROR(Status)) {
		Print(L"Graphics Output Protocol is not supported.\n");
		while(1);
	}
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* ModeInfo;
	UINTN szInfo;
	Status = GraphicsProtocol->QueryMode(GraphicsProtocol, GraphicsProtocol->Mode == NULL ? 0 : GraphicsProtocol->Mode->Mode, &szInfo, &ModeInfo);
	if(Status == EFI_NOT_STARTED) {
		if(EFI_ERROR(GraphicsProtocol->SetMode(GraphicsProtocol, 0)) || !GraphicsProtocol->Mode || !GraphicsProtocol->Mode->FrameBufferBase) {
			Print(L"Failed to set video mode.\n");
			while(1);
		}
	}
	OsKernelData->BootFb.BaseAddress = (void*)GraphicsProtocol->Mode->FrameBufferBase;
	OsKernelData->BootFb.Size = GraphicsProtocol->Mode->FrameBufferSize;
	OsKernelData->BootFb.hRes = ModeInfo->HorizontalResolution;
	OsKernelData->BootFb.vRes = ModeInfo->VerticalResolution;
	OsKernelData->BootFb.Pitch = ModeInfo->PixelsPerScanLine * 4;
    Gop = GraphicsProtocol;
}