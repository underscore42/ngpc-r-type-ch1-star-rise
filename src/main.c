#include "ngpc.h"
#define CARTHDR_IMPL
#include "carthdr.h"
#include "library.h"

#define STATE_MENU   0
#define STATE_GAME   1
#define STATE_SCORES 2

/* Tiles */
/* Ship: 2 tiles wide — sleek fighter */
static const u16 tile_ship_l[8] = {
    0x0000, 0x0006, 0x003E, 0x01FE,
    0x0FFE, 0x01FE, 0x003E, 0x0000
};
static const u16 tile_ship_r[8] = {
    0x0000, 0xF000, 0xFC00, 0xFFF0,
    0xFFFC, 0xFFF0, 0xFC00, 0x0000
};
static const u16 tile_shot[8] = {
    0x0000, 0x0000, 0x0000, 0x07E0,
    0x07E0, 0x0000, 0x0000, 0x0000
};
static const u16 tile_enemy1[8] = {
    0x0000, 0x0C00, 0x0FC0, 0x3FF0,
    0x3FF0, 0x0FC0, 0x0C00, 0x0000
};
static const u16 tile_enemy2[8] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC,
    0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};
static const u16 tile_enemy3[8] = {
    0x1818, 0x3C3C, 0x7FFE, 0xFFFF,
    0xFFFF, 0x7FFE, 0x3C3C, 0x1818
};
static const u16 tile_boss[8] = {
    0x0FF0, 0x1FF8, 0x3FFC, 0x7FFE,
    0xFFFF, 0x7FFE, 0x3FFC, 0x1FF8
};
static const u16 tile_powerup[8] = {
    0x0180, 0x03C0, 0x07E0, 0x0FF0,
    0x0FF0, 0x07E0, 0x03C0, 0x0180
};
static const u16 tile_explode[8] = {
    0x2184, 0x1248, 0x0810, 0x4422,
    0x4422, 0x0810, 0x1248, 0x2184
};
static const u16 tile_wall[8] = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
};
static const u16 tile_star[8] = {
    0x0000, 0x0000, 0x0000, 0x0080,
    0x0000, 0x0000, 0x0000, 0x0000
};

#define T_SHIP_L  128
#define T_SHIP_R  139
#define T_SHOT    129
#define T_ENEMY1  130
#define T_ENEMY2  131
#define T_ENEMY3  132
#define T_BOSS    133
#define T_POWER   134
#define T_EXPLODE 135
#define T_WALL    136
#define T_STAR    137

/* Entities */
#define MAX_SHOTS   5
#define MAX_ENEMIES 12
#define MAX_STARS   10

/* Enemy types */
#define E_NONE    0
#define E_GRUNT   1    /* straight left */
#define E_WAVE    2    /* sine wave */
#define E_DIVE    3    /* dive at player */
#define E_BOSS    4
#define E_POWER   5

static u8 shot_x[MAX_SHOTS];
static u8 shot_y[MAX_SHOTS];
static u8 shot_active[MAX_SHOTS];

static u8 en_type[MAX_ENEMIES];
static u8 en_x[MAX_ENEMIES];
static u8 en_y[MAX_ENEMIES];
static u8 en_hp[MAX_ENEMIES];
static u8 en_tick[MAX_ENEMIES];
static u8 en_base_y[MAX_ENEMIES];  /* for wave enemies */

static u8 star_x[MAX_STARS];
static u8 star_y[MAX_STARS];

/* Player */
static u8 px;
static u8 py;
static u8 alive;
static u8 death_timer;
static u8 power_level;    /* 0=single, 1=double, 2=triple */

/* Game */
static u8 pad_cur;
static u8 pad_prev;
static u8 pad_press;
static u8 game_over;
static u8 lives;
static u8 level;
static u8 menu_sel;
static u8 state;
static u8 skip;
static u16 score;
static u8 spawn_tick;
static u8 spawn_rate;
static u8 wave_counter;
static u8 boss_spawned;
static u8 fire_tick;
static u8 scroll_tick;
static u16 high_scores[5] = {5000, 3000, 2000, 1000, 500};
static u8 rand_seed;

