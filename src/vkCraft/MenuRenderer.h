#pragma once

/**
 * MenuRenderer
 * ============
 * Renders the Minecraft-style main menu and pause menu directly
 * inside the GLFW window using raw OpenGL 2 immediate mode so that
 * we do NOT depend on Vulkan being initialised yet (the menu runs
 * BEFORE VkCraft::initialize()).  The window is created with
 * GLFW_CLIENT_API = GLFW_OPENGL_API for the menu phase; VkCraft
 * then recreates it with GLFW_NO_API for Vulkan.
 *
 * If you prefer a pure-software framebuffer approach, replace the
 * GL calls with a pixel-buffer written via glfwSetWindowSize /
 * stbi_write, but the OpenGL approach is simpler and zero-dep.
 *
 * Font: a compact 8x8 bitmap font is baked in as a static array.
 * Buttons: axis-aligned coloured quads with centred text.
 * Mouse: standard GLFW cursor-pos + mouse-button callbacks.
 *
 * No external GUI library is needed.
 */

#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <random>
#include <chrono>
#include "WorldSettings.h"

// ---------------------------------------------------------------------------
// Minimal 8x8 bitmap font (ASCII 32-127, each char = 8 bytes = 8 rows of 8 bits)
// Source: public-domain font by Marcel Sondaar / IBM
// ---------------------------------------------------------------------------
static const uint8_t FONT8x8[96][8] = {
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 32 space
  {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // 33 !
  {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // 34 "
  {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // 35 #
  {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // 36 $
  {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // 37 %
  {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // 38 &
  {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // 39 '
  {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // 40 (
  {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // 41 )
  {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // 42 *
  {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // 43 +
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // 44 ,
  {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // 45 -
  {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // 46 .
  {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // 47 /
  {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 48 0
  {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 49 1
  {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 50 2
  {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 51 3
  {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 52 4
  {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 53 5
  {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 54 6
  {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 55 7
  {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 56 8
  {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 57 9
  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // 58 :
  {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // 59 ;
  {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // 60 <
  {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // 61 =
  {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // 62 >
  {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // 63 ?
  {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // 64 @
  {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // 65 A
  {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // 66 B
  {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // 67 C
  {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // 68 D
  {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // 69 E
  {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // 70 F
  {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // 71 G
  {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // 72 H
  {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 73 I
  {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // 74 J
  {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // 75 K
  {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // 76 L
  {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // 77 M
  {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // 78 N
  {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // 79 O
  {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // 80 P
  {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // 81 Q
  {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // 82 R
  {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // 83 S
  {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 84 T
  {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // 85 U
  {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 86 V
  {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // 87 W
  {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // 88 X
  {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // 89 Y
  {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // 90 Z
  {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, // 91 [
  {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // 92 backslash
  {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, // 93 ]
  {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // 94 ^
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // 95 _
  {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // 96 `
  {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // 97 a
  {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // 98 b
  {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // 99 c
  {0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0x00}, // 100 d
  {0x00,0x00,0x1E,0x33,0x3f,0x03,0x1E,0x00}, // 101 e
  {0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0x00}, // 102 f
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // 103 g
  {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // 104 h
  {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // 105 i
  {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // 106 j
  {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // 107 k
  {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // 108 l
  {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // 109 m
  {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // 110 n
  {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // 111 o
  {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // 112 p
  {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // 113 q
  {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // 114 r
  {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // 115 s
  {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // 116 t
  {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // 117 u
  {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // 118 v
  {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // 119 w
  {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // 120 x
  {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // 121 y
  {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // 122 z
  {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // 123 {
  {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // 124 |
  {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // 125 }
  {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // 126 ~
  {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}  // 127 DEL (filled block)
};

// ---------------------------------------------------------------------------
// MenuState
// ---------------------------------------------------------------------------
enum class MenuState {
    MAIN,
    SINGLEPLAYER,   // world create
    OPTIONS,
    IN_GAME,
    PAUSE
};

// ---------------------------------------------------------------------------
// MenuRenderer
// Draws inside a GLFW window with an OpenGL 2 context.
// ---------------------------------------------------------------------------
class MenuRenderer
{
public:
    MenuState    state    = MenuState::MAIN;
    WorldSettings settings;

    bool isInGame() const { return state == MenuState::IN_GAME; }

    // -----------------------------------------------------------------------
    // init: call once after creating the GL window
    // -----------------------------------------------------------------------
    void init(GLFWwindow* w)
    {
        window = w;
        glfwSetWindowUserPointer(w, this);
        glfwSetCursorPosCallback(w, [](GLFWwindow* ww, double x, double y){
            auto* m = static_cast<MenuRenderer*>(glfwGetWindowUserPointer(ww));
            m->mx = (float)x; m->my = (float)y;
        });
        glfwSetMouseButtonCallback(w, [](GLFWwindow* ww, int btn, int action, int){
            if (btn == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
                static_cast<MenuRenderer*>(glfwGetWindowUserPointer(ww))->clicked = true;
        });
        glfwSetCharCallback(w, [](GLFWwindow* ww, unsigned int cp){
            static_cast<MenuRenderer*>(glfwGetWindowUserPointer(ww))->onChar(cp);
        });
        glfwSetKeyCallback(w, [](GLFWwindow* ww, int key, int, int action, int){
            static_cast<MenuRenderer*>(glfwGetWindowUserPointer(ww))->onKey(key, action);
        });
        settings.randomizeSeed();
        seedStr = std::to_string(settings.seed);
    }

    // -----------------------------------------------------------------------
    // renderFrame: call every iteration of the menu loop
    // -----------------------------------------------------------------------
    void renderFrame()
    {
        int W, H;
        glfwGetFramebufferSize(window, &W, &H);
        glViewport(0, 0, W, H);

        switch (state) {
            case MenuState::MAIN:         drawMain(W,H);        break;
            case MenuState::SINGLEPLAYER: drawSingleplayer(W,H);break;
            case MenuState::OPTIONS:      drawOptions(W,H);     break;
            case MenuState::PAUSE:        drawPause(W,H);       break;
            default: break;
        }

        glfwSwapBuffers(window);
        clicked = false;
    }

private:
    GLFWwindow* window = nullptr;
    float mx = 0, my = 0;
    bool  clicked = false;

    // text input
    std::string nameStr   = "New World";
    std::string seedStr;
    bool        focusName = false;
    bool        focusSeed = false;

    // -----------------------------------------------------------------------
    // 2-D projection helpers
    // -----------------------------------------------------------------------
    void begin2D(int W, int H)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, W, H, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    // Fill a rectangle
    void fillRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.f)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2f(x,   y);   glVertex2f(x+w, y);
        glVertex2f(x+w, y+h); glVertex2f(x,   y+h);
        glEnd();
    }

    // Draw a single glyph at pixel position (px,py), scale s
    void drawChar(int c, float px, float py, float s,
                  float r, float g, float b)
    {
        if (c < 32 || c > 127) c = 63; // '?'
        const uint8_t* bmp = FONT8x8[c - 32];
        glColor3f(r, g, b);
        glBegin(GL_QUADS);
        for (int row = 0; row < 8; row++)
            for (int col = 0; col < 8; col++)
                if (bmp[row] & (0x80 >> col)) {
                    float x0 = px + col * s, y0 = py + row * s;
                    glVertex2f(x0,   y0);   glVertex2f(x0+s, y0);
                    glVertex2f(x0+s, y0+s); glVertex2f(x0,   y0+s);
                }
        glEnd();
    }

    // Draw a string; returns total width in pixels
    float drawText(const std::string& txt, float px, float py, float s,
                   float r = 1, float g = 1, float b = 1)
    {
        float x = px;
        for (char c : txt) { drawChar((unsigned char)c, x, py, s, r, g, b); x += 8*s + s; }
        return x - px;
    }

    float textWidth(const std::string& t, float s) { return t.size() * (8*s + s); }

    // -----------------------------------------------------------------------
    // Button helper -- returns true when clicked this frame
    // -----------------------------------------------------------------------
    bool button(const std::string& label, float x, float y, float w, float h,
                float s = 2.f, bool enabled = true)
    {
        bool hover = enabled && mx>=x && mx<=x+w && my>=y && my<=y+h;
        float br = hover ? 0.73f : 0.55f;
        float bg = hover ? 0.73f : 0.55f;
        float bb = hover ? 0.73f : 0.55f;
        // Minecraft-style: dark stone border, lighter center
        fillRect(x,   y,   w,   h,   0.20f,0.20f,0.20f); // dark border
        fillRect(x+2, y+2, w-4, h-4, br, bg, bb);        // fill
        // Text centered
        float tw = textWidth(label, s);
        float tx = x + (w - tw) * 0.5f;
        float ty = y + (h - 8*s) * 0.5f;
        if (enabled)
            drawText(label, tx, ty, s);
        else
            drawText(label, tx, ty, s, 0.4f, 0.4f, 0.4f);
        return hover && clicked;
    }

    // Text input field
    bool inputField(const std::string& label, std::string& value,
                    bool& focused, float x, float y, float w, float h)
    {
        bool hover = mx>=x && mx<=x+w && my>=y && my<=y+h;
        if (hover && clicked) focused = true;
        fillRect(x,   y,   w,   h,   0.10f,0.10f,0.10f);
        fillRect(x+2, y+2, w-4, h-4, focused ? 0.18f : 0.12f,
                                     focused ? 0.18f : 0.12f,
                                     focused ? 0.18f : 0.12f);
        // Label above
        drawText(label, x, y-18, 1.5f, 0.85f, 0.85f, 0.85f);
        // Value
        std::string disp = value + (focused ? "_" : "");
        drawText(disp, x+6, y+(h-12)/2, 1.5f);
        return false;
    }

    // -----------------------------------------------------------------------
    // Screen: MAIN
    // -----------------------------------------------------------------------
    void drawMain(int W, int H)
    {
        // Background: dark gradient
        glClearColor(0.05f, 0.07f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        begin2D(W, H);

        // Panorama hint: repeating dark-blue tiled bg (no texture, just solid)
        fillRect(0, 0, W, H, 0.05f, 0.07f, 0.15f);

        // Title: "FCraft" large yellow
        float ts = 5.f;
        float tw = textWidth("FCraft", ts);
        drawText("FCraft", (W-tw)*0.5f, H*0.12f, ts, 1.f, 0.84f, 0.f);
        // Drop shadow
        drawText("FCraft", (W-tw)*0.5f+3, H*0.12f+3, ts, 0.3f, 0.25f, 0.f);
        drawText("FCraft", (W-tw)*0.5f, H*0.12f, ts, 1.f, 0.84f, 0.f);

        // Version tag
        drawText("FCraft Alpha 0.1", 4, H-16, 1.3f, 0.6f, 0.6f, 0.6f);
        float cw2 = textWidth("Copyright Minecraft425HD", 1.3f);
        drawText("Copyright Minecraft425HD", W-cw2-4, H-16, 1.3f, 0.6f,0.6f,0.6f);

        float bw = std::min((float)W*0.6f, 400.f);
        float bh = 40.f;
        float bx = (W - bw) * 0.5f;
        float by = H * 0.38f;

        if (button("Einzelspieler",   bx, by,        bw,   bh)) { state = MenuState::SINGLEPLAYER; }
        if (button("Mehrspieler",     bx, by+50,     bw,   bh, 2.f, false)) {} // grayed
        float hw = (bw-4)*0.5f;
        if (button("Optionen",        bx,      by+100, hw, bh)) { state = MenuState::OPTIONS; }
        if (button("Beenden",         bx+hw+4, by+100, hw, bh)) { glfwSetWindowShouldClose(window, 1); }
    }

    // -----------------------------------------------------------------------
    // Screen: SINGLEPLAYER (world create)
    // -----------------------------------------------------------------------
    void drawSingleplayer(int W, int H)
    {
        glClearColor(0.05f, 0.07f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        begin2D(W, H);
        fillRect(0, 0, W, H, 0.05f, 0.07f, 0.15f);

        float ts = 2.5f;
        float tw = textWidth("Welt erstellen", ts);
        drawText("Welt erstellen", (W-tw)*0.5f, 30, ts);

        float fw = std::min((float)W*0.6f, 400.f);
        float fx = (W-fw)*0.5f;

        inputField("Weltname",  nameStr,  focusName, fx, 100, fw, 36);
        inputField("Seed (leer = zufaellig)", seedStr, focusSeed, fx, 180, fw, 36);

        // Render distance info
        std::string rdStr = "Sichtweite: " + std::to_string(settings.renderDistance) + " Chunks";
        float rw = textWidth(rdStr, 1.5f);
        drawText(rdStr, (W-rw)*0.5f, 240, 1.5f, 0.8f,0.8f,0.8f);

        float bw = fw;
        float bx = fx;
        if (button("Welt erstellen", bx, 300, bw, 40)) {
            settings.worldName = nameStr.empty() ? "New World" : nameStr;
            if (seedStr.empty()) {
                settings.randomizeSeed();
            } else {
                try { settings.seed = static_cast<uint32_t>(std::stoul(seedStr)); }
                catch (...) { settings.randomizeSeed(); }
            }
            state = MenuState::IN_GAME;
        }
        float hw = (bw-4)*0.5f;
        if (button("Zurueck", bx, 350, hw, 36)) { state = MenuState::MAIN; focusName=false; focusSeed=false; }
    }

    // -----------------------------------------------------------------------
    // Screen: OPTIONS
    // -----------------------------------------------------------------------
    void drawOptions(int W, int H)
    {
        glClearColor(0.05f, 0.07f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        begin2D(W, H);
        fillRect(0, 0, W, H, 0.05f, 0.07f, 0.15f);

        float ts = 2.5f;
        float tw = textWidth("Optionen", ts);
        drawText("Optionen", (W-tw)*0.5f, 30, ts);

        float bw = std::min((float)W*0.6f, 400.f);
        float bx = (W-bw)*0.5f;
        float hw = (bw-4)*0.5f;
        float by = 100;

        // Render distance
        std::string rd = "Sichtweite: " + std::to_string(settings.renderDistance);
        if (button(rd,  bx, by, bw, 36)) {}
        if (button("-", bx,          by+40, hw, 32)) settings.renderDistance = std::max(1,  settings.renderDistance-1);
        if (button("+", bx+hw+4,     by+40, hw, 32)) settings.renderDistance = std::min(32, settings.renderDistance+1);

        // FOV
        std::string fov = "Sichtfeld: " + std::to_string(settings.fovDegrees);
        if (button(fov, bx, by+90, bw, 36)) {}
        if (button("-", bx,      by+130, hw, 32)) settings.fovDegrees = std::max(60,  settings.fovDegrees-5);
        if (button("+", bx+hw+4, by+130, hw, 32)) settings.fovDegrees = std::min(110, settings.fovDegrees+5);

        // Mouse sensitivity
        std::ostringstream ss;
        ss.precision(1); ss << std::fixed << settings.mouseSensitivity;
        std::string ms = "Maus: " + ss.str();
        if (button(ms,  bx, by+180, bw, 36)) {}
        if (button("-", bx,      by+220, hw, 32)) settings.mouseSensitivity = std::max(0.1f, settings.mouseSensitivity-0.1f);
        if (button("+", bx+hw+4, by+220, hw, 32)) settings.mouseSensitivity = std::min(5.0f, settings.mouseSensitivity+0.1f);

        if (button("Zurueck", bx, by+270, bw, 38)) { state = MenuState::MAIN; }
    }

    // -----------------------------------------------------------------------
    // Screen: PAUSE (overlay over running game -- called from VkCraft loop)
    // -----------------------------------------------------------------------
public:
    void drawPause(int W, int H)
    {
        begin2D(W, H);
        // Semi-transparent dark overlay
        fillRect(0, 0, W, H, 0.f, 0.f, 0.f, 0.55f);

        float ts = 2.5f;
        float tw = textWidth("Pause", ts);
        drawText("Pause", (W-tw)*0.5f, H*0.25f, ts);

        float bw = std::min((float)W*0.5f, 300.f);
        float bx = (W-bw)*0.5f;
        float by = H*0.38f;

        if (button("Weiter spielen",       bx, by,    bw, 40)) { state = MenuState::IN_GAME; }
        if (button("Optionen",             bx, by+50, bw, 40)) { state = MenuState::OPTIONS; }
        if (button("Zur Titelseite",       bx, by+100,bw, 40)) { state = MenuState::MAIN;    }
    }

    // -----------------------------------------------------------------------
    // Keyboard / character input
    // -----------------------------------------------------------------------
private:
    void onChar(unsigned int cp)
    {
        if (cp > 127) return;
        char c = (char)cp;
        if (focusName && state == MenuState::SINGLEPLAYER) nameStr += c;
        if (focusSeed && state == MenuState::SINGLEPLAYER) {
            if (c >= '0' && c <= '9') seedStr += c;
        }
    }

    void onKey(int key, int action)
    {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        if (key == GLFW_KEY_BACKSPACE) {
            if (focusName && !nameStr.empty()) nameStr.pop_back();
            if (focusSeed && !seedStr.empty()) seedStr.pop_back();
        }
        if (key == GLFW_KEY_ESCAPE) {
            if (state == MenuState::SINGLEPLAYER || state == MenuState::OPTIONS)
                state = MenuState::MAIN;
        }
        if (key == GLFW_KEY_TAB && state == MenuState::SINGLEPLAYER) {
            focusName = !focusName;
            focusSeed = !focusName;
        }
    }
};
