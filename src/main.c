#include "flecs.h"
#include "flecs/addons/flecs_c.h"
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef Vector2 Velocity;
typedef Vector2 Position;

typedef float Rotation;
typedef float Scale;

typedef uint8_t Team;
typedef int32_t Health;
typedef int32_t ContactDamage;

typedef struct IFrames {
    uint8_t init;
    uint8_t cur;
} IFrames;

typedef uint64_t Flags;
#define PARTICLE 1 << 0
#define EXPLODE_ON_DEATH 1 << 1

typedef struct Animation {
    Texture sheet;
    uint8_t frame_width;
    uint8_t cur_frame;
    uint8_t fps;
    float time;
} Animation;

uint8_t FrameCount(Animation anim) {
    return anim.sheet.width / anim.frame_width;
}

Vector2 FrameSize(Animation anim) {
    return (Vector2){anim.frame_width, anim.sheet.height};
}

float ToDeg(float rad) { return (180 / PI) * rad; }
float ToRad(float deg) { return (PI / 180) * deg; }

Rectangle RecV(Vector2 pos, Vector2 size) {
    return (Rectangle){pos.x, pos.y, size.x, size.y};
}

Rectangle RecEx(Vector2 pos, Vector2 size, Rotation r, Scale s) {
    Vector2 ss = Vector2Scale(size, s);
    Vector2 halfss = Vector2Scale(ss, 0.5);

    Vector2 p = Vector2Subtract(pos, Vector2Rotate(halfss, r));

    return RecV(p, ss);
}

void Move(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Velocity *v = ecs_field(it, Velocity, 2);
    Rotation *r = ecs_field(it, Rotation, 3);

    for (int i = 0; i < it->count; i++) {
        p[i].x += v[i].x * it->delta_time;
        p[i].y += v[i].y * it->delta_time;

        Rotation target = -Vector2Angle((Vector2){0, -1}, v[i]);
        r[i] = target;
    }
}

Shader sh_immunity;

void DrawAnimation(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Rotation *r = ecs_field(it, Rotation, 2);
    Scale *s = ecs_field(it, Scale, 3);
    Animation *a = ecs_field(it, Animation, 4);
    IFrames *im = ecs_field(it, IFrames, 5);

    for (int i = 0; i < it->count; i++) {
        Rectangle source = {a[i].cur_frame * a[i].frame_width, 0, a[i].frame_width, a[i].sheet.height};

        Rectangle dest = RecEx(p[i], FrameSize(a[i]), r[i], s[i]);
        Rectangle dest_norot = RecEx(p[i], FrameSize(a[i]), 0, s[i]);

        if (im[i].cur > 0) {
            BeginShaderMode(sh_immunity);
            DrawTexturePro(a[i].sheet, source, dest, Vector2Zero(), ToDeg(r[i]), WHITE);
            EndShaderMode();
        } else {
            DrawTexturePro(a[i].sheet, source, dest, Vector2Zero(), ToDeg(r[i]), WHITE);
        }

        a[i].time += it->delta_time;
        if (a[i].time > 1. / a[i].fps) {
            a[i].time = 0;
            a[i].cur_frame++;
            a[i].cur_frame %= FrameCount(a[i]);
        }

        // Bounding box
        // DrawRectangleLinesEx(dest_norot, 5, RED);
    }
}

void DrawHealth(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Health *h = ecs_field(it, Health, 2);

    for (int i = 0; i < it->count; i++) {
        char buf[255];
        sprintf(buf, "Health: %d\n", h[i]);
        DrawText(buf, p[i].x - 100, p[i].y - 100, 14, RAYWHITE);
    }
}

