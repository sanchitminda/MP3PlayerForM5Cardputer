#ifndef SPLASH_SCREEN_H
#define SPLASH_SCREEN_H

#include <M5Cardputer.h>

// Forward declaration for BootMenu
class BootMenu;

// ==========================================
// SPLASH SCREEN ANIMATION - 8-BIT NES STYLE
// ==========================================
class SplashScreen {
public:
    // NES-style color palette (limited colors like real NES)
    static const uint16_t NES_BLACK = 0x0000;
    static const uint16_t NES_WHITE = 0xFFFF;
    static const uint16_t NES_SKY = 0x5DBF;      // Light blue sky
    static const uint16_t NES_SKIN = 0xFCC0;     // Peach skin
    static const uint16_t NES_BROWN = 0x8200;    // Brown
    static const uint16_t NES_RED = 0xF800;      // Bright red
    static const uint16_t NES_BLUE = 0x001F;     // Blue
    static const uint16_t NES_GREEN = 0x07C0;    // Green
    static const uint16_t NES_DKGREEN = 0x03C0;  // Dark green
    static const uint16_t NES_YELLOW = 0xFFE0;   // Yellow
    static const uint16_t NES_ORANGE = 0xFC00;   // Orange
    static const uint16_t NES_GRAY = 0x8410;     // Gray
    static const uint16_t NES_DKGRAY = 0x4208;   // Dark gray
    static const uint16_t NES_CYAN = 0x07FF;     // Cyan
    static const uint16_t NES_MAGENTA = 0xF81F;  // Magenta
    
    // Pixel size for chunky 8-bit look (2x2 pixels)
    static const int PX = 2;
    
    // Sprite for double buffering
    static LGFX_Sprite* splashSprite;
    
    // Draw a single "big pixel" (NES style chunky pixel) - direct to display
    static void px(int x, int y, uint16_t c) {
        M5Cardputer.Display.fillRect(x * PX, y * PX, PX, PX, c);
    }
    
    // Draw a single "big pixel" to sprite buffer
    static void spx(int x, int y, uint16_t c) {
        splashSprite->fillRect(x * PX, y * PX, PX, PX, c);
    }
    
    // Draw 8-bit style character sprite to display
    static void drawBoy8bit(int x, int y, int frame) {
        M5Cardputer.Display.fillRect(x * PX - PX, y * PX, 18 * PX, 26 * PX, NES_SKY);
        
        int f = frame % 4;
        
        // Hair
        for(int i = 2; i < 7; i++) px(x+i, y, NES_DKGRAY);
        for(int i = 1; i < 8; i++) px(x+i, y+1, NES_DKGRAY);
        for(int i = 1; i < 8; i++) px(x+i, y+2, NES_DKGRAY);
        
        // Face
        for(int i = 1; i < 8; i++) px(x+i, y+3, NES_SKIN);
        px(x+1, y+4, NES_SKIN); px(x+2, y+4, NES_BLACK); px(x+3, y+4, NES_SKIN);
        px(x+4, y+4, NES_SKIN); px(x+5, y+4, NES_BLACK); px(x+6, y+4, NES_SKIN);
        px(x+7, y+4, NES_SKIN);
        for(int i = 1; i < 8; i++) px(x+i, y+5, NES_SKIN);
        px(x+1, y+6, NES_SKIN); px(x+2, y+6, NES_SKIN); px(x+3, y+6, NES_BLACK);
        px(x+4, y+6, NES_BLACK); px(x+5, y+6, NES_SKIN); px(x+6, y+6, NES_SKIN);
        px(x+7, y+6, NES_SKIN);
        
        // Headphones
        px(x, y+3, NES_RED); px(x, y+4, NES_RED); px(x, y+5, NES_RED);
        px(x+8, y+3, NES_RED); px(x+8, y+4, NES_RED); px(x+8, y+5, NES_RED);
        for(int i = 1; i < 8; i++) px(x+i, y-1, NES_RED);
        
        // Shirt
        for(int j = 7; j <= 11; j++) {
            for(int i = 2; i < 7; i++) px(x+i, y+j, NES_BLUE);
        }
        
        // Arms
        int armOffset = (f < 2) ? 0 : 1;
        px(x+1, y+7+armOffset, NES_SKIN); px(x+1, y+8+armOffset, NES_SKIN);
        px(x+1, y+9+armOffset, NES_SKIN);
        px(x+7, y+7+(1-armOffset), NES_SKIN); px(x+7, y+8+(1-armOffset), NES_SKIN);
        px(x+7, y+9+(1-armOffset), NES_SKIN);
        
        // Pants
        for(int j = 12; j <= 14; j++) {
            for(int i = 2; i < 7; i++) px(x+i, y+j, NES_DKGRAY);
        }
        
        // Legs
        int legL = 0, legR = 0;
        switch(f) {
            case 0: legL = 0; legR = 2; break;
            case 1: legL = 1; legR = 1; break;
            case 2: legL = 2; legR = 0; break;
            case 3: legL = 1; legR = 1; break;
        }
        px(x+2+legL, y+15, NES_DKGRAY); px(x+2+legL, y+16, NES_DKGRAY);
        px(x+2+legL, y+17, NES_BROWN); px(x+3+legL, y+17, NES_BROWN);
        px(x+5-legR, y+15, NES_DKGRAY); px(x+5-legR, y+16, NES_DKGRAY);
        px(x+5-legR, y+17, NES_BROWN); px(x+6-legR, y+17, NES_BROWN);
    }
    
