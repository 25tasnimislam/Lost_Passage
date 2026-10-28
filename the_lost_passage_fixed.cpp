/*******************************************************************************************
*
*   THE LOST PASSAGE: VOID RUNNER - Hangar & Modern Home UI Edition (Raylib C++17)
*
*   Updates:
*     - Clean, compact button-based Home UI with interactive hover & selection states
*     - 3 Distinct Spaceship Classes:
*         [1] VALKYRIE-IX (Cyan Interceptor - Balanced & Agile)
*         [2] PHANTOM-STRIKER (Emerald Raider - High Speed & Energy)
*         [3] DREADNOUGHT-V (Amber Cruiser - Heavy Hull & Thick Armor)
*     - In-Menu Ship Selection Hangar with interactive previews
*     - Enhanced Kinetic Shield ([E] / MMB) and Nanite Damage Control ([Q] / [R])
*     - Retains bright pale yellow pathway, Fredoka Bold Game Over dialog, and sound engine
*
*   Compile on Windows (cmd.exe / MinGW / w64devkit):
*     g++ -O2 -std=c++17 the_lost_passage_raylib.cpp -lraylib -lopengl32 -lgdi32 -lwinmm -o lost_passage.exe
*
*   Compile on Linux:
*     g++ -O2 -std=c++17 the_lost_passage_raylib.cpp -lraylib -lGL -lm -lpthread -ldl -lrt -lX11 -o lost_passage
*
*   Compile on macOS:
*     clang++ -O2 -std=c++17 the_lost_passage_raylib.cpp -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -o lost_passage
*
********************************************************************************************/

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "raylib.h"
#include "raymath.h"

#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------
// Global Constants & Pale Yellow Pathway Colors
// ---------------------------------------------------------
const int SCREEN_WIDTH  = 1280;
const int SCREEN_HEIGHT = 720;

const Color PATH_BG_PALE_YELLOW   = Color{ 253, 250, 228, 255 }; 
const Color PATH_RUNWAY_PALE      = Color{ 255, 244, 185, 255 }; 
const Color PATH_WALL_CEIL_PALE   = Color{ 236, 222, 172, 255 }; 
const Color PATH_WALL_FLOOR_PALE  = Color{ 226, 210, 154, 255 }; 
const Color PATH_RIM_GLOW         = Color{ 255, 232, 110, 255 }; 
const Color PATH_GUIDE_LINE       = Color{ 245, 205, 70, 140 };  

enum class GameState {
    TITLE_MENU,
    SHIP_SELECT_MODAL,
    PLAYING,
    PAUSED,
    GAME_OVER,
    SECTOR_CLEARED
};

enum class Difficulty {
    CADET,       // Recon - 160 HP, 90s timer, wide cavern
    COMMANDER,   // Tactical - 100 HP, 60s timer, balanced
    VOID_WALKER, // Hardcore - 70 HP, 45s timer, tight chokepoints
    APOCALYPSE   // Insane - 50 HP, 35s timer, lethal barrage
};

enum class ShipType {
    VALKYRIE,  // Speed & Bal: Azure Cyan
    PHANTOM,   // Agile & Capacitor: Emerald Neon
    DREADNOUGHT// Heavy Armor & Shield: Solar Amber
};

// ---------------------------------------------------------
// Interactive UI Button with Hover & Selection States
// ---------------------------------------------------------
struct HoverButton {
    Rectangle rect;
    std::string text;
    Color baseBg;
    Color hoverBg;
    Color borderCol;
    Color hoverBorderCol;
    Color textCol;
    Color hoverTextCol;
    bool selected = false;

    bool IsHovered() const {
        return CheckCollisionPointRec(GetMousePosition(), rect);
    }

    bool IsClicked() const {
        return IsHovered() && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }

    void Draw(Font font, float fontSize) const {
        bool hovered = IsHovered();
        Color bg = selected ? hoverBg : (hovered ? hoverBg : baseBg);
        Color border = selected ? Color{ 255, 255, 255, 255 } : (hovered ? hoverBorderCol : borderCol);
        Color tc = selected ? Color{ 255, 255, 255, 255 } : (hovered ? hoverTextCol : textCol);

        Rectangle drawRect = rect;
        if (hovered || selected) {
            drawRect.x -= 2.0f;
            drawRect.y -= 2.0f;
            drawRect.width += 4.0f;
            drawRect.height += 4.0f;
            DrawRectangleRounded(Rectangle{ drawRect.x - 3, drawRect.y - 3, drawRect.width + 6, drawRect.height + 6 }, 0.25f, 6, Color{ border.r, border.g, border.b, 75 });
        }

        DrawRectangleRounded(drawRect, 0.25f, 6, bg);
        DrawRectangleRoundedLines(drawRect, 0.25f, 6, selected ? 3.0f : 2.0f, border);

        Vector2 textSize = MeasureTextEx(font, text.c_str(), fontSize, 1.0f);
        Vector2 textPos = {
            drawRect.x + (drawRect.width - textSize.x) / 2.0f,
            drawRect.y + (drawRect.height - textSize.y) / 2.0f
        };

        DrawTextEx(font, text.c_str(), { textPos.x + 1, textPos.y + 1 }, fontSize, 1.0f, Color{ 0, 0, 0, 180 });
        DrawTextEx(font, text.c_str(), textPos, fontSize, 1.0f, tc);
    }
};

// ---------------------------------------------------------
// Particle System
// ---------------------------------------------------------
struct Particle {
    Vector2 position;
    Vector2 velocity;
    Color color;
    float alpha;
    float size;
    float life;
    float maxLife;
};

class ParticleSystem {
private:
    std::vector<Particle> particles;

public:
    void Clear() { particles.clear(); }

    void Emit(Vector2 pos, Vector2 vel, Color col, float size, float life) {
        particles.push_back(Particle{ pos, vel, col, 1.0f, size, life, life });
    }

    void Update(float dt) {
        for (size_t i = 0; i < particles.size();) {
            particles[i].life -= dt;
            if (particles[i].life <= 0.0f) {
                particles.erase(particles.begin() + i);
            } else {
                particles[i].position = Vector2Add(particles[i].position, Vector2Scale(particles[i].velocity, dt));
                particles[i].alpha = particles[i].life / particles[i].maxLife;
                ++i;
            }
        }
    }

    void Draw() const {
        for (const auto &p : particles) {
            Color c = p.color;
            c.a = (unsigned char)(p.alpha * 255.0f);
            DrawCircleV(p.position, p.size, c);
        }
    }
};

// ---------------------------------------------------------
// Procedural Synthetic Audio & Ambient Sci-Fi BGM
// ---------------------------------------------------------
class AudioSynthesizer {
public:
    Sound sndBoost;
    Sound sndShield;
    Sound sndHit;
    Sound sndCrystal;
    Sound sndHeart;
    Sound sndPing;
    Sound sndAlarm;
    Sound sndRepair;
    Sound sndRoar;
    Sound sndGameOver;

    AudioStream bgmStream;
    float bgmTime = 0.0f;
    bool bgmPlaying = false;
    bool muted = false;

    void Init() {
        InitAudioDevice();
        sndBoost    = GenerateTone(140, 220, 0.25f, 0.35f, 1);
        sndShield   = GenerateTone(350, 480, 0.30f, 0.30f, 0);
        sndHit      = GenerateNoise(0.28f, 0.45f);
        sndCrystal  = GenerateTone(880, 1320, 0.20f, 0.25f, 0);
        sndHeart    = GenerateTone(520, 1040, 0.30f, 0.30f, 0);
        sndPing     = GenerateTone(600, 950, 0.40f, 0.30f, 0);
        sndAlarm    = GenerateTone(440, 330, 0.15f, 0.35f, 2);
        sndRepair   = GenerateTone(520, 680, 0.35f, 0.25f, 0);
        sndRoar     = GenerateMonsterRoar(0.9f, 0.55f);
        sndGameOver = GenerateGameOverTune(1.8f, 0.50f);

        SetAudioStreamBufferSizeDefault(4096);
        bgmStream = LoadAudioStream(44100, 16, 1);
        PlayAudioStream(bgmStream);
        bgmPlaying = true;
    }

    void UpdateBGM(bool inGame, float threatIntensity) {
        if (!bgmPlaying || !IsAudioStreamProcessed(bgmStream)) return;

        const int sampleCount = 4096;
        short buffer[sampleCount];
        float sampleRate = 44100.0f;
        static const float baseNotes[4] = { 110.0f, 130.81f, 164.81f, 196.0f };

        for (int i = 0; i < sampleCount; ++i) {
            bgmTime += 1.0f / sampleRate;

            if (muted) {
                buffer[i] = 0;
                continue;
            }

            if (!inGame) {
                float drone = sinf(2.0f * PI * 55.0f * bgmTime) * 0.15f + sinf(2.0f * PI * 110.0f * bgmTime) * 0.08f;
                buffer[i] = (short)(drone * 32767.0f * 0.22f);
                continue;
            }

            int chordIdx = ((int)(bgmTime * 0.5f)) % 4;
            float rootFreq = baseNotes[chordIdx];

            int step = ((int)(bgmTime * 8.0f)) % 8;
            float arpMult = (step == 0 || step == 4) ? 1.0f : (step == 2 || step == 6) ? 1.5f : (step == 7) ? 2.0f : 1.25f;
            float arpFreq = rootFreq * arpMult;

            float bassTone = sinf(2.0f * PI * (rootFreq * 0.5f) * bgmTime);
            float arpTone  = fmodf(bgmTime * arpFreq, 1.0f) * 2.0f - 1.0f;

            float pulse = 0.5f + 0.5f * sinf(bgmTime * 4.0f);
            float monsterTension = threatIntensity * 0.35f * sinf(2.0f * PI * 44.0f * bgmTime);

            float mix = (bassTone * 0.22f) + (arpTone * 0.10f * pulse) + monsterTension;
            mix = Clamp(mix, -1.0f, 1.0f);

            buffer[i] = (short)(mix * 32767.0f * 0.25f);
        }

        UpdateAudioStream(bgmStream, buffer, sampleCount);
    }

    void Close() {
        UnloadAudioStream(bgmStream);
        UnloadSound(sndBoost);
        UnloadSound(sndShield);
        UnloadSound(sndHit);
        UnloadSound(sndCrystal);
        UnloadSound(sndHeart);
        UnloadSound(sndPing);
        UnloadSound(sndAlarm);
        UnloadSound(sndRepair);
        UnloadSound(sndRoar);
        UnloadSound(sndGameOver);
        CloseAudioDevice();
    }

private:
    Sound GenerateTone(float freqStart, float freqEnd, float duration, float volume, int waveType) {
        int sampleRate = 44100;
        int sampleCount = (int)(sampleRate * duration);
        short *data = (short *)malloc(sampleCount * sizeof(short));

        for (int i = 0; i < sampleCount; ++i) {
            float t = (float)i / sampleCount;
            float freq = freqStart + (freqEnd - freqStart) * t;
            float timeSec = (float)i / sampleRate;
            float sampleVal = 0.0f;

            if (waveType == 0)      sampleVal = sinf(2.0f * PI * freq * timeSec);
            else if (waveType == 1) sampleVal = fmodf(freq * timeSec, 1.0f) * 2.0f - 1.0f;
            else                    sampleVal = (sinf(2.0f * PI * freq * timeSec) > 0.0f) ? 0.7f : -0.7f;

            float envelope = 1.0f - t;
            data[i] = (short)(sampleVal * envelope * volume * 32767.0f);
        }

        Wave wave{};
        wave.frameCount = (unsigned int)sampleCount;
        wave.sampleRate = (unsigned int)sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = data;
        Sound s = LoadSoundFromWave(wave);
        UnloadWave(wave);
        return s;
    }

    Sound GenerateNoise(float duration, float volume) {
        int sampleRate = 44100;
        int sampleCount = (int)(sampleRate * duration);
        short *data = (short *)malloc(sampleCount * sizeof(short));

        for (int i = 0; i < sampleCount; ++i) {
            float t = (float)i / sampleCount;
            float rnd = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            float envelope = expf(-t * 6.0f);
            data[i] = (short)(rnd * envelope * volume * 32767.0f);
        }

        Wave wave{};
        wave.frameCount = (unsigned int)sampleCount;
        wave.sampleRate = (unsigned int)sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = data;
        Sound s = LoadSoundFromWave(wave);
        UnloadWave(wave);
        return s;
    }

