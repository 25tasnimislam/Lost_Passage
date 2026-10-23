/*******************************************************************************************
*
*   THE LOST PASSAGE - 2D Procedural Cavern Jet Runner with Raylib (C++17)
*
*   Features:
*     - Player Shape: Supersonic Delta-Wing Jet Fighter with twin afterburners & cockpit
*     - Enemy Shapes: Guided Cruise Missiles with smoke trails, Aerial Bombs, Proximity Spiky Mines
*     - Life Pickups: Floating Red Hearts that restore Hull Integrity (+30 HP)
*     - Full Menu System with easy return [M] Key after finishing / Game Over
*     - 4 Hardness Difficulty Levels:
*         [1] CADET (Recon - Wide caverns, gentle hazards, 160 HP, 90s timer)
*         [2] COMMANDER (Tactical - Standard hazards & traps, 100 HP, 60s timer)
*         [3] VOID WALKER (Hardcore - Narrow chokepoints, high speed missiles, 70 HP, 45s timer)
*         [4] APOCALYPSE (Insane - Swarms of homing missiles & cluster bombs, 50 HP, 35s timer)
*     - Clean, High-Contrast Typography with dark backing plates for ultra-clear readability
*     - Procedural audio sound effects synthesized directly in code (no external assets needed)
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
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------
// Constants & Enums
// ---------------------------------------------------------
const int SCREEN_WIDTH  = 1280;
const int SCREEN_HEIGHT = 720;

enum class GameState {
    TITLE_MENU,
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

enum class TrapType {
    STALACTITE_FALLING,
    PROXIMITY_MINE,
    LASER_TRIPWIRE,
    CRUSHER_GATE
};

enum class HazardType {
    CRUISE_MISSILE,
    HEAVY_BOMB,
    SEEKER_DRONE
};

// ---------------------------------------------------------
// Data Structures
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

struct PassageSlice {
    float x;
    float centerY;
    float halfWidth;
    float ceilingY() const { return centerY - halfWidth; }
    float floorY() const   { return centerY + halfWidth; }
};

struct Trap {
    TrapType type;
    Vector2 position;
    Vector2 originalPos;
    float radius;
    bool triggered;
    bool active;
    float timer;
    float cycleRate;
    float alertDistance;
    Vector2 velocity;
};

struct ChronoCrystal {
    Vector2 position;
    bool collected;
    float timeBonus; // +5.0s
    float pulsePhase;
};

struct HeartPickup {
    Vector2 position;
    bool collected;
    float healAmount; // +30 HP
    float pulsePhase;
    float floatOffset;
};

struct EnemyHazard {
    HazardType type;
    Vector2 position;
    Vector2 velocity;
    float radius;
    bool active;
    float threatLevel;
    float animTimer;
    std::string signatureName;
};

struct PlayerJet {
    Vector2 position;
    Vector2 velocity;
    float angle;
    float radius;

    // Strategic Vital Stats
    float hull;
    float maxHull;
    float energy;
    float maxEnergy;

    // Systems
    bool shieldActive;
    float boostCooldown;
    bool boosting;
    bool damageControlActive;
    float damageControlTimer;

    // Sensor / Fog
    float visionRadius;
    float radarPingRadius;
    bool radarPingActive;
    float radarPingAlpha;

    // Score & Telemetry
    float distanceTraveled;
    int crystalsCollected;
    int heartsCollected;
    int trapsEvaded;
    int currentSector;
};

struct LeaderboardEntry {
    std::string pilot;
    int score;
    float distance;
    std::string difficulty;
    std::string date;
};

// ---------------------------------------------------------
// Procedural Synthetic Audio (No external WAV required!)
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

    void Init() {
        InitAudioDevice();
        sndBoost   = GenerateTone(140, 220, 0.25f, 0.35f, 1);
        sndShield  = GenerateTone(350, 480, 0.30f, 0.30f, 0);
        sndHit     = GenerateNoise(0.28f, 0.45f);
        sndCrystal = GenerateTone(880, 1320, 0.20f, 0.25f, 0);
        sndHeart   = GenerateTone(520, 1040, 0.30f, 0.30f, 0); // Joyous ascending chord
        sndPing    = GenerateTone(600, 950, 0.40f, 0.30f, 0);
        sndAlarm   = GenerateTone(440, 330, 0.15f, 0.35f, 2);
        sndRepair  = GenerateTone(520, 680, 0.35f, 0.25f, 0);
    }

    void Close() {
        UnloadSound(sndBoost);
        UnloadSound(sndShield);
        UnloadSound(sndHit);
        UnloadSound(sndCrystal);
        UnloadSound(sndHeart);
        UnloadSound(sndPing);
        UnloadSound(sndAlarm);
        UnloadSound(sndRepair);
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

            if (waveType == 0) { // Sine
                sampleVal = sinf(2.0f * PI * freq * timeSec);
            } else if (waveType == 1) { // Triangle / Saw
                sampleVal = fmodf(freq * timeSec, 1.0f) * 2.0f - 1.0f;
            } else { // Square
                sampleVal = (sinf(2.0f * PI * freq * timeSec) > 0.0f) ? 0.7f : -0.7f;
            }

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
};

// ---------------------------------------------------------
// Global Game Engine Class
// ---------------------------------------------------------
class LostPassageGame {
public:
    GameState state;
    Difficulty difficulty;
    PlayerJet player;
    Camera2D camera;

    std::vector<PassageSlice> passage;
    std::vector<Trap> traps;
    std::vector<ChronoCrystal> crystals;
    std::vector<HeartPickup> hearts;
    std::vector<EnemyHazard> hazards;
    std::vector<Particle> particles;
    std::vector<LeaderboardEntry> leaderboard;

    RenderTexture2D fogTexture;
    AudioSynthesizer audio;

    float passageLength;
    float timeRemaining;
    float totalElapsedTime;
    float hazardSpawnTimer;
    float screenShake;
    int finalScore;
    std::string gameOverReason;

    LostPassageGame() {
        state = GameState::TITLE_MENU;
        difficulty = Difficulty::COMMANDER;
        passageLength = 12000.0f;
        screenShake = 0.0f;
        finalScore = 0;
        gameOverReason = "";
    }

    void Init() {
        InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "The Lost Passage - Cavern Jet Flight Engine");
        SetTargetFPS(60);

        audio.Init();
        fogTexture = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

        camera.offset = Vector2{ (float)SCREEN_WIDTH * 0.30f, (float)SCREEN_HEIGHT * 0.50f };
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        LoadLeaderboard();
        ResetGame();
    }

    void Shutdown() {
        UnloadRenderTexture(fogTexture);
        audio.Close();
        CloseWindow();
    }

    void ResetGame() {
        float maxH = 100.0f;
        float baseTime = 60.0f;

        if (difficulty == Difficulty::CADET) {
            maxH = 160.0f;
            baseTime = 90.0f;
        } else if (difficulty == Difficulty::COMMANDER) {
            maxH = 100.0f;
            baseTime = 60.0f;
        } else if (difficulty == Difficulty::VOID_WALKER) {
            maxH = 70.0f;
            baseTime = 45.0f;
        } else if (difficulty == Difficulty::APOCALYPSE) {
            maxH = 50.0f;
            baseTime = 35.0f;
        }

        player.position = { 200.0f, (float)SCREEN_HEIGHT / 2.0f };
        player.velocity = { 0.0f, 0.0f };
        player.angle = 0.0f;
        player.radius = 18.0f;
        player.maxHull = maxH;
        player.hull = maxH;
        player.maxEnergy = 100.0f;
        player.energy = 100.0f;
        player.shieldActive = false;
        player.boostCooldown = 0.0f;
        player.boosting = false;
        player.damageControlActive = false;
        player.damageControlTimer = 0.0f;
        player.visionRadius = 250.0f;
        player.radarPingRadius = 0.0f;
        player.radarPingActive = false;
        player.radarPingAlpha = 0.0f;
        player.distanceTraveled = 0.0f;
        player.crystalsCollected = 0;
        player.heartsCollected = 0;
        player.trapsEvaded = 0;
        player.currentSector = 1;

        timeRemaining = baseTime;
        totalElapsedTime = 0.0f;
        hazardSpawnTimer = 3.0f;
        screenShake = 0.0f;

        GeneratePassage();
        particles.clear();
    }

    void GeneratePassage() {
        passage.clear();
        traps.clear();
        crystals.clear();
        hearts.clear();
        hazards.clear();

        const float sliceStep = 25.0f;
        const int totalSlices = (int)(passageLength / sliceStep);

        float minWidth = 140.0f;
        float maxWidth = 260.0f;
        if (difficulty == Difficulty::CADET) {
            minWidth = 190.0f;
            maxWidth = 330.0f;
        } else if (difficulty == Difficulty::COMMANDER) {
            minWidth = 140.0f;
            maxWidth = 260.0f;
        } else if (difficulty == Difficulty::VOID_WALKER) {
            minWidth = 110.0f;
            maxWidth = 210.0f;
        } else if (difficulty == Difficulty::APOCALYPSE) {
            minWidth = 95.0f;
            maxWidth = 185.0f;
        }

        for (int i = 0; i < totalSlices; ++i) {
            float x = i * sliceStep;

            // Multi-frequency harmonic curves for natural subterranean passages
            float wave1 = sinf(x * 0.0018f) * 160.0f;
            float wave2 = cosf(x * 0.0045f) * 75.0f;
            float wave3 = sinf(x * 0.0090f) * 35.0f;
            float centerY = (float)SCREEN_HEIGHT / 2.0f + wave1 + wave2 + wave3;

            centerY = Clamp(centerY, 180.0f, (float)SCREEN_HEIGHT - 180.0f);

            float widthWave = sinf(x * 0.003f) * 0.5f + 0.5f;
            float halfWidth = Lerp(minWidth, maxWidth, widthWave);

            PassageSlice slice = { x, centerY, halfWidth };
            passage.push_back(slice);

            // Populate Traps every ~250px after initial safe zone (x > 600px)
            if (x > 600.0f && (i % 12 == 0)) {
                int trapChoice = rand() % 4;
                Trap t;
                t.active = true;
                t.triggered = false;
                t.timer = 0.0f;
                t.cycleRate = 1.0f + ((float)rand() / RAND_MAX) * 1.5f;
                t.velocity = { 0.0f, 0.0f };

                if (trapChoice == 0) {
                    // Falling Stalactite attached to ceiling
                    t.type = TrapType::STALACTITE_FALLING;
                    t.position = { x, slice.ceilingY() + 15.0f };
                    t.originalPos = t.position;
                    t.radius = 18.0f;
                    t.alertDistance = 180.0f;
                } else if (trapChoice == 1) {
                    // Proximity Spiky Mine
                    t.type = TrapType::PROXIMITY_MINE;
                    float yOffset = ((float)rand() / RAND_MAX - 0.5f) * (halfWidth * 0.9f);
                    t.position = { x, centerY + yOffset };
                    t.originalPos = t.position;
                    t.radius = 17.0f;
                    t.alertDistance = 150.0f;
                } else if (trapChoice == 2) {
                    // Laser Tripwire oscillating between floor and ceiling
                    t.type = TrapType::LASER_TRIPWIRE;
                    t.position = { x, centerY };
                    t.originalPos = t.position;
                    t.radius = halfWidth;
                    t.alertDistance = 220.0f;
                } else {
                    // Hydraulic crusher gate
                    t.type = TrapType::CRUSHER_GATE;
                    t.position = { x, centerY };
                    t.originalPos = t.position;
                    t.radius = halfWidth;
                    t.alertDistance = 180.0f;
                }
                traps.push_back(t);
            }

            // Populate Chrono-Crystals (+5s Time Extension)
            if (x > 400.0f && (i % 9 == 0)) {
                ChronoCrystal c;
                float yOffset = ((float)rand() / RAND_MAX - 0.5f) * (halfWidth * 0.7f);
                c.position = { x, centerY + yOffset };
                c.collected = false;
                c.timeBonus = 5.0f;
                c.pulsePhase = (float)rand() / RAND_MAX * 6.28f;
                crystals.push_back(c);
            }

            // Life (Hearts) Pickups (+30 HP Recovery) scattered along the passage
            if (x > 500.0f && (i % 16 == 5)) {
                HeartPickup hp;
                float yOffset = ((float)rand() / RAND_MAX - 0.5f) * (halfWidth * 0.65f);
                hp.position = { x, centerY + yOffset };
                hp.collected = false;
                hp.healAmount = 30.0f;
                hp.pulsePhase = (float)rand() / RAND_MAX * 6.28f;
                hp.floatOffset = 0.0f;
                hearts.push_back(hp);
            }
        }
    }

    void SpawnEnemyHazard() {
        EnemyHazard h;
        float aheadX = player.position.x + 950.0f + (float)(rand() % 350);
        
        float cY = (float)SCREEN_HEIGHT / 2.0f;
        for (const auto &s : passage) {
            if (fabs(s.x - aheadX) < 40.0f) {
                cY = s.centerY + ((float)rand() / RAND_MAX - 0.5f) * (s.halfWidth * 0.8f);
                break;
            }
        }

        h.position = { aheadX, cY };
        int choice = rand() % 3;
        float baseSpeed = 320.0f;
        if (difficulty == Difficulty::VOID_WALKER) baseSpeed = 440.0f;
        if (difficulty == Difficulty::APOCALYPSE) baseSpeed = 520.0f;

        if (choice == 0) {
            // Cruise Missile: fast, straight, high kinetic damage
            h.type = HazardType::CRUISE_MISSILE;
            h.velocity = { -baseSpeed * 1.25f, ((float)rand() / RAND_MAX - 0.5f) * 40.0f };
            h.radius = 16.0f;
            h.threatLevel = 4.0f;
            h.signatureName = "CRUISE-MISSILE";
        } else if (choice == 1) {
            // Heavy Bomb: slower, massive blast radius
            h.type = HazardType::HEAVY_BOMB;
            h.velocity = { -baseSpeed * 0.85f, 50.0f }; // arcs downward
            h.radius = 20.0f;
            h.threatLevel = 5.0f;
            h.signatureName = "HEAVY-BOMB";
        } else {
            // Seeker Drone: drifts toward jet altitude
            h.type = HazardType::SEEKER_DRONE;
            h.velocity = { -baseSpeed, 0.0f };
            h.radius = 18.0f;
            h.threatLevel = 3.0f;
            h.signatureName = "SEEKER-DRONE";
        }

        h.active = true;
        h.animTimer = 0.0f;
        hazards.push_back(h);
    }

    void TriggerRadarPing() {
        if (player.energy >= 15.0f && !player.radarPingActive) {
            player.energy -= 15.0f;
            player.radarPingActive = true;
            player.radarPingRadius = 20.0f;
            player.radarPingAlpha = 1.0f;
            PlaySound(audio.sndPing);
        }
    }

    void Update(float dt) {
        // Return to Menu from anywhere via [M] Key
        if (IsKeyPressed(KEY_M)) {
            state = GameState::TITLE_MENU;
            return;
        }

        if (state == GameState::TITLE_MENU) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                ResetGame();
                state = GameState::PLAYING;
            }
            if (IsKeyPressed(KEY_ONE))   difficulty = Difficulty::CADET;
            if (IsKeyPressed(KEY_TWO))   difficulty = Difficulty::COMMANDER;
            if (IsKeyPressed(KEY_THREE)) difficulty = Difficulty::VOID_WALKER;
            if (IsKeyPressed(KEY_FOUR))  difficulty = Difficulty::APOCALYPSE;
            return;
        }

        if (state == GameState::PAUSED) {
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                state = GameState::PLAYING;
            }
            return;
        }

        if (state == GameState::GAME_OVER || state == GameState::SECTOR_CLEARED) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_R)) {
                ResetGame();
                state = GameState::PLAYING;
            }
            return;
        }

        if (IsKeyPressed(KEY_P)) {
            state = GameState::PAUSED;
            return;
        }

        // -----------------------------------------------------
        // Player Input & Physics
        // -----------------------------------------------------
        totalElapsedTime += dt;
        timeRemaining -= dt;
        if (timeRemaining <= 0.0f) {
            timeRemaining = 0.0f;
            TriggerGameOver("CHRONO CORE DEPLETED - TIME EXPIRED");
            return;
        }

        Vector2 inputDir = { 0.0f, 0.0f };
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) inputDir.x += 1.0f;
        if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) inputDir.x -= 0.6f;
        if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) inputDir.y -= 1.0f;
        if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) inputDir.y += 1.0f;

        // Speed Booster / Afterburners (Shift / Space / Boost)
        float baseSpeed = 270.0f;
        player.boosting = false;
        if (IsKeyDown(KEY_LEFT_SHIFT) && player.energy > 5.0f) {
            baseSpeed = 500.0f;
            player.energy -= 22.0f * dt;
            player.boosting = true;

            // Twin afterburner flame trail
            Vector2 flameOffset1 = Vector2{ cosf(player.angle) * 20.0f + sinf(player.angle) * 7.0f, sinf(player.angle) * 20.0f - cosf(player.angle) * 7.0f };
            Vector2 flameOffset2 = Vector2{ cosf(player.angle) * 20.0f - sinf(player.angle) * 7.0f, sinf(player.angle) * 20.0f + cosf(player.angle) * 7.0f };
            Vector2 flamePos1 = Vector2Subtract(player.position, flameOffset1);
            Vector2 flamePos2 = Vector2Subtract(player.position, flameOffset2);

            SpawnParticle(flamePos1, Vector2{ -cosf(player.angle) * 220.0f + (float)(rand() % 50 - 25), -sinf(player.angle) * 220.0f + (float)(rand() % 50 - 25) }, Color{ 0, 220, 255, 255 }, 4.5f, 0.35f);
            SpawnParticle(flamePos2, Vector2{ -cosf(player.angle) * 220.0f + (float)(rand() % 50 - 25), -sinf(player.angle) * 220.0f + (float)(rand() % 50 - 25) }, Color{ 255, 140, 20, 255 }, 4.5f, 0.35f);

            if (!IsSoundPlaying(audio.sndBoost)) PlaySound(audio.sndBoost);
        }

        // Kinetic Shield Toggle (E key)
        if (IsKeyDown(KEY_E) && player.energy > 8.0f) {
            player.shieldActive = true;
            player.energy -= 18.0f * dt;
        } else {
            player.shieldActive = false;
        }

        // Active Nanite Hull Repair (Q key)
        if (IsKeyDown(KEY_Q) && player.energy > 20.0f && player.hull < player.maxHull) {
            player.damageControlActive = true;
            player.energy -= 30.0f * dt;
            player.hull = fminf(player.maxHull, player.hull + 18.0f * dt);
            player.damageControlTimer += dt;

            // Nanite green healing sparks
            SpawnParticle(
                Vector2{ player.position.x + (float)(rand() % 28 - 14), player.position.y + (float)(rand() % 28 - 14) },
                Vector2{ 0.0f, -40.0f },
                Color{ 50, 255, 120, 255 }, 3.5f, 0.4f
            );
        } else {
            player.damageControlActive = false;
        }

        // BVR Radar Ping (Space key)
        if (IsKeyPressed(KEY_SPACE)) {
            TriggerRadarPing();
        }

        // Energy Core Regeneration
        float regenRate = (difficulty == Difficulty::CADET) ? 15.0f : 10.0f;
        if (!player.boosting && !player.shieldActive && !player.damageControlActive) {
            player.energy = fminf(player.maxEnergy, player.energy + regenRate * dt);
        }

        // Jet Aerodynamics & Velocity
        Vector2 targetVel = Vector2Scale(Vector2Normalize(inputDir), baseSpeed);
        player.velocity = Vector2Lerp(player.velocity, targetVel, 8.5f * dt);
        player.position = Vector2Add(player.position, Vector2Scale(player.velocity, dt));

        if (Vector2Length(player.velocity) > 10.0f) {
            float targetAngle = atan2f(player.velocity.y, player.velocity.x);
            player.angle = Lerp(player.angle, targetAngle, 10.0f * dt);
        }

        player.distanceTraveled = fmaxf(player.distanceTraveled, player.position.x);
        player.currentSector = 1 + (int)(player.position.x / 2500.0f);

        // Radar ping propagation
        if (player.radarPingActive) {
            player.radarPingRadius += 1200.0f * dt;
            player.radarPingAlpha -= 1.2f * dt;
            if (player.radarPingAlpha <= 0.0f || player.radarPingRadius > 1400.0f) {
                player.radarPingActive = false;
            }
        }

        // -----------------------------------------------------
        // Passage Wall Collision Check
        // -----------------------------------------------------
        PassageSlice currentSlice = GetPassageAtX(player.position.x);
        if (player.position.y - player.radius < currentSlice.ceilingY()) {
            player.position.y = currentSlice.ceilingY() + player.radius;
            ApplyImpactDamage(15.0f);
        } else if (player.position.y + player.radius > currentSlice.floorY()) {
            player.position.y = currentSlice.floorY() - player.radius;
            ApplyImpactDamage(15.0f);
        }

        // -----------------------------------------------------
        // Traps Logic & Collision
        // -----------------------------------------------------
        for (auto &t : traps) {
            if (!t.active) continue;
            float distToPlayer = Vector2Distance(player.position, t.position);

            if (t.type == TrapType::STALACTITE_FALLING) {
                if (!t.triggered && distToPlayer < t.alertDistance && player.position.x < t.position.x + 40.0f) {
                    t.triggered = true;
                    PlaySound(audio.sndAlarm);
                }
                if (t.triggered) {
                    t.velocity.y += 980.0f * dt;
                    t.position.y += t.velocity.y * dt;
                    PassageSlice s = GetPassageAtX(t.position.x);
                    if (t.position.y >= s.floorY()) {
                        t.active = false;
                        for (int k = 0; k < 8; ++k) {
                            SpawnParticle(t.position, { (float)(rand() % 160 - 80), (float)-(rand() % 140 + 20) }, Color{ 150, 140, 130, 255 }, 4.0f, 0.5f);
                        }
                    }
                }
            } else if (t.type == TrapType::PROXIMITY_MINE) {
                if (distToPlayer < t.alertDistance) {
                    t.triggered = true;
                    Vector2 dir = Vector2Normalize(Vector2Subtract(player.position, t.position));
                    t.position = Vector2Add(t.position, Vector2Scale(dir, 145.0f * dt));
                }
            } else if (t.type == TrapType::LASER_TRIPWIRE) {
                t.timer += dt * t.cycleRate;
                t.triggered = (fmodf(t.timer, 3.5f) < 2.0f);
            }

            // Trap Collision
            if (t.active && distToPlayer < (player.radius + t.radius)) {
                if (t.type == TrapType::LASER_TRIPWIRE && !t.triggered) {
                    // Safe during window
                } else {
                    ApplyImpactDamage(28.0f);
                    t.active = false;
                    for (int k = 0; k < 14; ++k) {
                        SpawnParticle(t.position, { (float)(rand() % 240 - 120), (float)(rand() % 240 - 120) }, Color{ 255, 60, 40, 255 }, 5.0f, 0.6f);
                    }
                }
            }
        }

        // -----------------------------------------------------
        // Chrono-Crystals Collection (+5s)
        // -----------------------------------------------------
        for (auto &c : crystals) {
            if (c.collected) continue;
            c.pulsePhase += dt * 4.0f;
            if (Vector2Distance(player.position, c.position) < (player.radius + 18.0f)) {
                c.collected = true;
                timeRemaining += c.timeBonus;
                player.crystalsCollected++;
                PlaySound(audio.sndCrystal);
                for (int k = 0; k < 12; ++k) {
                    SpawnParticle(c.position, { (float)(rand() % 180 - 90), (float)(rand() % 180 - 90) }, Color{ 0, 255, 220, 255 }, 4.0f, 0.5f);
                }
            }
        }

        // -----------------------------------------------------
        // Life (Heart) Pickups (+30 HP Healing)
        // -----------------------------------------------------
        for (auto &hp : hearts) {
            if (hp.collected) continue;
            hp.pulsePhase += dt * 5.0f;
            hp.floatOffset = sinf(hp.pulsePhase) * 6.0f;

            if (Vector2Distance(player.position, { hp.position.x, hp.position.y + hp.floatOffset }) < (player.radius + 20.0f)) {
                hp.collected = true;
                player.hull = fminf(player.maxHull, player.hull + hp.healAmount);
                player.heartsCollected++;
                PlaySound(audio.sndHeart);

                // Burst of radiant pink & red heart healing particles
                for (int k = 0; k < 18; ++k) {
                    SpawnParticle(hp.position, { (float)(rand() % 180 - 90), (float)(rand() % 180 - 90) }, Color{ 255, 50, 100, 255 }, 5.0f, 0.6f);
                }
            }
        }

        // -----------------------------------------------------
        // Enemy Hazards (Missiles, Bombs, Seekers)
        // -----------------------------------------------------
        hazardSpawnTimer -= dt;
        if (hazardSpawnTimer <= 0.0f) {
            SpawnEnemyHazard();
            float spawnInt = 3.5f;
            if (difficulty == Difficulty::VOID_WALKER) spawnInt = 2.2f;
            if (difficulty == Difficulty::APOCALYPSE) spawnInt = 1.4f;
            hazardSpawnTimer = spawnInt;
        }

        for (auto &h : hazards) {
            if (!h.active) continue;
            h.animTimer += dt;

            // Seeker drones track jet elevation gently
            if (h.type == HazardType::SEEKER_DRONE) {
                float dy = player.position.y - h.position.y;
                h.velocity.y = Clamp(dy * 2.0f, -120.0f, 120.0f);
            }

            h.position = Vector2Add(h.position, Vector2Scale(h.velocity, dt));

            // Missiles leave smoke trail
            if (h.type == HazardType::CRUISE_MISSILE && (rand() % 2 == 0)) {
                SpawnParticle(
                    Vector2{ h.position.x + 16.0f, h.position.y },
                    Vector2{ 60.0f + (float)(rand() % 30), (float)(rand() % 30 - 15) },
                    Color{ 180, 180, 190, 180 }, 3.5f, 0.35f
                );
            }

            if (Vector2Distance(player.position, h.position) < (player.radius + h.radius)) {
                float dmg = (h.type == HazardType::HEAVY_BOMB) ? 45.0f : 32.0f;
                ApplyImpactDamage(dmg);
                h.active = false;
                for (int k = 0; k < 20; ++k) {
                    SpawnParticle(h.position, { (float)(rand() % 300 - 150), (float)(rand() % 300 - 150) }, Color{ 255, 120, 20, 255 }, 6.0f, 0.7f);
                }
            }

            if (h.position.x < player.position.x - 600.0f) {
                h.active = false;
            }
        }

        // -----------------------------------------------------
        // Particles Update
        // -----------------------------------------------------
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

        // Check Sector Victory
        if (player.position.x >= passageLength - 400.0f) {
            state = GameState::SECTOR_CLEARED;
            CalculateFinalScore(true);
            SaveScore("PILOT_ACE", finalScore, player.distanceTraveled);
        }

        // Camera Tracking & Screen Shake
        camera.target.x = player.position.x + 120.0f;
        camera.target.y = player.position.y;
        if (screenShake > 0.0f) {
            screenShake -= dt * 18.0f;
            camera.offset.x = SCREEN_WIDTH * 0.30f + ((float)(rand() % 20) - 10.0f) * screenShake;
            camera.offset.y = SCREEN_HEIGHT * 0.50f + ((float)(rand() % 20) - 10.0f) * screenShake;
        } else {
            camera.offset = Vector2{ (float)SCREEN_WIDTH * 0.30f, (float)SCREEN_HEIGHT * 0.50f };
        }
    }

    void ApplyImpactDamage(float rawDamage) {
        if (player.shieldActive) {
            player.energy = fmaxf(0.0f, player.energy - rawDamage * 0.8f);
            player.hull -= rawDamage * 0.25f;
            PlaySound(audio.sndShield);
            screenShake = 0.4f;
        } else {
            player.hull -= rawDamage;
            PlaySound(audio.sndHit);
            screenShake = 0.8f;
        }

        if (player.hull <= 0.0f) {
            player.hull = 0.0f;
            TriggerGameOver("JET HULL DESTROYED IN IMPACT");
        }
    }

    void TriggerGameOver(const std::string &reason) {
        state = GameState::GAME_OVER;
        gameOverReason = reason;
        CalculateFinalScore(false);
        SaveScore("PILOT_JET", finalScore, player.distanceTraveled);
    }

    void CalculateFinalScore(bool cleared) {
        int base = (int)player.distanceTraveled;
        int crystalPts = player.crystalsCollected * 250;
        int heartPts = player.heartsCollected * 300;
        int timePts = (int)(timeRemaining * 100);
        int clearBonus = cleared ? 10000 : 0;
        float diffMult = 1.0f;
        if (difficulty == Difficulty::CADET) diffMult = 0.7f;
        else if (difficulty == Difficulty::COMMANDER) diffMult = 1.0f;
        else if (difficulty == Difficulty::VOID_WALKER) diffMult = 1.5f;
        else if (difficulty == Difficulty::APOCALYPSE) diffMult = 2.2f;

        finalScore = (int)((base + crystalPts + heartPts + timePts + clearBonus) * diffMult);
    }

    PassageSlice GetPassageAtX(float x) {
        if (passage.empty()) return { x, (float)SCREEN_HEIGHT / 2.0f, 150.0f };
        int idx = (int)(x / 25.0f);
        idx = Clamp(idx, 0, (int)passage.size() - 1);
        return passage[idx];
    }

    void SpawnParticle(Vector2 pos, Vector2 vel, Color col, float size, float life) {
        Particle p = { pos, vel, col, 1.0f, size, life, life };
        particles.push_back(p);
    }

    // ---------------------------------------------------------
    // Rendering Pipeline
    // ---------------------------------------------------------
    void Draw() {
        BeginDrawing();
        ClearBackground(Color{ 8, 12, 18, 255 });

        if (state == GameState::TITLE_MENU) {
            DrawTitleScreen();
            EndDrawing();
            return;
        }

        // 1. World Rendering with Camera2D
        BeginMode2D(camera);
        DrawPassageWalls();
        DrawCrystals();
        DrawHearts();
        DrawTraps();
        DrawEnemyHazardsWorld();
        DrawParticles();
        DrawPlayerJet();
        EndMode2D();

        // 2. Dynamic Fog-of-War Render Texture Mask
        DrawDynamicFogOfWar();

        // 3. Ultra-Crisp Tactical HUD, BVR Radar & Life status
        DrawTacticalHUD();

        if (state == GameState::PAUSED) {
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 5, 8, 15, 200 });
            DrawCard(SCREEN_WIDTH / 2 - 240, SCREEN_HEIGHT / 2 - 90, 480, 180, Color{ 14, 20, 32, 250 }, Color{ 0, 220, 255, 255 });
            DrawTextBold("MISSION PAUSED", SCREEN_WIDTH / 2 - MeasureText("MISSION PAUSED", 28) / 2, SCREEN_HEIGHT / 2 - 50, 28, RAYWHITE);
            DrawTextBold("Press [P] or [ESC] to Resume Flight", SCREEN_WIDTH / 2 - MeasureText("Press [P] or [ESC] to Resume Flight", 16) / 2, SCREEN_HEIGHT / 2 + 5, 16, Color{ 160, 200, 240, 255 });
            DrawTextBold("Press [M] to Abort and Return to Menu", SCREEN_WIDTH / 2 - MeasureText("Press [M] to Abort and Return to Menu", 14) / 2, SCREEN_HEIGHT / 2 + 35, 14, Color{ 255, 200, 60, 255 });
        } else if (state == GameState::GAME_OVER) {
            DrawEndScreen(false);
        } else if (state == GameState::SECTOR_CLEARED) {
            DrawEndScreen(true);
        }

        EndDrawing();
    }

    void DrawPassageWalls() {
        for (size_t i = 0; i < passage.size() - 1; ++i) {
            const auto &p1 = passage[i];
            const auto &p2 = passage[i + 1];

            if (p2.x < camera.target.x - SCREEN_WIDTH * 0.7f || p1.x > camera.target.x + SCREEN_WIDTH * 0.9f) {
                continue;
            }

            // Ceiling Rock Slab
            Vector2 cA = { p1.x, -500.0f };
            Vector2 cB = { p2.x, -500.0f };
            Vector2 cC = { p2.x, p2.ceilingY() };
            Vector2 cD = { p1.x, p1.ceilingY() };
            DrawTriangle(cA, cD, cC, Color{ 20, 26, 38, 255 });
            DrawTriangle(cA, cC, cB, Color{ 20, 26, 38, 255 });
            DrawLineEx(cD, cC, 3.0f, Color{ 48, 68, 100, 255 });

            // Floor Rock Slab
            Vector2 fA = { p1.x, p1.floorY() };
            Vector2 fB = { p2.x, p2.floorY() };
            Vector2 fC = { p2.x, (float)SCREEN_HEIGHT + 500.0f };
            Vector2 fD = { p1.x, (float)SCREEN_HEIGHT + 500.0f };
            DrawTriangle(fA, fC, fB, Color{ 18, 24, 34, 255 });
            DrawTriangle(fA, fD, fC, Color{ 18, 24, 34, 255 });
            DrawLineEx(fA, fB, 3.0f, Color{ 40, 56, 85, 255 });
        }
    }

    void DrawCrystals() {
        for (const auto &c : crystals) {
            if (c.collected) continue;
            float pulse = sinf(c.pulsePhase) * 3.0f;
            DrawCircleGradient((int)c.position.x, (int)c.position.y, 16.0f + pulse, Color{ 0, 255, 200, 160 }, BLANK);
            DrawPoly(c.position, 6, 9.0f, c.pulsePhase * 20.0f, Color{ 80, 255, 230, 255 });
            DrawPolyLines(c.position, 6, 10.0f, c.pulsePhase * 20.0f, Color{ 200, 255, 250, 255 });
        }
    }

    // DRAW LIFE HEARTS
    void DrawHearts() {
        for (const auto &hp : hearts) {
            if (hp.collected) continue;
            Vector2 pos = { hp.position.x, hp.position.y + hp.floatOffset };
            float pulse = sinf(hp.pulsePhase) * 2.0f;

            // Heart glow
            DrawCircleGradient((int)pos.x, (int)pos.y, 22.0f + pulse, Color{ 255, 40, 80, 150 }, BLANK);

            // Draw Heart Shape using two circles and a triangle
            float size = 8.0f + pulse * 0.4f;
            Vector2 leftCenter  = { pos.x - size * 0.55f, pos.y - size * 0.3f };
            Vector2 rightCenter = { pos.x + size * 0.55f, pos.y - size * 0.3f };
            DrawCircleV(leftCenter, size * 0.55f, Color{ 255, 40, 80, 255 });
            DrawCircleV(rightCenter, size * 0.55f, Color{ 255, 40, 80, 255 });

            Vector2 tA = { pos.x - size * 1.05f, pos.y - size * 0.2f };
            Vector2 tB = { pos.x + size * 1.05f, pos.y - size * 0.2f };
            Vector2 tC = { pos.x, pos.y + size * 1.1f };
            DrawTriangle(tA, tB, tC, Color{ 255, 40, 80, 255 });

            // Highlight glint
            DrawCircle((int)(pos.x - size * 0.5f), (int)(pos.y - size * 0.5f), 2.0f, RAYWHITE);
        }
    }

    void DrawTraps() {
        for (const auto &t : traps) {
            if (!t.active) continue;

            if (t.type == TrapType::STALACTITE_FALLING) {
                // Rock cone
                Vector2 topA = { t.position.x - 12.0f, t.position.y - 22.0f };
                Vector2 topB = { t.position.x + 12.0f, t.position.y - 22.0f };
                Vector2 tip  = { t.position.x, t.position.y + 16.0f };
                DrawTriangle(topA, tip, topB, Color{ 190, 80, 70, 255 });
                DrawTriangleLines(topA, tip, topB, Color{ 255, 140, 110, 255 });
            } else if (t.type == TrapType::PROXIMITY_MINE) {
                // Spiky Naval/Space Proximity Mine
                DrawCircleV(t.position, t.radius, Color{ 45, 15, 15, 255 });
                DrawCircleLines((int)t.position.x, (int)t.position.y, t.radius, t.triggered ? RED : Color{ 220, 50, 50, 255 });

                // Spikes protruding around circle
                for (int sp = 0; sp < 8; ++sp) {
                    float ang = (float)sp * (PI / 4.0f) + totalElapsedTime * 1.5f;
                    Vector2 base = { t.position.x + cosf(ang) * t.radius, t.position.y + sinf(ang) * t.radius };
                    Vector2 tip  = { t.position.x + cosf(ang) * (t.radius + 6.0f), t.position.y + sinf(ang) * (t.radius + 6.0f) };
                    DrawLineEx(base, tip, 2.5f, Color{ 255, 70, 70, 255 });
                }

                // Core blinking eye
                DrawCircleV(t.position, 5.0f, t.triggered ? RED : ORANGE);
            } else if (t.type == TrapType::LASER_TRIPWIRE) {
                PassageSlice s = GetPassageAtX(t.position.x);
                if (t.triggered) {
                    DrawLineEx({ t.position.x, s.ceilingY() }, { t.position.x, s.floorY() }, 4.0f, Color{ 255, 50, 70, 220 });
                    DrawLineEx({ t.position.x, s.ceilingY() }, { t.position.x, s.floorY() }, 1.5f, RAYWHITE);
                } else {
                    DrawLineEx({ t.position.x, s.ceilingY() }, { t.position.x, s.floorY() }, 1.0f, Color{ 255, 50, 70, 60 });
                }
            } else if (t.type == TrapType::CRUSHER_GATE) {
                PassageSlice s = GetPassageAtX(t.position.x);
                float openAmount = sinf(t.timer * t.cycleRate) * 0.5f + 0.5f;
                float gateH = s.halfWidth * 0.45f * (1.0f - openAmount);
                DrawRectangle((int)t.position.x - 8, (int)s.ceilingY(), 16, (int)gateH, Color{ 140, 120, 100, 255 });
                DrawRectangle((int)t.position.x - 8, (int)(s.floorY() - gateH), 16, (int)gateH, Color{ 140, 120, 100, 255 });
            }
        }
    }

    // DRAW LOGICAL ENEMY SHAPES: MISSILE, BOMB, SEEKER
    void DrawEnemyHazardsWorld() {
        for (const auto &h : hazards) {
            if (!h.active) continue;

            if (h.type == HazardType::CRUISE_MISSILE) {
                // Sleek Cruise Missile flying left
                Vector2 p = h.position;
                DrawCircleGradient((int)p.x + 14, (int)p.y, 16.0f, Color{ 255, 120, 30, 200 }, BLANK);

                // Missile body
                DrawRectangle((int)p.x - 12, (int)p.y - 4, 24, 8, Color{ 210, 220, 230, 255 });
                // Warhead nose cone pointing left
                Vector2 nose = { p.x - 20.0f, p.y };
                Vector2 nA   = { p.x - 12.0f, p.y - 4.0f };
                Vector2 nB   = { p.x - 12.0f, p.y + 4.0f };
                DrawTriangle(nose, nA, nB, RED);

                // Tail stabilizer fins
                DrawTriangle({ p.x + 12.0f, p.y - 4.0f }, { p.x + 8.0f, p.y - 10.0f }, { p.x + 16.0f, p.y - 4.0f }, Color{ 70, 80, 95, 255 });
                DrawTriangle({ p.x + 12.0f, p.y + 4.0f }, { p.x + 8.0f, p.y + 10.0f }, { p.x + 16.0f, p.y + 4.0f }, Color{ 70, 80, 95, 255 });
            } else if (h.type == HazardType::HEAVY_BOMB) {
                // Heavy Aerial Bomb (rounded teardrop with cross fins)
                Vector2 p = h.position;
                DrawCircleGradient((int)p.x, (int)p.y, 22.0f, Color{ 255, 80, 20, 160 }, BLANK);
                DrawCircleV(p, 13.0f, Color{ 55, 60, 70, 255 });
                DrawCircleV({ p.x - 4.0f, p.y }, 11.0f, Color{ 75, 85, 95, 255 });

                // Yellow Hazard Stripe
                DrawRectangle((int)p.x - 4, (int)p.y - 10, 4, 20, YELLOW);

                // Tail fins
                DrawRectangle((int)p.x + 10, (int)p.y - 6, 6, 12, Color{ 40, 45, 50, 255 });
            } else {
                // Seeker Drone (diamond hull with red scanning lens)
                Vector2 p = h.position;
                DrawPoly(p, 4, 15.0f, totalElapsedTime * 90.0f, Color{ 160, 40, 40, 255 });
                DrawPolyLines(p, 4, 16.0f, totalElapsedTime * 90.0f, RED);
                DrawCircleV(p, 4.0f, RAYWHITE);
            }
        }
    }

    void DrawParticles() {
        for (const auto &p : particles) {
            Color c = p.color;
            c.a = (unsigned char)(p.alpha * 255.0f);
            DrawCircleV(p.position, p.size, c);
        }
    }

    // DRAW LOGICAL PLAYER CHARACTER: SUPERSONIC DELTA-WING JET FIGHTER
    void DrawPlayerJet() {
        Vector2 p = player.position;
        float ang = player.angle;

        // Twin Afterburner Thruster Glow
        DrawCircleGradient((int)p.x, (int)p.y, 38.0f, Color{ 0, 180, 255, 65 }, BLANK);

        // Jet Coordinates
        Vector2 noseTip   = { p.x + cosf(ang) * 26.0f, p.y + sinf(ang) * 26.0f };
        Vector2 leftWing  = { p.x + cosf(ang + 2.45f) * 24.0f, p.y + sinf(ang + 2.45f) * 24.0f };
        Vector2 rightWing = { p.x + cosf(ang - 2.45f) * 24.0f, p.y + sinf(ang - 2.45f) * 24.0f };
        Vector2 tailLeft  = { p.x + cosf(ang + 2.85f) * 16.0f, p.y + sinf(ang + 2.85f) * 16.0f };
        Vector2 tailRight = { p.x + cosf(ang - 2.85f) * 16.0f, p.y + sinf(ang - 2.85f) * 16.0f };

        // Delta Wings (Main Airframe)
        DrawTriangle(noseTip, leftWing, tailLeft, Color{ 210, 225, 245, 255 });
        DrawTriangle(noseTip, tailRight, rightWing, Color{ 190, 205, 230, 255 });
        DrawTriangle(noseTip, tailLeft, tailRight, Color{ 230, 240, 255, 255 });

        // Delta Wing Outlines & Panel Lines
        DrawLineEx(noseTip, leftWing, 2.0f, Color{ 56, 189, 248, 255 });
        DrawLineEx(noseTip, rightWing, 2.0f, Color{ 56, 189, 248, 255 });
        DrawLineEx(leftWing, tailLeft, 2.0f, Color{ 30, 58, 110, 255 });
        DrawLineEx(rightWing, tailRight, 2.0f, Color{ 30, 58, 110, 255 });

        // Wingtip Navigation Lights (Green starboard, Red port)
        DrawCircleV(leftWing, 3.0f, RED);
        DrawCircleV(rightWing, 3.0f, GREEN);

        // Cockpit Canopy (Tinted Cyan Glass with reflection)
        Vector2 cockpitPos = { p.x + cosf(ang) * 9.0f, p.y + sinf(ang) * 9.0f };
        DrawEllipse((int)cockpitPos.x, (int)cockpitPos.y, 7, 3, Color{ 6, 182, 212, 255 });
        DrawCircle((int)(cockpitPos.x + 2), (int)(cockpitPos.y - 1), 1.5f, RAYWHITE);

        // Active Kinetic Shield Barrier
        if (player.shieldActive) {
            DrawCircleGradient((int)p.x, (int)p.y, 36.0f, Color{ 0, 160, 255, 80 }, BLANK);
            DrawPolyLines(p, 6, 32.0f, totalElapsedTime * 60.0f, Color{ 100, 220, 255, 220 });
        }
    }

    void DrawDynamicFogOfWar() {
        BeginTextureMode(fogTexture);
        ClearBackground(Color{ 6, 8, 14, 235 });

        Vector2 screenPlayer = GetWorldToScreen2D(player.position, camera);

        float vR = player.visionRadius;
        if (player.boosting) vR += 40.0f;
        DrawCircleGradient((int)screenPlayer.x, (int)screenPlayer.y, vR, BLANK, Color{ 6, 8, 14, 235 });

        if (player.radarPingActive) {
            DrawCircleLines((int)screenPlayer.x, (int)screenPlayer.y, player.radarPingRadius, Color{ 0, 255, 200, (unsigned char)(player.radarPingAlpha * 255.0f) });
        }

        for (const auto &c : crystals) {
            if (c.collected) continue;
            Vector2 sc = GetWorldToScreen2D(c.position, camera);
            DrawCircleGradient((int)sc.x, (int)sc.y, 50.0f, BLANK, Color{ 6, 8, 14, 235 });
        }

        for (const auto &hp : hearts) {
            if (hp.collected) continue;
            Vector2 sc = GetWorldToScreen2D(hp.position, camera);
            DrawCircleGradient((int)sc.x, (int)sc.y, 55.0f, BLANK, Color{ 6, 8, 14, 235 });
        }

        EndTextureMode();
        DrawTextureRec(fogTexture.texture, Rectangle{ 0.0f, 0.0f, (float)SCREEN_WIDTH, -(float)SCREEN_HEIGHT }, Vector2{ 0.0f, 0.0f }, WHITE);
    }

    // ULTRA-CRISP HIGH CONTRAST HUD
    void DrawTacticalHUD() {
        // Top Bar Backing
        DrawRectangle(0, 0, SCREEN_WIDTH, 56, Color{ 10, 14, 24, 240 });
        DrawLine(0, 56, SCREEN_WIDTH, 56, Color{ 36, 50, 78, 255 });

        // Title Badge
        DrawCard(15, 10, 180, 36, Color{ 15, 23, 42, 255 }, Color{ 56, 189, 248, 255 });
        DrawTextBold("LOST PASSAGE", 25, 18, 18, Color{ 224, 242, 254, 255 });

        // Sector Badge
        std::string secStr = "SECTOR 0" + std::to_string(player.currentSector);
        DrawCard(205, 10, 120, 36, Color{ 15, 23, 42, 255 }, Color{ 0, 220, 255, 200 });
        DrawTextBold(secStr.c_str(), 215, 19, 16, Color{ 56, 189, 248, 255 });

        // Countdown Timer Clock
        std::stringstream ssTime;
        ssTime << "TIME: " << std::fixed << std::setprecision(1) << timeRemaining << "s";
        Color timeBg = (timeRemaining < 15.0f) ? Color{ 80, 20, 20, 255 } : Color{ 40, 35, 15, 255 };
        Color timeBorder = (timeRemaining < 15.0f) ? Color{ 240, 60, 60, 255 } : Color{ 250, 204, 21, 255 };
        DrawCard(335, 10, 150, 36, timeBg, timeBorder);
        DrawTextBold(ssTime.str().c_str(), 350, 18, 18, timeBorder);

        // Distance & Hearts Count
        DrawCard(495, 10, 130, 36, Color{ 15, 23, 42, 255 }, Color{ 70, 85, 110, 255 });
        std::string distStr = std::to_string((int)player.distanceTraveled) + "m";
        DrawTextBold(distStr.c_str(), 510, 19, 16, RAYWHITE);

        DrawCard(635, 10, 120, 36, Color{ 35, 15, 25, 255 }, Color{ 255, 70, 120, 255 });
        std::string heartStr = "HEARTS: " + std::to_string(player.heartsCollected);
        DrawTextBold(heartStr.c_str(), 645, 19, 15, Color{ 255, 100, 150, 255 });

        DrawCard(765, 10, 120, 36, Color{ 15, 30, 30, 255 }, Color{ 45, 212, 191, 255 });
        std::string crysStr = "CRYSTALS: " + std::to_string(player.crystalsCollected);
        DrawTextBold(crysStr.c_str(), 775, 19, 14, Color{ 94, 234, 212, 255 });

        // Menu hint button
        DrawCard(895, 10, 110, 36, Color{ 20, 25, 40, 255 }, Color{ 120, 140, 180, 255 });
        DrawTextBold("[M] MENU", 910, 20, 14, Color{ 180, 200, 230, 255 });

        // -----------------------------------------------------
        // Strategic Vital Gauges (Bottom Left)
        // -----------------------------------------------------
        int barY = SCREEN_HEIGHT - 95;
        DrawCard(20, barY - 10, 340, 85, Color{ 10, 15, 26, 240 }, Color{ 36, 50, 78, 255 });

        // Hull Life Bar
        DrawTextBold("JET HULL INTEGRITY", 32, barY + 2, 12, Color{ 200, 215, 235, 255 });
        float hullRatio = player.hull / player.maxHull;
        DrawRectangle(170, barY, 175, 14, Color{ 25, 35, 50, 255 });
        Color hullCol = (hullRatio > 0.35f) ? Color{ 34, 197, 94, 255 } : Color{ 239, 68, 68, 255 };
        DrawRectangle(170, barY, (int)(175 * hullRatio), 14, hullCol);

        // Energy Core Bar
        DrawTextBold("ENERGY CORE", 32, barY + 24, 12, Color{ 200, 215, 235, 255 });
        float energyRatio = player.energy / player.maxEnergy;
        DrawRectangle(170, barY + 22, 175, 14, Color{ 25, 35, 50, 255 });
        DrawRectangle(170, barY + 22, (int)(175 * energyRatio), 14, Color{ 2, 132, 199, 255 });

        // Key Control Hints
        DrawTextBold("[SHIFT] Boost  [E] Shield  [Q] Repair  [SPACE] Ping  [M] Menu", 32, barY + 48, 11, Color{ 125, 211, 252, 255 });

        // -----------------------------------------------------
        // BVR Tactical Scanner Radar (Top Right)
        // -----------------------------------------------------
        int radarX = SCREEN_WIDTH - 220;
        int radarY = 70;
        int radarW = 200;
        int radarH = 150;

        DrawCard(radarX, radarY, radarW, radarH, Color{ 8, 14, 24, 235 }, Color{ 56, 189, 248, 180 });
        DrawTextBold("BVR RADAR SCANNER", radarX + 12, radarY + 10, 12, Color{ 56, 189, 248, 255 });

        DrawLine(radarX + 30, radarY + radarH / 2, radarX + radarW - 10, radarY + radarH / 2, Color{ 0, 120, 180, 100 });
        DrawLine(radarX + 40, radarY + 25, radarX + 40, radarY + radarH - 10, Color{ 0, 120, 180, 100 });

        // Jet dot
        DrawCircle(radarX + 40, radarY + radarH / 2, 4.0f, Color{ 34, 211, 238, 255 });

        // Hazards on radar
        for (const auto &h : hazards) {
            if (!h.active) continue;
            float dx = h.position.x - player.position.x;
            if (dx > 0.0f && dx < 1400.0f) {
                float radarDotX = radarX + 40.0f + (dx / 1400.0f) * (radarW - 55.0f);
                float dy = (h.position.y - player.position.y) * 0.15f;
                float radarDotY = Clamp(radarY + radarH / 2 + dy, (float)radarY + 25.0f, (float)radarY + radarH - 12.0f);

                DrawCircle((int)radarDotX, (int)radarDotY, 4.0f, RED);
                std::string tag = std::to_string((int)(dx / 10.0f)) + "m";
                DrawTextBold(tag.c_str(), (int)radarDotX - 10, (int)radarDotY - 12, 10, Color{ 254, 202, 202, 255 });
            }
        }
    }

    void DrawTitleScreen() {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 8, 12, 20, 255 });

        // Grid lines
        for (int x = 0; x < SCREEN_WIDTH; x += 60) {
            DrawLine(x, 0, x, SCREEN_HEIGHT, Color{ 20, 30, 48, 80 });
        }
        for (int y = 0; y < SCREEN_HEIGHT; y += 60) {
            DrawLine(0, y, SCREEN_WIDTH, y, Color{ 20, 30, 48, 80 });
        }

        // Title Header Card
        DrawCard(SCREEN_WIDTH / 2 - 340, 60, 680, 110, Color{ 12, 18, 30, 250 }, Color{ 56, 189, 248, 255 });
        DrawTextBold("THE LOST PASSAGE", SCREEN_WIDTH / 2 - MeasureText("THE LOST PASSAGE", 42) / 2, 80, 42, Color{ 240, 249, 255, 255 });
        DrawTextBold("2D PROCEDURAL CAVERN JET RUNNER · RAYLIB C++ ENGINE", SCREEN_WIDTH / 2 - MeasureText("2D PROCEDURAL CAVERN JET RUNNER · RAYLIB C++ ENGINE", 14) / 2, 130, 14, Color{ 56, 189, 248, 255 });

        // Multiple Hardness Levels Card
        DrawCard(SCREEN_WIDTH / 2 - 340, 185, 680, 180, Color{ 12, 18, 30, 250 }, Color{ 40, 60, 90, 255 });
        DrawTextBold("SELECT HARDNESS LEVEL (PRESS 1, 2, 3, or 4):", SCREEN_WIDTH / 2 - 200, 200, 16, RAYWHITE);

        DrawTextBold("[1] CADET (Recon - Wide Caverns, Gentle Traps, 160 HP, 90s Timer)", SCREEN_WIDTH / 2 - 300, 235, 14, (difficulty == Difficulty::CADET) ? Color{ 45, 212, 191, 255 } : Color{ 100, 116, 139, 255 });
        DrawTextBold("[2] COMMANDER (Tactical - Balanced Traps & Hazards, 100 HP, 60s Timer)", SCREEN_WIDTH / 2 - 300, 265, 14, (difficulty == Difficulty::COMMANDER) ? Color{ 45, 212, 191, 255 } : Color{ 100, 116, 139, 255 });
        DrawTextBold("[3] VOID WALKER (Hardcore - Narrow Chokepoints, High-Speed Missiles, 70 HP)", SCREEN_WIDTH / 2 - 300, 295, 14, (difficulty == Difficulty::VOID_WALKER) ? Color{ 45, 212, 191, 255 } : Color{ 100, 116, 139, 255 });
        DrawTextBold("[4] APOCALYPSE (Insane - Lethal Barrages, Dense Mines, 50 HP, 35s Timer)", SCREEN_WIDTH / 2 - 300, 325, 14, (difficulty == Difficulty::APOCALYPSE) ? Color{ 239, 68, 68, 255 } : Color{ 100, 116, 139, 255 });

        // Pilot Controls Briefing
        DrawCard(SCREEN_WIDTH / 2 - 340, 380, 680, 150, Color{ 12, 18, 30, 250 }, Color{ 40, 60, 90, 255 });
        DrawTextBold("FLIGHT CONTROLS & SUBSYSTEMS", SCREEN_WIDTH / 2 - 130, 395, 15, Color{ 56, 189, 248, 255 });
        DrawTextBold("- WASD or ARROW KEYS : Delta Jet Propulsion & Directional Vector", SCREEN_WIDTH / 2 - 310, 425, 13, RAYWHITE);
        DrawTextBold("- LEFT SHIFT : Afterburner Boosters (Consumes Energy Core)", SCREEN_WIDTH / 2 - 310, 448, 13, RAYWHITE);
        DrawTextBold("- E KEY : Deflector Shield  |  Q KEY : Nanite Repair  |  SPACE : BVR Ping", SCREEN_WIDTH / 2 - 310, 471, 13, RAYWHITE);
        DrawTextBold("- HEARTS (Red Life Pickups) : Gather them in flight to heal +30 HP", SCREEN_WIDTH / 2 - 310, 494, 13, Color{ 255, 100, 150, 255 });

        // Launch Button CTA
        DrawCard(SCREEN_WIDTH / 2 - 260, 550, 520, 60, Color{ 16, 40, 30, 250 }, Color{ 34, 197, 94, 255 });
        DrawTextBold("PRESS [ENTER] OR [SPACE] TO LAUNCH JET", SCREEN_WIDTH / 2 - MeasureText("PRESS [ENTER] OR [SPACE] TO LAUNCH JET", 20) / 2, 570, 20, Color{ 74, 222, 128, 255 });
    }

    void DrawEndScreen(bool victory) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Color{ 6, 10, 18, 220 });

        int boxW = 580;
        int boxH = 460;
        int boxX = SCREEN_WIDTH / 2 - boxW / 2;
        int boxY = SCREEN_HEIGHT / 2 - boxH / 2;

        Color borderCol = victory ? Color{ 45, 212, 191, 255 } : Color{ 239, 68, 68, 255 };
        DrawCard(boxX, boxY, boxW, boxH, Color{ 12, 18, 30, 250 }, borderCol);

        if (victory) {
            DrawTextBold("SECTOR RUN COMPLETED", boxX + boxW / 2 - MeasureText("SECTOR RUN COMPLETED", 26) / 2, boxY + 30, 26, Color{ 45, 212, 191, 255 });
            DrawTextBold("The subterranean passage was successfully mapped and navigated.", boxX + boxW / 2 - MeasureText("The subterranean passage was successfully mapped and navigated.", 13) / 2, boxY + 68, 13, RAYWHITE);
        } else {
            DrawTextBold("MISSION TERMINATED", boxX + boxW / 2 - MeasureText("MISSION TERMINATED", 26) / 2, boxY + 30, 26, Color{ 239, 68, 68, 255 });
            DrawTextBold(gameOverReason.c_str(), boxX + boxW / 2 - MeasureText(gameOverReason.c_str(), 14) / 2, boxY + 68, 14, Color{ 254, 202, 202, 255 });
        }

        // Stats
        int statY = boxY + 110;
        DrawTextBold(TextFormat("FINAL FLIGHT DEPTH: %d m", (int)player.distanceTraveled), boxX + 60, statY, 16, RAYWHITE);
        DrawTextBold(TextFormat("TIME REMAINING: %.1f s", timeRemaining), boxX + 60, statY + 30, 16, RAYWHITE);
        DrawTextBold(TextFormat("HEARTS COLLECTED: %d (+HP Recovered)", player.heartsCollected), boxX + 60, statY + 60, 16, Color{ 255, 100, 150, 255 });
        DrawTextBold(TextFormat("CHRONO-CRYSTALS: %d", player.crystalsCollected), boxX + 60, statY + 90, 16, Color{ 94, 234, 212, 255 });
        DrawTextBold(TextFormat("FINAL SCORE: %d PTS", finalScore), boxX + 60, statY + 125, 22, Color{ 250, 204, 21, 255 });

        // Leaderboard
        DrawTextBold("TOP FLIGHT LOGS", boxX + 60, statY + 165, 14, Color{ 56, 189, 248, 255 });
        int leadY = statY + 190;
        for (size_t i = 0; i < (std::min)((size_t)3, leaderboard.size()); ++i) {
            DrawTextBold(TextFormat("#%d %s - %d pts (%s)", (int)(i + 1), leaderboard[i].pilot.c_str(), leaderboard[i].score, leaderboard[i].difficulty.c_str()), boxX + 60, leadY, 13, Color{ 180, 200, 230, 255 });
            leadY += 22;
        }

        // Return to Menu & Restart Options
        DrawCard(boxX + 40, boxY + boxH - 65, 240, 45, Color{ 20, 28, 45, 255 }, Color{ 56, 189, 248, 255 });
        DrawTextBold("[M] RETURN TO MENU", boxX + 70, boxY + boxH - 50, 14, Color{ 56, 189, 248, 255 });

        DrawCard(boxX + 300, boxY + boxH - 65, 240, 45, Color{ 20, 40, 30, 255 }, Color{ 34, 197, 94, 255 });
        DrawTextBold("[R] RE-ENGAGE FLIGHT", boxX + 330, boxY + boxH - 50, 14, Color{ 74, 222, 128, 255 });
    }

    // Helper to draw clean high-contrast rounded cards
    void DrawCard(int x, int y, int w, int h, Color bg, Color border) {
        DrawRectangle(x, y, w, h, bg);
        DrawRectangleLines(x, y, w, h, border);
    }

    // Bold readable text helper
    void DrawTextBold(const char *text, int x, int y, int fontSize, Color color) {
        // Shadow for ultra-readability
        DrawText(text, x + 1, y + 1, fontSize, Color{ 0, 0, 0, 220 });
        DrawText(text, x, y, fontSize, color);
    }

    // ---------------------------------------------------------
    // Leaderboard Persistence
    // ---------------------------------------------------------
    void LoadLeaderboard() {
        leaderboard.clear();
        std::ifstream file("leaderboard.txt");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::stringstream ss(line);
                LeaderboardEntry entry;
                if (ss >> entry.pilot >> entry.score >> entry.distance >> entry.difficulty >> entry.date) {
                    leaderboard.push_back(entry);
                }
            }
            file.close();
        }

        if (leaderboard.empty()) {
            leaderboard.push_back({ "VALKYRIE-9", 42000, 11800.0f, "VOID_WALKER", "2026-10-18" });
            leaderboard.push_back({ "CHRONOS",    31500, 10450.0f, "COMMANDER",   "2026-10-19" });
            leaderboard.push_back({ "DEEP_RUNNER", 19800, 8920.0f, "CADET",       "2026-10-20" });
        }
    }

    void SaveScore(const std::string &pilotName, int score, float dist) {
        std::string diffName = "COMMANDER";
        if (difficulty == Difficulty::CADET) diffName = "CADET";
        else if (difficulty == Difficulty::VOID_WALKER) diffName = "VOID_WALKER";
        else if (difficulty == Difficulty::APOCALYPSE) diffName = "APOCALYPSE";

        LeaderboardEntry entry = { pilotName, score, dist, diffName, "2026-10-21" };
        leaderboard.push_back(entry);

        std::sort(leaderboard.begin(), leaderboard.end(), [](const LeaderboardEntry &a, const LeaderboardEntry &b) {
            return a.score > b.score;
        });

        std::ofstream file("leaderboard.txt");
        if (file.is_open()) {
            for (const auto &e : leaderboard) {
                file << e.pilot << " " << e.score << " " << e.distance << " " << e.difficulty << " " << e.date << "\n";
            }
            file.close();
        }
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