    // Draw 8-bit boy to sprite buffer
    static void drawBoy8bitSprite(int x, int y, int frame) {
        int f = frame % 4;
        
        // Hair
        for(int i = 2; i < 7; i++) spx(x+i, y, NES_DKGRAY);
        for(int i = 1; i < 8; i++) spx(x+i, y+1, NES_DKGRAY);
        for(int i = 1; i < 8; i++) spx(x+i, y+2, NES_DKGRAY);
        
        // Face
        for(int i = 1; i < 8; i++) spx(x+i, y+3, NES_SKIN);
        spx(x+1, y+4, NES_SKIN); spx(x+2, y+4, NES_BLACK); spx(x+3, y+4, NES_SKIN);
        spx(x+4, y+4, NES_SKIN); spx(x+5, y+4, NES_BLACK); spx(x+6, y+4, NES_SKIN);
        spx(x+7, y+4, NES_SKIN);
        for(int i = 1; i < 8; i++) spx(x+i, y+5, NES_SKIN);
        spx(x+1, y+6, NES_SKIN); spx(x+2, y+6, NES_SKIN); spx(x+3, y+6, NES_BLACK);
        spx(x+4, y+6, NES_BLACK); spx(x+5, y+6, NES_SKIN); spx(x+6, y+6, NES_SKIN);
        spx(x+7, y+6, NES_SKIN);
        
        // Headphones
        spx(x, y+3, NES_RED); spx(x, y+4, NES_RED); spx(x, y+5, NES_RED);
        spx(x+8, y+3, NES_RED); spx(x+8, y+4, NES_RED); spx(x+8, y+5, NES_RED);
        for(int i = 1; i < 8; i++) spx(x+i, y-1, NES_RED);
        
        // Shirt
        for(int j = 7; j <= 11; j++) {
            for(int i = 2; i < 7; i++) spx(x+i, y+j, NES_BLUE);
        }
        
        // Arms
        int armOffset = (f < 2) ? 0 : 1;
        spx(x+1, y+7+armOffset, NES_SKIN); spx(x+1, y+8+armOffset, NES_SKIN);
        spx(x+1, y+9+armOffset, NES_SKIN);
        spx(x+7, y+7+(1-armOffset), NES_SKIN); spx(x+7, y+8+(1-armOffset), NES_SKIN);
        spx(x+7, y+9+(1-armOffset), NES_SKIN);
        
        // Pants
        for(int j = 12; j <= 14; j++) {
            for(int i = 2; i < 7; i++) spx(x+i, y+j, NES_DKGRAY);
        }
        
        // Legs
        int legL = 0, legR = 0;
        switch(f) {
            case 0: legL = 0; legR = 2; break;
            case 1: legL = 1; legR = 1; break;
            case 2: legL = 2; legR = 0; break;
            case 3: legL = 1; legR = 1; break;
        }
        spx(x+2+legL, y+15, NES_DKGRAY); spx(x+2+legL, y+16, NES_DKGRAY);
        spx(x+2+legL, y+17, NES_BROWN); spx(x+3+legL, y+17, NES_BROWN);
        spx(x+5-legR, y+15, NES_DKGRAY); spx(x+5-legR, y+16, NES_DKGRAY);
        spx(x+5-legR, y+17, NES_BROWN); spx(x+6-legR, y+17, NES_BROWN);
    }
    