    Sound GenerateMonsterRoar(float duration, float volume) {
        int sampleRate = 44100;
        int sampleCount = (int)(sampleRate * duration);
        short *data = (short *)malloc(sampleCount * sizeof(short));

        for (int i = 0; i < sampleCount; ++i) {
            float t = (float)i / sampleCount;
            float timeSec = (float)i / sampleRate;
            float pitch = 140.0f * expf(-t * 1.8f) + 40.0f;
            float rawGrowl = sinf(2.0f * PI * pitch * timeSec) + 0.6f * sinf(2.0f * PI * (pitch * 0.5f) * timeSec);
            float noise = (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f) * 0.45f;
            float envelope = sinf(t * PI);
            data[i] = (short)((rawGrowl + noise) * envelope * volume * 32767.0f);
        }

        Wave wave{};
        wave.frameCount = (unsigned int)sampleCount;
        wave.sampleRate = (unsigned int)sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = data;
        Sound s = LoadSoundFromWave(wave);
        UnloadWave(wave);
        return s;
    }

    Sound GenerateGameOverTune(float duration, float volume) {
        int sampleRate = 44100;
        int sampleCount = (int)(sampleRate * duration);
        short *data = (short *)malloc(sampleCount * sizeof(short));
        const float notes[4] = { 164.81f, 130.81f, 110.00f, 87.31f };

        for (int i = 0; i < sampleCount; ++i) {
            float t = (float)i / (float)sampleRate;
            int noteIndex = (int)(t / 0.40f);
            if (noteIndex > 3) noteIndex = 3;

            float noteTime = fmodf(t, 0.40f);
            float freq = notes[noteIndex];

            float tone = sinf(2.0f * PI * freq * t) * 0.6f + sinf(2.0f * PI * (freq * 0.5f) * t) * 0.3f;
            float decay = expf(-noteTime * 4.0f);
            float masterFade = 1.0f - (t / duration);

            float mix = tone * decay * masterFade * volume;
            data[i] = (short)(mix * 32767.0f);
        }

        Wave wave{};
        wave.frameCount = (unsigned int)sampleCount;
        wave.sampleRate = (unsigned int)sampleRate;
        wave.sampleSize = 16;
        wave.channels = 1;
        wave.data = data;
        Sound s = LoadSoundFromWave(wave);
        UnloadWave(wave);
        return s;
    }
};

// ---------------------------------------------------------
// Terrain System: Pale Yellow Passage Slice & Generator
// ---------------------------------------------------------
struct PassageSlice {
    float x;
    float centerY;
    float halfWidth;
    float ceilingY() const { return centerY - halfWidth; }
    float floorY() const   { return centerY + halfWidth; }
};

class PassageGenerator {
public:
    std::vector<PassageSlice> passage;
    float totalLength = 12000.0f;
    const float sliceStep = 25.0f;

    void Generate(Difficulty diff) {
        passage.clear();
        const int totalSlices = (int)(totalLength / sliceStep);

        float minWidth = 140.0f;
        float maxWidth = 260.0f;
        if (diff == Difficulty::CADET)            { minWidth = 190.0f; maxWidth = 330.0f; }
        else if (diff == Difficulty::COMMANDER)   { minWidth = 140.0f; maxWidth = 260.0f; }
        else if (diff == Difficulty::VOID_WALKER) { minWidth = 110.0f; maxWidth = 210.0f; }
        else if (diff == Difficulty::APOCALYPSE)  { minWidth = 95.0f;  maxWidth = 185.0f; }

        for (int i = 0; i < totalSlices; ++i) {
            float x = i * sliceStep;
            float wave1 = sinf(x * 0.0018f) * 160.0f;
            float wave2 = cosf(x * 0.0045f) * 75.0f;
            float wave3 = sinf(x * 0.0090f) * 35.0f;
            float centerY = (float)SCREEN_HEIGHT / 2.0f + wave1 + wave2 + wave3;
            centerY = Clamp(centerY, 180.0f, (float)SCREEN_HEIGHT - 180.0f);

            float widthWave = sinf(x * 0.003f) * 0.5f + 0.5f;
            float halfWidth = Lerp(minWidth, maxWidth, widthWave);

            passage.push_back({ x, centerY, halfWidth });
        }
    }

    PassageSlice GetSliceAt(float x) const {
        if (passage.empty()) return { x, (float)SCREEN_HEIGHT / 2.0f, 150.0f };
        int idx = (int)(x / sliceStep);
        idx = Clamp(idx, 0, (int)passage.size() - 1);
        return passage[idx];
    }

    void Draw(float targetX) const {
        for (size_t i = 0; i < passage.size() - 1; ++i) {
            const auto &p1 = passage[i];
            const auto &p2 = passage[i + 1];

            if (p2.x < targetX - SCREEN_WIDTH * 0.7f || p1.x > targetX + SCREEN_WIDTH * 0.9f) {
                continue;
            }

            Vector2 pathA = { p1.x, p1.ceilingY() };
            Vector2 pathB = { p2.x, p2.ceilingY() };
            Vector2 pathC = { p2.x, p2.floorY() };
            Vector2 pathD = { p1.x, p1.floorY() };
            DrawTriangle(pathA, pathD, pathC, PATH_BG_PALE_YELLOW);
            DrawTriangle(pathA, pathC, pathB, PATH_BG_PALE_YELLOW);

            Vector2 centerStart = { p1.x, p1.centerY };
            Vector2 centerEnd   = { p2.x, p2.centerY };
            DrawLineEx(centerStart, centerEnd, 4.0f, PATH_RUNWAY_PALE);
            if ((i % 4) == 0) {
                DrawLineEx(centerStart, centerEnd, 2.0f, PATH_GUIDE_LINE);
            }

            Vector2 cA = { p1.x, -500.0f };
            Vector2 cB = { p2.x, -500.0f };
            Vector2 cC = { p2.x, p2.ceilingY() };
            Vector2 cD = { p1.x, p1.ceilingY() };
            DrawTriangle(cA, cD, cC, PATH_WALL_CEIL_PALE);
            DrawTriangle(cA, cC, cB, PATH_WALL_CEIL_PALE);
            DrawLineEx(cD, cC, 4.0f, PATH_RIM_GLOW);

            Vector2 fA = { p1.x, p1.floorY() };
            Vector2 fB = { p2.x, p2.floorY() };
            Vector2 fC = { p2.x, (float)SCREEN_HEIGHT + 500.0f };
            Vector2 fD = { p1.x, (float)SCREEN_HEIGHT + 500.0f };
            DrawTriangle(fA, fC, fB, PATH_WALL_FLOOR_PALE);
            DrawTriangle(fA, fD, fC, PATH_WALL_FLOOR_PALE);
            DrawLineEx(fA, fB, 4.0f, PATH_RIM_GLOW);
        }
    }
};

// ---------------------------------------------------------
// OOP Hierarchy: Entity Base Class
// ---------------------------------------------------------
class Entity {
public:
    virtual ~Entity() = default;
    virtual void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) = 0;
    virtual void Draw() const = 0;
    virtual Vector2 GetPosition() const = 0;
    virtual float GetRadius() const = 0;
    virtual bool IsActive() const = 0;
};

// ---------------------------------------------------------
// Pickups: Abstract Base & Derived Classes
// ---------------------------------------------------------
class PickupBase : public Entity {
protected:
    Vector2 position;
    float radius;
    bool collected;
    float pulsePhase;

public:
    PickupBase(Vector2 pos, float r)
        : position(pos), radius(r), collected(false), pulsePhase((float)rand() / RAND_MAX * 6.28f) {}

    Vector2 GetPosition() const override { return position; }
    float GetRadius() const override { return radius; }
    bool IsActive() const override { return !collected; }
    void MarkCollected() { collected = true; }
};

class ChronoCrystalPickup : public PickupBase {
public:
    ChronoCrystalPickup(Vector2 pos) : PickupBase(pos, 16.0f) {}

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (collected) return;
        pulsePhase += dt * 4.0f;
    }

    void Draw() const override {
        if (collected) return;
        float pulse = sinf(pulsePhase) * 3.0f;
        DrawCircleGradient((int)position.x, (int)position.y, 16.0f + pulse, Color{ 0, 255, 200, 160 }, BLANK);
        DrawPoly(position, 6, 9.0f, pulsePhase * 20.0f, Color{ 80, 255, 230, 255 });
        DrawPolyLines(position, 6, 10.0f, pulsePhase * 20.0f, Color{ 200, 255, 250, 255 });
    }
};

class HeartPickup : public PickupBase {
private:
    float floatOffset = 0.0f;

public:
    HeartPickup(Vector2 pos) : PickupBase(pos, 18.0f) {}

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (collected) return;
        pulsePhase += dt * 5.0f;
        floatOffset = sinf(pulsePhase) * 6.0f;
    }

    void Draw() const override {
        if (collected) return;
        Vector2 pos = { position.x, position.y + floatOffset };
        float pulse = sinf(pulsePhase) * 2.0f;

        DrawCircleGradient((int)pos.x, (int)pos.y, 22.0f + pulse, Color{ 255, 40, 80, 150 }, BLANK);

        float size = 8.0f + pulse * 0.4f;
        Vector2 leftCenter  = { pos.x - size * 0.55f, pos.y - size * 0.3f };
        Vector2 rightCenter = { pos.x + size * 0.55f, pos.y - size * 0.3f };
        DrawCircleV(leftCenter, size * 0.55f, Color{ 255, 40, 80, 255 });
        DrawCircleV(rightCenter, size * 0.55f, Color{ 255, 40, 80, 255 });

        Vector2 tA = { pos.x - size * 1.05f, pos.y - size * 0.2f };
        Vector2 tB = { pos.x + size * 1.05f, pos.y - size * 0.2f };
        Vector2 tC = { pos.x, pos.y + size * 1.1f };
        DrawTriangle(tA, tB, tC, Color{ 255, 40, 80, 255 });
        DrawCircle((int)(pos.x - size * 0.5f), (int)(pos.y - size * 0.5f), 2.0f, RAYWHITE);
    }
};

// ---------------------------------------------------------
// Traps: Abstract Base & Derived Classes
// ---------------------------------------------------------
class TrapBase : public Entity {
protected:
    Vector2 position;
    Vector2 originalPos;
    float radius;
    bool active;
    bool triggered;
    float timer;
    float cycleRate;
    float alertDistance;

public:
    TrapBase(Vector2 pos, float r, float alertDist)
        : position(pos), originalPos(pos), radius(r), active(true), triggered(false),
          timer(0.0f), cycleRate(1.0f + ((float)rand() / RAND_MAX) * 1.5f), alertDistance(alertDist) {}

    Vector2 GetPosition() const override { return position; }
    float GetRadius() const override { return radius; }
    bool IsActive() const override { return active; }
    void Deactivate() { active = false; }
    virtual float GetDamage() const { return 28.0f; }
    virtual void OnPlayerProximity(const Vector2 &playerPos, AudioSynthesizer &audio) = 0;
};

class StalactiteTrap : public TrapBase {
private:
    float velocityY = 0.0f;
    float floorLimit = 1000.0f;

public:
    StalactiteTrap(Vector2 pos, float floorY) : TrapBase(pos, 18.0f, 180.0f), floorLimit(floorY) {}

    void OnPlayerProximity(const Vector2 &playerPos, AudioSynthesizer &audio) override {
        if (!triggered && Vector2Distance(playerPos, position) < alertDistance && playerPos.x < position.x + 40.0f) {
            triggered = true;
            PlaySound(audio.sndAlarm);
        }
    }

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        if (triggered) {
            velocityY += 980.0f * dt;
            position.y += velocityY * dt;
            if (position.y >= floorLimit) {
                active = false;
                for (int k = 0; k < 8; ++k) {
                    particles.Emit(position, { (float)(rand() % 160 - 80), (float)-(rand() % 140 + 20) }, Color{ 150, 140, 130, 255 }, 4.0f, 0.5f);
                }
            }
        }
    }

    void Draw() const override {
        if (!active) return;
        Vector2 topA = { position.x - 12.0f, position.y - 22.0f };
        Vector2 topB = { position.x + 12.0f, position.y - 22.0f };
        Vector2 tip  = { position.x, position.y + 16.0f };
        DrawTriangle(topA, tip, topB, Color{ 190, 80, 70, 255 });
        DrawTriangleLines(topA, tip, topB, Color{ 255, 140, 110, 255 });
    }
};

