#include <bootx64.h>
#include <Library/BaseMemoryLib.h>  // For CopyMem
#include <Library/MemoryAllocationLib.h> // For AllocatePool, FreePool
static float fmaxf(float x, float y) {
    return (x > y) ? x : y;
}

// Box blur with configurable radius
VOID DrawBoxBlur(
    UINT8* pixels,
    UINT32 width,
    UINT32 height,
    UINT32 radius
) {
    if (radius == 0) return;

    UINT8* temp = AllocatePool(width * height * 4);
    if (!temp) return;

    if (radius > 40) radius = 40; // Allow bigger blur

    for (UINT32 y = 0; y < height; y++) {
        for (UINT32 x = 0; x < width; x++) {
            UINT32 sumB = 0, sumG = 0, sumR = 0, sumA = 0;
            UINT32 count = 0;

            INT32 yStart = (INT32)y - (INT32)radius;
            INT32 yEnd = (INT32)y + (INT32)radius;
            INT32 xStart = (INT32)x - (INT32)radius;
            INT32 xEnd = (INT32)x + (INT32)radius;

            if (yStart < 0) yStart = 0;
            if (yEnd >= (INT32)height) yEnd = height - 1;
            if (xStart < 0) xStart = 0;
            if (xEnd >= (INT32)width) xEnd = width - 1;

            for (INT32 ny = yStart; ny <= yEnd; ny++) {
                for (INT32 nx = xStart; nx <= xEnd; nx++) {
                    UINT8* p = &pixels[(ny * width + nx) * 4];
                    sumB += p[0];
                    sumG += p[1];
                    sumR += p[2];
                    sumA += p[3];
                    count++;
                }
            }

            UINT8* tp = &temp[(y * width + x) * 4];
            tp[0] = (UINT8)(sumB / count);
            tp[1] = (UINT8)(sumG / count);
            tp[2] = (UINT8)(sumR / count);
            tp[3] = (UINT8)(sumA / count);
        }
    }

    CopyMem(pixels, temp, width * height * 4);
    FreePool(temp);
}

// Blend any color over pixels with alpha (0.0-1.0)
VOID DrawBlendOverlay(
    UINT8* pixels,
    UINT32 width,
    UINT32 height,
    UINT8 blendR,
    UINT8 blendG,
    UINT8 blendB,
    float alpha
) {
    UINT8 alphaInt = (UINT8)(alpha * 255);

    for (UINT32 i = 0; i < width * height; i++) {
        UINT8* p = &pixels[i * 4];
        p[0] = (UINT8)((p[0] * (255 - alphaInt) + blendB * alphaInt) / 255); // B
        p[1] = (UINT8)((p[1] * (255 - alphaInt) + blendG * alphaInt) / 255); // G
        p[2] = (UINT8)((p[2] * (255 - alphaInt) + blendR * alphaInt) / 255); // R
    }
}

