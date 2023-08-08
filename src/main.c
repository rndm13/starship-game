#include "flecs.h"
#include "raylib.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>

typedef Vector2 Velocity;
typedef Vector2 Position;
typedef float Rotation;
typedef float Scale;

typedef struct Animation {
    Texture sheet;
    uint8_t frame_width;
    uint8_t cur_frame;
    uint8_t fps;
    float time;
} Animation;

uint8_t frame_count(Animation anim) {
    return anim.sheet.width / anim.frame_width;
}

float ToDeg(float rad) { return (180 / PI) * rad; }
float ToRad(float deg) { return (PI / 180) * deg; }

Rectangle RecV(Vector2 a, Vector2 size) {
  return (Rectangle){a.x, a.y, size.x, size.y};
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

void Draw(ecs_iter_t *it) {
  Position *p = ecs_field(it, Position, 1);
  Rotation *r = ecs_field(it, Rotation, 2);
  Scale *s = ecs_field(it, Scale, 3);
  Animation *a = ecs_field(it, Animation, 4);

  for (int i = 0; i < it->count; i++) {
    // char buf[255];
    // sprintf(buf, "%f\n", r[i]);
    // DrawText(buf, p[i].x, p[i].y - 100, 14, BLACK);

    Vector2 size = Vector2Scale((Vector2){a[i].frame_width, a[i].sheet.height}, s[i]);
    Vector2 halfsize = Vector2Scale(size, 0.5);
    
    Vector2 pos = Vector2Subtract(p[i], Vector2Rotate(halfsize, r[i]));

    Rectangle source = {a[i].cur_frame * a[i].frame_width, 0, a[i].frame_width, a[i].sheet.height};
    Rectangle dest = {pos.x, pos.y, a[i].sheet.height * s[i], a[i].frame_width * s[i]};
    
    DrawTexturePro(a[i].sheet, source, dest, Vector2Zero(), ToDeg(r[i]), WHITE);
    
    a[i].time += it->delta_time;
    if (a[i].time > 1. / a[i].fps) {
        a[i].time = 0;
        a[i].cur_frame++;
        a[i].cur_frame %= frame_count(a[i]);
    }

    // Bounding box
    // DrawRectangleLinesEx(RecV(Vector2Subtract(p[i], halfsize), size), 5, RED);
  }
}

int main(void) {
    const int screenWidth = 1360;
    const int screenHeight = 700;

    InitWindow(screenWidth, screenHeight, "gaming game gamers");
    ecs_world_t *ecs = ecs_init();

    SetTargetFPS(60);
    // ecs_set_target_fps(ecs, 60);

    Camera2D camera = {
        .zoom = 2.,
        .offset = {screenWidth / 2., screenHeight / 2.},
        .target = {0., 0.},
        .rotation = 0.,
    };

    ECS_COMPONENT(ecs, Position);
    ECS_COMPONENT(ecs, Velocity);
    ECS_COMPONENT(ecs, Rotation);
    ECS_COMPONENT(ecs, Scale);
    ECS_COMPONENT(ecs, Animation);

    ECS_SYSTEM(ecs, Move, EcsOnUpdate, Position, Velocity, Rotation);
    ECS_SYSTEM(ecs, Draw, EcsOnUpdate, Position, Rotation, Scale, Animation);

    ecs_entity_t player = ecs_new_id(ecs);
    ecs_set(ecs, player, Position, {0, 0});
    ecs_set(ecs, player, Velocity, {0, 0});
    ecs_set(ecs, player, Rotation, {0});
    ecs_set(ecs, player, Scale, {5});

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
        .frame_width = 16,
        .time = 0,
        .fps = 8,
    };

    Texture t_bg = LoadTexture(ASSET "Background.png");
    Texture t_mg = LoadTexture(ASSET "Midground.png");
    Texture t_fg = LoadTexture(ASSET "Foreground.png");

    ecs_set_ptr(ecs, player, Animation, &a_starship);

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
            bg_offset.x = bg_offset.x - t_bg.width;
            bg_offset.y = bg_offset.y - t_bg.height;
            
            for (int16_t x = bg_offset.x + top.x; x <= bot.x; x += t_bg.width) {
                for (int16_t y = bg_offset.y + top.y; y <= bot.y; y += t_bg.height) {
                    DrawTextureEx(t_bg, (Vector2){x, y}, 0, 1, WHITE);
                }
            }

            Position mg_offset = Vector2Scale(camera.target, -0.4);
            mg_offset.x = mg_offset.x - t_mg.width;
            mg_offset.y = mg_offset.y - t_mg.height;
            
            for (int16_t x = mg_offset.x + top.x; x <= bot.x; x += t_mg.width) {
                for (int16_t y = mg_offset.y + top.y; y <= bot.y; y += t_mg.height) {
                    DrawTextureEx(t_mg, (Vector2){x, y}, 0, 1, WHITE);
                }
            }
            
            Position fg_offset = Vector2Scale(camera.target, -0.9);
            fg_offset.x = fg_offset.x - t_fg.width;
            fg_offset.y = fg_offset.y - t_fg.height;
            
            for (int16_t x = fg_offset.x + top.x; x <= bot.x; x += t_fg.width) {
                for (int16_t y = fg_offset.y + top.y; y <= bot.y; y += t_fg.height) {
                    DrawTextureEx(t_fg, (Vector2){x, y}, 0, 1, WHITE);
                }
            }
        }
        
        const float dt = GetFrameTime();
        ecs_progress(ecs, dt); // Also draws every entity

        { // player movement
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
        }

        { // Camera
            camera.target = Vector2Lerp(camera.target, player_pos, 1 * dt);

            if (IsKeyDown(KEY_LEFT_BRACKET)) camera.zoom -= 0.01;
            if (IsKeyDown(KEY_RIGHT_BRACKET)) camera.zoom += 0.01;
            camera.zoom = Clamp(camera.zoom, 0.1, 5);
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ecs_entity_t laser = ecs_new_id(ecs);

            const Rotation* rot = ecs_get(ecs, player, Rotation);
            ecs_set_ptr(ecs, laser, Rotation, rot);
            
            Velocity vel = Vector2Rotate((Vector2){0, -500}, *ecs_get(ecs, player, Rotation));
            ecs_set_ptr(ecs, laser, Velocity, &vel);
            
            Velocity init_vel = Vector2Rotate((Vector2){0, -3 * a_starship.sheet.height}, *ecs_get(ecs, player, Rotation));
            Position pos = Vector2Add(init_vel, *ecs_get(ecs, player, Position));
            ecs_set_ptr(ecs, laser, Position, &pos);
            
            ecs_set(ecs, laser, Scale, {3});

            ecs_set_ptr(ecs, laser, Animation, &a_laser);
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