class ProximityMineTrap : public TrapBase {
private:
    float elapsedTime = 0.0f;

public:
    ProximityMineTrap(Vector2 pos) : TrapBase(pos, 17.0f, 150.0f) {}

    void OnPlayerProximity(const Vector2 &playerPos, AudioSynthesizer &audio) override {
        if (Vector2Distance(playerPos, position) < alertDistance) {
            triggered = true;
            Vector2 dir = Vector2Normalize(Vector2Subtract(playerPos, position));
            position = Vector2Add(position, Vector2Scale(dir, 145.0f * GetFrameTime()));
        }
    }

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        elapsedTime += dt;
    }

    void Draw() const override {
        if (!active) return;
        DrawCircleV(position, radius, Color{ 45, 15, 15, 255 });
        DrawCircleLines((int)position.x, (int)position.y, radius, triggered ? RED : Color{ 220, 50, 50, 255 });

        for (int sp = 0; sp < 8; ++sp) {
            float ang = (float)sp * (PI / 4.0f) + elapsedTime * 1.5f;
            Vector2 base = { position.x + cosf(ang) * radius, position.y + sinf(ang) * radius };
            Vector2 tip  = { position.x + cosf(ang) * (radius + 6.0f), position.y + sinf(ang) * (radius + 6.0f) };
            DrawLineEx(base, tip, 2.5f, Color{ 255, 70, 70, 255 });
        }
        DrawCircleV(position, 5.0f, triggered ? RED : ORANGE);
    }
};

class LaserTripwireTrap : public TrapBase {
private:
    float ceilingY;
    float floorY;

public:
    LaserTripwireTrap(Vector2 pos, float cY, float fY) : TrapBase(pos, 8.0f, 220.0f), ceilingY(cY), floorY(fY) {}

    void OnPlayerProximity(const Vector2 &playerPos, AudioSynthesizer &audio) override {}

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        timer += dt * cycleRate;
        triggered = (fmodf(timer, 3.5f) < 2.0f);
    }

    void Draw() const override {
        if (!active) return;
        if (triggered) {
            DrawLineEx({ position.x, ceilingY }, { position.x, floorY }, 4.0f, Color{ 255, 50, 70, 220 });
            DrawLineEx({ position.x, ceilingY }, { position.x, floorY }, 1.5f, RAYWHITE);
        } else {
            DrawLineEx({ position.x, ceilingY }, { position.x, floorY }, 1.0f, Color{ 255, 50, 70, 60 });
        }
    }
};

class CrusherGateTrap : public TrapBase {
private:
    float ceilingY;
    float floorY;
    float halfWidth;

public:
    CrusherGateTrap(Vector2 pos, float cY, float fY, float hWidth)
        : TrapBase(pos, hWidth, 180.0f), ceilingY(cY), floorY(fY), halfWidth(hWidth) {}

    void OnPlayerProximity(const Vector2 &playerPos, AudioSynthesizer &audio) override {}

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        timer += dt * cycleRate;
    }

    void Draw() const override {
        if (!active) return;
        float openAmount = sinf(timer * cycleRate) * 0.5f + 0.5f;
        float gateH = halfWidth * 0.45f * (1.0f - openAmount);
        DrawRectangle((int)position.x - 8, (int)ceilingY, 16, (int)gateH, Color{ 140, 120, 100, 255 });
        DrawRectangle((int)position.x - 8, (int)(floorY - gateH), 16, (int)gateH, Color{ 140, 120, 100, 255 });
    }
};

// ---------------------------------------------------------
// Enemy Hazards: Abstract Base & Derived Classes
// ---------------------------------------------------------
class HazardBase : public Entity {
protected:
    Vector2 position;
    Vector2 velocity;
    float radius;
    bool active;
    float damage;
    std::string signature;

public:
    HazardBase(Vector2 pos, Vector2 vel, float r, float dmg, const std::string &sig)
        : position(pos), velocity(vel), radius(r), active(true), damage(dmg), signature(sig) {}

    Vector2 GetPosition() const override { return position; }
    float GetRadius() const override { return radius; }
    bool IsActive() const override { return active; }
    void Deactivate() { active = false; }
    float GetDamage() const { return damage; }
    virtual void TrackTarget(const Vector2 &targetPos, float dt) {}
};

class CruiseMissile : public HazardBase {
public:
    CruiseMissile(Vector2 pos, Vector2 vel)
        : HazardBase(pos, vel, 16.0f, 32.0f, "CRUISE-MISSILE") {}

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        position = Vector2Add(position, Vector2Scale(velocity, dt));

        if (rand() % 2 == 0) {
            particles.Emit(
                Vector2{ position.x + 16.0f, position.y },
                Vector2{ 60.0f + (float)(rand() % 30), (float)(rand() % 30 - 15) },
                Color{ 180, 180, 190, 180 }, 3.5f, 0.35f
            );
        }
    }

    void Draw() const override {
        if (!active) return;
        DrawCircleGradient((int)position.x + 14, (int)position.y, 16.0f, Color{ 255, 120, 30, 200 }, BLANK);
        DrawRectangle((int)position.x - 12, (int)position.y - 4, 24, 8, Color{ 210, 220, 230, 255 });
        Vector2 nose = { position.x - 20.0f, position.y };
        Vector2 nA   = { position.x - 12.0f, position.y - 4.0f };
        Vector2 nB   = { position.x - 12.0f, position.y + 4.0f };
        DrawTriangle(nose, nA, nB, RED);
        DrawTriangle({ position.x + 12.0f, position.y - 4.0f }, { position.x + 8.0f, position.y - 10.0f }, { position.x + 16.0f, position.y - 4.0f }, Color{ 70, 80, 95, 255 });
        DrawTriangle({ position.x + 12.0f, position.y + 4.0f }, { position.x + 8.0f, position.y + 10.0f }, { position.x + 16.0f, position.y + 4.0f }, Color{ 70, 80, 95, 255 });
    }
};

class HeavyBomb : public HazardBase {
public:
    HeavyBomb(Vector2 pos, Vector2 vel)
        : HazardBase(pos, vel, 20.0f, 45.0f, "HEAVY-BOMB") {}

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        position = Vector2Add(position, Vector2Scale(velocity, dt));
    }

    void Draw() const override {
        if (!active) return;
        DrawCircleGradient((int)position.x, (int)position.y, 22.0f, Color{ 255, 80, 20, 160 }, BLANK);
        DrawCircleV(position, 13.0f, Color{ 55, 60, 70, 255 });
        DrawCircleV({ position.x - 4.0f, position.y }, 11.0f, Color{ 75, 85, 95, 255 });
        DrawRectangle((int)position.x - 4, (int)position.y - 10, 4, 20, YELLOW);
        DrawRectangle((int)position.x + 10, (int)position.y - 6, 6, 12, Color{ 40, 45, 50, 255 });
    }
};

class SeekerDrone : public HazardBase {
private:
    float animTimer = 0.0f;

public:
    SeekerDrone(Vector2 pos, Vector2 vel)
        : HazardBase(pos, vel, 18.0f, 32.0f, "SEEKER-DRONE") {}

    void TrackTarget(const Vector2 &targetPos, float dt) override {
        float dy = targetPos.y - position.y;
        velocity.y = Clamp(dy * 2.0f, -120.0f, 120.0f);
    }

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!active) return;
        animTimer += dt;
        position = Vector2Add(position, Vector2Scale(velocity, dt));
    }

    void Draw() const override {
        if (!active) return;
        DrawPoly(position, 4, 15.0f, animTimer * 90.0f, Color{ 160, 40, 40, 255 });
        DrawPolyLines(position, 4, 16.0f, animTimer * 90.0f, RED);
        DrawCircleV(position, 4.0f, RAYWHITE);
    }
};

// ---------------------------------------------------------
// Void Leviathan Monster: Boss Chaser Entity
// ---------------------------------------------------------
class VoidLeviathan : public Entity {
public:
    Vector2 position;
    Vector2 velocity;
    float radius;
    bool active;
    bool pursuing;
    float spawnTimer;
    float roarTimer;
    float jawAnim;
    std::vector<Vector2> tailSegments;

    VoidLeviathan() {
        Reset();
    }

    void Reset() {
        active = true;
        pursuing = false;
        position = { -600.0f, (float)SCREEN_HEIGHT / 2.0f };
        velocity = { 0.0f, 0.0f };
        radius = 38.0f;
        spawnTimer = 8.0f;
        roarTimer = 0.0f;
        jawAnim = 0.0f;
        tailSegments.clear();
        for (int i = 0; i < 9; ++i) tailSegments.push_back(position);
    }

    Vector2 GetPosition() const override { return position; }
    float GetRadius() const override { return radius; }
    bool IsActive() const override { return active && pursuing; }

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        jawAnim += dt * 8.0f;
        if (!pursuing) {
            spawnTimer -= dt;
            if (spawnTimer <= 0.0f) {
                pursuing = true;
                roarTimer = 0.0f;
                PlaySound(audio.sndRoar);
            }
            return;
        }

        roarTimer += dt;
        if (roarTimer > 6.5f) {
            roarTimer = 0.0f;
            PlaySound(audio.sndRoar);
        }

        if (rand() % 2 == 0) {
            particles.Emit(
                Vector2{ position.x + (float)(rand() % 30 - 15), position.y + (float)(rand() % 30 - 15) },
                Vector2{ -80.0f - (float)(rand() % 60), (float)(rand() % 60 - 30) },
                Color{ 180, 20, 50, 200 }, 6.0f, 0.45f
            );
        }
    }

    void FollowPlayer(const Vector2 &playerPos, float baseSpeed, float dt) {
        if (!pursuing) return;

        Vector2 toPlayer = Vector2Subtract(playerPos, position);
        Vector2 monsterDir = Vector2Normalize(toPlayer);
        velocity = Vector2Scale(monsterDir, baseSpeed);
        position = Vector2Add(position, Vector2Scale(velocity, dt));

        tailSegments[0] = position;
        for (size_t i = 1; i < tailSegments.size(); ++i) {
            Vector2 segDir = Vector2Subtract(tailSegments[i], tailSegments[i - 1]);
            float segDist = Vector2Length(segDir);
            if (segDist > 24.0f) {
                segDir = Vector2Normalize(segDir);
                tailSegments[i] = Vector2Add(tailSegments[i - 1], Vector2Scale(segDir, 24.0f));
            }
        }
    }

    void Draw() const override {
        if (!pursuing) return;

        for (size_t i = tailSegments.size() - 1; i > 0; --i) {
            float t = (float)i / (float)tailSegments.size();
            float segR = Lerp(radius * 0.85f, 10.0f, t);
            Color segCol = Color{ (unsigned char)Lerp(120, 40, t), (unsigned char)Lerp(15, 5, t), (unsigned char)Lerp(30, 10, t), 255 };
            DrawCircleV(tailSegments[i], segR, segCol);
            DrawCircleLines((int)tailSegments[i].x, (int)tailSegments[i].y, segR, Color{ 230, 40, 70, 120 });
        }

        DrawCircleGradient((int)position.x, (int)position.y, 75.0f, Color{ 255, 20, 60, 140 }, BLANK);
        DrawCircleV(position, radius, Color{ 45, 10, 20, 255 });
        DrawCircleLines((int)position.x, (int)position.y, radius, Color{ 255, 60, 80, 255 });

        float jawOpen = sinf(jawAnim) * 0.35f + 0.45f;
        Vector2 upperMandible = { position.x + cosf(-jawOpen) * 44.0f, position.y + sinf(-jawOpen) * 44.0f };
        Vector2 lowerMandible = { position.x + cosf(jawOpen) * 44.0f, position.y + sinf(jawOpen) * 44.0f };

        DrawTriangle(position, upperMandible, { position.x + cosf(-jawOpen - 0.2f) * 22.0f, position.y + sinf(-jawOpen - 0.2f) * 22.0f }, Color{ 200, 20, 50, 255 });
        DrawTriangle(position, lowerMandible, { position.x + cosf(jawOpen + 0.2f) * 22.0f, position.y + sinf(jawOpen + 0.2f) * 22.0f }, Color{ 200, 20, 50, 255 });

        Vector2 eye1 = { position.x + cosf(-0.4f) * 22.0f, position.y + sinf(-0.4f) * 22.0f };
        Vector2 eye2 = { position.x + cosf(0.4f) * 22.0f, position.y + sinf(0.4f) * 22.0f };
        DrawCircleV(eye1, 6.0f, Color{ 255, 240, 50, 255 });
        DrawCircleV(eye2, 6.0f, Color{ 255, 240, 50, 255 });
        DrawCircleV(eye1, 2.5f, BLACK);
        DrawCircleV(eye2, 2.5f, BLACK);
    }
};