    // 8-bit music note to sprite
    static void drawNote8bitSprite(int x, int y, uint16_t color) {
        spx(x, y+2, color); spx(x+1, y+2, color);
        spx(x, y+3, color); spx(x+1, y+3, color);
        spx(x+1, y, color); spx(x+1, y+1, color);
        spx(x+2, y, color); spx(x+2, y+1, color);
    }
    
    // 8-bit double note to sprite
    static void drawDoubleNote8bitSprite(int x, int y, uint16_t color) {
        spx(x, y+2, color); spx(x+1, y+2, color);
        spx(x+3, y+2, color); spx(x+4, y+2, color);
        spx(x+1, y, color); spx(x+1, y+1, color);
        spx(x+4, y, color); spx(x+4, y+1, color);
        spx(x+1, y, color); spx(x+2, y, color); spx(x+3, y, color); spx(x+4, y, color);
    }
    
    // 8-bit house to sprite
    static void drawHouse8bitSprite(int hx, int hy) {
        // Roof
        for(int i = 0; i < 8; i++) {
            for(int j = 8-i; j <= 8+i; j++) {
                spx(hx+j, hy+i, NES_RED);
            }
        }
        // Walls
        for(int j = 8; j < 16; j++) {
            for(int i = 1; i < 16; i++) {
                spx(hx+i, hy+j, NES_GRAY);
            }
        }
        // Door
        for(int j = 10; j < 16; j++) {
            spx(hx+7, hy+j, NES_BROWN); spx(hx+8, hy+j, NES_BROWN); spx(hx+9, hy+j, NES_BROWN);
        }
        spx(hx+9, hy+13, NES_YELLOW);
        // Windows
        spx(hx+3, hy+10, NES_CYAN); spx(hx+4, hy+10, NES_CYAN);
        spx(hx+3, hy+11, NES_CYAN); spx(hx+4, hy+11, NES_CYAN);
        spx(hx+12, hy+10, NES_CYAN); spx(hx+13, hy+10, NES_CYAN);
        spx(hx+12, hy+11, NES_CYAN); spx(hx+13, hy+11, NES_CYAN);
        // Chimney
        spx(hx+12, hy+2, NES_BROWN); spx(hx+13, hy+2, NES_BROWN);
        spx(hx+12, hy+3, NES_BROWN); spx(hx+13, hy+3, NES_BROWN);
        spx(hx+12, hy+4, NES_BROWN); spx(hx+13, hy+4, NES_BROWN);
    }
    
    // 8-bit tree to sprite
    static void drawTree8bitSprite(int tx, int ty) {
        // Trunk
        spx(tx+2, ty+6, NES_BROWN); spx(tx+3, ty+6, NES_BROWN);
        spx(tx+2, ty+7, NES_BROWN); spx(tx+3, ty+7, NES_BROWN);
        spx(tx+2, ty+8, NES_BROWN); spx(tx+3, ty+8, NES_BROWN);
        // Leaves
        for(int i = 0; i < 6; i++) spx(tx+i, ty+5, NES_GREEN);
        for(int i = 0; i < 6; i++) spx(tx+i, ty+4, NES_GREEN);
        for(int i = 1; i < 5; i++) spx(tx+i, ty+3, NES_GREEN);
        for(int i = 1; i < 5; i++) spx(tx+i, ty+2, NES_DKGREEN);
        spx(tx+2, ty+1, NES_DKGREEN); spx(tx+3, ty+1, NES_DKGREEN);
        spx(tx+2, ty, NES_GREEN); spx(tx+3, ty, NES_GREEN);
    }
    