static u8 cheap_rand(u8 max)
{
    rand_seed = rand_seed + 7;
    rand_seed = rand_seed ^ (rand_seed << 3);
    rand_seed = rand_seed ^ (rand_seed >> 1);
    if (max == 0) return 0;
    return rand_seed % max;
}

static void read_input(void)
{
    pad_prev  = pad_cur;
    pad_cur   = JOYPAD & 0x7F;
    pad_press = pad_cur & ~pad_prev;
}

static void install_tiles(void)
{
    InstallTileSetAt((const unsigned short (*)[8])tile_ship_l, 8, T_SHIP_L);
    InstallTileSetAt((const unsigned short (*)[8])tile_ship_r, 8, T_SHIP_R);
    InstallTileSetAt((const unsigned short (*)[8])tile_shot, 8, T_SHOT);
    InstallTileSetAt((const unsigned short (*)[8])tile_enemy1, 8, T_ENEMY1);
    InstallTileSetAt((const unsigned short (*)[8])tile_enemy2, 8, T_ENEMY2);
    InstallTileSetAt((const unsigned short (*)[8])tile_enemy3, 8, T_ENEMY3);
    InstallTileSetAt((const unsigned short (*)[8])tile_boss, 8, T_BOSS);
    InstallTileSetAt((const unsigned short (*)[8])tile_powerup, 8, T_POWER);
    InstallTileSetAt((const unsigned short (*)[8])tile_explode, 8, T_EXPLODE);
    InstallTileSetAt((const unsigned short (*)[8])tile_wall, 8, T_WALL);
    InstallTileSetAt((const unsigned short (*)[8])tile_star, 8, T_STAR);
}

static void put_score(u8 x, u8 y, u16 val)
{
    u16 v;
    v = val;
    PutTile(SCR_1_PLANE, 0, x + 4, y, '0' + v % 10); v = v / 10;
    PutTile(SCR_1_PLANE, 0, x + 3, y, '0' + v % 10); v = v / 10;
    PutTile(SCR_1_PLANE, 0, x + 2, y, '0' + v % 10); v = v / 10;
    PutTile(SCR_1_PLANE, 0, x + 1, y, '0' + v % 10); v = v / 10;
    PutTile(SCR_1_PLANE, 0, x, y, '0' + v % 10);
}

static void draw_hud(void)
{
    put_score(0, 0, score);
    PutTile(SCR_1_PLANE, 0, 8, 0, '0' + lives);
    PutTile(SCR_1_PLANE, 0, 12, 0, '0' + power_level + 1);
    PutTile(SCR_1_PLANE, 0, 18, 0, '0' + level / 10);
    PutTile(SCR_1_PLANE, 0, 19, 0, '0' + level % 10);
}

static void init_stars(void)
{
    u8 i;
    for (i = 0; i < MAX_STARS; i++) {
        star_x[i] = cheap_rand(20);
        star_y[i] = 2 + cheap_rand(16);
    }
}

static void draw_stars(void)
{
    u8 i;
    for (i = 0; i < MAX_STARS; i++) {
        PutTile(SCR_1_PLANE, 0, star_x[i], star_y[i], T_STAR);
    }
}

static void scroll_stars(void)
{
    u8 i;
    for (i = 0; i < MAX_STARS; i++) {
        PutTile(SCR_1_PLANE, 0, star_x[i], star_y[i], ' ');
        if (star_x[i] == 0) {
            star_x[i] = 19;
            star_y[i] = 2 + cheap_rand(16);
        } else {
            star_x[i] = star_x[i] - 1;
        }
        PutTile(SCR_1_PLANE, 0, star_x[i], star_y[i], T_STAR);
    }
}