// Clamp helper
STATIC INT32 Clamp(INT32 value, INT32 min, INT32 max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Draw filled rounded rectangle with alpha blend
VOID DrawFilledRoundedRect(
    UINT8* framebuffer,
    UINT32 screenWidth,
    UINT32 screenHeight,
    INT32 x,
    INT32 y,
    UINT32 w,
    UINT32 h,
    UINT32 radius,
    UINT8 r,
    UINT8 g,
    UINT8 b,
    UINT8 a
) {
    if (radius > w/2) radius = w/2;
    if (radius > h/2) radius = h/2;

    for (INT32 py = 0; py < (INT32)h; py++) {
        INT32 screenY = y + py;
        if (screenY < 0 || screenY >= (INT32)screenHeight) continue;

        for (INT32 px = 0; px < (INT32)w; px++) {
            INT32 screenX = x + px;
            if (screenX < 0 || screenX >= (INT32)screenWidth) continue;

            INT32 dx = 0, dy = 0;
            if (px < (INT32)radius && py < (INT32)radius) {
                dx = (INT32)radius - px - 1;
                dy = (INT32)radius - py - 1;
            } else if (px >= (INT32)(w - radius) && py < (INT32)radius) {
                dx = px - (INT32)(w - radius);
                dy = (INT32)radius - py - 1;
            } else if (px < (INT32)radius && py >= (INT32)(h - radius)) {
                dx = (INT32)radius - px - 1;
                dy = py - (INT32)(h - radius);
            } else if (px >= (INT32)(w - radius) && py >= (INT32)(h - radius)) {
                dx = px - (INT32)(w - radius);
                dy = py - (INT32)(h - radius);
            }

            if ((dx != 0 || dy != 0) && (dx*dx + dy*dy > (INT32)(radius*radius))) {
                continue;
            }

            UINT8* p = &framebuffer[(screenY * screenWidth + screenX) * 4];

            UINT8 srcA = a;
            UINT8 dstR = p[2];
            UINT8 dstG = p[1];
            UINT8 dstB = p[0];

            p[2] = (UINT8)((r * srcA + dstR * (255 - srcA)) / 255);
            p[1] = (UINT8)((g * srcA + dstG * (255 - srcA)) / 255);
            p[0] = (UINT8)((b * srcA + dstB * (255 - srcA)) / 255);
        }
    }
}

// Draw smooth, large dark shadow with alpha falloff to simulate elevation
VOID DrawElevatedShadow(
    UINT8* framebuffer,
    UINT32 screenWidth,
    UINT32 screenHeight,
    UINT32 x,
    UINT32 y,
    UINT32 w,
    UINT32 h,
    UINT32 shadowRadius,
    float maxAlpha
) {
    UINT32 shadowDiameter = shadowRadius * 2;
    UINT32 shadowWidth = w + shadowDiameter * 2;
    UINT32 shadowHeight = h + shadowDiameter * 2;

    UINT8* shadowBuf = AllocatePool(shadowWidth * shadowHeight * 4);
    if (!shadowBuf) return;

    // Clear shadow buffer (transparent)
    SetMem(shadowBuf, shadowWidth * shadowHeight * 4, 0);

    // Draw opaque black rounded rect in center of shadow buffer
    DrawFilledRoundedRect(
        shadowBuf, shadowWidth, shadowHeight,
        shadowRadius, shadowRadius,
        w, h,
        shadowRadius,
        0, 0, 0, 255
    );

    // Blur shadow heavily
    DrawBoxBlur(shadowBuf, shadowWidth, shadowHeight, shadowRadius);

    // Blend shadow with framebuffer with vertical & horizontal alpha falloff
    for (UINT32 py = 0; py < shadowHeight; py++) {
        INT32 fbY = (INT32)y + (INT32)py - (INT32)shadowRadius;
        if (fbY < 0 || fbY >= (INT32)screenHeight) continue;

        for (UINT32 px = 0; px < shadowWidth; px++) {
            INT32 fbX = (INT32)x + (INT32)px - (INT32)shadowRadius;
            if (fbX < 0 || fbX >= (INT32)screenWidth) continue;

            UINT8* s = &shadowBuf[(py * shadowWidth + px) * 4];
            UINT8* f = &framebuffer[(fbY * screenWidth + fbX) * 4];

            // Calculate alpha multiplier by distance to center rect for soft edges
            float dx = 0, dy = 0;
            if (px < shadowRadius) dx = (float)(shadowRadius - px) / shadowRadius;
            else if (px >= (shadowRadius + w)) dx = (float)(px - (shadowRadius + w - 1)) / shadowRadius;

            if (py < shadowRadius) dy = (float)(shadowRadius - py) / shadowRadius;
            else if (py >= (shadowRadius + h)) dy = (float)(py - (shadowRadius + h - 1)) / shadowRadius;

            float distAlpha = 1.0f - fmaxf(dx, dy);
            if (distAlpha < 0.0f) distAlpha = 0.0f;
            if (distAlpha > 1.0f) distAlpha = 1.0f;

            float shadowAlpha = (s[3] / 255.0f) * maxAlpha * distAlpha;
            UINT8 sa = (UINT8)(shadowAlpha * 255);

            // Blend black shadow over pixel
            f[0] = (UINT8)((f[0] * (255 - sa)) / 255);
            f[1] = (UINT8)((f[1] * (255 - sa)) / 255);
            f[2] = (UINT8)((f[2] * (255 - sa)) / 255);
        }
    }

    FreePool(shadowBuf);
}

// Main function to draw the liquid glass bottom bar with improved shadow, radius and transparency
void DrawLiquidGlassBottomBar(
    UINT8* framebuffer,
    UINT32 screenWidth,
    UINT32 screenHeight,
    UINT32 barHeight,
    UINT32 margin,
    UINT8 blendR,
    UINT8 blendG,
    UINT8 blendB,
    float blendAlpha,     // 0.0 to 1.0, transparency of the bar fill
    UINT32 blurRadius,    // background blur radius behind the bar
    float shadowAlpha,    // 0.0 to 1.0, shadow max opacity
    UINT32 cornerRadius,  // rounded corner radius
    UINT32 shadowRadius   // shadow blur and size
) {
    if (margin + barHeight >= screenHeight) return;

    UINT32 barWidth = screenWidth - 2 * margin;
    UINT32 barLeft = margin;
    UINT32 barTop = screenHeight - margin - barHeight;

    // 1) Blur the background behind the bar first
    UINT8* barPixels = AllocatePool(barWidth * barHeight * 4);
    if (!barPixels) return;

    for (UINT32 y = 0; y < barHeight; y++) {
        CopyMem(
            &barPixels[y * barWidth * 4],
            &framebuffer[((barTop + y) * screenWidth + barLeft) * 4],
            barWidth * 4
        );
    }

    DrawBoxBlur(barPixels, barWidth, barHeight, blurRadius);

    // 2) Blend the dark translucent bar fill to show more background
    DrawBlendOverlay(barPixels, barWidth, barHeight, blendR, blendG, blendB, blendAlpha);

    // 3) Draw shadow underneath bar for elevation effect
    DrawElevatedShadow(
        framebuffer, screenWidth, screenHeight,
        barLeft, barTop, barWidth, barHeight,
        shadowRadius,
        shadowAlpha
    );

    // 4) Draw blurred, blended bar pixels back onto framebuffer with rounded corners mask
    for (UINT32 y = 0; y < barHeight; y++) {
        UINT8* dst = &framebuffer[((barTop + y) * screenWidth + barLeft) * 4];
        UINT8* src = &barPixels[y * barWidth * 4];

        for (UINT32 x = 0; x < barWidth; x++) {
            // Rounded corner mask
            // INT32 dx = (INT32)x - (INT32)(barWidth - cornerRadius);
            // INT32 dy = (INT32)y - (INT32)(barHeight - cornerRadius);

            BOOLEAN insideCorner = TRUE;
            // top-left corner
            if (x < (INT32)cornerRadius && y < (INT32)cornerRadius) {
                INT32 cx = (INT32)cornerRadius - x - 1;
                INT32 cy = (INT32)cornerRadius - y - 1;
                insideCorner = (cx*cx + cy*cy) <= (INT32)(cornerRadius * cornerRadius);
            }
            // top-right corner
            else if (x >= (INT32)(barWidth - cornerRadius) && y < (INT32)cornerRadius) {
                INT32 cx = x - (INT32)(barWidth - cornerRadius);
                INT32 cy = (INT32)cornerRadius - y - 1;
                insideCorner = (cx*cx + cy*cy) <= (INT32)(cornerRadius * cornerRadius);
            }
            // bottom-left corner
            else if (x < (INT32)cornerRadius && y >= (INT32)(barHeight - cornerRadius)) {
                INT32 cx = (INT32)cornerRadius - x - 1;
                INT32 cy = y - (INT32)(barHeight - cornerRadius);
                insideCorner = (cx*cx + cy*cy) <= (INT32)(cornerRadius * cornerRadius);
            }
            // bottom-right corner
            else if (x >= (INT32)(barWidth - cornerRadius) && y >= (INT32)(barHeight - cornerRadius)) {
                INT32 cx = x - (INT32)(barWidth - cornerRadius);
                INT32 cy = y - (INT32)(barHeight - cornerRadius);
                insideCorner = (cx*cx + cy*cy) <= (INT32)(cornerRadius * cornerRadius);
            }

            if (insideCorner) {
                // Copy pixel normally
                UINT8* d = &dst[x * 4];
                UINT8* s = &src[x * 4];

                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = s[3];
            }
        }
    }

    FreePool(barPixels);
}

void TestUi() {
DrawLiquidGlassBottomBar(
    (UINT8*)OsKernelData->BootFb.BaseAddress,
    OsKernelData->BootFb.hRes,
    OsKernelData->BootFb.vRes,
    75,       // bar height px (bigger bar)
    38,       // margin px
    35, 35, 35, // dark gray blend color
    0.2f,    // more transparent fill (show more bg)
    15,       // bigger blur radius for strong blur behind bar
    0.1f,     // stronger shadow alpha (max 1.0)
    28,       // bigger corner radius
    5        // bigger shadow radius/blur for stronger shadow
);


}