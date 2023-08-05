#include "raylib.h"

int main(void)
{
    const int screenWidth = 1360;
    const int screenHeight = 700;

    InitWindow(screenWidth, screenHeight, "gaming game gamers");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

    Camera2D camera = {
        .zoom = 1.,
        .offset = {screenWidth/2.,screenHeight/2.},
        .target = {0.,0.},
        .rotation = 0.,

    };

    while (!WindowShouldClose())
    {
        BeginDrawing();
        BeginMode2D(camera);
            if (IsKeyDown(KEY_RIGHT)) camera.target.x += 2.0f;
            if (IsKeyDown(KEY_LEFT)) camera.target.x -= 2.0f;
            if (IsKeyDown(KEY_UP)) camera.target.y -= 2.0f;
            if (IsKeyDown(KEY_DOWN)) camera.target.y += 2.0f;

            ClearBackground(RAYWHITE);

            DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

        EndMode2D();
        EndDrawing();
    }

    CloseWindow();        // Close window and OpenGL context

    return 0;
}
