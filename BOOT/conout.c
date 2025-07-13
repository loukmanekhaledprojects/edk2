#include <bootx64.h>





void BlitGrayBitmapToFramebuffer(
    OSKERNELDATA* osData,
    int dest_x,
    int dest_y,
    FONT_GLYPH_BITMAP* bitmap,
    int bitmap_left,
    int bitmap_top,
    UINT32 fg_color    // foreground color as 0x00RRGGBB
) {
    UINT32 width = osData->BootFb.hRes;
    UINT32 height = osData->BootFb.vRes;
    UINT32 pitch_pixels = osData->BootFb.Pitch / sizeof(UINT32);
    UINT32* framebuffer = osData->BootFb.BaseAddress;

    for (UINT32 row = 0; row < bitmap->rows; row++) {
        int y = dest_y + row - bitmap_top;
        if (y < 0 || y >= (int)height)
            continue;

        for (UINT32 col = 0; col < bitmap->width; col++) {
            int x = dest_x + col + bitmap_left;
            if (x < 0 || x >= (int)width)
                continue;

            UINT8 alpha = bitmap->buffer[row * bitmap->pitch + col];
            UINT8 inv_alpha = 255 - alpha;

            UINT32* pixel = &framebuffer[y * pitch_pixels + x];

            // Extract RGB components of fg_color and bg_color
            UINT8 fg_r = (fg_color >> 16) & 0xFF;
            UINT8 fg_g = (fg_color >> 8) & 0xFF;
            UINT8 fg_b = fg_color & 0xFF;


            // Existing framebuffer pixel RGB (assuming no alpha)
            UINT8 dst_r = (*pixel >> 16) & 0xFF;
            UINT8 dst_g = (*pixel >> 8) & 0xFF;
            UINT8 dst_b = *pixel & 0xFF;

            // Blend foreground glyph color with existing pixel based on alpha
            UINT8 out_r = (fg_r * alpha + dst_r * inv_alpha) / 255;
            UINT8 out_g = (fg_g * alpha + dst_g * inv_alpha) / 255;
            UINT8 out_b = (fg_b * alpha + dst_b * inv_alpha) / 255;

            *pixel = (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

void DrawText(const UINT16* Text, UINT32 DestX, UINT32 DestY, UINT32 FontSize, UINT32 FontWeight, UINT32 ForegroundColor) {
    UINTN Len = StrLen(Text);
    
    if(!FontSetSize(OsKernelData->StartupFont, FontSize) || !FontSetWeight(OsKernelData->StartupFont, FontWeight)) {
        Print(L"Could not use the startup font.\n");
            gBS->Exit(gImageHandle, EFI_LOAD_ERROR, 0, NULL);
    }
    signed long x = DestX;
    UINT16 PrevChar = 0;
    FONT_GLYPH_DATA GlyphData = {0};

    for (UINT32 i = 0; i < (UINT32)Len; i++) {
        if (!FontRenderGlyf(OsKernelData->StartupFont, &GlyphData, Text[i], PrevChar)) {
            continue;
            // Print(L"Failed to load glyph bitmap.\n");
            // gBS->Exit(gImageHandle, EFI_LOAD_ERROR, 0, NULL);
        }

        // Add kerning between previous char and current char BEFORE drawing
        x += GlyphData.KerningX;

        // Draw glyph at current position x
        BlitGrayBitmapToFramebuffer(OsKernelData, x, DestY + GlyphData.Top, GlyphData.Bitmap, 0, 0, ForegroundColor);

        // Advance x by glyph's advance width for next glyph
        x += GlyphData.AdvanceX;

        PrevChar = Text[i];
    }
}
UINT16 __debug_fmt[0x100];
#include <stdarg.h>
#include <Library/PrintLib.h>

#define COM1 0x3F8
#pragma intrinsic(__outbyte)
#pragma intrinsic(__inbyte)

void serial_init() {
    __outbyte(COM1 + 1, 0x00); // Disable interrupts
    __outbyte(COM1 + 3, 0x80); // Enable DLAB
    __outbyte(COM1 + 0, 0x03); // 38400 baud
    __outbyte(COM1 + 1, 0x00);
    __outbyte(COM1 + 3, 0x03); // 8 bits, no parity, one stop
    __outbyte(COM1 + 2, 0xC7); // Enable FIFO, clear them
    __outbyte(COM1 + 4, 0x0B); // IRQs enabled, RTS/DSR set
}

void serial_write(char c) {
    while (!(__inbyte(COM1 + 5) & 0x20));
    __outbyte(COM1, c);
}

void serialprint(const char* str, ...) {
    va_list args;
    va_start(args, str);
    AsciiVSPrint((char*)__debug_fmt, 0xFF0, str, args);
    va_end(args);
    char* f = (char*)__debug_fmt;
    while (*str) serial_write(*f++);
    serial_write('\n');
}
UINT32 ConOff = 0;

void KCALL SimpleConOut(const UINT16* Str, ...) {
    va_list args;
    va_start(args, Str);
    UnicodeVSPrint(__debug_fmt, 0xFF0, Str, args);
    va_end(args);
    DrawText(__debug_fmt, 20, ConOff+=20, 16, 500, 0xAAAAAA);
}