/* ---- Menu ---- */
static void show_menu(void)
{
    ClearScreen(SCR_1_PLANE);
    SysSetSystemFont();
    install_tiles();

    SetPalette(SCR_1_PLANE, 1, 0,
        RGB(15,15,15), RGB(10,10,12), RGB(7,7,9));
    SetPalette(SCR_1_PLANE, 4, 0,
        RGB(15,4,0), RGB(15,8,0), RGB(15,0,0));

    /* Title */
    PutTile(SCR_1_PLANE, 1, 8, 2, T_SHIP_L);
    PutTile(SCR_1_PLANE, 1, 9, 2, T_SHIP_R);
    PrintString(SCR_1_PLANE, 0, 6, 4, "R-TYPE");

    /* Decorative enemies */
    PutTile(SCR_1_PLANE, 4, 4, 6, T_ENEMY1);
    PutTile(SCR_1_PLANE, 4, 15, 6, T_ENEMY2);

    PrintString(SCR_1_PLANE, 0, 7, 9, "START");
    PrintString(SCR_1_PLANE, 0, 5, 11, "HIGH SCORES");
    PrintString(SCR_1_PLANE, 0, 6, 15, "A:SELECT");
    PrintString(SCR_1_PLANE, 1, 5, 9 + menu_sel * 2, ">");
}

static void show_scores(void)
{
    u8 i;
    ClearScreen(SCR_1_PLANE);
    PrintString(SCR_1_PLANE, 0, 4, 1, "HIGH SCORES");
    for (i = 0; i < 5; i++) {
        PutTile(SCR_1_PLANE, 0, 5, 4 + i * 2, '1' + i);
        PutTile(SCR_1_PLANE, 0, 6, 4 + i * 2, '.');
        put_score(8, 4 + i * 2, high_scores[i]);
    }
    PrintString(SCR_1_PLANE, 0, 4, 16, "OPT:BACK");
}

/* ---- Enemies ---- */
static void spawn_enemy(u8 type)
{
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (en_type[i] == E_NONE) {
            en_type[i] = type;
            en_x[i] = 19;
            en_y[i] = 2 + cheap_rand(15);
            en_base_y[i] = en_y[i];
            en_tick[i] = 0;
            if (type == E_BOSS) {
                en_hp[i] = 10;
                en_y[i] = 8;
            } else if (type == E_POWER) {
                en_hp[i] = 1;
            } else {
                en_hp[i] = 1;
            }
            return;
        }
    }
}

static void update_enemies(void)
{
    u8 i;
    u8 new_y;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (en_type[i] == E_NONE) continue;

        en_tick[i]++;

        /* Erase old */
        if (en_x[i] < 20 && en_y[i] >= 2 && en_y[i] < 18) {
            PutTile(SCR_1_PLANE, 0, en_x[i], en_y[i], ' ');
        }

        /* Move based on type */
        if (en_type[i] == E_GRUNT || en_type[i] == E_POWER) {
            if (en_tick[i] >= 4) {
                en_tick[i] = 0;
                if (en_x[i] == 0) { en_type[i] = E_NONE; continue; }
                en_x[i] = en_x[i] - 1;
            }
        } else if (en_type[i] == E_WAVE) {
            if (en_tick[i] >= 3) {
                en_tick[i] = 0;
                if (en_x[i] == 0) { en_type[i] = E_NONE; continue; }
                en_x[i] = en_x[i] - 1;
                /* Sine-ish wave: bounce between base_y-2 and base_y+2 */
                new_y = en_base_y[i] + ((en_x[i] & 3) > 1 ? 2 : 0);
                if (new_y < 2) new_y = 2;
                if (new_y > 17) new_y = 17;
                en_y[i] = new_y;
            }
        } else if (en_type[i] == E_DIVE) {
            if (en_tick[i] >= 3) {
                en_tick[i] = 0;
                if (en_x[i] == 0) { en_type[i] = E_NONE; continue; }
                en_x[i] = en_x[i] - 1;
                /* Dive toward player */
                if (en_y[i] < py && en_y[i] < 17) en_y[i] = en_y[i] + 1;
                if (en_y[i] > py && en_y[i] > 2) en_y[i] = en_y[i] - 1;
            }
        } else if (en_type[i] == E_BOSS) {
            if (en_tick[i] >= 6) {
                en_tick[i] = 0;
                /* Boss moves slowly, stays right side */
                if (en_x[i] > 14) en_x[i] = en_x[i] - 1;
                /* Bob up and down */
                new_y = en_base_y[i] + ((en_x[i] & 3) > 1 ? 2 : 0);
                if (new_y < 2) new_y = 2;
                if (new_y > 16) new_y = 16;
                en_y[i] = new_y;
            }
        }

        /* Draw */
        if (en_x[i] < 20 && en_y[i] >= 2 && en_y[i] < 18) {
            u8 epal;
            u16 etile;
            if (en_type[i] == E_GRUNT) { etile = T_ENEMY1; epal = 4; }
            else if (en_type[i] == E_WAVE) { etile = T_ENEMY2; epal = 5; }
            else if (en_type[i] == E_DIVE) { etile = T_ENEMY3; epal = 4; }
            else if (en_type[i] == E_BOSS) { etile = T_BOSS; epal = 6; }
            else if (en_type[i] == E_POWER) { etile = T_POWER; epal = 3; }
            else { etile = T_ENEMY1; epal = 4; }
            PutTile(SCR_1_PLANE, epal, en_x[i], en_y[i], etile);
        }
    }
}

