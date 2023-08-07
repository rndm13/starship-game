#include "flecs.h"
#include "raylib.h"
#include "raymath.h"
#include <stdio.h>

typedef Vector2 Velocity;
typedef Vector2 Position;
typedef float Rotation;
typedef float Scale;

typedef struct Animation {
    Texture sheet;
    uint8_t frame_width;
    uint8_t cur_frame;
} Animation;

float ToDeg(float rad) { return (180 / PI) * rad; }
float ToRad(float deg) { return (PI / 180) * deg; }

Rectangle RecV(Vector2 a, Vector2 size) {
  return (Rectangle){a.x, a.y, size.x, size.y};
}


// 2.7 -> -1.5
// 2.7 + 1.5 = 4.2
// 2.7 - 1.5 = 1.2
float RotationLerp(float rot, float target, float mul) {
    if (fabs(target + rot) > fabs(target - rot)) {
        rot += mul * (target - rot);
    } else {
        rot += mul * (target + rot);
    }
    if (rot > PI) {
        rot -= 2*PI;
    } else if (rot < -PI) {
        rot += 2*PI;
    }
    return rot;
}

void Move(ecs_iter_t *it) {
  Position *p = ecs_field(it, Position, 1);
  Velocity *v = ecs_field(it, Velocity, 2);
  Rotation *r = ecs_field(it, Rotation, 3);

  for (int i = 0; i < it->count; i++) {
    p[i].x += v[i].x;
    p[i].y += v[i].y;

    Rotation target = -Vector2Angle((Vector2){0, -1}, v[i]);
    r[i] = RotationLerp(r[i], target, 0.3);
    // r[i] = target;
  }
}

void Draw(ecs_iter_t *it) {
  Position *p = ecs_field(it, Position, 1);
  Rotation *r = ecs_field(it, Rotation, 2);
  Scale *s = ecs_field(it, Scale, 3);
  Texture *t = ecs_field(it, Texture, 4);

  for (int i = 0; i < it->count; i++) {
    char buf[255];
    sprintf(buf, "%f\n", r[i]);
    DrawText(buf, p[i].x, p[i].y - 100, 14, BLACK);

    Vector2 size = Vector2Scale((Vector2){t[i].width, t[i].height}, s[i]);
    Vector2 halfsize = Vector2Scale(size, 0.5);
    
    Vector2 pos = Vector2Subtract(p[i], Vector2Rotate(halfsize, r[i]));

    DrawTextureEx(t[i], pos, ToDeg(r[i]), s[i], WHITE);

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
      .zoom = 1.,
      .offset = {screenWidth / 2., screenHeight / 2.},
      .target = {0., 0.},
      .rotation = 0.,
  };

  ECS_COMPONENT(ecs, Position);
  ECS_COMPONENT(ecs, Velocity);
  ECS_COMPONENT(ecs, Rotation);
  ECS_COMPONENT(ecs, Scale);
  ECS_COMPONENT(ecs, Texture);

  ECS_SYSTEM(ecs, Move, EcsOnUpdate, Position, Velocity, Rotation);
  ECS_SYSTEM(ecs, Draw, EcsOnUpdate, Position, Rotation, Scale, Texture);

  ecs_entity_t player = ecs_new_id(ecs);
  ecs_set(ecs, player, Position, {0, 0});
  ecs_set(ecs, player, Velocity, {0, 0});
  ecs_set(ecs, player, Rotation, {0});
  ecs_set(ecs, player, Scale, {5});
  Texture arrow = LoadTexture(ASSET "Arrow.png");
  ecs_set_ptr(ecs, player, Texture, &arrow);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(WHITE);
    BeginMode2D(camera);

    ecs_progress(ecs, GetFrameTime());

    { // player movement
      static Velocity player_vel;
      player_vel = *ecs_get(ecs, player, Velocity);
      bool changed = false;

      if (IsKeyDown(KEY_RIGHT)) {
        player_vel.x = Clamp(player_vel.x + 2, -3, 3);
        changed = true;
      }
      if (IsKeyDown(KEY_LEFT)) {
        player_vel.x = Clamp(player_vel.x - 2, -3, 3);
        changed = true;
      }
      if (IsKeyDown(KEY_UP)) {
        player_vel.y = Clamp(player_vel.y - 2, -3, 3);
        changed = true;
      }
      if (IsKeyDown(KEY_DOWN)) {
        player_vel.y = Clamp(player_vel.y + 2, -3, 3);
        changed = true;
      }

      if (!changed) { // Slow down if no movement keys are pressed
        player_vel.x = Lerp(player_vel.x, 0, 0.3);
        player_vel.y = Lerp(player_vel.y, 0, 0.3);
      }

      ecs_set_ptr(ecs, player, Velocity, &player_vel);
    }

    float wheel = GetMouseWheelMove();

    if (wheel != 0) {
      // Get the world point that is under the mouse
      Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

      // Set the offset to where the mouse is
      camera.offset = GetMousePosition();

      // Set the target to match, so that the camera maps the world space point
      // under the cursor to the screen space point under the cursor at any zoom
      camera.target = mouseWorldPos;

      // Zoom increment
      const float zoomIncrement = 0.125f;

      camera.zoom += (wheel * zoomIncrement);
      if (camera.zoom < zoomIncrement)
        camera.zoom = zoomIncrement;
    }

    EndMode2D();
    DrawFPS(5, 5);
    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context
  ecs_fini(ecs);

  return 0;
}