    // 8-bit cloud to sprite
    static void drawCloud8bitSprite(int cx, int cy) {
        for(int i = 1; i < 6; i++) spx(cx+i, cy, NES_WHITE);
        for(int i = 0; i < 7; i++) spx(cx+i, cy+1, NES_WHITE);
        for(int i = 1; i < 6; i++) spx(cx+i, cy+2, NES_WHITE);
    }
    
    // 8-bit sun to sprite with animation
    static void drawSun8bitSprite(int sx, int sy, int frame) {
        for(int j = 0; j < 4; j++) {
            for(int i = 0; i < 4; i++) {
                spx(sx+i, sy+j, NES_YELLOW);
            }
        }
        int rayPhase = (frame / 5) % 2;
        if(rayPhase == 0) {
            spx(sx+1, sy-1, NES_YELLOW); spx(sx+2, sy-1, NES_YELLOW);
            spx(sx-1, sy+1, NES_YELLOW); spx(sx+4, sy+1, NES_YELLOW);
            spx(sx-1, sy+2, NES_YELLOW); spx(sx+4, sy+2, NES_YELLOW);
            spx(sx+1, sy+4, NES_YELLOW); spx(sx+2, sy+4, NES_YELLOW);
        } else {
            spx(sx-1, sy-1, NES_ORANGE); spx(sx+4, sy-1, NES_ORANGE);
            spx(sx+1, sy-1, NES_YELLOW); spx(sx+2, sy-1, NES_YELLOW);
            spx(sx-1, sy+4, NES_ORANGE); spx(sx+4, sy+4, NES_ORANGE);
            spx(sx+1, sy+4, NES_YELLOW); spx(sx+2, sy+4, NES_YELLOW);
        }
    }
    
    // 8-bit ground with grass to sprite
    static void drawGround8bitSprite() {
        int groundY = 52;
        // Grass
        for(int y = groundY; y < groundY + 5; y++) {
            for(int x = 0; x < 120; x++) {
                spx(x, y, NES_GREEN);
            }
        }
        // Grass detail
        for(int x = 0; x < 120; x += 5) {
            spx(x, groundY, NES_DKGREEN);
            spx(x+2, groundY, NES_DKGREEN);
        }
        // Road
        for(int y = groundY + 5; y < 68; y++) {
            for(int x = 0; x < 120; x++) {
                spx(x, y, NES_DKGRAY);
            }
        }
        // Road stripes
        for(int x = 0; x < 120; x += 10) {
            for(int i = 0; i < 5; i++) {
                spx(x+i, groundY + 8, NES_YELLOW);
            }
        }
    }
    
    // Show the splash screen animation
    // Returns true if animation completed, false if boot menu took over
    static bool show(int userBrightness);
};

// Static member definition
LGFX_Sprite* SplashScreen::splashSprite = nullptr;