// ---------------------------------------------------------
// Player Spaceship Entity (3 Selectable Craft Types)
// ---------------------------------------------------------
class PlayerSpaceship : public Entity {
public:
    Vector2 position;
    Vector2 velocity;
    float angle;
    float radius;
    ShipType type;

    float hull;
    float maxHull;
    float energy;
    float maxEnergy;

    bool shieldActive;
    bool boosting;
    bool damageControlActive;

    float visionRadius;
    float radarPingRadius;
    bool radarPingActive;
    float radarPingAlpha;

    float distanceTraveled;
    int crystalsCollected;
    int heartsCollected;
    int currentSector;

    PlayerSpaceship() {
        type = ShipType::VALKYRIE;
        Reset(100.0f, type);
    }

    void Reset(float startingHull, ShipType chosenType) {
        type = chosenType;
        position = { 200.0f, (float)SCREEN_HEIGHT / 2.0f };
        velocity = { 0.0f, 0.0f };
        angle = 0.0f;
        radius = 20.0f;

        // Class stats modifiers
        float hullMod = 1.0f;
        maxEnergy = 100.0f;
        if (type == ShipType::PHANTOM) {
            maxEnergy = 135.0f; // High capacitor
            hullMod = 0.85f;    // Lighter armor
        } else if (type == ShipType::DREADNOUGHT) {
            hullMod = 1.45f;    // Super heavy armor
            maxEnergy = 90.0f;
        }

        maxHull = startingHull * hullMod;
        hull = maxHull;
        energy = maxEnergy;

        shieldActive = false;
        boosting = false;
        damageControlActive = false;
        visionRadius = 260.0f;
        radarPingRadius = 0.0f;
        radarPingActive = false;
        radarPingAlpha = 0.0f;
        distanceTraveled = 0.0f;
        crystalsCollected = 0;
        heartsCollected = 0;
        currentSector = 1;
    }

    Vector2 GetPosition() const override { return position; }
    float GetRadius() const override { return radius; }
    bool IsActive() const override { return hull > 0.0f; }

    void ProcessInput(Camera2D &camera, float dt, ParticleSystem &particles, AudioSynthesizer &audio) {
        Vector2 inputDir = { 0.0f, 0.0f };
        bool hasKeyboardSteering = false;

        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { inputDir.x += 1.0f; hasKeyboardSteering = true; }
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) { inputDir.x -= 0.6f; hasKeyboardSteering = true; }
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) { inputDir.y -= 1.0f; hasKeyboardSteering = true; }
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) { inputDir.y += 1.0f; hasKeyboardSteering = true; }

        Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), camera);
        Vector2 toMouse = Vector2Subtract(mouseWorld, position);
        float mouseDist = Vector2Length(toMouse);

        if (!hasKeyboardSteering && (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || mouseDist > 30.0f)) {
            Vector2 mouseNorm = Vector2Normalize(toMouse);
            inputDir.x = Clamp(mouseNorm.x + 0.45f, -0.6f, 1.0f);
            inputDir.y = mouseNorm.y;
        }

        // Speed modified by ship type
        float baseSpeed = 280.0f;
        if (type == ShipType::PHANTOM) baseSpeed = 310.0f;
        if (type == ShipType::DREADNOUGHT) baseSpeed = 250.0f;

        boosting = false;
        bool boostRequest = (IsKeyDown(KEY_LEFT_SHIFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT));

        if (boostRequest && energy > 5.0f) {
            baseSpeed *= 1.9f;
            energy -= 24.0f * dt;
            boosting = true;

            Color flameColor = (type == ShipType::VALKYRIE) ? Color{ 0, 240, 255, 255 } :
                               (type == ShipType::PHANTOM)   ? Color{ 50, 255, 120, 255 } : Color{ 255, 140, 20, 255 };

            Vector2 ex1 = Vector2Subtract(position, Vector2{ cosf(angle) * 24.0f + sinf(angle) * 11.0f, sinf(angle) * 24.0f - cosf(angle) * 11.0f });
            Vector2 ex2 = Vector2Subtract(position, Vector2{ cosf(angle) * 24.0f - sinf(angle) * 11.0f, sinf(angle) * 24.0f + cosf(angle) * 11.0f });

            particles.Emit(ex1, Vector2{ -cosf(angle) * 260.0f + (rand() % 60 - 30), -sinf(angle) * 260.0f + (rand() % 60 - 30) }, flameColor, 5.0f, 0.35f);
            particles.Emit(ex2, Vector2{ -cosf(angle) * 260.0f + (rand() % 60 - 30), -sinf(angle) * 260.0f + (rand() % 60 - 30) }, Color{ 255, 220, 60, 255 }, 5.0f, 0.35f);

            if (!IsSoundPlaying(audio.sndBoost)) PlaySound(audio.sndBoost);
        }

        // ENHANCED POWER: Kinetic Deflector Shield ([E] Key or Middle Mouse Button)
        if ((IsKeyDown(KEY_E) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) && energy > 8.0f) {
            shieldActive = true;
            energy -= 20.0f * dt;
            if (!IsSoundPlaying(audio.sndShield) && (rand() % 8 == 0)) PlaySound(audio.sndShield);
        } else {
            shieldActive = false;
        }

        // ENHANCED POWER: Active Nanite Damage Control ([Q] or [R] Key)
        if ((IsKeyDown(KEY_Q) || IsKeyDown(KEY_R)) && energy > 18.0f && hull < maxHull) {
            damageControlActive = true;
            energy -= 32.0f * dt;
            hull = fminf(maxHull, hull + 25.0f * dt); // Rapid nanite healing
            particles.Emit(Vector2{ position.x + (rand() % 32 - 16), position.y + (rand() % 32 - 16) }, { 0.0f, -45.0f }, Color{ 34, 211, 238, 255 }, 3.5f, 0.4f);
            if (!IsSoundPlaying(audio.sndRepair) && (rand() % 12 == 0)) PlaySound(audio.sndRepair);
        } else {
            damageControlActive = false;
        }

        // Sensor Radar Ping ([SPACE])
        if (IsKeyPressed(KEY_SPACE) && energy >= 15.0f && !radarPingActive) {
            energy -= 15.0f;
            radarPingActive = true;
            radarPingRadius = 20.0f;
            radarPingAlpha = 1.0f;
            PlaySound(audio.sndPing);
        }

        Vector2 targetVel = Vector2Scale(Vector2Normalize(inputDir), baseSpeed);
        velocity = Vector2Lerp(velocity, targetVel, 8.5f * dt);
        position = Vector2Add(position, Vector2Scale(velocity, dt));

        if (Vector2Length(velocity) > 10.0f) {
            float targetAngle = atan2f(velocity.y, velocity.x);
            angle = Lerp(angle, targetAngle, 10.0f * dt);
        }

        distanceTraveled = fmaxf(distanceTraveled, position.x);
        currentSector = 1 + (int)(position.x / 2500.0f);
    }

    void Update(float dt, ParticleSystem &particles, AudioSynthesizer &audio) override {
        if (!boosting && !shieldActive && !damageControlActive) {
            float rechargeMult = (type == ShipType::PHANTOM) ? 16.0f : 12.0f;
            energy = fminf(maxEnergy, energy + rechargeMult * dt);
        }

        if (radarPingActive) {
            radarPingRadius += 1200.0f * dt;
            radarPingAlpha -= 1.2f * dt;
            if (radarPingAlpha <= 0.0f || radarPingRadius > 1400.0f) {
                radarPingActive = false;
            }
        }
    }

    void Draw() const override {
        DrawShipGraphics(position, angle, type, boosting, shieldActive);
    }

    // Static ship renderer helper usable for gameplay and in-menu Hangar previews
    static void DrawShipGraphics(Vector2 pos, float ang, ShipType sType, bool isBoosting, bool isShielded) {
        Color glowCol = (sType == ShipType::VALKYRIE) ? Color{ 56, 189, 248, 80 } :
                        (sType == ShipType::PHANTOM)   ? Color{ 74, 222, 128, 80 } : Color{ 251, 146, 60, 80 };
        DrawCircleGradient((int)pos.x, (int)pos.y, isBoosting ? 55.0f : 36.0f, glowCol, BLANK);

        auto Trans = [&](float fwd, float right) -> Vector2 {
            return Vector2{
                pos.x + cosf(ang) * fwd - sinf(ang) * right,
                pos.y + sinf(ang) * fwd + cosf(ang) * right
            };
        };

        if (sType == ShipType::VALKYRIE) {
            // [VALKYRIE]: Sleek Cyan Delta Interceptor
            Vector2 nose = Trans(32.0f, 0.0f);
            Vector2 wL   = Trans(-4.0f, -28.0f);
            Vector2 wR   = Trans(-4.0f, 28.0f);
            Vector2 tail = Trans(-16.0f, 0.0f);
            Vector2 nacL = Trans(-18.0f, -10.0f);
            Vector2 nacR = Trans(-18.0f, 10.0f);

            DrawTriangle(nose, wL, tail, Color{ 30, 42, 68, 255 });
            DrawTriangle(nose, tail, wR, Color{ 36, 50, 80, 255 });
            DrawTriangle(nose, Trans(18.0f, -6.0f), Trans(-6.0f, 0.0f), Color{ 234, 244, 255, 255 });
            DrawTriangle(nose, Trans(-6.0f, 0.0f), Trans(18.0f, 6.0f), Color{ 205, 222, 245, 255 });

            DrawLineEx(nose, Trans(8.0f, -26.0f), 2.0f, Color{ 56, 189, 248, 255 });
            DrawLineEx(nose, Trans(8.0f, 26.0f), 2.0f, Color{ 56, 189, 248, 255 });
            DrawCircleV(nacL, 4.0f, isBoosting ? Color{ 0, 240, 255, 255 } : Color{ 249, 115, 22, 255 });
            DrawCircleV(nacR, 4.0f, isBoosting ? Color{ 0, 240, 255, 255 } : Color{ 249, 115, 22, 255 });

            DrawLineEx(Trans(-3.0f, 0.0f), Trans(14.0f, 0.0f), 5.0f, Color{ 6, 182, 212, 255 });
        } else if (sType == ShipType::PHANTOM) {
            // [PHANTOM]: Twin-Boom Emerald Stealth Raider
            Vector2 nose = Trans(30.0f, 0.0f);
            Vector2 fL   = Trans(10.0f, -12.0f);
            Vector2 fR   = Trans(10.0f, 12.0f);
            Vector2 bL   = Trans(-24.0f, -16.0f);
            Vector2 bR   = Trans(-24.0f, 16.0f);
            Vector2 tail = Trans(-12.0f, 0.0f);

            DrawTriangle(nose, fL, tail, Color{ 15, 23, 42, 255 });
            DrawTriangle(nose, tail, fR, Color{ 15, 23, 42, 255 });
            DrawTriangle(fL, bL, tail, Color{ 34, 197, 94, 255 });
            DrawTriangle(fR, tail, bR, Color{ 34, 197, 94, 255 });

            DrawLineEx(nose, bL, 2.5f, Color{ 74, 222, 128, 255 });
            DrawLineEx(nose, bR, 2.5f, Color{ 74, 222, 128, 255 });
            DrawCircleV(bL, 4.5f, Color{ 52, 211, 153, 255 });
            DrawCircleV(bR, 4.5f, Color{ 52, 211, 153, 255 });

            DrawCircleV(Trans(6.0f, 0.0f), 4.0f, Color{ 16, 185, 129, 255 });
        } else {
            // [DREADNOUGHT]: Heavy Amber Citadel Cruiser
            Vector2 nose = Trans(26.0f, 0.0f);
            Vector2 armL = Trans(4.0f, -22.0f);
            Vector2 armR = Trans(4.0f, 22.0f);
            Vector2 finL = Trans(-22.0f, -24.0f);
            Vector2 finR = Trans(-22.0f, 24.0f);
            Vector2 eng  = Trans(-20.0f, 0.0f);

            DrawRectanglePro(Rectangle{ pos.x, pos.y, 36.0f, 22.0f }, Vector2{ 18.0f, 11.0f }, ang * RAD2DEG, Color{ 60, 30, 20, 255 });
            DrawTriangle(nose, armL, eng, Color{ 245, 158, 11, 255 });
            DrawTriangle(nose, eng, armR, Color{ 217, 119, 6, 255 });
            DrawTriangle(armL, finL, eng, Color{ 180, 83, 9, 255 });
            DrawTriangle(armR, eng, finR, Color{ 180, 83, 9, 255 });

            DrawLineEx(armL, armR, 3.5f, Color{ 251, 191, 36, 255 });
            DrawCircleV(Trans(-22.0f, -8.0f), 5.0f, RED);
            DrawCircleV(Trans(-22.0f, 8.0f), 5.0f, RED);
            DrawCircleV(Trans(-22.0f, 0.0f), 6.0f, ORANGE);
        }

        if (isShielded) {
            DrawCircleGradient((int)pos.x, (int)pos.y, 44.0f, Color{ 0, 180, 255, 95 }, BLANK);
            DrawPolyLines(pos, 6, 38.0f, (float)GetTime() * 60.0f, Color{ 100, 225, 255, 240 });
        }
    }
};

