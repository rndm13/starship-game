#include "flecs.h"
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

typedef struct HitBox {
    int32_t damage;
    
    enum {
        LINE,
        CIRCLE,
    } type;

    union { // Relative to the Position component
        struct {
            float length;
        } line_data;
        
        struct {
            float radius;
        } circle_data;
    } data;
} HitBox;

HitBox CircleHitBox(int32_t damage, float radius) {
    return (HitBox){
        .damage = damage,
        .type = CIRCLE,
        .data.circle_data.radius = radius / 2,
    };
}

HitBox LineHitBox(int32_t damage, float length) {
    return (HitBox){
        .damage = damage,
        .type = LINE,
        .data.line_data.length = length / 2,
    };
}

Vector2 Vector2MoveRotation(Vector2 pos, float dist, float rot) {
    return Vector2Add(pos, Vector2Rotate((Vector2){.x = 0, .y = -dist}, rot));
}

float Vector2AngleTo(Vector2 pos_a, Vector2 pos_b) {
    return Vector2LineAngle(pos_a, pos_b) - PI/2;
}

Vector2 GetLineBegin(Position pos, Rotation rot, HitBox hb) {
    assert(hb.type == LINE);

    return Vector2MoveRotation(pos, -hb.data.line_data.length, rot);
}

Vector2 GetLineEnd(Position pos, Rotation rot, HitBox hb) {
    assert(hb.type == LINE);

    return Vector2MoveRotation(pos, hb.data.line_data.length, rot);
}

bool CheckHit(
        Position a_pos, Rotation a_rot, HitBox a,
        Position b_pos, Rotation b_rot, HitBox b) {
    // quite ugly.......
    switch (a.type) {
        case LINE: {
            Vector2 a_begin = GetLineBegin(a_pos, a_rot, a);
            Vector2 a_end = GetLineEnd(a_pos, a_rot, a);
            switch (b.type) {
                case LINE: {
                    Vector2 b_begin = GetLineBegin(b_pos, b_rot, b);
                    Vector2 b_end = GetLineEnd(b_pos, b_rot, b);
                    return CheckCollisionLines(
                            a_begin, 
                            a_end,
                            b_begin, 
                            b_end, 0);
                }
                case CIRCLE: {
                    return CheckCollisionPointLine(b_pos, a_begin, a_end, b.data.circle_data.radius);
                }
            }
        }
        case CIRCLE:
            switch (b.type) {
                case LINE: {
                    Vector2 b_begin = GetLineBegin(b_pos, b_rot, b);
                    Vector2 b_end = GetLineEnd(b_pos, b_rot, b);
                    return CheckCollisionPointLine(a_pos, b_begin, b_end, a.data.circle_data.radius);
                }
                case CIRCLE: {
                    return CheckCollisionCircles(
                            a_pos, a.data.circle_data.radius,
                            b_pos, b.data.circle_data.radius);
                }
            }
    }
}

typedef struct IFrames {
    uint8_t init;
    uint8_t cur;
} IFrames;

typedef enum Flags {
    PARTICLE = 1 << 0,
    EXPLODE_ON_DEATH = 1 << 1,
    PUSH_ON_COLLISION = 1 << 2,
} Flags;

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

typedef enum AIType {
    NONE,
    HOMING,
} AIType;

typedef struct AIInfo {
    AIType type;

    float max_velocity;
    float max_turning_speed;
} AIInfo;

#define COMPONENTS(ecs) \
    ECS_COMPONENT(ecs, Animation); \
    \
    ECS_COMPONENT(ecs, AIInfo); \
    \
    ECS_COMPONENT(ecs, IFrames); \
    \
    ECS_COMPONENT(ecs, Position); \
    ECS_COMPONENT(ecs, Velocity); \
    ECS_COMPONENT(ecs, Rotation); \
    ECS_COMPONENT(ecs, Scale); \
    \
    ECS_COMPONENT(ecs, Health); \
    \
    ECS_COMPONENT(ecs, HitBox); \
    \
    ECS_COMPONENT(ecs, Team); \
    \
    ECS_COMPONENT(ecs, Flags) 

typedef enum GameState {
    MAIN_MENU = 0,
    GAME = 1,
    DEATH_SCREEN = 2,
} GameState;

Rectangle RecV(Vector2 pos, Vector2 size) {
    return (Rectangle){pos.x, pos.y, size.x, size.y};
}