void Collisions(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    // Rotation *r = ecs_field(it, Rotation, 2);
    Scale *s = ecs_field(it, Scale, 2);
    Animation *a = ecs_field(it, Animation, 3);


    Health *h = ecs_field(it, Health, 4);
    ContactDamage *d = ecs_field(it, ContactDamage, 5);
    Team *t = ecs_field(it, Team, 6);
    IFrames *im = ecs_field(it, IFrames, 7);

    for (int i = 0; i < it->count; i++) {
        if (im[i].cur > 0) continue; // i is immune
        
        Rectangle irec = RecEx(p[i], FrameSize(a[i]), 0, s[i]);
        for (int j = i; j < it->count; j++) {
            if (t[j] == t[i] || im[j].cur > 0) continue; // Skip if same teams or j is immune
            
            Rectangle jrec = RecEx(p[j], FrameSize(a[j]), 0, s[j]);

            if (CheckCollisionRecs(irec, jrec)) {
                // Decrement health
                h[i] -= d[j];
                h[j] -= d[i];
                // Add iframes
                im[i].cur += im[i].init;
                im[j].cur += im[j].init;
            } 
        }
    }
}

Animation a_explosion;

void HealthCheck(ecs_iter_t *it) {
    Health *h = ecs_field(it, Health, 1);
    Flags *f = ecs_field(it, Flags, 2);
    Animation *a = ecs_field(it, Animation, 3);

    for (int i = 0; i < it->count; i++) {
        if (h[i] > 0) continue;
        if (f[i] & PARTICLE) continue;
        if (f[i] & EXPLODE_ON_DEATH) {  
            f[i] |= PARTICLE;
            a[i] = a_explosion;
            a[i].time = 0.01;
        } else {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

void RemoveParticles(ecs_iter_t *it) {
    Flags *f = ecs_field(it, Flags, 1);
    Animation *a = ecs_field(it, Animation, 2);

    for (int i = 0; i < it->count; i++) {
        if (!(f[i] & PARTICLE)) continue; // Not a particle
        if (a[i].cur_frame >= FrameCount(a[i]) - 1) { // Particle on the last frame
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

void DecrementIFrames(ecs_iter_t *it) {
    IFrames *im = ecs_field(it, IFrames, 1);

    for (int i = 0; i < it->count; i++) {
        if (im[i].cur > 0) {
            im[i].cur--;
        }
    }
}

int main(void) {
    const int screenWidth = 1360;
    const int screenHeight = 700;

    InitWindow(screenWidth, screenHeight, "gaming game gamers");
    ToggleBorderlessWindowed();
    ecs_world_t *ecs = ecs_init();

    SetTargetFPS(60);

    Camera2D camera = {
        .zoom = 1,
        .offset = {screenWidth / 2., screenHeight / 2.},
        .target = {0., 0.},
        .rotation = 0.,
    };

    ECS_COMPONENT(ecs, Position);
    ECS_COMPONENT(ecs, Velocity);
    
    ECS_COMPONENT(ecs, Rotation);
    ECS_COMPONENT(ecs, Scale);
   
    ECS_COMPONENT(ecs, Animation);
   
    ECS_COMPONENT(ecs, Health);
    ECS_COMPONENT(ecs, ContactDamage);
    ECS_COMPONENT(ecs, IFrames);
    ECS_COMPONENT(ecs, Team);
    
    ECS_COMPONENT(ecs, Flags);

    ECS_SYSTEM(ecs, Move, EcsOnUpdate, Position, Velocity, Rotation);     
    
    ECS_SYSTEM(ecs, DrawAnimation, EcsOnUpdate, Position, Rotation, Scale, Animation, IFrames); 
    // ECS_SYSTEM(ecs, DrawHealth, EcsOnUpdate, Position, Health); 
    
    ECS_SYSTEM(ecs, Collisions, EcsOnUpdate, Position, Scale, Animation, Health, ContactDamage, Team, IFrames);
    
    ECS_SYSTEM(ecs, HealthCheck, EcsOnUpdate, Health, Flags, Animation);
    ECS_SYSTEM(ecs, RemoveParticles, EcsOnUpdate, Flags, Animation); 
    ECS_SYSTEM(ecs, DecrementIFrames, EcsOnUpdate, IFrames); 

    Animation a_starship = {
        .sheet = LoadTexture(ASSET "starship.png"),
        .cur_frame = 0,
        .frame_width = 16,
        .time = 0,
        .fps = 8,
    };

    Animation a_laser = {
        .sheet = LoadTexture(ASSET "laser.png"),
        .cur_frame = 0,
        .frame_width = 1,
        .time = 0,
        .fps = 60,
    };

    a_explosion = (Animation){
        .sheet = LoadTexture(ASSET "Explosion.png"),
        .cur_frame = 0,
        .frame_width = 16,
        .time = 0,
        .fps = 8,
    };

    Texture t_bg = LoadTexture(ASSET "Background.png");
    Texture t_mg = LoadTexture(ASSET "Midground.png");
    Texture t_fg = LoadTexture(ASSET "Foreground.png");

    sh_immunity = LoadShader(0, ASSET "immunity.fs");

    ecs_entity_t player = ecs_new_id(ecs);
    ecs_set(ecs, player, Position, {0, 0});
    ecs_set(ecs, player, Velocity, {0, 0});
    ecs_set(ecs, player, Rotation, {0});
    ecs_set(ecs, player, Scale, {5});
    ecs_set(ecs, player, Health, {5});
    ecs_set(ecs, player, ContactDamage, {0});
    ecs_set(ecs, player, Team, {0});
    ecs_set(ecs, player, Flags, {EXPLODE_ON_DEATH});
    ecs_set(ecs, player, IFrames, {16, 0});

    ecs_set_ptr(ecs, player, Animation, &a_starship);

    int sh_im_time = GetShaderLocation(sh_immunity, "time");
    float timeSec = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        BeginMode2D(camera);

        static Position player_pos;
        player_pos = *ecs_get(ecs, player, Position);
        static Velocity player_vel;
        player_vel = *ecs_get(ecs, player, Velocity);

        // ClearBackground(WHITE);

        { // Backgrounds
            Position top = GetScreenToWorld2D((Vector2){-1, -1}, camera);
            Position bot = GetScreenToWorld2D((Vector2){GetScreenWidth(), GetScreenHeight()}, camera);

            Position bg_offset = Vector2Scale(camera.target, -0.1);
            bg_offset.x = (int)bg_offset.x % t_bg.width - t_bg.width; 
            bg_offset.y = (int)bg_offset.y % t_bg.height - t_bg.height;

            for (int16_t x = bg_offset.x + top.x; x <= bot.x; x += t_bg.width) {
                for (int16_t y = bg_offset.y + top.y; y <= bot.y; y += t_bg.height) {
                    DrawTextureEx(t_bg, (Vector2){x, y}, 0, 1, WHITE);
                }
            }

            Position mg_offset = Vector2Scale(camera.target, -0.4);
            mg_offset.x = (int)mg_offset.x % t_bg.width - t_mg.width;
            mg_offset.y = (int)mg_offset.y % t_bg.height - t_mg.height;

            for (int16_t x = mg_offset.x + top.x; x <= bot.x; x += t_mg.width) {
                for (int16_t y = mg_offset.y + top.y; y <= bot.y; y += t_mg.height) {
                    DrawTextureEx(t_mg, (Vector2){x, y}, 0, 1, WHITE);
                }
            }

            Position fg_offset = Vector2Scale(camera.target, -0.9);
            fg_offset.x = (int)fg_offset.x % t_bg.width - t_fg.width;
            fg_offset.y = (int)fg_offset.y % t_bg.height - t_fg.height;

            for (int16_t x = fg_offset.x + top.x; x <= bot.x; x += t_fg.width) {
                for (int16_t y = fg_offset.y + top.y; y <= bot.y; y += t_fg.height) {
                    DrawTextureEx(t_fg, (Vector2){x, y}, 0, 1, WHITE);
                }
            }
        }

        const float dt = GetFrameTime();
        timeSec += dt;
        SetShaderValue(sh_immunity, sh_im_time, &timeSec, SHADER_UNIFORM_FLOAT);
        ecs_progress(ecs, dt); // Also draws every entity

        { // player controls
            bool changed = false;

            if (IsKeyDown(KEY_RIGHT)) {
                player_vel.x = Clamp(player_vel.x + 200, -200, 200);
                changed = true;
            }
            if (IsKeyDown(KEY_LEFT)) {
                player_vel.x = Clamp(player_vel.x - 200, -200, 200);
                changed = true;
            }
            if (!changed) { // Slow down if no movement keys are pressed
                player_vel.x = Lerp(player_vel.x, 0, 0.3);
            }
            changed = false;
            
            if (IsKeyDown(KEY_UP)) {
                player_vel.y = Clamp(player_vel.y - 200, -200, 200);
                changed = true;
            }
            if (IsKeyDown(KEY_DOWN)) {
                player_vel.y = Clamp(player_vel.y + 200, -200, 200);
                changed = true;
            }
            if (!changed) { // Slow down if no movement keys are pressed
                player_vel.y = Lerp(player_vel.y, 0, 0.3);
            }

            ecs_set_ptr(ecs, player, Velocity, &player_vel);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ecs_entity_t laser = ecs_new_id(ecs);

                const Rotation* rot = ecs_get(ecs, player, Rotation);
                ecs_set_ptr(ecs, laser, Rotation, rot);

                Velocity vel = Vector2Rotate((Vector2){0, -500}, *ecs_get(ecs, player, Rotation));
                ecs_set_ptr(ecs, laser, Velocity, &vel);

                Velocity init_vel = Vector2Rotate((Vector2){0, -3 * a_starship.sheet.height}, *ecs_get(ecs, player, Rotation));
                Position pos = Vector2Add(init_vel, *ecs_get(ecs, player, Position));
                ecs_set_ptr(ecs, laser, Position, &pos);

                ecs_set(ecs, laser, Scale, {5});
                
                ecs_set(ecs, laser, Health, {3});
                ecs_set(ecs, laser, ContactDamage, {1});
                ecs_set(ecs, laser, Team, {0});
                ecs_set(ecs, laser, Flags, {0});
                ecs_set(ecs, laser, IFrames, {0, 0});

                ecs_set_ptr(ecs, laser, Animation, &a_laser);
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                ecs_entity_t enemy = ecs_new_id(ecs);

                const Rotation* rot = 0;
                ecs_set_ptr(ecs, enemy, Rotation, rot);

                Velocity vel = {0, 0};
                ecs_set_ptr(ecs, enemy, Velocity, &vel);

                Position pos = GetScreenToWorld2D(GetMousePosition(), camera);
                ecs_set_ptr(ecs, enemy, Position, &pos);

                ecs_set(ecs, enemy, Scale, {3});
                
                ecs_set(ecs, enemy, Health, {5});
                ecs_set(ecs, enemy, ContactDamage, {1});
                ecs_set(ecs, enemy, Team, {1});
                ecs_set(ecs, enemy, Flags, {EXPLODE_ON_DEATH});
                ecs_set(ecs, enemy, IFrames, {16, 0});

                ecs_set_ptr(ecs, enemy, Animation, &a_starship);
            }
        }

        { // Camera 
            camera.target = Vector2Lerp(camera.target, player_pos, 1 * dt);

            if (IsKeyDown(KEY_LEFT_BRACKET)) camera.zoom -= 0.01;
            if (IsKeyDown(KEY_RIGHT_BRACKET)) camera.zoom += 0.01;
            camera.zoom = Clamp(camera.zoom, 0.1, 5);
        }

        EndMode2D();

        { // UI
            DrawFPS(5, 5);
        }
        EndDrawing();
    }


    CloseWindow(); // Close window and OpenGL context
    ecs_fini(ecs);

    return 0;
}