// ---------------------------------------------------------
// HUD & Interface Rendering Class
// ---------------------------------------------------------
class GameHUD {
public:
    static void DrawCard(int x, int y, int w, int h, Color bg, Color border) {
        DrawRectangle(x, y, w, h, bg);
        DrawRectangleLines(x, y, w, h, border);
    }

    static void DrawTextBold(Font font, const char *text, int x, int y, int fontSize, Color color) {
        DrawTextEx(font, text, { (float)x + 1, (float)y + 1 }, (float)fontSize, 1.0f, Color{ 0, 0, 0, 220 });
        DrawTextEx(font, text, { (float)x, (float)y }, (float)fontSize, 1.0f, color);
    }

    static void Render(Font font, const PlayerSpaceship &player, const VoidLeviathan &monster, const std::vector<std::unique_ptr<HazardBase>> &hazards, float timeRemaining) {
        DrawRectangle(0, 0, SCREEN_WIDTH, 56, Color{ 16, 22, 36, 245 });
        DrawLine(0, 56, SCREEN_WIDTH, 56, Color{ 48, 65, 98, 255 });

        DrawCard(15, 10, 180, 36, Color{ 24, 34, 56, 255 }, Color{ 56, 189, 248, 255 });
        DrawTextBold(font, "LOST PASSAGE", 25, 18, 18, Color{ 224, 242, 254, 255 });

        std::string secStr = "SECTOR 0" + std::to_string(player.currentSector);
        DrawCard(205, 10, 120, 36, Color{ 24, 34, 56, 255 }, Color{ 0, 220, 255, 200 });
        DrawTextBold(font, secStr.c_str(), 215, 19, 16, Color{ 56, 189, 248, 255 });

        std::stringstream ssTime;
        ssTime << "TIME: " << std::fixed << std::setprecision(1) << timeRemaining << "s";
        Color timeBg = (timeRemaining < 15.0f) ? Color{ 90, 25, 25, 255 } : Color{ 50, 45, 20, 255 };
        Color timeBorder = (timeRemaining < 15.0f) ? Color{ 240, 60, 60, 255 } : Color{ 250, 204, 21, 255 };
        DrawCard(335, 10, 150, 36, timeBg, timeBorder);
        DrawTextBold(font, ssTime.str().c_str(), 350, 18, 18, timeBorder);

        DrawCard(495, 10, 130, 36, Color{ 24, 34, 56, 255 }, Color{ 90, 110, 140, 255 });
        std::string distStr = std::to_string((int)player.distanceTraveled) + "m";
        DrawTextBold(font, distStr.c_str(), 510, 19, 16, RAYWHITE);

        DrawCard(635, 10, 120, 36, Color{ 45, 20, 32, 255 }, Color{ 255, 70, 120, 255 });
        std::string heartStr = "HEARTS: " + std::to_string(player.heartsCollected);
        DrawTextBold(font, heartStr.c_str(), 645, 19, 15, Color{ 255, 100, 150, 255 });

        DrawCard(765, 10, 120, 36, Color{ 20, 40, 40, 255 }, Color{ 45, 212, 191, 255 });
        std::string crysStr = "CRYSTALS: " + std::to_string(player.crystalsCollected);
        DrawTextBold(font, crysStr.c_str(), 775, 19, 14, Color{ 94, 234, 212, 255 });

        DrawCard(895, 10, 110, 36, Color{ 28, 36, 56, 255 }, Color{ 140, 160, 200, 255 });
        DrawTextBold(font, "[M] MENU", 910, 20, 14, Color{ 190, 215, 245, 255 });

        if (monster.pursuing) {
            float distToMonster = Vector2Distance(player.position, monster.position);
            int alertAlpha = (int)(fabs(sinf((float)GetTime() * 6.0f)) * 255.0f);
            DrawCard(SCREEN_WIDTH / 2 - 230, 68, 460, 34, Color{ 75, 15, 20, 220 }, Color{ 239, 68, 68, (unsigned char)alertAlpha });
            std::string threatStr = TextFormat("! WARNING: LEVIATHAN PURSUING (%dm) - BOOST [SHIFT/RMB] !", (int)distToMonster);
            DrawTextBold(font, threatStr.c_str(), SCREEN_WIDTH / 2 - (int)MeasureTextEx(font, threatStr.c_str(), 13, 1.0f).x / 2, 78, 13, Color{ 255, 140, 140, 255 });
        }

        int barY = SCREEN_HEIGHT - 95;
        DrawCard(20, barY - 10, 430, 85, Color{ 16, 22, 36, 245 }, Color{ 48, 65, 98, 255 });

        DrawTextBold(font, "SHIP HULL INTEGRITY", 32, barY + 2, 12, Color{ 215, 230, 250, 255 });
        float hullRatio = player.hull / player.maxHull;
        DrawRectangle(195, barY, 215, 14, Color{ 30, 42, 60, 255 });
        Color hullCol = (hullRatio > 0.35f) ? Color{ 34, 197, 94, 255 } : Color{ 239, 68, 68, 255 };
        DrawRectangle(195, barY, (int)(215 * hullRatio), 14, hullCol);

        DrawTextBold(font, "ENERGY CAPACITOR", 32, barY + 24, 12, Color{ 215, 230, 250, 255 });
        float energyRatio = player.energy / player.maxEnergy;
        DrawRectangle(195, barY + 22, 215, 14, Color{ 30, 42, 60, 255 });
        DrawRectangle(195, barY + 22, (int)(215 * energyRatio), 14, Color{ 2, 132, 199, 255 });

        DrawTextBold(font, "[E/MMB] Kinetic Shield  |  [Q/R] Nanite Repair  |  [SHIFT] Boost", 32, barY + 48, 11, Color{ 140, 220, 255, 255 });

        int radarX = SCREEN_WIDTH - 220;
        int radarY = 70;
        int radarW = 200;
        int radarH = 150;

        DrawCard(radarX, radarY, radarW, radarH, Color{ 14, 20, 34, 235 }, Color{ 56, 189, 248, 180 });
        DrawTextBold(font, "BVR SCANNER", radarX + 12, radarY + 10, 12, Color{ 56, 189, 248, 255 });
        DrawLine(radarX + 30, radarY + radarH / 2, radarX + radarW - 10, radarY + radarH / 2, Color{ 0, 140, 200, 110 });
        DrawLine(radarX + 60, radarY + 25, radarX + 60, radarY + radarH - 10, Color{ 0, 140, 200, 110 });
        DrawCircle(radarX + 60, radarY + radarH / 2, 4.0f, Color{ 34, 211, 238, 255 });

        if (monster.pursuing) {
            float mdx = monster.position.x - player.position.x;
            float rX = Clamp(radarX + 60.0f + (mdx / 1400.0f) * (radarW - 55.0f), (float)radarX + 10.0f, (float)radarX + radarW - 10.0f);
            float mdy = (monster.position.y - player.position.y) * 0.15f;
            float rY = Clamp(radarY + radarH / 2 + mdy, (float)radarY + 25.0f, (float)radarY + radarH - 12.0f);
            DrawCircle((int)rX, (int)rY, 6.0f, Color{ 255, 30, 60, 255 });
            DrawTextBold(font, "BEAST", (int)rX - 16, (int)rY - 14, 10, Color{ 255, 110, 110, 255 });
        }

        for (const auto &h : hazards) {
            if (!h->IsActive()) continue;
            float dx = h->GetPosition().x - player.position.x;
            if (dx > 0.0f && dx < 1400.0f) {
                float rX = radarX + 60.0f + (dx / 1400.0f) * (radarW - 75.0f);
                float dy = (h->GetPosition().y - player.position.y) * 0.15f;
                float rY = Clamp(radarY + radarH / 2 + dy, (float)radarY + 25.0f, (float)radarY + radarH - 12.0f);
                DrawCircle((int)rX, (int)rY, 4.0f, RED);
            }
        }
    }
};

// ---------------------------------------------------------
// Master Game Engine Class (Home UI, Hangar & Gameplay)
// ---------------------------------------------------------
class LostPassageGame {
private:
    GameState state;
    Difficulty difficulty;
    ShipType selectedShip;

    PlayerSpaceship player;
    VoidLeviathan monster;
    PassageGenerator passage;
    ParticleSystem particles;
    AudioSynthesizer audio;

    std::vector<std::unique_ptr<TrapBase>> traps;
    std::vector<std::unique_ptr<PickupBase>> pickups;
    std::vector<std::unique_ptr<HazardBase>> hazards;

    Camera2D camera;
    RenderTexture2D fogTexture;
    Font fontFredokaBold;
    bool fontLoadedCustom = false;

    float timeRemaining;
    float hazardSpawnTimer;
    float screenShake;
    int finalScore;
    std::string gameOverReason;

    // Home Interactive Buttons
    HoverButton btnCadet;
    HoverButton btnCommander;
    HoverButton btnVoidWalker;
    HoverButton btnApocalypse;
    HoverButton btnSelectShip;
    HoverButton btnLaunch;
    HoverButton btnAudioToggle;
    HoverButton btnExitGame;

    // Ship Selection Modal Buttons
    HoverButton btnShipValkyrie;
    HoverButton btnShipPhantom;
    HoverButton btnShipDreadnought;
    HoverButton btnShipConfirm;

    // Game Over Buttons
    HoverButton btnReplay;
    HoverButton btnHome;
    HoverButton btnExit;

public:
    LostPassageGame() {
        state = GameState::TITLE_MENU;
        difficulty = Difficulty::COMMANDER;
        selectedShip = ShipType::VALKYRIE;
        timeRemaining = 60.0f;
        hazardSpawnTimer = 3.0f;
        screenShake = 0.0f;
        finalScore = 0;
    }

    void Init() {
        InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "The Lost Passage: Void Runner");
        SetTargetFPS(60);

        audio.Init();
        fogTexture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

        if (FileExists("Fredoka-Bold.ttf")) {
            fontFredokaBold = LoadFontEx("Fredoka-Bold.ttf", 64, nullptr, 0);
            SetTextureFilter(fontFredokaBold.texture, TEXTURE_FILTER_BILINEAR);
            fontLoadedCustom = true;
        } else if (FileExists("resources/Fredoka-Bold.ttf")) {
            fontFredokaBold = LoadFontEx("resources/Fredoka-Bold.ttf", 64, nullptr, 0);
            SetTextureFilter(fontFredokaBold.texture, TEXTURE_FILTER_BILINEAR);
            fontLoadedCustom = true;
        } else {
            fontFredokaBold = GetFontDefault();
            fontLoadedCustom = false;
        }