/* ---- Shots ---- */
static void fire_shot(void)
{
    u8 i;
    u8 count;
    count = 0;

    /* Fire based on power level */
    for (i = 0; i < MAX_SHOTS; i++) {
        if (!shot_active[i] && count <= power_level) {
            shot_x[i] = px + 2;
            if (count == 0) shot_y[i] = py;
            else if (count == 1) shot_y[i] = py > 2 ? py - 1 : py;
            else shot_y[i] = py < 17 ? py + 1 : py;
            shot_active[i] = 1;
            count++;
            if (count > power_level) return;
        }
    }
}

static void update_shots(void)
{
    u8 i;
    u8 j;

    for (i = 0; i < MAX_SHOTS; i++) {
        if (!shot_active[i]) continue;

        /* Erase */
        if (shot_x[i] < 20 && shot_y[i] >= 2) {
            PutTile(SCR_1_PLANE, 0, shot_x[i], shot_y[i], ' ');
        }

        /* Move right */
        shot_x[i] = shot_x[i] + 1;
        if (shot_x[i] >= 20) {
            shot_active[i] = 0;
            continue;
        }

        /* Check hit */
        for (j = 0; j < MAX_ENEMIES; j++) {
            if (en_type[j] == E_NONE) continue;
            if (shot_x[i] == en_x[j] && shot_y[i] == en_y[j]) {
                en_hp[j] = en_hp[j] - 1;
                shot_active[i] = 0;
                if (en_hp[j] == 0) {
                    /* Destroyed */
                    PutTile(SCR_1_PLANE, 4, en_x[j], en_y[j], T_EXPLODE);
                    if (en_type[j] == E_BOSS) {
                        score = score + 500;
                        boss_spawned = 0;
                    } else if (en_type[j] == E_POWER) {
                        if (power_level < 2) power_level++;
                    } else {
                        score = score + 50;
                    }
                    en_type[j] = E_NONE;
                }
                break;
            }
        }

        /* Draw */
        if (shot_active[i] && shot_x[i] < 20 && shot_y[i] >= 2) {
            PutTile(SCR_1_PLANE, 1, shot_x[i], shot_y[i], T_SHOT);
        }
    }
}

/* ---- Collision: enemies vs player ---- */
static void check_player_hit(void)
{
    u8 i;
    u8 ptx;
    u8 pty;

    if (!alive) return;

    ptx = px;
    pty = py;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (en_type[i] == E_NONE || en_type[i] == E_POWER) continue;
        if ((en_x[i] == ptx || en_x[i] == ptx + 1) && en_y[i] == pty) {
            /* Hit! */
            alive = 0;
            death_timer = 30;
            PutTile(SCR_1_PLANE, 4, px, py, T_EXPLODE);
            if (px + 1 < 20) PutTile(SCR_1_PLANE, 4, px + 1, py, T_EXPLODE);
            lives = lives - 1;
            power_level = 0;
            if (lives == 0) {
                game_over = 1;
                {
                    u8 hi;
                    u8 hj;
                    for (hi = 0; hi < 5; hi++) {
                        if (score > high_scores[hi]) {
                            hj = 4;
                            while (hj > hi) {
                                high_scores[hj] = high_scores[hj - 1];
                                hj--;
                            }
                            high_scores[hi] = score;
                            break;
                        }
                    }
                }
                PrintString(SCR_1_PLANE, 0, 5, 9, "GAME OVER!");
                PrintString(SCR_1_PLANE, 0, 2, 11, "B:RETRY  OPT:MENU");
            }
            return;
        }
    }
}