Rectangle RecEx(Vector2 pos, Vector2 size, Rotation r, Scale s) {
    Vector2 ss = Vector2Scale(size, s);
    Vector2 halfss = Vector2Scale(ss, 0.5);

    Vector2 p = Vector2Subtract(pos, Vector2Rotate(halfss, r));

    return RecV(p, ss);
}

float ToDeg(float rad) { return (180 / PI) * rad; }
float ToRad(float deg) { return (PI / 180) * deg; }
float LerpRad(float A, float B, float w) {
    float CS = (1-w)*cos(A) + w*cos(B);
    float SN = (1-w)*sin(A) + w*sin(B);
    float C = atan2(SN,CS);
    return C;
}

typedef struct Button {
    const char* text;
    int fsize;
    Color color;
    Color hcolor;

    Position pos;
    enum {
        DRAW_BORDER = 1 << 0,
    } flags;
} Button;

#define SPACING 5
#define FONT GetFontDefault()

Vector2 MeasureButton(Button b) {
    return MeasureTextEx(FONT, b.text, b.fsize, SPACING);
}

bool IsMouseHoveringButton(Button b) {
    Vector2 size = MeasureButton(b);
    Vector2 halfsize = Vector2Scale(size, 0.5);
    
    int offset = 15;
    
    Vector2 size_offset = Vector2AddValue(size, offset*2);
    Vector2 pos_offset = Vector2AddValue(Vector2Subtract(b.pos, halfsize), -offset);

    Rectangle rec = RecV(pos_offset, size_offset);

    return CheckCollisionPointRec(GetMousePosition(), rec);
}