        camera.offset = Vector2{ (float)SCREEN_WIDTH * 0.30f, (float)SCREEN_HEIGHT * 0.50f };
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        BuildHomeUI();
        ResetGame();
    }

    void BuildHomeUI() {
        // Compact smart difficulty buttons
        float diffY = 285.0f;
        float diffW = 140.0f;
        float diffH = 46.0f;

        btnCadet = { Rectangle{ 310, diffY, diffW, diffH }, "CADET", Color{ 20, 32, 48, 255 }, Color{ 45, 212, 191, 255 }, Color{ 45, 212, 191, 160 }, Color{ 255, 255, 255, 255 }, Color{ 94, 234, 212, 255 }, Color{ 10, 30, 30, 255 } };
        btnCommander = { Rectangle{ 465, diffY, diffW, diffH }, "CMD", Color{ 20, 32, 48, 255 }, Color{ 56, 189, 248, 255 }, Color{ 56, 189, 248, 160 }, Color{ 255, 255, 255, 255 }, Color{ 186, 230, 253, 255 }, Color{ 10, 25, 45, 255 } };
        btnVoidWalker = { Rectangle{ 620, diffY, diffW, diffH }, "VOID", Color{ 20, 32, 48, 255 }, Color{ 250, 204, 21, 255 }, Color{ 250, 204, 21, 160 }, Color{ 255, 255, 255, 255 }, Color{ 254, 240, 138, 255 }, Color{ 35, 30, 10, 255 } };
        btnApocalypse = { Rectangle{ 775, diffY, diffW, diffH }, "APOC", Color{ 20, 32, 48, 255 }, Color{ 239, 68, 68, 255 }, Color{ 239, 68, 68, 160 }, Color{ 255, 255, 255, 255 }, Color{ 254, 202, 202, 255 }, Color{ 40, 10, 15, 255 } };

        // Home action buttons
        btnSelectShip = { Rectangle{ 430, 380, 200, 52 }, "SELECT SHIP", Color{ 30, 28, 55, 255 }, Color{ 168, 85, 247, 255 }, Color{ 168, 85, 247, 200 }, Color{ 255, 255, 255, 255 }, Color{ 233, 213, 255, 255 }, Color{ 25, 10, 45, 255 } };
        btnLaunch     = { Rectangle{ 650, 380, 200, 52 }, "LAUNCH", Color{ 20, 55, 35, 255 }, Color{ 34, 197, 94, 255 }, Color{ 34, 197, 94, 220 }, Color{ 255, 255, 255, 255 }, Color{ 187, 247, 208, 255 }, Color{ 10, 30, 15, 255 } };

        btnAudioToggle = { Rectangle{ 430, 450, 200, 40 }, "AUDIO: ON", Color{ 25, 35, 52, 255 }, Color{ 100, 116, 139, 255 }, Color{ 71, 85, 105, 255 }, Color{ 255, 255, 255, 255 }, Color{ 203, 213, 225, 255 }, Color{ 15, 23, 42, 255 } };
        btnExitGame    = { Rectangle{ 650, 450, 200, 40 }, "EXIT", Color{ 45, 22, 26, 255 }, Color{ 239, 68, 68, 255 }, Color{ 239, 68, 68, 180 }, Color{ 255, 255, 255, 255 }, Color{ 254, 202, 202, 255 }, Color{ 35, 10, 15, 255 } };

        // Ship Selection Modal Buttons
        float shipCardY = 220.0f;
        btnShipValkyrie    = { Rectangle{ 230, shipCardY, 240, 260 }, "VALKYRIE-IX", Color{ 20, 30, 48, 255 }, Color{ 56, 189, 248, 255 }, Color{ 56, 189, 248, 180 }, Color{ 255, 255, 255, 255 }, Color{ 186, 230, 253, 255 }, Color{ 15, 23, 42, 255 } };
        btnShipPhantom     = { Rectangle{ 520, shipCardY, 240, 260 }, "PHANTOM",     Color{ 18, 38, 30, 255 }, Color{ 34, 197, 94, 255 }, Color{ 34, 197, 94, 180 }, Color{ 255, 255, 255, 255 }, Color{ 187, 247, 208, 255 }, Color{ 10, 30, 15, 255 } };
        btnShipDreadnought = { Rectangle{ 810, shipCardY, 240, 260 }, "DREADNOUGHT", Color{ 42, 28, 18, 255 }, Color{ 245, 158, 11, 255 }, Color{ 245, 158, 11, 180 }, Color{ 255, 255, 255, 255 }, Color{ 254, 240, 138, 255 }, Color{ 35, 20, 10, 255 } };
        btnShipConfirm     = { Rectangle{ SCREEN_WIDTH / 2 - 120, 520, 240, 50 }, "CONFIRM SHIP", Color{ 20, 55, 35, 255 }, Color{ 34, 197, 94, 255 }, Color{ 34, 197, 94, 255 }, Color{ 255, 255, 255, 255 }, Color{ 187, 247, 208, 255 }, Color{ 10, 30, 15, 255 } };

        // Game Over Buttons
        int dialogX = SCREEN_WIDTH / 2 - 280;
        int dialogY = SCREEN_HEIGHT / 2 - 200;
        int btnW = 150;
        int btnH = 50;
        int btnY = dialogY + 400 - 75;

        btnReplay = { Rectangle{ (float)dialogX + 35, (float)btnY, (float)btnW, (float)btnH }, "REPLAY", Color{ 20, 60, 40, 255 }, Color{ 34, 197, 94, 255 }, Color{ 34, 197, 94, 255 }, Color{ 187, 247, 208, 255 }, Color{ 74, 222, 128, 255 }, Color{ 10, 30, 20, 255 } };
        btnHome   = { Rectangle{ (float)dialogX + 205, (float)btnY, (float)btnW, (float)btnH }, "HOME", Color{ 24, 40, 65, 255 }, Color{ 56, 189, 248, 255 }, Color{ 56, 189, 248, 255 }, Color{ 224, 242, 254, 255 }, Color{ 125, 211, 252, 255 }, Color{ 10, 25, 45, 255 } };
        btnExit   = { Rectangle{ (float)dialogX + 375, (float)btnY, (float)btnW, (float)btnH }, "EXIT", Color{ 60, 20, 25, 255 }, Color{ 239, 68, 68, 255 }, Color{ 239, 68, 68, 255 }, Color{ 254, 202, 202, 255 }, Color{ 248, 113, 113, 255 }, Color{ 30, 10, 15, 255 } };
    }

    void Shutdown() {
        if (fontLoadedCustom) UnloadFont(fontFredokaBold);
        UnloadRenderTexture(fogTexture);
        audio.Close();
        CloseWindow();
    }

    void ResetGame() {
        float startHull = 100.0f;
        float baseTime = 60.0f;

        if (difficulty == Difficulty::CADET)            { startHull = 160.0f; baseTime = 90.0f; }
        else if (difficulty == Difficulty::COMMANDER)   { startHull = 100.0f; baseTime = 60.0f; }
        else if (difficulty == Difficulty::VOID_WALKER) { startHull = 70.0f;  baseTime = 45.0f; }
        else if (difficulty == Difficulty::APOCALYPSE)  { startHull = 50.0f;  baseTime = 35.0f; }

        player.Reset(startHull, selectedShip);
        monster.Reset();
        passage.Generate(difficulty);
        particles.Clear();

        traps.clear();
        pickups.clear();
        hazards.clear();

        timeRemaining = baseTime;
        hazardSpawnTimer = 3.0f;
        screenShake = 0.0f;

        for (size_t i = 0; i < passage.passage.size(); ++i) {
            const auto &slice = passage.passage[i];
            float x = slice.x;

            if (x > 600.0f && (i % 12 == 0)) {
                int trapChoice = rand() % 4;
                if (trapChoice == 0) {
                    traps.push_back(std::make_unique<StalactiteTrap>(Vector2{ x, slice.ceilingY() + 15.0f }, slice.floorY()));
                } else if (trapChoice == 1) {
                    float yOff = ((float)rand() / RAND_MAX - 0.5f) * (slice.halfWidth * 0.9f);
                    traps.push_back(std::make_unique<ProximityMineTrap>(Vector2{ x, slice.centerY + yOff }));
                } else if (trapChoice == 2) {
                    traps.push_back(std::make_unique<LaserTripwireTrap>(Vector2{ x, slice.centerY }, slice.ceilingY(), slice.floorY()));
                } else {
                    traps.push_back(std::make_unique<CrusherGateTrap>(Vector2{ x, slice.centerY }, slice.ceilingY(), slice.floorY(), slice.halfWidth));
                }
            }

            if (x > 400.0f && (i % 9 == 0)) {
                float yOff = ((float)rand() / RAND_MAX - 0.5f) * (slice.halfWidth * 0.7f);
                pickups.push_back(std::make_unique<ChronoCrystalPickup>(Vector2{ x, slice.centerY + yOff }));
            }

            if (x > 500.0f && (i % 16 == 5)) {
                float yOff = ((float)rand() / RAND_MAX - 0.5f) * (slice.halfWidth * 0.65f);
                pickups.push_back(std::make_unique<HeartPickup>(Vector2{ x, slice.centerY + yOff }));
            }
        }
    }

    void SpawnHazard() {
        float aheadX = player.position.x + 950.0f + (rand() % 350);
        PassageSlice slice = passage.GetSliceAt(aheadX);
        float cY = slice.centerY + ((float)rand() / RAND_MAX - 0.5f) * (slice.halfWidth * 0.8f);

        int choice = rand() % 3;
        float baseSpeed = 320.0f;
        if (difficulty == Difficulty::VOID_WALKER) baseSpeed = 440.0f;
        if (difficulty == Difficulty::APOCALYPSE)  baseSpeed = 520.0f;

        if (choice == 0) {
            hazards.push_back(std::make_unique<CruiseMissile>(Vector2{ aheadX, cY }, Vector2{ -baseSpeed * 1.25f, ((float)rand() / RAND_MAX - 0.5f) * 40.0f }));
        } else if (choice == 1) {
            hazards.push_back(std::make_unique<HeavyBomb>(Vector2{ aheadX, cY }, Vector2{ -baseSpeed * 0.85f, 50.0f }));
        } else {
            hazards.push_back(std::make_unique<SeekerDrone>(Vector2{ aheadX, cY }, Vector2{ -baseSpeed, 0.0f }));
        }
    }

    void Update(float dt) {
        float threat = 0.0f;
        if (monster.pursuing) {
            threat = Clamp(1.0f - (Vector2Distance(player.position, monster.position) / 900.0f), 0.0f, 1.0f);
        }
        audio.UpdateBGM(state == GameState::PLAYING, threat);

        bool isHovering = false;

        // -----------------------------------------------------
        // Title Screen & Hangar Modal Interactions
        // -----------------------------------------------------
        if (state == GameState::TITLE_MENU) {
            btnCadet.selected      = (difficulty == Difficulty::CADET);
            btnCommander.selected  = (difficulty == Difficulty::COMMANDER);
            btnVoidWalker.selected = (difficulty == Difficulty::VOID_WALKER);
            btnApocalypse.selected = (difficulty == Difficulty::APOCALYPSE);

            if (btnCadet.IsHovered() || btnCommander.IsHovered() || btnVoidWalker.IsHovered() || btnApocalypse.IsHovered() ||
                btnSelectShip.IsHovered() || btnLaunch.IsHovered() || btnAudioToggle.IsHovered() || btnExitGame.IsHovered()) {
                isHovering = true;
            }

            if (btnCadet.IsClicked())      difficulty = Difficulty::CADET;
            if (btnCommander.IsClicked())  difficulty = Difficulty::COMMANDER;
            if (btnVoidWalker.IsClicked()) difficulty = Difficulty::VOID_WALKER;
            if (btnApocalypse.IsClicked()) difficulty = Difficulty::APOCALYPSE;

            if (btnSelectShip.IsClicked()) {
                state = GameState::SHIP_SELECT_MODAL;
                return;
            }

            if (btnAudioToggle.IsClicked()) {
                audio.muted = !audio.muted;
                btnAudioToggle.text = audio.muted ? "AUDIO: OFF" : "AUDIO: ON";
            }

            if (btnExitGame.IsClicked() || IsKeyPressed(KEY_ESCAPE)) {
                CloseWindow();
                exit(0);
            }

            if (btnLaunch.IsClicked() || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                ResetGame();
                state = GameState::PLAYING;
                return;
            }
        } else if (state == GameState::SHIP_SELECT_MODAL) {
            btnShipValkyrie.selected    = (selectedShip == ShipType::VALKYRIE);
            btnShipPhantom.selected     = (selectedShip == ShipType::PHANTOM);
            btnShipDreadnought.selected = (selectedShip == ShipType::DREADNOUGHT);

            if (btnShipValkyrie.IsHovered() || btnShipPhantom.IsHovered() || btnShipDreadnought.IsHovered() || btnShipConfirm.IsHovered()) {
                isHovering = true;
            }

            if (btnShipValkyrie.IsClicked())    selectedShip = ShipType::VALKYRIE;
            if (btnShipPhantom.IsClicked())     selectedShip = ShipType::PHANTOM;
            if (btnShipDreadnought.IsClicked()) selectedShip = ShipType::DREADNOUGHT;

            if (btnShipConfirm.IsClicked() || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                state = GameState::TITLE_MENU;
                return;
            }
        }

        // -----------------------------------------------------
        // Game Over Dialog Buttons
        // -----------------------------------------------------
        if (state == GameState::GAME_OVER) {
            if (btnReplay.IsHovered() || btnHome.IsHovered() || btnExit.IsHovered()) {
                isHovering = true;
            }

            if (btnReplay.IsClicked() || IsKeyPressed(KEY_R)) {
                ResetGame();
                state = GameState::PLAYING;
                return;
            }
            if (btnHome.IsClicked() || IsKeyPressed(KEY_M)) {
                state = GameState::TITLE_MENU;
                return;
            }
            if (btnExit.IsClicked() || IsKeyPressed(KEY_ESCAPE)) {
                CloseWindow();
                exit(0);
            }
            return;
        }

        if (isHovering) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
        else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

        if (IsKeyPressed(KEY_M)) {
            state = GameState::TITLE_MENU;
            return;
        }

        if (state == GameState::PAUSED) {
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) state = GameState::PLAYING;
            return;
        }

        if (state == GameState::SECTOR_CLEARED) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_R) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ResetGame();
                state = GameState::PLAYING;
            }
            return;
        }

        if (state != GameState::PLAYING) return;

        if (IsKeyPressed(KEY_P)) {
            state = GameState::PAUSED;
            return;
        }

        timeRemaining -= dt;
        if (timeRemaining <= 0.0f) {
            timeRemaining = 0.0f;
            TriggerGameOver("CHRONO CORE DEPLETED - TIME EXPIRED");
            return;
        }

        player.ProcessInput(camera, dt, particles, audio);
        player.Update(dt, particles, audio);

        PassageSlice currentSlice = passage.GetSliceAt(player.position.x);
        if (player.position.y - player.radius < currentSlice.ceilingY()) {
            player.position.y = currentSlice.ceilingY() + player.radius;
            ApplyDamage(15.0f);
        } else if (player.position.y + player.radius > currentSlice.floorY()) {
            player.position.y = currentSlice.floorY() - player.radius;
            ApplyDamage(15.0f);
        }

        monster.Update(dt, particles, audio);
        float monsterSpeed = (difficulty == Difficulty::APOCALYPSE) ? 430.0f : (difficulty == Difficulty::VOID_WALKER) ? 385.0f : 340.0f;
        monster.FollowPlayer(player.position, monsterSpeed, dt);

        if (monster.pursuing && Vector2Distance(player.position, monster.position) < (player.radius + monster.radius - 8.0f)) {
            TriggerGameOver("DEVOUR-KILL BY VOID LEVIATHAN - OUTRUN FAILED");
            return;
        }

        for (auto &trap : traps) {
            if (!trap->IsActive()) continue;
            trap->OnPlayerProximity(player.position, audio);
            trap->Update(dt, particles, audio);

            if (Vector2Distance(player.position, trap->GetPosition()) < (player.radius + trap->GetRadius())) {
                ApplyDamage(trap->GetDamage());
                trap->Deactivate();
                for (int k = 0; k < 12; ++k) {
                    particles.Emit(trap->GetPosition(), { (float)(rand() % 240 - 120), (float)(rand() % 240 - 120) }, Color{ 255, 60, 40, 255 }, 5.0f, 0.6f);
                }
            }
        }

        for (auto &pickup : pickups) {
            if (!pickup->IsActive()) continue;
            pickup->Update(dt, particles, audio);

            if (Vector2Distance(player.position, pickup->GetPosition()) < (player.radius + pickup->GetRadius())) {
                pickup->MarkCollected();

                if (auto heart = dynamic_cast<HeartPickup*>(pickup.get())) {
                    player.hull = fminf(player.maxHull, player.hull + 30.0f);
                    player.heartsCollected++;
                    PlaySound(audio.sndHeart);
                    for (int k = 0; k < 16; ++k) {
                        particles.Emit(heart->GetPosition(), { (float)(rand() % 160 - 80), (float)(rand() % 160 - 80) }, Color{ 255, 50, 100, 255 }, 5.0f, 0.6f);
                    }
                } else if (auto crystal = dynamic_cast<ChronoCrystalPickup*>(pickup.get())) {
                    timeRemaining += 5.0f;
                    player.crystalsCollected++;
                    PlaySound(audio.sndCrystal);
                    for (int k = 0; k < 12; ++k) {
                        particles.Emit(crystal->GetPosition(), { (float)(rand() % 160 - 80), (float)(rand() % 160 - 80) }, Color{ 0, 255, 220, 255 }, 4.0f, 0.5f);
                    }
                }
            }
        }

        hazardSpawnTimer -= dt;
        if (hazardSpawnTimer <= 0.0f) {
            SpawnHazard();
            hazardSpawnTimer = (difficulty == Difficulty::APOCALYPSE) ? 1.4f : (difficulty == Difficulty::VOID_WALKER) ? 2.2f : 3.5f;
        }

        for (auto &hazard : hazards) {
            if (!hazard->IsActive()) continue;
            hazard->TrackTarget(player.position, dt);
            hazard->Update(dt, particles, audio);

            if (Vector2Distance(player.position, hazard->GetPosition()) < (player.radius + hazard->GetRadius())) {
                ApplyDamage(hazard->GetDamage());
                hazard->Deactivate();
                for (int k = 0; k < 20; ++k) {
                    particles.Emit(hazard->GetPosition(), { (float)(rand() % 300 - 150), (float)(rand() % 300 - 150) }, Color{ 255, 120, 20, 255 }, 6.0f, 0.7f);
                }
            }

            if (hazard->GetPosition().x < player.position.x - 600.0f) hazard->Deactivate();
        }

        particles.Update(dt);

        if (player.position.x >= passage.totalLength - 400.0f) {
            state = GameState::SECTOR_CLEARED;
            finalScore = (int)(player.distanceTraveled + player.crystalsCollected * 250 + player.heartsCollected * 300 + timeRemaining * 100 + 10000);
        }

        camera.target.x = player.position.x + 120.0f;
        camera.target.y = player.position.y;
        if (screenShake > 0.0f) {
            screenShake -= dt * 18.0f;
            camera.offset.x = SCREEN_WIDTH * 0.30f + (rand() % 20 - 10) * screenShake;
            camera.offset.y = SCREEN_HEIGHT * 0.50f + (rand() % 20 - 10) * screenShake;
        } else {
            camera.offset = Vector2{ (float)SCREEN_WIDTH * 0.30f, (float)SCREEN_HEIGHT * 0.50f };
        }
    }

    void ApplyDamage(float dmg) {
        if (player.shieldActive) {
            // Enhanced Kinetic Deflector: 85% kinetic negation
            player.energy = fmaxf(0.0f, player.energy - dmg * 0.6f);
            player.hull -= dmg * 0.15f;
            PlaySound(audio.sndShield);
            screenShake = 0.3f;
        } else {
            player.hull -= dmg;
            PlaySound(audio.sndHit);
            screenShake = 0.8f;
        }

        if (player.hull <= 0.0f) {
            player.hull = 0.0f;
            TriggerGameOver("HULL INTEGRITY COMPROMISED - FATAL BREACH");
        }
    }

    void TriggerGameOver(const std::string &reason) {
        state = GameState::GAME_OVER;
        gameOverReason = reason;
        finalScore = (int)(player.distanceTraveled + player.crystalsCollected * 250 + player.heartsCollected * 300);
        PlaySound(audio.sndGameOver);
    }

    void Draw() {
        BeginDrawing();
        ClearBackground(PATH_BG_PALE_YELLOW);

        if (state == GameState::TITLE_MENU || state == GameState::SHIP_SELECT_MODAL) {
            DrawTitleScreen();
            if (state == GameState::SHIP_SELECT_MODAL) DrawShipSelectModal();
            EndDrawing();
            return;
        }

        BeginMode2D(camera);
        passage.Draw(camera.target.x);

        for (const auto &p : pickups) p->Draw();
        for (const auto &t : traps)   t->Draw();
        for (const auto &h : hazards) h->Draw();

        monster.Draw();
        particles.Draw();
        player.Draw();
        EndMode2D();

        DrawDynamicFog();

        GameHUD::Render(fontFredokaBold, player, monster, hazards, timeRemaining);

        if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 8, 14, 26, 200 });
            GameHUD::DrawCard(SCREEN_WIDTH / 2 - 240, SCREEN_HEIGHT / 2 - 90, 480, 180, Color{ 20, 28, 45, 250 }, Color{ 0, 220, 255, 255 });
            GameHUD::DrawTextBold(fontFredokaBold, "MISSION PAUSED", SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 - 50, 28, RAYWHITE);
            GameHUD::DrawTextBold(fontFredokaBold, "Press [P] or [ESC] to Resume Flight", SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2 + 5, 16, Color{ 160, 200, 240, 255 });
        } else if (state == GameState::GAME_OVER) {
            DrawGameOverDialog();
        } else if (state == GameState::SECTOR_CLEARED) {
            DrawVictoryScreen();
        }

        EndDrawing();
    }

    void DrawDynamicFog() {
        BeginTextureMode(fogTexture);
        ClearBackground(Color{ 45, 40, 25, 210 });

        Vector2 screenPlayer = GetWorldToScreen2D(player.position, camera);
        float vR = player.visionRadius + (player.boosting ? 45.0f : 0.0f);
        DrawCircleGradient((int)screenPlayer.x, (int)screenPlayer.y, vR, BLANK, Color{ 45, 40, 25, 210 });

        if (player.radarPingActive) {
            DrawCircleLines((int)screenPlayer.x, (int)screenPlayer.y, player.radarPingRadius, Color{ 0, 255, 200, (unsigned char)(player.radarPingAlpha * 255.0f) });
        }

        if (monster.pursuing) {
            Vector2 screenMonster = GetWorldToScreen2D(monster.position, camera);
            DrawCircleGradient((int)screenMonster.x, (int)screenMonster.y, 110.0f, BLANK, Color{ 45, 40, 25, 210 });
        }

        for (const auto &p : pickups) {
            if (!p->IsActive()) continue;
            Vector2 sc = GetWorldToScreen2D(p->GetPosition(), camera);
            DrawCircleGradient((int)sc.x, (int)sc.y, 50.0f, BLANK, Color{ 45, 40, 25, 210 });
        }

        EndTextureMode();
        DrawTextureRec(fogTexture.texture, Rectangle{ 0.0f, 0.0f, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT }, Vector2{ 0.0f, 0.0f }, WHITE);
    }

    // ---------------------------------------------------------
    // Redesigned Clean Home UI (Smart Buttons & Grid)
    // ---------------------------------------------------------
    void DrawTitleScreen() {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, PATH_BG_PALE_YELLOW);

        for (int x = 0; x < SCREEN_WIDTH; x += 60) DrawLine(x, 0, x, SCREEN_HEIGHT, Color{ 235, 220, 175, 110 });
        for (int y = 0; y < SCREEN_HEIGHT; y += 60) DrawLine(0, y, SCREEN_WIDTH, y, Color{ 235, 220, 175, 110 });

        // Header Banner
        GameHUD::DrawCard(SCREEN_WIDTH / 2 - 380, 50, 760, 120, Color{ 20, 28, 44, 250 }, Color{ 56, 189, 248, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "THE LOST PASSAGE: VOID RUNNER", SCREEN_WIDTH / 2 - 250, 75, 34, Color{ 240, 249, 255, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "TACTICAL JET FLIGHT SIMULATOR · RAYLIB C++ ENGINE", SCREEN_WIDTH / 2 - 180, 130, 14, Color{ 56, 189, 248, 255 });

        // Main Home Command Deck Card
        GameHUD::DrawCard(SCREEN_WIDTH / 2 - 380, 195, 760, 325, Color{ 16, 24, 38, 245 }, Color{ 50, 70, 105, 255 });

        // Difficulty Selection Header
        GameHUD::DrawTextBold(fontFredokaBold, "OPERATIONAL DIFFICULTY", SCREEN_WIDTH / 2 - 95, 220, 16, Color{ 203, 213, 225, 255 });
        std::string diffDesc = (difficulty == Difficulty::CADET) ? "Cadet: 160 HP, 90s Timer, Wide Caverns" :
                               (difficulty == Difficulty::COMMANDER) ? "Commander: 100 HP, 60s Timer, Standard Traps" :
                               (difficulty == Difficulty::VOID_WALKER) ? "Void Walker: 70 HP, 45s Timer, Fast Hazards" :
                               "Apocalypse: 50 HP, 35s Timer, Swarm Barrage";
        DrawTextEx(fontFredokaBold, diffDesc.c_str(), { (float)SCREEN_WIDTH / 2.0f - MeasureTextEx(fontFredokaBold, diffDesc.c_str(), 13, 1.0f).x / 2.0f, 250 }, 13, 1.0f, Color{ 148, 163, 184, 255 });

        // Draw 4 Compact Difficulty Buttons
        btnCadet.Draw(fontFredokaBold, 15);
        btnCommander.Draw(fontFredokaBold, 15);
        btnVoidWalker.Draw(fontFredokaBold, 15);
        btnApocalypse.Draw(fontFredokaBold, 15);

        // Active Space Ship Profile Preview
        std::string sName = (selectedShip == ShipType::VALKYRIE) ? "ACTIVE CRAFT: VALKYRIE-IX (BALANCED)" :
                            (selectedShip == ShipType::PHANTOM)   ? "ACTIVE CRAFT: PHANTOM (HIGH SPEED)" : "ACTIVE CRAFT: DREADNOUGHT-V (HEAVY ARMOR)";
        Color sCol = (selectedShip == ShipType::VALKYRIE) ? Color{ 56, 189, 248, 255 } :
                     (selectedShip == ShipType::PHANTOM)   ? Color{ 74, 222, 128, 255 } : Color{ 251, 146, 60, 255 };

        DrawTextEx(fontFredokaBold, sName.c_str(), { (float)SCREEN_WIDTH / 2.0f - MeasureTextEx(fontFredokaBold, sName.c_str(), 14, 1.0f).x / 2.0f, 348 }, 14, 1.0f, sCol);

        // Core Action Buttons
        btnSelectShip.Draw(fontFredokaBold, 16);
        btnLaunch.Draw(fontFredokaBold, 18);
        btnAudioToggle.Draw(fontFredokaBold, 14);
        btnExitGame.Draw(fontFredokaBold, 15);

        // Quick Controls Footer
        GameHUD::DrawCard(SCREEN_WIDTH / 2 - 380, 535, 760, 100, Color{ 20, 28, 44, 245 }, Color{ 48, 65, 98, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "SUBSYSTEM CONTROLS", SCREEN_WIDTH / 2 - 80, 545, 14, Color{ 56, 189, 248, 255 });
        DrawTextEx(fontFredokaBold, "- STEER: WASD / Arrow Keys OR Cursor Drag  |  SPRINT / BOOST: Hold [LEFT SHIFT] or [RMB]", { (float)SCREEN_WIDTH / 2 - 340, 570 }, 12, 1.0f, RAYWHITE);
        DrawTextEx(fontFredokaBold, "- KINETIC SHIELD: Hold [E] or [MMB]  |  NANITE DAMAGE CONTROL: Hold [Q] or [R] (Repairs Hull)", { (float)SCREEN_WIDTH / 2 - 340, 592 }, 12, 1.0f, Color{ 148, 163, 184, 255 });
        DrawTextEx(fontFredokaBold, "- RECON RADAR: Press [SPACE]  |  COLLECT: Chrono-Crystals (+5s) and Red Hearts (+30 HP)", { (float)SCREEN_WIDTH / 2 - 340, 614 }, 12, 1.0f, Color{ 250, 204, 21, 255 });
    }

    // ---------------------------------------------------------
    // Ship Selection Hangar Modal
    // ---------------------------------------------------------
    void DrawShipSelectModal() {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 10, 14, 24, 230 });

        GameHUD::DrawCard(SCREEN_WIDTH / 2 - 470, 100, 940, 500, Color{ 15, 23, 42, 252 }, Color{ 168, 85, 247, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "STARFLEET HANGAR - SELECT YOUR CRAFT", SCREEN_WIDTH / 2 - 220, 130, 24, Color{ 240, 249, 255, 255 });
        DrawTextEx(fontFredokaBold, "Choose a ship calibrated for your tactical playstyle", { (float)SCREEN_WIDTH / 2 - 150, 165 }, 13, 1.0f, Color{ 192, 132, 252, 255 });

        // Ship 1: Valkyrie-IX
        btnShipValkyrie.Draw(fontFredokaBold, 16);
        PlayerSpaceship::DrawShipGraphics(Vector2{ 350, 310 }, -PI / 2.0f, ShipType::VALKYRIE, false, false);
        DrawTextEx(fontFredokaBold, "Class: Interceptor\nArmor: Standard\nEnergy: Balanced\nTop Speed: 530 px/s", { 260, 395 }, 12, 1.0f, Color{ 186, 230, 253, 255 });

        // Ship 2: Phantom
        btnShipPhantom.Draw(fontFredokaBold, 16);
        PlayerSpaceship::DrawShipGraphics(Vector2{ 640, 310 }, -PI / 2.0f, ShipType::PHANTOM, false, false);
        DrawTextEx(fontFredokaBold, "Class: Stealth Raider\nArmor: Lightweight\nEnergy: 135% Capacitor\nTop Speed: 590 px/s", { 550, 395 }, 12, 1.0f, Color{ 187, 247, 208, 255 });

        // Ship 3: Dreadnought
        btnShipDreadnought.Draw(fontFredokaBold, 16);
        PlayerSpaceship::DrawShipGraphics(Vector2{ 930, 310 }, -PI / 2.0f, ShipType::DREADNOUGHT, false, false);
        DrawTextEx(fontFredokaBold, "Class: Heavy Cruiser\nArmor: Reinforced (+45%)\nEnergy: Standard\nTop Speed: 475 px/s", { 840, 395 }, 12, 1.0f, Color{ 254, 240, 138, 255 });

        btnShipConfirm.Draw(fontFredokaBold, 18);
    }

    void DrawGameOverDialog() {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 15, 18, 28, 215 });

        int dialogW = 560;
        int dialogH = 400;
        int dialogX = SCREEN_WIDTH / 2 - dialogW / 2;
        int dialogY = SCREEN_HEIGHT / 2 - dialogH / 2;

        DrawRectangleRounded(Rectangle{ (float)dialogX + 6, (float)dialogY + 6, (float)dialogW, (float)dialogH }, 0.08f, 6, Color{ 0, 0, 0, 160 });
        DrawRectangleRounded(Rectangle{ (float)dialogX, (float)dialogY, (float)dialogW, (float)dialogH }, 0.08f, 6, Color{ 22, 28, 42, 252 });
        DrawRectangleRoundedLines(Rectangle{ (float)dialogX, (float)dialogY, (float)dialogW, (float)dialogH }, 0.08f, 6, 3.0f, Color{ 239, 68, 68, 255 });

        Vector2 titleSize = MeasureTextEx(fontFredokaBold, "GAME OVER", 38, 1.0f);
        Vector2 titlePos = { (float)dialogX + (dialogW - titleSize.x) / 2.0f, (float)dialogY + 28.0f };
        DrawTextEx(fontFredokaBold, "GAME OVER", { titlePos.x + 2, titlePos.y + 2 }, 38, 1.0f, Color{ 0, 0, 0, 240 });
        DrawTextEx(fontFredokaBold, "GAME OVER", titlePos, 38, 1.0f, Color{ 255, 75, 75, 255 });

        Vector2 reasonSize = MeasureTextEx(fontFredokaBold, gameOverReason.c_str(), 14, 1.0f);
        Vector2 reasonPos = { (float)dialogX + (dialogW - reasonSize.x) / 2.0f, (float)dialogY + 75.0f };
        DrawRectangle((int)dialogX + 25, (int)dialogY + 72, dialogW - 50, 26, Color{ 60, 20, 25, 230 });
        DrawRectangleLines((int)dialogX + 25, (int)dialogY + 72, dialogW - 50, 26, Color{ 239, 68, 68, 160 });
        DrawTextEx(fontFredokaBold, gameOverReason.c_str(), reasonPos, 14, 1.0f, Color{ 254, 202, 202, 255 });

        int statY = dialogY + 115;
        auto DrawStatRow = [&](const char *label, const char *value, int yOffset, Color valCol) {
            DrawTextEx(fontFredokaBold, label, { (float)dialogX + 50, (float)statY + yOffset }, 16, 1.0f, Color{ 200, 215, 235, 255 });
            DrawTextEx(fontFredokaBold, value, { (float)dialogX + (float)dialogW - 170, (float)statY + yOffset }, 16, 1.0f, valCol);
            DrawLine(dialogX + 45, statY + yOffset + 24, dialogX + dialogW - 45, statY + yOffset + 24, Color{ 40, 50, 75, 120 });
        };

        DrawStatRow("Flight Distance:", TextFormat("%d m", (int)player.distanceTraveled), 0, RAYWHITE);
        DrawStatRow("Chrono-Crystals:", TextFormat("%d", player.crystalsCollected), 30, Color{ 94, 234, 212, 255 });
        DrawStatRow("Life Hearts Saved:", TextFormat("%d", player.heartsCollected), 60, Color{ 255, 110, 160, 255 });
        DrawStatRow("Total Score:", TextFormat("%d PTS", finalScore), 92, Color{ 250, 204, 21, 255 });

        btnReplay.Draw(fontFredokaBold, 18);
        btnHome.Draw(fontFredokaBold, 18);
        btnExit.Draw(fontFredokaBold, 18);
    }

    void DrawVictoryScreen() {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 12, 18, 30, 220 });

        int boxW = 600;
        int boxH = 420;
        int boxX = SCREEN_WIDTH / 2 - boxW / 2;
        int boxY = SCREEN_HEIGHT / 2 - boxH / 2;

        GameHUD::DrawCard(boxX, boxY, boxW, boxH, Color{ 20, 28, 44, 250 }, Color{ 45, 212, 191, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "SECTOR RUN COMPLETED", boxX + boxW / 2 - 130, boxY + 30, 24, Color{ 45, 212, 191, 255 });

        int statY = boxY + 115;
        GameHUD::DrawTextBold(fontFredokaBold, TextFormat("FINAL DEPTH: %d m", (int)player.distanceTraveled), boxX + 60, statY, 16, RAYWHITE);
        GameHUD::DrawTextBold(fontFredokaBold, TextFormat("TIME REMAINING: %.1f s", timeRemaining), boxX + 60, statY + 32, 16, RAYWHITE);
        GameHUD::DrawTextBold(fontFredokaBold, TextFormat("HEARTS COLLECTED: %d", player.heartsCollected), boxX + 60, statY + 64, 16, Color{ 255, 100, 150, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, TextFormat("SCORE: %d PTS", finalScore), boxX + 60, statY + 135, 22, Color{ 250, 204, 21, 255 });

        GameHUD::DrawCard(boxX + 40, boxY + boxH - 65, 250, 45, Color{ 28, 38, 60, 255 }, Color{ 56, 189, 248, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "[M] RETURN TO MENU", boxX + 70, boxY + boxH - 50, 14, Color{ 56, 189, 248, 255 });

        GameHUD::DrawCard(boxX + 310, boxY + boxH - 65, 250, 45, Color{ 25, 50, 40, 255 }, Color{ 34, 197, 94, 255 });
        GameHUD::DrawTextBold(fontFredokaBold, "[R] RE-ENGAGE FLIGHT", boxX + 340, boxY + boxH - 50, 14, Color{ 74, 222, 128, 255 });
    }
};

// ---------------------------------------------------------
// Main Entry Point
// ---------------------------------------------------------
int main() {
    srand((unsigned int)time(nullptr));

    LostPassageGame game;
    game.Init();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        game.Update(dt);
        game.Draw();
    }

    game.Shutdown();
    return 0;
}