/* ---- Game ---- */
static void start_game(void)
{
    u8 i;

    rand_seed = rand_seed + VBCounter;
    game_over = 0;
    lives = 3;
    level = 1;
    score = 0;
    power_level = 0;
    alive = 1;
    death_timer = 0;
    px = 2;
    py = 10;
    spawn_tick = 0;
    spawn_rate = 40;
    wave_counter = 0;
    boss_spawned = 0;
    fire_tick = 0;
    scroll_tick = 0;

    for (i = 0; i < MAX_SHOTS; i++) shot_active[i] = 0;
    for (i = 0; i < MAX_ENEMIES; i++) en_type[i] = E_NONE;

    ClearScreen(SCR_1_PLANE);
    SetBackgroundColour(RGB(0, 0, 0));
    SysSetSystemFont();
    install_tiles();

    SetPalette(SCR_1_PLANE, 0, 0,
        RGB(15,15,15), RGB(15,15,15), RGB(15,15,15));
    /* Ship: silver/white */
    SetPalette(SCR_1_PLANE, 1, 0,
        RGB(15,15,15), RGB(10,10,12), RGB(7,7,9));
    /* Shield/stars */
    SetPalette(SCR_1_PLANE, 2, 0,
        RGB(8,8,8), RGB(6,6,6), RGB(12,12,12));
    /* Powerup: yellow */
    SetPalette(SCR_1_PLANE, 3, 0,
        RGB(15,15,0), RGB(12,12,0), RGB(15,15,4));
    /* Enemy1/dive: red */
    SetPalette(SCR_1_PLANE, 4, 0,
        RGB(15,2,0), RGB(12,0,0), RGB(15,6,2));
    /* Wave enemy: purple */
    SetPalette(SCR_1_PLANE, 5, 0,
        RGB(12,0,15), RGB(8,0,10), RGB(15,4,15));
    /* Boss: orange */
    SetPalette(SCR_1_PLANE, 6, 0,
        RGB(15,8,0), RGB(12,4,0), RGB(15,12,0));

    /* HUD labels */
    PrintString(SCR_1_PLANE, 0, 6, 0, "LIF:");
    PrintString(SCR_1_PLANE, 0, 10, 0, "PWR:");
    PrintString(SCR_1_PLANE, 0, 15, 0, "LV:");

    init_stars();
    draw_stars();
    draw_hud();
    PutTile(SCR_1_PLANE, 1, px, py, T_SHIP_L);
    if (px + 1 < 20) PutTile(SCR_1_PLANE, 1, px + 1, py, T_SHIP_R);
}