bool ShowButton(Button b) {
    Vector2 size = MeasureButton(b);
    Vector2 halfsize = Vector2Scale(size, 0.5);
    
    int offset = 15;
    
    Vector2 size_offset = Vector2AddValue(size, offset*2);
    Vector2 pos_offset = Vector2AddValue(Vector2Subtract(b.pos, halfsize), -offset);

    Color color = IsMouseHoveringButton(b) ? b.hcolor : b.color;
    
    Rectangle rec = RecV(pos_offset, size_offset);
   
    if (b.flags & DRAW_BORDER) {
        DrawRectangleRec(rec, BLACK);
    }

    DrawTextPro(FONT, b.text, b.pos, halfsize, 0, b.fsize, SPACING, color);

    if (b.flags & DRAW_BORDER) {
        DrawRectangleLinesEx(rec, 5, color);
    }

    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsMouseHoveringButton(b);
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

void Collisions(ecs_iter_t *it) {
    const Flags *f = ecs_field(it, Flags, 1);

    Position *p = ecs_field(it, Position, 2);
    Velocity *v = ecs_field(it, Velocity, 3);
    const Rotation *r = ecs_field(it, Rotation, 4);

    const HitBox *hb = ecs_field(it, HitBox, 5);
    const Team *t = ecs_field(it, Team, 6);

    Health *h = ecs_field(it, Health, 7);
    IFrames *im = ecs_field(it, IFrames, 8);

    for (int i = 0; i < it->count; i++) {
        for (int j = i + 1; j < it->count; j++) {
            if (!CheckHit(p[i], r[i], hb[i], p[j], r[j], hb[j])) continue;

            if (im[i].cur <= 0 && im[j].cur <= 0 && t[i] != t[j]) {
                // Decrement health
                h[i] -= hb[j].damage;
                h[j] -= hb[i].damage;

                // Add iframes
                im[i].cur += im[i].init;
                im[j].cur += im[j].init;
            } 
           
            if (hb[i].type != CIRCLE || hb[j].type != CIRCLE) continue; 
            
            if (f[i] & PUSH_ON_COLLISION) {
                p[i] = Vector2MoveRotation(p[i], 90 * it->delta_time, Vector2AngleTo(p[i], p[j]));
            }

            if (f[j] & PUSH_ON_COLLISION) {
                p[j] = Vector2MoveRotation(p[j], 90 * it->delta_time, Vector2AngleTo(p[j], p[i]));
            }
        }
    }
}

void HealthCheck(ecs_iter_t *it) {
    COMPONENTS(it->world);

    Health *h = ecs_field(it, Health, 1);
    Flags *f = ecs_field(it, Flags, 2);

    Animation* a_explosion = (Animation*)it->param;

    for (int i = 0; i < it->count; i++) {
        if (h[i] > 0) continue;
        if (f[i] & EXPLODE_ON_DEATH) {
            ecs_entity_t explosion = ecs_new_id(it->world);
            
            const Position* pos = ecs_get(it->world, it->entities[i], Position);
            
            if (pos) {
                ecs_set_ptr(it->world, explosion, Position, pos);

                ecs_set(it->world, explosion, Rotation, {0});
                ecs_set(it->world, explosion, Scale, {5});

                ecs_set_ptr(it->world, explosion, Animation, a_explosion);

                ecs_set(it->world, explosion, Flags, {PARTICLE});
            }
        } 
        ecs_delete(it->world, it->entities[i]);
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

void SimulateAI(ecs_iter_t *it) {
    Flags *f = ecs_field(it, Flags, 1);
    
    Position *p = ecs_field(it, Position, 2);
    Rotation *r = ecs_field(it, Rotation, 3);
    Velocity *v = ecs_field(it, Velocity, 4);

    AIInfo *ai = ecs_field(it, AIInfo, 5);

    Position player_pos = *(Position*)it->param;

    for (int i = 0; i < it->count; i++) {
        switch (ai[i].type) {
            case NONE: {
                break;
            }
            case HOMING: {
                float rot = -Vector2Angle((Vector2){0, -1}, Vector2Subtract(player_pos, p[i]));
                r[i] = LerpRad(r[i], rot, it->delta_time*ai[i].max_turning_speed);

                v[i] = Vector2Rotate((Vector2){0, -ai[i].max_velocity}, r[i]);
                break;
            }
        }
    }
}

void DrawAnimation(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Rotation *r = ecs_field(it, Rotation, 2);
    Scale *s = ecs_field(it, Scale, 3);
    Animation *a = ecs_field(it, Animation, 4);

    for (int i = 0; i < it->count; i++) {
        Rectangle source = {a[i].cur_frame * a[i].frame_width, 0, a[i].frame_width, a[i].sheet.height};

        Rectangle dest = RecEx(p[i], FrameSize(a[i]), r[i], s[i]);
        Rectangle dest_norot = RecEx(p[i], FrameSize(a[i]), 0, s[i]);

        DrawTexturePro(a[i].sheet, source, dest, Vector2Zero(), ToDeg(r[i]), WHITE);

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


void DrawAnimationIFrames(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Rotation *r = ecs_field(it, Rotation, 2);
    Scale *s = ecs_field(it, Scale, 3);
    Animation *a = ecs_field(it, Animation, 4);
    IFrames *im = ecs_field(it, IFrames, 5);

    Shader* sh_immunity = (Shader*)it->param;

    for (int i = 0; i < it->count; i++) {
        Rectangle source = {a[i].cur_frame * a[i].frame_width, 0, a[i].frame_width, a[i].sheet.height};

        Rectangle dest = RecEx(p[i], FrameSize(a[i]), r[i], s[i]);
        Rectangle dest_norot = RecEx(p[i], FrameSize(a[i]), 0, s[i]);

        if (im[i].cur > 0) {
            BeginShaderMode(*sh_immunity);
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

void DrawHitBox(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Rotation *r = ecs_field(it, Rotation, 2);
    HitBox *hb = ecs_field(it, HitBox, 3);

    for (int i = 0; i < it->count; i++) {
        switch(hb[i].type) {
            case LINE: {
                           Vector2 begin = GetLineBegin(p[i], r[i], hb[i]);
                           Vector2 end = GetLineEnd(p[i], r[i], hb[i]);
                           DrawLineEx(begin, end, 3, RED);
                           break;
                       }
            case CIRCLE:
                       DrawCircleV(p[i], hb[i].data.circle_data.radius, RED);
                       break;
        }
    }
}

typedef struct PlayerInfo {
    Animation anim;
} PlayerInfo;

ecs_entity_t MakePlayer(ecs_world_t *ecs, PlayerInfo info) {
    COMPONENTS(ecs);

    ecs_entity_t player = ecs_new_id(ecs);

    ecs_set(ecs, player, Position, {0, 0});
    ecs_set(ecs, player, Velocity, {0, 0});
    ecs_set(ecs, player, Rotation, {0});

    float scale = 5;
    ecs_set(ecs, player, Scale, {scale});
    ecs_set(ecs, player, Health, {5});

    HitBox hb = CircleHitBox(0, info.anim.sheet.height * scale);
    ecs_set_ptr(ecs, player, HitBox, &hb);

    ecs_set(ecs, player, Team, {0});
    ecs_set(ecs, player, Flags, {EXPLODE_ON_DEATH});
    ecs_set(ecs, player, IFrames, {16, 0});

    ecs_set_ptr(ecs, player, Animation, &info.anim);
    ecs_set(ecs, player, AIInfo, {NONE});

    return player;
}

void DrawBackground(Texture t_bg, Camera2D camera, float offset_scale) {
    Position top = GetScreenToWorld2D((Vector2){-1, -1}, camera);
    Position bot = GetScreenToWorld2D((Vector2){GetScreenWidth(), GetScreenHeight()}, camera);

    Position bg_offset = Vector2Scale(camera.target, -offset_scale);
    bg_offset.x = (int)bg_offset.x % t_bg.width - t_bg.width; 
    bg_offset.y = (int)bg_offset.y % t_bg.height - t_bg.height;

    for (int16_t x = bg_offset.x + top.x; x <= bot.x; x += t_bg.width) {
        for (int16_t y = bg_offset.y + top.y; y <= bot.y; y += t_bg.height) {
            DrawTextureEx(t_bg, (Vector2){x, y}, 0, 1, WHITE);
        }
    }
}

int main(void) {
    const int screenWidth = 1360;
    const int screenHeight = 700;

    InitWindow(screenWidth, screenHeight, "starship game");
    ToggleBorderlessWindowed();
    ecs_world_t *ecs = ecs_init();
    ecs_set_threads(ecs, 4);

    SetTargetFPS(60);

    Camera2D camera = {
        .zoom = 1,
        .offset = {screenWidth / 2., screenHeight / 2.},
        .target = {0., 0.},
        .rotation = 0.,
    };

    Shader sh_immunity = LoadShader(0, ASSET "immunity.fs");

    int sh_im_time = GetShaderLocation(sh_immunity, "time");
    float timeSec = 0;

    Animation a_starship = {
        .sheet = LoadTexture(ASSET "starship.png"),
        .cur_frame = 0,
        .frame_width = 16,
        .time = 0,
        .fps = 8,
    };

    Animation a_enemy = {
        .sheet = LoadTexture(ASSET "Enemy.png"),
        .frame_width = 32,
        .fps = 8,
        
        .time = 0,
        .cur_frame = 0,
    };


    Animation a_laser = {
        .sheet = LoadTexture(ASSET "laser.png"),
        .cur_frame = 0,
        .frame_width = 1,
        .time = 0,
        .fps = 60,
    };

    Animation a_explosion = (Animation){
        .sheet = LoadTexture(ASSET "Explosion.png"),
        .cur_frame = 0,
        .frame_width = 16,
        .time = 0,
        .fps = 8,
    };

    Texture t_bg = LoadTexture(ASSET "Background.png");
    Texture t_mg = LoadTexture(ASSET "Midground.png");
    Texture t_fg = LoadTexture(ASSET "Foreground.png");

    Texture t_heart = LoadTexture(ASSET "Heart.png");

    GameState gs = MAIN_MENU;

    COMPONENTS(ecs);

    AIInfo default_homing_ai = {
        .type = HOMING,
        .max_velocity = 100,
        .max_turning_speed = PI,
    };

    ecs_entity_t move = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
                .name = "Move"
        }),
        .query.filter.terms = {
            {.id = ecs_id(Position)},
            {.id = ecs_id(Velocity)},
            {.id = ecs_id(Rotation)},
        },
        .callback = Move,
        .multi_threaded = true, 
    });

    ecs_entity_t collisions = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
            .name = "Collisions"
        }),
        // .query.filter.flags = EcsTraverseAll | EcsTermMatchAny,
        .query.filter.terms = {
            {.id = ecs_id(Flags), .inout = EcsIn},
            {.id = ecs_id(Position), .inout = EcsInOut},
            {.id = ecs_id(Velocity), .inout = EcsInOut},
            {.id = ecs_id(Rotation), .inout = EcsIn},

            {.id = ecs_id(HitBox), .inout = EcsIn},
            {.id = ecs_id(Team), .inout = EcsIn},

            {.id = ecs_id(Health), .inout = EcsInOut},
            {.id = ecs_id(IFrames), .inout = EcsInOut},
            {.id = ecs_id(AIInfo), .inout = EcsInOutNone, .oper = EcsOptional},
        },
        .callback = Collisions,
        .multi_threaded = true, 
    });

    ecs_entity_t healthCheck = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
            .name = "HealthCheck"
        }),
        .query.filter.terms = {
            {.id = ecs_id(Health)},
            {.id = ecs_id(Flags)},
        },
        .callback = HealthCheck,
        .multi_threaded = true, 
    });
    
    ecs_entity_t removeParticles = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
            .name = "RemoveParticles",
        }),
        .query.filter.terms = {
            {.id = ecs_id(Flags)},
            {.id = ecs_id(Animation)},
        },
        .callback = RemoveParticles,
        .multi_threaded = true, 
    });
    
    ecs_entity_t decrementIFrames = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
            .name = "DecrementIFrames",
        }),
        .query.filter.terms = {
            {.id = ecs_id(IFrames)},
        },
        .callback = DecrementIFrames,
        .multi_threaded = true, 
    });

    ecs_entity_t simAI = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
            .name = "AISimulation"
        }),
        .query.filter.terms = {
            {.id = ecs_id(Flags)},
            {.id = ecs_id(Position)},
            {.id = ecs_id(Rotation)},
            {.id = ecs_id(Velocity)},
            {.id = ecs_id(AIInfo)},
        },
        .callback = SimulateAI,
        .multi_threaded = true, 
    });

    ecs_entity_t draw = ecs_system(ecs, {
        .entity = ecs_entity(ecs, {
            .name = "Draw"
        }),
        .query.filter.terms = {
            { .id = ecs_id(Position)},
            { .id = ecs_id(Rotation)},
            { .id = ecs_id(Scale)},
            { .id = ecs_id(Animation)},
            { .id = ecs_id(IFrames), .oper = EcsNot},
        },
        .callback = DrawAnimation,
        .multi_threaded = true, 
    });

    ecs_entity_t drawIFrames = ecs_system(ecs, {
                .entity = ecs_entity(ecs, {
                    .name = "DrawIFrames"
                }),
                .query.filter.terms = {
                    { .id = ecs_id(Position)},
                    { .id = ecs_id(Rotation)},
                    { .id = ecs_id(Scale)},
                    { .id = ecs_id(Animation)},
                    { .id = ecs_id(IFrames)},
                },
                .callback = DrawAnimationIFrames,
                .multi_threaded = true, 
            });
    
    ecs_entity_t drawHB = ecs_system(ecs, {
                .entity = ecs_entity(ecs, {
                    .name = "DrawHitBoxes"
                }),
                .query.filter.terms = {
                    { .id = ecs_id(Position)},
                    { .id = ecs_id(Rotation)},
                    { .id = ecs_id(HitBox)},
                },
                .callback = DrawHitBox, 
                .multi_threaded = true,
            });
    
    while (!WindowShouldClose()) {
        BeginDrawing();
        BeginMode2D(camera);
        const float dt = GetFrameTime();

        timeSec += dt;
        SetShaderValue(sh_immunity, sh_im_time, &timeSec, SHADER_UNIFORM_FLOAT);

        static ecs_entity_t player = 0;

        static Position player_pos = {0, 0};
        static Velocity player_vel = {0, 0};

        if (ecs_is_valid(ecs, player)) {
            player_vel = *ecs_get(ecs, player, Velocity);
            player_pos = *ecs_get(ecs, player, Position);
        }
        
        // ---------------- PROCESSING ----------------
        
        if (ecs_is_valid(ecs, player)) { 
            // Player controls

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

                float scale = 5;
                ecs_set(ecs, laser, Scale, {scale});

                ecs_set(ecs, laser, Health, {3});
                
                HitBox hb = LineHitBox(1, a_laser.sheet.height * scale);
                ecs_set_ptr(ecs, laser, HitBox, &hb);
                
                ecs_set(ecs, laser, Team, {0});
                ecs_set(ecs, laser, Flags, {0});
                ecs_set(ecs, laser, IFrames, {0, 0});

                ecs_set_ptr(ecs, laser, Animation, &a_laser);
                
                ecs_set(ecs, laser, AIInfo, {NONE});
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                ecs_entity_t enemy = ecs_new_id(ecs);

                ecs_set(ecs, enemy, Rotation, {0});

                ecs_set(ecs, enemy, Velocity, {0, 0});

                Position pos = GetScreenToWorld2D(GetMousePosition(), camera);
                ecs_set_ptr(ecs, enemy, Position, &pos);

                float scale = 2;
                ecs_set(ecs, enemy, Scale, {scale});

                ecs_set(ecs, enemy, Health, {3});

                HitBox hb = CircleHitBox(1, a_enemy.sheet.height * scale);
                ecs_set_ptr(ecs, enemy, HitBox, &hb);

                ecs_set(ecs, enemy, Team, {1});
                ecs_set(ecs, enemy, Flags, {EXPLODE_ON_DEATH | PUSH_ON_COLLISION});
                ecs_set(ecs, enemy, IFrames, {.init = 16, .cur = 0});
                
                ecs_set_ptr(ecs, enemy, AIInfo, &default_homing_ai);

                ecs_set_ptr(ecs, enemy, Animation, &a_enemy);
            }
        } 

        { // Camera 
            camera.target = Vector2Lerp(camera.target, player_pos, 1 * dt);

            if (IsKeyDown(KEY_LEFT_BRACKET)) camera.zoom -= 0.01;
            if (IsKeyDown(KEY_RIGHT_BRACKET)) camera.zoom += 0.01;
            camera.zoom = Clamp(camera.zoom, 0.1, 5);
        }
        
        ecs_run(ecs, simAI, dt, &player_pos);
        ecs_run(ecs, collisions, dt, 0);
        ecs_run(ecs, move, dt, 0);
        
        ecs_run(ecs, removeParticles, dt, 0);
        ecs_run(ecs, decrementIFrames, dt, 0);
        ecs_run(ecs, healthCheck, dt, &a_explosion);


        // ------------ DRAWING ----------------
        
        { // Backgrounds
            DrawBackground(t_bg, camera, 0.1);
            DrawBackground(t_mg, camera, 0.4);
            DrawBackground(t_fg, camera, 0.9);
        }

        // ecs_run(ecs, drawHB, dt, 0);
        ecs_run(ecs, draw, dt, 0);
        ecs_run(ecs, drawIFrames, dt, &sh_immunity);

        EndMode2D();

        // UI    
        {
            DrawFPS(GetScreenWidth() - 100, 5);

            Button b_default = {
                .text = "DEFAULT, YOU SHOULD SET THIS YOURSELF",
                .pos = {(float)GetScreenWidth() / 2, 500},
                .fsize = 48,
                .color = WHITE,
                .hcolor = RED,
                .flags = DRAW_BORDER,
            };

            switch (gs) {
                case GAME: {
                   static Health player_hp = 0; 

                   if (ecs_is_valid(ecs, player)) {
                       player_hp = *ecs_get(ecs, player, Health);
                   } else { 
                        gs = DEATH_SCREEN;
                   }

                   for (int i = 0; i < player_hp; ++i) {
                       DrawTextureEx(t_heart, (Vector2){i * (t_heart.width * 3 + 7) + 15, 15}, 0, 3, WHITE);
                   }
                } break;
                
                case MAIN_MENU: {
                    Button b_play = b_default;
                    b_play.text = "PLAY";
                    if (ShowButton(b_play)) {
                        { // Delete every entity (that has a flags component)
                            ecs_query_t *q = ecs_query(ecs, {
                                        .filter.terms = {
                                            {ecs_id(Flags)}
                                        }
                                    });

                            ecs_iter_t it = ecs_query_iter(ecs, q);

                            while (ecs_query_next(&it)) {
                                for (int i = 0; i < it.count; ++i) {
                                    ecs_delete(ecs, it.entities[i]);
                                }
                            }
                        }

                        player = MakePlayer(ecs, (PlayerInfo){.anim = a_starship});
                        
                        gs = GAME;
                    }
                } break;

                case DEATH_SCREEN: {
                    Button b_restart = b_default;
                    b_restart.text = "RESTART";
                    
                    Button b_main_menu = b_default;
                    b_main_menu.text = "MAIN MENU";
                    b_main_menu.pos.y = 600;

                    if (ShowButton(b_restart)) {
                        gs = GAME;
                        
                        { // Delete every entity (that has a flags component)
                            ecs_query_t *q = ecs_query(ecs, {
                                        .filter.terms = {
                                            {ecs_id(Flags)}
                                        }
                                    });

                            ecs_iter_t it = ecs_query_iter(ecs, q);

                            while (ecs_query_next(&it)) {
                                for (int i = 0; i < it.count; ++i) {
                                    ecs_delete(ecs, it.entities[i]);
                                }
                            }
                        }

                        player = MakePlayer(ecs, (PlayerInfo){ .anim = a_starship });
                        camera.target = *ecs_get(ecs, player, Position);
                    }
                    
                    if (ShowButton(b_main_menu)) {
                        gs = MAIN_MENU;
                    }
                } break;
            }
        }

        EndDrawing();
    }

    UnloadShader(sh_immunity);

    CloseWindow(); // Close window and OpenGL context
    ecs_fini(ecs);

    return 0;
}