// Implementation of show() - needs BootMenu, so defined after including main code
bool SplashScreen::show(int userBrightness) {
    // Create full-screen sprite for double buffering (no flicker)
    LGFX_Sprite frameBuffer(&M5Cardputer.Display);
    frameBuffer.setColorDepth(16);
    frameBuffer.createSprite(240, 135);
    splashSprite = &frameBuffer;
    
    // Animation variables
    int boyX = 35;  // Fixed position - boy does not move
    int boyY = 32;
    
    // Note tracking
    struct Note8 {
        int x, y;
        int type;
        uint16_t color;
        int floatY;
        bool active;
    };
    Note8 notes[3] = {{0,0,0,0,0,false},{0,0,0,0,0,false},{0,0,0,0,0,false}};
    uint16_t noteColors[] = {NES_CYAN, NES_MAGENTA, NES_YELLOW, NES_RED};
    
    // Main animation loop - 60 frames
    for(int frame = 0; frame < 60; frame++) {
        // Clear entire frame buffer with sky color
        frameBuffer.fillScreen(NES_SKY);
        
        // Draw all static elements to buffer
        drawGround8bitSprite();
        drawSun8bitSprite(100, 5, frame);
        drawCloud8bitSprite(10, 8);
        drawCloud8bitSprite(60, 12);
        drawTree8bitSprite(5, 40);
        drawTree8bitSprite(70, 42);
        drawHouse8bitSprite(85, 30);
        
        // Draw the walking boy (stationary position, only animates walk cycle)
        drawBoy8bitSprite(boyX, boyY, frame);
        
        // Spawn new note every 12 frames
        if(frame % 12 == 0) {
            for(int n = 0; n < 3; n++) {
                if(!notes[n].active) {
                    notes[n].x = boyX + 8;
                    notes[n].y = boyY - 2;
                    notes[n].type = random(0, 2);
                    notes[n].color = noteColors[random(0, 4)];
                    notes[n].floatY = 0;
                    notes[n].active = true;
                    break;
                }
            }
        }
        
        // Update and draw notes
        for(int n = 0; n < 3; n++) {
            if(notes[n].active) {
                int noteDrawX = notes[n].x + ((notes[n].floatY / 3) % 3) - 1;
                int noteDrawY = notes[n].y - notes[n].floatY;
                
                if(noteDrawY > 5 && noteDrawY < 50) {
                    if(notes[n].type == 0) {
                        drawNote8bitSprite(noteDrawX, noteDrawY, notes[n].color);
                    } else {
                        drawDoubleNote8bitSprite(noteDrawX, noteDrawY, notes[n].color);
                    }
                }
                
                notes[n].floatY += 1;
                if(notes[n].floatY > 25) {
                    notes[n].active = false;
                }
            }
        }
        
        // Title bar
        frameBuffer.fillRect(0, 0, 240, 22, NES_BLACK);
        uint16_t colors[] = {NES_CYAN, NES_WHITE, NES_YELLOW, NES_MAGENTA};
        uint16_t titleColor = colors[(frame / 10) % 4];
        frameBuffer.setFont(&fonts::Font0);
        frameBuffer.setTextColor(titleColor);
        frameBuffer.setCursor(45, 7);
        frameBuffer.print("SAM MUSIC PLAYER");
        spx(10, 3, NES_CYAN); spx(11, 4, NES_MAGENTA); spx(10, 5, NES_YELLOW);
        spx(108, 3, NES_CYAN); spx(109, 4, NES_MAGENTA); spx(108, 5, NES_YELLOW);
        
        // Loading bar
        frameBuffer.fillRect(75, 118, 90, 16, NES_BLACK);
        int barW = 80;
        int progress = (frame * barW) / 60;
        frameBuffer.drawRect(80, 118, barW, 4, NES_WHITE);
        if(progress > 2) frameBuffer.fillRect(81, 119, progress - 2, 2, NES_CYAN);
        
        frameBuffer.setTextColor(NES_WHITE);
        frameBuffer.setCursor(80, 124);
        frameBuffer.print("LOADING");
        int dots = (frame / 8) % 4;
        for(int d = 0; d < dots; d++) frameBuffer.print(".");
        
        // Press any key hint (blinking)
        if((frame / 15) % 2 == 0) {
            frameBuffer.setTextColor(NES_DKGRAY);
            frameBuffer.setCursor(50, 108);
            frameBuffer.print("PRESS ANY KEY...");
        }
        
        // Push entire frame to display at once (no flicker!)
        frameBuffer.pushSprite(0, 0);
        
        delay(50);
        
        // Check for key press
        M5Cardputer.update();
        if(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            frameBuffer.deleteSprite();
            // Return false to indicate boot menu should be shown
            return false;
        }
    }
    
    // Delete sprite buffer
    frameBuffer.deleteSprite();
    
    // End animation - flash effect (NES style)
    for(int i = 0; i < 3; i++) {
        M5Cardputer.Display.fillScreen(NES_WHITE);
        delay(50);
        M5Cardputer.Display.fillScreen(NES_BLACK);
        delay(50);
    }
    
    M5Cardputer.Display.setBrightness(userBrightness);
    return true;
}

#endif // SPLASH_SCREEN_H