static void update_game(void)
{
    u8 etype;

    if (game_over) return;

    /* Death respawn */
    if (!alive) {
        if (death_timer > 0) {
            death_timer--;
        } else if (!game_over) {
            alive = 1;
            px = 2;
            py = 10;
            PutTile(SCR_1_PLANE, 1, px, py, T_SHIP_L);
    if (px + 1 < 20) PutTile(SCR_1_PLANE, 1, px + 1, py, T_SHIP_R);
        }
        update_enemies();
        update_shots();
        scroll_tick++;
        if (scroll_tick >= 8) { scroll_tick = 0; scroll_stars(); }
        draw_hud();
        return;
    }

    /* Player movement — free 8-way */
    if (pad_cur & J_UP) {
        if (py > 2) {
            PutTile(SCR_1_PLANE, 0, px, py, ' ');
            if (px + 1 < 20) PutTile(SCR_1_PLANE, 0, px + 1, py, ' ');
            py = py - 1;
        }
    }
    if (pad_cur & J_DOWN) {
        if (py < 17) {
            PutTile(SCR_1_PLANE, 0, px, py, ' ');
            if (px + 1 < 20) PutTile(SCR_1_PLANE, 0, px + 1, py, ' ');
            py = py + 1;
        }
    }
    if (pad_cur & J_LEFT) {
        if (px > 0) {
            PutTile(SCR_1_PLANE, 0, px, py, ' ');
            if (px + 1 < 20) PutTile(SCR_1_PLANE, 0, px + 1, py, ' ');
            px = px - 1;
        }
    }
    if (pad_cur & J_RIGHT) {
        if (px < 8) {
            PutTile(SCR_1_PLANE, 0, px, py, ' ');
            if (px + 1 < 20) PutTile(SCR_1_PLANE, 0, px + 1, py, ' ');
            px = px + 1;
        }
    }

    /* Fire */
    fire_tick++;
    if ((pad_cur & J_A) && fire_tick >= 6) {
        fire_tick = 0;
        fire_shot();
    }

    /* Spawn enemies */
    spawn_tick++;
    if (spawn_tick >= spawn_rate) {
        spawn_tick = 0;
        wave_counter++;

        if (wave_counter >= 20 && !boss_spawned) {
            spawn_enemy(E_BOSS);
            boss_spawned = 1;
        } else {
            etype = cheap_rand(4);
            if (etype == 0) spawn_enemy(E_GRUNT);
            else if (etype == 1) spawn_enemy(E_WAVE);
            else if (etype == 2) spawn_enemy(E_DIVE);
            else if (etype == 3 && cheap_rand(4) == 0) spawn_enemy(E_POWER);
            else spawn_enemy(E_GRUNT);
        }

        /* Speed up over time */
        if (spawn_rate > 15 && (wave_counter & 7) == 0) {
            spawn_rate = spawn_rate - 2;
        }
    }

    /* Boss killed = next level */
    if (wave_counter >= 20 && !boss_spawned && level < 99) {
        level++;
        wave_counter = 0;
        spawn_rate = 40;
        if (spawn_rate > 20) spawn_rate = spawn_rate - level;
    }

    /* Scrolling stars */
    scroll_tick++;
    if (scroll_tick >= 8) {
        scroll_tick = 0;
        scroll_stars();
    }

    update_enemies();
    update_shots();
    check_player_hit();

    /* Draw player — 2 tiles wide */
    if (alive) {
        PutTile(SCR_1_PLANE, 1, px, py, T_SHIP_L);
        if (px + 1 < 20) PutTile(SCR_1_PLANE, 1, px + 1, py, T_SHIP_R);
    }

    draw_hud();
}

/* ---- Main ---- */
void main(void)
{
    InitNGPC();
    SysSetSystemFont();
    SetBackgroundColour(RGB(0, 0, 0));
    SetPalette(SCR_1_PLANE, 0, 0,
        RGB(15,15,15), RGB(15,15,15), RGB(15,15,15));
    SetPalette(SCR_1_PLANE, 1, 0,
        RGB(0,15,0), RGB(0,10,0), RGB(0,15,0));

    state = STATE_MENU;
    menu_sel = 0;
    skip = 10;
    show_menu();

    while (1) {
        WaitVsync();
        read_input();

        if (skip > 0) { skip--; continue; }

        if (state == STATE_MENU) {
            if (pad_press & J_DOWN) { menu_sel = menu_sel ? 0 : 1; show_menu(); }
            if (pad_press & J_UP) { menu_sel = menu_sel ? 0 : 1; show_menu(); }
            if (pad_press & J_A) {
                if (menu_sel == 0) { state = STATE_GAME; skip = 10; start_game(); }
                else { state = STATE_SCORES; show_scores(); }
            }
        } else if (state == STATE_GAME) {
            if (game_over) {
                if (pad_press & J_B) { start_game(); skip = 10; }
            } else {
                update_game();
            }
            if (pad_press & J_OPTION) {
                state = STATE_MENU; skip = 10; menu_sel = 0; show_menu();
            }
        } else if (state == STATE_SCORES) {
            if (pad_press & J_OPTION) {
                state = STATE_MENU; skip = 10; menu_sel = 0; show_menu();
            }
        }
    }
}
