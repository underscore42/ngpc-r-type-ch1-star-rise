/* main.c - R-Type Neo: Chapter 1 - Star Rise
 * NGPC homebrew side-scrolling shooter
 * 10 waves + boss fight, 3 selectable ships
 * Single-file C89 for cc900 compiler
 */

#define CARTHDR_IMPL
#include "carthdr.h"
#include "library.h"
#include "ch1_sprites.h"
#include "enemy_tiles.h"
#include "title_tiles.h"

/* ============================================================
 * Sound effects — T6W28 PSG
 * SOUNDEFFECT: {Channel, Length, Repeat, InitTone, ToneStep,
 *   ToneSpeed, ToneOWB, ToneLower, ToneUpper,
 *   InitVol, VolStep, VolSpeed, VolOWB, VolLower, VolUpper}
 * Channel: 0-1=tone, 2=noise
 * Tone: lower value = higher pitch (~0x40=high, ~0x3FF=low)
 * Volume: 0-15, 15=loudest
 * ToneOWB: bit0=oscillate, bit1=wrap
 * ============================================================ */

#define SFX_SHOOT     1
#define SFX_EXPLODE   2
#define SFX_POP       3
#define SFX_POWERUP   4
#define SFX_DEATH     5
#define SFX_BOSS_HIT  6
#define SFX_BGM_SPACE 7
#define SFX_BGM_BOSS  8
#define SFX_COUNT     8

static const SOUNDEFFECT game_sounds[SFX_COUNT] = {
    /* 0: SFX_SHOOT — short high laser blip */
    { 0, 4, 0, 0x0060, 0x0010, 1, 0, 0x0040, 0x00C0,
      10, 3, 1, 0, 0, 10 },

    /* 1: SFX_EXPLODE — big boss explosion: low noise rumble */
    { 2, 20, 0, 0x0004, 0x0000, 1, 0, 0x0001, 0x0007,
      15, 1, 1, 0, 0, 15 },

    /* 2: SFX_POP — small enemy pop: quick descending tone */
    { 0, 6, 0, 0x0080, 0x0018, 1, 0, 0x0060, 0x0180,
      12, 3, 1, 0, 0, 12 },

    /* 3: SFX_POWERUP — ascending arpeggio */
    { 0, 12, 0, 0x0180, 0x0020, 1, 0, 0x0060, 0x0200,
      13, 1, 2, 0, 0, 13 },

    /* 4: SFX_DEATH — player death: descending + noise */
    { 1, 30, 0, 0x0060, 0x0008, 2, 0, 0x0040, 0x03FF,
      15, 1, 2, 0, 0, 15 },

    /* 5: SFX_BOSS_HIT — boss damage: mid thud */
    { 1, 6, 0, 0x0100, 0x0020, 1, 0, 0x0080, 0x0200,
      14, 3, 1, 0, 0, 14 },

    /* 6: SFX_BGM_SPACE — looping space ambient: slow oscillating low tone */
    { 1, 255, 1, 0x0200, 0x0008, 4, 1, 0x0180, 0x0280,
      4, 0, 1, 0, 4, 4 },

    /* 7: SFX_BGM_BOSS — boss music: fast aggressive oscillating tone */
    { 1, 255, 1, 0x0100, 0x0010, 2, 1, 0x0080, 0x0180,
      7, 0, 1, 0, 7, 7 },
};





static const u16 small_tile_data[][8] = {
    /* thrust_f1 */  { 0x0000, 0x0100, 0x0640, 0x1B90, 0x6FE4, 0x1B90, 0x0640, 0x0100 },
    /* thrust_f2 */  { 0x0000, 0x0400, 0x1900, 0x6E40, 0xBF90, 0x6E40, 0x1900, 0x0400 },
    /* laser */      { 0x0000, 0x0000, 0x0000, 0x00A8, 0x00A8, 0x0000, 0x0000, 0x0000 },
    /* wave_cannon */{ 0x0000, 0x0000, 0x0540, 0x1A90, 0x1A90, 0x0540, 0x0000, 0x0000 },
    /* expl_f1 */    { 0x0000, 0x0000, 0x0140, 0x07D0, 0x07D0, 0x0140, 0x0000, 0x0000 },
    /* expl_f2 */    { 0x0000, 0x0540, 0x1B90, 0x1FD0, 0x1FD0, 0x0690, 0x0100, 0x0000 },
    /* expl_f3 */    { 0x0100, 0x1640, 0x1BE4, 0x6FF8, 0x2FF4, 0x1BE0, 0x0650, 0x0040 },
    /* expl_f4 */    { 0x0410, 0x1144, 0x4690, 0x1964, 0x1961, 0x0690, 0x1144, 0x0410 },
    /* powerup */    { 0x0550, 0x1AA4, 0x6EB9, 0x6BE9, 0x6BE9, 0x6EB9, 0x1AA4, 0x0550 },
    /* boss_wp */    { 0x0140, 0x06B0, 0x1BFC, 0x2FFC, 0x2FFC, 0x1BFC, 0x06B0, 0x0140 },
    /* boss_arm */   { 0x5555, 0xAAAA, 0xFFFF, 0xAAAA, 0x5555, 0xAAAA, 0xFFFF, 0xAAAA },
};




/* ============================================================
 * Constants
 * ============================================================ */
#define NGPC_RGB(r,g,b) ((u16)((r)|((g)<<4)|((b)<<8)))

/* Game states */
#define STATE_TITLE    0
#define STATE_SELECT   1
#define STATE_WAVE_IN  2
#define STATE_PLAY     3
#define STATE_WAVE_OUT 4
#define STATE_BOSS_IN  5
#define STATE_BOSS     6
#define STATE_DEAD     7
#define STATE_COMPLETE 8
#define STATE_GAMEOVER 9
#define STATE_PAUSED   10

/* Game config */
#define MAX_WAVES      10
#define MAX_BULLETS    4
#define MAX_ENEMIES    4
#define MAX_EXPLOSIONS 3
#define MAX_POWERUPS   1
#define PLAYER_SPEED   2
#define BULLET_SPEED   4
#define EXPL_FRAMES    4
#define EXPL_TICKS     6

/* Max hw sprites per ship frame (allow up to 20 tiles per frame) */
#define MAX_SHIP_TILES 12

/* Sprite HW slot allocation:
 * 0-7:   Player ship (up to 8 tiles for 32x16 frame)
 * 8:     Thrust flame
 * 9-12:  Bullets (4 max)
 * 13-16: Enemy 0 (4 tiles for 16x16 drone)
 * 17-20: Enemy 1 (4 tiles)
 * 21-24: Enemy 2 (4 tiles)
 * 25-27: Explosions
 * 28:    Powerup
 * 29-33: Boss sprites (weakpoint + arm tips)
 */
#define SPR_SHIP_BASE    0
#define SPR_THRUST       12
#define SPR_BULLET_BASE  13
#define SPR_ENEMY0_BASE  17
#define SPR_ENEMY1_BASE  21
#define SPR_ENEMY2_BASE  25
#define SPR_ENEMY3_BASE  29
#define SPR_EXPL_BASE    33
#define SPR_POWER_BASE   36
#define SPR_BOSS_BASE    37

/* Ship IDs */
#define SHIP_R9A  0
#define SHIP_R9D  1
#define SHIP_R9F  2

/* ============================================================
 * Structures
 * ============================================================ */
typedef struct {
    s16 x, y;
    u8 active;
} Bullet;

typedef struct {
    s16 x, y;
    s16 vy;
    u8 type;
    u8 hp;
    u8 active;
    u16 timer;
} Enemy;

typedef struct {
    s16 x, y;
    u8 frame;
    u8 timer;
    u8 active;
} Explosion;

/* ============================================================
 * Globals
 * ============================================================ */
u8 game_state;
u8 joy, joy_prev, joy_edge;
u16 frame_count;
u8 state_timer;

/* Player */
s16 player_x, player_y;
u8 player_lives;
u8 player_power;       /* weapon level 0-4 */
u8 player_speed;
u8 ship_type;       /* 0=R9A, 1=R9D, 2=R9F */
u8 ship_frame;      /* 0=level, 1=up, 2=down */
u8 thrust_anim;
u8 fire_cooldown;
u8 invuln_timer;

/* Score */
u32 score;
u32 hiscore;
u8 continues;
u8 konami_active;
u8 bw_mode;

/* Wave system */
u8 current_wave;     /* 1-10 */
u8 wave_spawned;     /* enemies spawned this wave */
u8 wave_killed;      /* enemies killed this wave */
u8 wave_total;       /* total enemies for this wave */
u8 spawn_timer;
u8 spawn_delay;      /* ticks between spawns (decreases with wave) */

/* Entities */
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
Explosion explosions[MAX_EXPLOSIONS];
u8 powerup_active;
s16 powerup_x, powerup_y;

/* Scrolling */
u16 scroll_x;
u16 scroll_speed;
u16 bg_scroll_x;

/* Boss */
u8 boss_active;
u8 boss_hp;
s16 boss_x, boss_y;
u8 boss_phase;
u16 boss_timer;
u8 boss_fire_cd;
u8 boss_proj_count;
u8 boss_hit_flash;
u8 boss_prev_tx, boss_prev_ty;

/* ============================================================
 * Ship frame table
 * Each ship has 3 frames (level/up/down).
 * We store pointers to the SprTile arrays and their counts.
 * ============================================================ */






/* Wave definitions: enemies_total, spawn_delay, enemy_speed */
static const u8 wave_enemies[MAX_WAVES]  = { 50, 60, 70, 80, 90, 100, 100, 120, 120, 150 };
static const u8 wave_delay[MAX_WAVES]    = { 50, 45, 40, 38, 35, 32, 30, 28, 25, 20 };

/* ============================================================
 * Forward declarations
 * ============================================================ */
void init_palettes(void);
void init_tiles(void);
void init_starfield(void);
void draw_hud(void);
void update_hud(void);
void title_screen(void);
void ship_select(void);
void start_game(void);
void start_wave(void);
void update_player(void);
void update_bullets(void);
void update_enemies(void);
void update_explosions(void);
void update_powerup(void);
void check_collisions(void);
void scroll_level(void);
void fire_bullet(void);
void draw_ship(void);
void kill_player(void);
void spawn_enemy(void);
void spawn_explosion(s16 x, s16 y);
void update_boss(void);
void draw_boss(void);
void draw_number(u8 x, u8 y, u32 val, u8 digits);
void show_color_title(void);
void draw_ship_frame(u8 ship, u8 frame, u8 base_spr, s16 x, s16 y);


/* ============================================================
 * B&W palette override
 * ============================================================ */
void init_bw_palettes(void)
{
    u16 c0, c1, c2, c3;
    c0 = NGPC_RGB(0,0,0);
    c1 = NGPC_RGB(5,5,5);
    c2 = NGPC_RGB(9,9,9);
    c3 = NGPC_RGB(14,14,14);
    SetPalette(SPRITE_PLANE, 0, c0, c1, c2, c3);
    SetPalette(SPRITE_PLANE, 1, c0, c1, c2, c3);
    SetPalette(SPRITE_PLANE, 2, c0, c1, c2, c3);
    SetPalette(SPRITE_PLANE, 3, c0, c1, c2, c3);
    SetPalette(SPRITE_PLANE, 4, c0, c1, c2, c3);
    SetPalette(SPRITE_PLANE, 5, c0, c1, c2, c3);
    SetPalette(SCR_1_PLANE, 0, c0, c1, c2, c3);
    SetPalette(SCR_1_PLANE, 1, c0, c1, c2, c3);
    SetPalette(SCR_2_PLANE, 0, NGPC_RGB(0,0,0), NGPC_RGB(4,4,4),
               NGPC_RGB(8,8,8), NGPC_RGB(12,12,12));
    SetBackgroundColour(NGPC_RGB(0,0,0));
    BG_COL = 0x80;
}

/* ============================================================
 * Palette setup
 * ============================================================ */
void init_palettes(void)
{
    SetPalette(SPRITE_PLANE, 0, PAL_HULL_BRIGHT_C0, PAL_HULL_BRIGHT_C1,
               PAL_HULL_BRIGHT_C2, PAL_HULL_BRIGHT_C3);
    SetPalette(SPRITE_PLANE, 1, PAL_HULL_DARK_C0, PAL_HULL_DARK_C1,
               PAL_HULL_DARK_C2, PAL_HULL_DARK_C3);
    SetPalette(SPRITE_PLANE, 2, PAL_WARM_C0, PAL_WARM_C1,
               PAL_WARM_C2, PAL_WARM_C3);
    SetPalette(SPRITE_PLANE, 3, PAL_HOT_C0, PAL_HOT_C1,
               PAL_HOT_C2, PAL_HOT_C3);
    SetPalette(SPRITE_PLANE, 4, PAL_PURPLE_C0, PAL_PURPLE_C1,
               PAL_PURPLE_C2, PAL_PURPLE_C3);
    SetPalette(SPRITE_PLANE, 5, PAL_GREEN_C0, PAL_GREEN_C1,
               PAL_GREEN_C2, PAL_GREEN_C3);
    SetPalette(SCR_1_PLANE, 0, NGPC_RGB(0,0,0), NGPC_RGB(3,3,4),
               NGPC_RGB(6,6,7), NGPC_RGB(10,10,11));
    SetPalette(SCR_1_PLANE, 1, NGPC_RGB(0,0,0), NGPC_RGB(8,8,10),
               NGPC_RGB(12,12,14), NGPC_RGB(15,15,15));
    SetPalette(SCR_2_PLANE, 0, NGPC_RGB(0,0,1), NGPC_RGB(7,7,10),
               NGPC_RGB(12,12,15), NGPC_RGB(15,15,15));
    SetBackgroundColour(NGPC_RGB(0,0,1));
    BG_COL = 0x80;
}

/* ============================================================
 * Tile installation
 * ============================================================ */
void init_tiles(void)
{
    InstallTileSetAt((const unsigned short (*)[8])all_tile_data, ALL_TILE_COUNT * 8, ENEMY_TILE_BASE);
    InstallTileSetAt((const unsigned short (*)[8])ch1_scroll_tiles, CH1_SCROLL_COUNT * 8, CH1_SCROLL_BASE);
}

/* ============================================================
 * Starfield
 * ============================================================ */
void init_starfield(void)
{
    u8 x, y;
    u16 rng;

    ClearScreen(SCR_2_PLANE);
    rng = 0xACE1;
    for (y = 0; y < 19; y++) {
        for (x = 0; x < 32; x++) {
            rng ^= rng << 7;
            rng ^= rng >> 9;
            rng ^= rng << 8;
            if ((rng & 0x0F) < 2) {
                PutTile(SCR_2_PLANE, 0, x, y,
                        STAR_TILE_1 + (rng >> 4) % 3);
            }
        }
    }
}


/* ============================================================
 * HUD
 * ============================================================ */
void draw_hud(void)
{
    PrintString(SCR_1_PLANE, 1, 0, 0, "SCORE");
    PrintString(SCR_1_PLANE, 1, 14, 0, "HI");
    PrintString(SCR_1_PLANE, 1, 0, 1, "LIVES");
    PrintString(SCR_1_PLANE, 1, 11, 1, "WAVE");
}

void draw_number(u8 x, u8 y, u32 val, u8 digits)
{
    u8 i;
    u8 buf[8];
    u32 v;
    v = val;
    for (i = 0; i < digits; i++) {
        buf[digits - 1 - i] = (u8)(v % 10);
        v = v / 10;
    }
    for (i = 0; i < digits; i++) {
        PrintString(SCR_1_PLANE, 1, x + i, y,
                    buf[i] == 0 ? "0" : buf[i] == 1 ? "1" :
                    buf[i] == 2 ? "2" : buf[i] == 3 ? "3" :
                    buf[i] == 4 ? "4" : buf[i] == 5 ? "5" :
                    buf[i] == 6 ? "6" : buf[i] == 7 ? "7" :
                    buf[i] == 8 ? "8" : "9");
    }
}

void update_hud(void)
{
    if (score > hiscore) hiscore = score;
    draw_number(6, 0, score, 7);
    draw_number(17, 0, hiscore, 6);
    draw_number(6, 1, (u32)player_lives, 2);
    draw_number(16, 1, (u32)current_wave, 2);
}

/* ============================================================
 * Title screen (uses title_tiles.h for graphics)
 * ============================================================ */
void show_color_title(void)
{
    u8 x, y;
    u16 i;
    InstallTileSetAt((const unsigned short (*)[8])title_tiles, TITLE_TILE_COUNT * 8, TITLE_TILE_BASE);
    SetPalette(SCR_1_PLANE, 0, TITLE_PAL0_C0, TITLE_PAL0_C1, TITLE_PAL0_C2, TITLE_PAL0_C3);
    SetPalette(SCR_1_PLANE, 1, TITLE_PAL1_C0, TITLE_PAL1_C1, TITLE_PAL1_C2, TITLE_PAL1_C3);
    SetPalette(SCR_1_PLANE, 2, TITLE_PAL2_C0, TITLE_PAL2_C1, TITLE_PAL2_C2, TITLE_PAL2_C3);
    SetPalette(SCR_1_PLANE, 3, TITLE_PAL3_C0, TITLE_PAL3_C1, TITLE_PAL3_C2, TITLE_PAL3_C3);
    for (y = 0; y < TITLE_ROWS; y++) {
        for (x = 0; x < TITLE_COLS; x++) {
            i = (u16)(y * TITLE_COLS + x);
            PutTile(SCR_1_PLANE, title_pal[i], x, y + 3, title_map[i]);
        }
    }
}

void title_screen(void)
{
    u8 blink;
    u8 i;

    ClearScreen(SCR_1_PLANE);
    ClearScreen(SCR_2_PLANE);
    for (i = 0; i < 64; i++) UnsetSprite(i);
    ShiftScroll(SCR_1_PLANE, 0, 0);
    ShiftScroll(SCR_2_PLANE, 0, 0);

    SetBackgroundColour(NGPC_RGB(1,1,1));
    BG_COL = 0x80;

    /* Load font FIRST, then title tiles overwrite higher IDs */
    SysSetSystemFont();
    show_color_title();

    /* Text palette for PRESS START (pal 15 so it doesn't conflict with title pals 0-3) */
    SetPalette(SCR_1_PLANE, 15, NGPC_RGB(0,0,0), NGPC_RGB(8,8,10),
               NGPC_RGB(12,12,14), NGPC_RGB(15,15,15));

    blink = 0;
    { u8 kstep;
      kstep = 0;
      konami_active = 0;
      while (1) {
        WaitVsync();
        blink++;
        if ((blink & 0x20) == 0) {
            PrintString(SCR_1_PLANE, 15, 4, 17, "PRESS START");
        } else {
            PrintString(SCR_1_PLANE, 15, 4, 17, "           ");
        }
        joy = JOYPAD & 0x7F;
        joy_edge = joy & ~joy_prev;
        joy_prev = joy;
        /* Konami: UP UP DOWN DOWN LEFT RIGHT LEFT RIGHT B A */
        if (joy_edge) {
            if ((kstep==0||kstep==1) && (joy_edge&J_UP)) kstep++;
            else if ((kstep==2||kstep==3) && (joy_edge&J_DOWN)) kstep++;
            else if (kstep==4 && (joy_edge&J_LEFT)) kstep++;
            else if (kstep==5 && (joy_edge&J_RIGHT)) kstep++;
            else if (kstep==6 && (joy_edge&J_LEFT)) kstep++;
            else if (kstep==7 && (joy_edge&J_RIGHT)) kstep++;
            else if (kstep==8 && (joy_edge&J_B)) kstep++;
            else if (kstep==9 && (joy_edge&J_A)) {
                konami_active = 1;
                PrintString(SCR_1_PLANE, 15, 2, 15, "CODE ACCEPTED!");
                { u8 kk; for(kk=0;kk<90;kk++) WaitVsync(); }
                break;
            }
            else if (joy_edge & J_A) break;
            else if (joy_edge & J_OPTION) {
                /* Options menu */
                ClearScreen(SCR_1_PLANE);
                SysSetSystemFont();
                SetPalette(SCR_1_PLANE, 15, NGPC_RGB(0,0,0), NGPC_RGB(8,8,10),
                           NGPC_RGB(12,12,14), NGPC_RGB(15,15,15));
                PrintString(SCR_1_PLANE, 15, 6, 3, "OPTIONS");
                if (bw_mode) PrintString(SCR_1_PLANE, 15, 2, 7, "A: COLOR MODE");
                else PrintString(SCR_1_PLANE, 15, 2, 7, "A: B+W MODE");
                PrintString(SCR_1_PLANE, 15, 2, 10, "B: BACK");
                while (JOYPAD & 0x7F) WaitVsync();
                while (1) {
                    WaitVsync();
                    joy = JOYPAD & 0x7F;
                    joy_edge = joy & ~joy_prev;
                    joy_prev = joy;
                    if (joy_edge & J_A) {
                        bw_mode = bw_mode ? 0 : 1;
                        if (bw_mode) PrintString(SCR_1_PLANE, 15, 2, 7, "A: COLOR MODE ");
                        else PrintString(SCR_1_PLANE, 15, 2, 7, "A: B+W MODE   ");
                    }
                    if (joy_edge & (J_B | J_OPTION)) break;
                }
                /* Redraw title screen */
                ClearScreen(SCR_1_PLANE);
                if (bw_mode) {
                    SetBackgroundColour(NGPC_RGB(0,0,0));
                } else {
                    SetBackgroundColour(NGPC_RGB(1,1,1));
                }
                BG_COL = 0x80;
                SysSetSystemFont();
                show_color_title();
                if (bw_mode) {
                    SetPalette(SCR_1_PLANE, 0, NGPC_RGB(0,0,0), NGPC_RGB(5,5,5), NGPC_RGB(9,9,9), NGPC_RGB(14,14,14));
                    SetPalette(SCR_1_PLANE, 1, NGPC_RGB(0,0,0), NGPC_RGB(5,5,5), NGPC_RGB(9,9,9), NGPC_RGB(14,14,14));
                    SetPalette(SCR_1_PLANE, 2, NGPC_RGB(0,0,0), NGPC_RGB(5,5,5), NGPC_RGB(9,9,9), NGPC_RGB(14,14,14));
                    SetPalette(SCR_1_PLANE, 3, NGPC_RGB(0,0,0), NGPC_RGB(5,5,5), NGPC_RGB(9,9,9), NGPC_RGB(14,14,14));
                }
                SetPalette(SCR_1_PLANE, 15, NGPC_RGB(0,0,0), NGPC_RGB(8,8,10),
                           NGPC_RGB(12,12,14), NGPC_RGB(15,15,15));
                kstep = 0;
            }
            else kstep = 0;
        }
      }
    }
}

/* ============================================================
 * Ship select screen
 * ============================================================ */
void ship_select(void)
{
    u8 sel;
    u8 prev_sel;
    u8 blink;
    u8 tx;
    u8 ty;

    ClearScreen(SCR_1_PLANE);
    ClearScreen(SCR_2_PLANE);
    { u8 i; for (i = 0; i < 64; i++) UnsetSprite(i); }
    ShiftScroll(SCR_1_PLANE, 0, 0);
    ShiftScroll(SCR_2_PLANE, 0, 0);

    SysSetSystemFont();
    if (bw_mode) init_bw_palettes(); else init_palettes();
    init_tiles();

    /* Ship palettes on scroll plane 1 */
    if (bw_mode) {
        u8 pi;
        for (pi = 0; pi < 6; pi++)
            SetPalette(SCR_1_PLANE, pi, NGPC_RGB(0,0,0), NGPC_RGB(5,5,5), NGPC_RGB(9,9,9), NGPC_RGB(14,14,14));
    } else {
        SetPalette(SCR_1_PLANE, 0, PAL_HULL_BRIGHT_C0, PAL_HULL_BRIGHT_C1, PAL_HULL_BRIGHT_C2, PAL_HULL_BRIGHT_C3);
        SetPalette(SCR_1_PLANE, 1, PAL_HULL_DARK_C0, PAL_HULL_DARK_C1, PAL_HULL_DARK_C2, PAL_HULL_DARK_C3);
        SetPalette(SCR_1_PLANE, 2, PAL_WARM_C0, PAL_WARM_C1, PAL_WARM_C2, PAL_WARM_C3);
        SetPalette(SCR_1_PLANE, 3, PAL_HOT_C0, PAL_HOT_C1, PAL_HOT_C2, PAL_HOT_C3);
        SetPalette(SCR_1_PLANE, 4, PAL_PURPLE_C0, PAL_PURPLE_C1, PAL_PURPLE_C2, PAL_PURPLE_C3);
        SetPalette(SCR_1_PLANE, 5, PAL_GREEN_C0, PAL_GREEN_C1, PAL_GREEN_C2, PAL_GREEN_C3);
    }
    SetPalette(SCR_1_PLANE, 15, NGPC_RGB(0,0,0), NGPC_RGB(8,8,10),
               NGPC_RGB(12,12,14), NGPC_RGB(15,15,15));

    sel = 0;
    prev_sel = 255;
    blink = 0;

    while (JOYPAD & (J_A | J_OPTION)) WaitVsync();

    while (1) {
        WaitVsync();
        blink++;

        joy = JOYPAD & 0x7F;
        joy_edge = joy & ~joy_prev;
        joy_prev = joy;

        if (joy_edge & (J_RIGHT | J_DOWN)) {
            sel++;
            if (sel > 2) sel = 0;
        }
        if (joy_edge & (J_LEFT | J_UP)) {
            if (sel == 0) sel = 2;
            else sel--;
        }

        /* Redraw only when selection changes */
        if (sel != prev_sel) {
            ClearScreen(SCR_1_PLANE);
            SysSetSystemFont();
            if (bw_mode) {
                u8 pi;
                for (pi = 0; pi < 6; pi++)
                    SetPalette(SCR_1_PLANE, pi, NGPC_RGB(0,0,0), NGPC_RGB(5,5,5), NGPC_RGB(9,9,9), NGPC_RGB(14,14,14));
            } else {
                SetPalette(SCR_1_PLANE, 0, PAL_HULL_BRIGHT_C0, PAL_HULL_BRIGHT_C1, PAL_HULL_BRIGHT_C2, PAL_HULL_BRIGHT_C3);
                SetPalette(SCR_1_PLANE, 1, PAL_HULL_DARK_C0, PAL_HULL_DARK_C1, PAL_HULL_DARK_C2, PAL_HULL_DARK_C3);
                SetPalette(SCR_1_PLANE, 2, PAL_WARM_C0, PAL_WARM_C1, PAL_WARM_C2, PAL_WARM_C3);
                SetPalette(SCR_1_PLANE, 3, PAL_HOT_C0, PAL_HOT_C1, PAL_HOT_C2, PAL_HOT_C3);
                SetPalette(SCR_1_PLANE, 4, PAL_PURPLE_C0, PAL_PURPLE_C1, PAL_PURPLE_C2, PAL_PURPLE_C3);
                SetPalette(SCR_1_PLANE, 5, PAL_GREEN_C0, PAL_GREEN_C1, PAL_GREEN_C2, PAL_GREEN_C3);
            }
            SetPalette(SCR_1_PLANE, 15, NGPC_RGB(0,0,0), NGPC_RGB(8,8,10),
                       NGPC_RGB(12,12,14), NGPC_RGB(15,15,15));

            PrintString(SCR_1_PLANE, 15, 3, 1, "SELECT SHIP");

            /* Draw ship centered: col 5, row 7 (R-9D row 8 since it's shorter) */
            if (sel == 0) {
                for (tx = 0; tx < SELECT_R9A_W; tx++)
                    for (ty = 0; ty < SELECT_R9A_H; ty++)
                        if (select_r9a_tiles[tx][ty])
                            PutTile(SCR_1_PLANE, select_r9a_pals[tx][ty],
                                    tx + 5, ty + 7, select_r9a_tiles[tx][ty]);
                PrintString(SCR_1_PLANE, 15, 2, 14, "R-9A ARROWHEAD");
            } else if (sel == 1) {
                for (tx = 0; tx < SELECT_R9D_W; tx++)
                    for (ty = 0; ty < SELECT_R9D_H; ty++)
                        if (select_r9d_tiles[tx][ty])
                            PutTile(SCR_1_PLANE, select_r9d_pals[tx][ty],
                                    tx + 5, ty + 8, select_r9d_tiles[tx][ty]);
                PrintString(SCR_1_PLANE, 15, 0, 14, "R-9D SHOOTING STAR");
            } else {
                for (tx = 0; tx < SELECT_R9F_W; tx++)
                    for (ty = 0; ty < SELECT_R9F_H; ty++)
                        if (select_r9f_tiles[tx][ty])
                            PutTile(SCR_1_PLANE, select_r9f_pals[tx][ty],
                                    tx + 5, ty + 7, select_r9f_tiles[tx][ty]);
                PrintString(SCR_1_PLANE, 15, 3, 14, "R-9F JUDGEMENT");
            }
            prev_sel = sel;
        }

        /* Blink prompt */
        if ((blink & 0x20) == 0) {
            PrintString(SCR_1_PLANE, 15, 4, 17, "PRESS START");
        } else {
            PrintString(SCR_1_PLANE, 15, 4, 17, "           ");
        }

        if (joy_edge & (J_A | J_OPTION)) {
            ship_type = sel;
            break;
        }
        if (joy_edge & J_B) {
            ship_type = 255; /* signal to go back to title */
            break;
        }
    }
}



/* ============================================================
 * Game init
 * ============================================================ */
void start_game(void)
{
    u8 i;

    ClearScreen(SCR_1_PLANE);
    ClearScreen(SCR_2_PLANE);
    for (i = 0; i < 64; i++) UnsetSprite(i);

    SysSetSystemFont();
    if (bw_mode) init_bw_palettes(); else init_palettes();
    init_tiles();
    init_starfield();

    player_x = 20;
    player_y = 68;
    player_lives = 3;
    player_power = 0;
    player_speed = PLAYER_SPEED;
    ship_frame = 0;
    thrust_anim = 0;
    fire_cooldown = 0;
    invuln_timer = 0;
    score = 0;

    for (i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    for (i = 0; i < MAX_ENEMIES; i++) enemies[i].active = 0;
    for (i = 0; i < MAX_EXPLOSIONS; i++) explosions[i].active = 0;
    powerup_active = 0;

    scroll_x = 0;
    scroll_speed = 1;
    bg_scroll_x = 0;

    boss_active = 0;
    current_wave = 1;

    draw_hud();
    frame_count = 0;

    /* Init sound */
    InstallSoundDriver();
    WaitVsync();
    WaitVsync();
    InstallSounds(game_sounds, SFX_COUNT);
    PlaySound(SFX_BGM_SPACE);
}

/* ============================================================
 * Wave management
 * ============================================================ */
void start_wave(void)
{
    wave_spawned = 0;
    wave_killed = 0;
    wave_total = wave_enemies[current_wave - 1];
    spawn_timer = 0;
    spawn_delay = wave_delay[current_wave - 1];
}

void spawn_enemy(void)
{
    u8 i;
    u8 dtype;
    u8 is_last;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies[i].active) {
            enemies[i].x = SCRN_W + 8 + (u8)(wave_spawned % 5) * 12;
            enemies[i].y = 30 + (u8)((wave_spawned * 23 + frame_count * 3) % 90);
            enemies[i].vy = 0;
            enemies[i].timer = 0;

            is_last = (wave_spawned + 1 >= wave_total) ? 1 : 0;

            /* Large unit mini-boss at end of waves 2,4,6,8 */
            if (is_last && (current_wave == 2 || current_wave == 4 ||
                            current_wave == 6 || current_wave == 8)) {
                dtype = 10 + (current_wave / 4);
                if (dtype > 11) dtype = 11;
                enemies[i].hp = current_wave * 2;
                enemies[i].y = 50;
            } else {
                /* Regular enemies: drones early, small units later */
                if (current_wave <= 2) dtype = wave_spawned % 4;
                else if (current_wave <= 4) dtype = (wave_spawned % 2 == 0) ? (wave_spawned % 4) : (4 + wave_spawned % 2);
                else if (current_wave <= 6) dtype = (wave_spawned % 3 == 0) ? (wave_spawned % 4) : (4 + wave_spawned % 4);
                else if (current_wave <= 8) dtype = (wave_spawned % 4 == 0) ? (wave_spawned % 4) : (4 + wave_spawned % 6);
                else dtype = (wave_spawned % 5 == 0) ? (wave_spawned % 4) : (4 + wave_spawned % 6);
                enemies[i].hp = 1 + (current_wave / 4);
            }

            enemies[i].type = dtype;
            enemies[i].active = 1;
            wave_spawned++;
            break;
        }
    }
}

/* ============================================================
 * Player
 * ============================================================ */
void update_player(void)
{
    u8 moved;
    moved = 0;

    if (joy & J_UP)    { if (player_y > 16) player_y -= player_speed; moved = 1; ship_frame = 1; }
    if (joy & J_DOWN)  { if (player_y < 120) player_y += player_speed; moved = 2; ship_frame = 2; }
    if (!moved) ship_frame = 0;
    if (joy & J_LEFT)  { if (player_x > 4) player_x -= player_speed; }
    if (joy & J_RIGHT) { if (player_x < 120) player_x += player_speed; }

    if (fire_cooldown > 0) fire_cooldown--;
    if ((joy & J_A) && fire_cooldown == 0) { fire_bullet(); fire_cooldown = 8; PlaySound(SFX_SHOOT); }

    thrust_anim++;
    if (invuln_timer > 0) invuln_timer--;
}

void fire_bullet(void)
{
    u8 i;
    u8 count;
    u8 max_shots;

    /* Weapon levels: 0=single, 1=double, 2=triple, 3+=wave cannon */
    if (player_power <= 0) max_shots = 1;
    else if (player_power == 1) max_shots = 2;
    else max_shots = 3;

    count = 0;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active && count < max_shots) {
            bullets[i].x = player_x + 36;
            bullets[i].y = player_y + 10;
            if (count == 1) bullets[i].y = player_y + 4;
            if (count == 2) bullets[i].y = player_y + 16;
            bullets[i].active = 1;
            count++;
        }
    }
}

void draw_ship_frame(u8 ship, u8 frame, u8 base_spr, s16 x, s16 y)
{
    u8 si;
    u8 cnt;
    /* Level frame only — visual tilt not needed at this scale */
    if (ship == 0) {
        cnt = SHIP_R9A_LEVEL_COUNT;
        for (si = 0; si < cnt; si++)
            SetSprite(base_spr+si, ship_r9a_level_tid[si], 0,
                (u8)(x+ship_r9a_level_xoff[si]), (u8)(y+ship_r9a_level_yoff[si]),
                ship_r9a_level_pal[si]);
    } else if (ship == 1) {
        cnt = SHIP_R9D_LEVEL_COUNT;
        for (si = 0; si < cnt; si++)
            SetSprite(base_spr+si, ship_r9d_level_tid[si], 0,
                (u8)(x+ship_r9d_level_xoff[si]), (u8)(y+ship_r9d_level_yoff[si]),
                ship_r9d_level_pal[si]);
    } else {
        cnt = SHIP_R9F_LEVEL_COUNT;
        for (si = 0; si < cnt; si++)
            SetSprite(base_spr+si, ship_r9f_level_tid[si], 0,
                (u8)(x+ship_r9f_level_xoff[si]), (u8)(y+ship_r9f_level_yoff[si]),
                ship_r9f_level_pal[si]);
    }
    for (si = cnt; si < MAX_SHIP_TILES; si++) UnsetSprite(base_spr + si);
}

void draw_ship(void)
{
        u8 visible;

    visible = 1;
    if (invuln_timer > 0 && (frame_count & 0x04)) visible = 0;

    if (visible) {
        draw_ship_frame(ship_type, ship_frame, SPR_SHIP_BASE, player_x, player_y);
        /* Thrust */
        {
            u8 thrust_pal;
            if (ship_type == 0) thrust_pal = 2;
            else if (ship_type == 1) thrust_pal = 4;
            else thrust_pal = 5;
            if ((thrust_anim & 0x04) != 0) {
                SetSprite(SPR_THRUST, TILE_THRUST_F1, 0,
                          (u8)(player_x - 6), (u8)(player_y + 8),
                          thrust_pal);
            } else {
                SetSprite(SPR_THRUST, TILE_THRUST_F2, 0,
                          (u8)(player_x - 8), (u8)(player_y + 8),
                          thrust_pal);
            }
        }
    } else {
        { u8 _h; for(_h=0;_h<MAX_SHIP_TILES;_h++) UnsetSprite(SPR_SHIP_BASE+_h); };
        UnsetSprite(SPR_THRUST);
    }
}

/* ============================================================
 * Bullets
 * ============================================================ */
void update_bullets(void)
{
    u8 i;
    for (i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].x += BULLET_SPEED;
            if (bullets[i].x > SCRN_W) {
                bullets[i].active = 0;
                UnsetSprite(SPR_BULLET_BASE + i);
            } else {
                {
                    u16 btile;
                    u8 bpal;
                    if (player_power >= 3) { btile = TILE_WAVE_CANNON; bpal = 2; }
                    else { btile = TILE_LASER; bpal = 0; }
                    SetSprite(SPR_BULLET_BASE + i, btile, 0,
                              (u8)bullets[i].x, (u8)bullets[i].y, bpal);
                }
            }
        }
    }
}

/* ============================================================
 * Enemies
 * ============================================================ */
void update_enemies(void)
{
    u8 i;
    u8 base_spr;

    for (i = 0; i < MAX_ENEMIES; i++) {
        if (i == 0) base_spr = SPR_ENEMY0_BASE;
        else if (i == 1) base_spr = SPR_ENEMY1_BASE;
        else if (i == 2) base_spr = SPR_ENEMY2_BASE;
        else base_spr = SPR_ENEMY3_BASE;

        if (!enemies[i].active) {
            { u8 _h; for(_h=0;_h<4;_h++) UnsetSprite(base_spr+_h); };
            continue;
        }

        enemies[i].timer++;

        /* Apply vertical velocity for boss projectiles */
        enemies[i].y += enemies[i].vy;

        /* Small units (types 4-9) fire toward player */
        if (enemies[i].type >= 4 && enemies[i].type <= 9 &&
            enemies[i].timer > 40 && (enemies[i].timer % 50) == 0) {
            u8 bi;
            for (bi = 0; bi < MAX_ENEMIES; bi++) {
                if (!enemies[bi].active) {
                    enemies[bi].x = enemies[i].x - 4;
                    enemies[bi].y = enemies[i].y + 4;
                    enemies[bi].type = 0;
                    enemies[bi].hp = 1;
                    enemies[bi].active = 1;
                    enemies[bi].timer = 0;
                    enemies[bi].vy = 0;
                    break;
                }
            }
        }

        if (enemies[i].type >= 10) {
            /* Large unit: hover at right side, bob up/down */
            if (enemies[i].x > 120) enemies[i].x -= 1;
            if ((enemies[i].timer & 0x3F) < 32) {
                enemies[i].y += 1;
            } else {
                enemies[i].y -= 1;
            }
        } else {
            /* Regular enemy: fly left */
            enemies[i].x -= 2;
            if ((enemies[i].timer & 0x1F) < 16) {
                enemies[i].y += 1;
            } else {
                enemies[i].y -= 1;
            }
        }

        if (enemies[i].y < 16) enemies[i].y = 16;
        if (enemies[i].y > 130) enemies[i].y = 130;

        if (enemies[i].x < -32) {
            enemies[i].active = 0;
            { u8 _h; for(_h=0;_h<4;_h++) UnsetSprite(base_spr+_h); };
            continue;
        }

        /* Draw enemy using extracted tiles */
        {
            u16 et0, et1, et2, et3;
            u8 ep;
            u8 etype;
            etype = enemies[i].type;
            ep = 2; /* warm palette for all enemies */
            if (etype == 0) { et0=drone_0_tid[0]; et1=drone_0_tid[1]; et2=drone_0_tid[2]; et3=drone_0_tid[3]; }
            else if (etype == 1) { et0=drone_1_tid[0]; et1=drone_1_tid[1]; et2=drone_1_tid[2]; et3=drone_1_tid[3]; }
            else if (etype == 2) { et0=drone_2_tid[0]; et1=drone_2_tid[1]; et2=drone_2_tid[2]; et3=drone_2_tid[3]; }
            else if (etype == 3) { et0=drone_3_tid[0]; et1=drone_3_tid[1]; et2=drone_3_tid[2]; et3=drone_3_tid[3]; }
            else if (etype == 4) { et0=small_0_tid[0]; et1=small_0_tid[1]; et2=small_0_tid[2]; et3=small_0_tid[3]; }
            else if (etype == 5) { et0=small_1_tid[0]; et1=small_1_tid[1]; et2=small_1_tid[2]; et3=small_1_tid[3]; }
            else if (etype == 6) { et0=small_2_tid[0]; et1=small_2_tid[1]; et2=small_2_tid[2]; et3=small_2_tid[3]; }
            else if (etype == 7) { et0=small_3_tid[0]; et1=small_3_tid[1]; et2=small_3_tid[2]; et3=small_3_tid[3]; }
            else if (etype == 8) { et0=small_4_tid[0]; et1=small_4_tid[1]; et2=small_4_tid[2]; et3=small_4_tid[3]; }
            else if (etype == 9) { et0=small_5_tid[0]; et1=small_5_tid[1]; et2=small_5_tid[2]; et3=small_5_tid[3]; }
            else if (etype == 10) { et0=large_0_tid[0]; et1=large_0_tid[1]; et2=large_0_tid[2]; et3=large_0_tid[3]; }
            else { et0=large_1_tid[0]; et1=large_1_tid[1]; et2=large_1_tid[2]; et3=large_1_tid[3]; }
            if (et0) SetSprite(base_spr,   et0, 0, (u8)enemies[i].x,     (u8)enemies[i].y,     ep);
            else UnsetSprite(base_spr);
            if (et1) SetSprite(base_spr+1, et1, 0, (u8)(enemies[i].x+8), (u8)enemies[i].y,     ep);
            else UnsetSprite(base_spr+1);
            if (et2) SetSprite(base_spr+2, et2, 0, (u8)enemies[i].x,     (u8)(enemies[i].y+8), ep);
            else UnsetSprite(base_spr+2);
            if (et3) SetSprite(base_spr+3, et3, 0, (u8)(enemies[i].x+8), (u8)(enemies[i].y+8), ep);
            else UnsetSprite(base_spr+3);
        }
    }
}

/* ============================================================
 * Explosions
 * ============================================================ */
void spawn_explosion(s16 x, s16 y)
{
    u8 i;
    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            explosions[i].x = x;
            explosions[i].y = y;
            explosions[i].frame = 0;
            explosions[i].timer = 0;
            explosions[i].active = 1;
            break;
        }
    }
}

void update_explosions(void)
{
    u8 i;

    for (i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].active) {
            UnsetSprite(SPR_EXPL_BASE + i);
            continue;
        }
        explosions[i].timer++;
        if (explosions[i].timer >= EXPL_TICKS) {
            explosions[i].timer = 0;
            explosions[i].frame++;
            if (explosions[i].frame >= EXPL_FRAMES) {
                explosions[i].active = 0;
                UnsetSprite(SPR_EXPL_BASE + i);
                continue;
            }
        }
        /* Pick explosion frame tile */
        {
            u16 etile;
            if (explosions[i].frame == 0) etile = TILE_EXPL_F1;
            else if (explosions[i].frame == 1) etile = TILE_EXPL_F2;
            else if (explosions[i].frame == 2) etile = TILE_EXPL_F3;
            else etile = TILE_EXPL_F4;
            SetSprite(SPR_EXPL_BASE + i, etile, 0,
                      (u8)explosions[i].x, (u8)explosions[i].y, 3);
        }
    }
}

/* ============================================================
 * Powerup
 * ============================================================ */
void update_powerup(void)
{
    if (!powerup_active) {
        UnsetSprite(SPR_POWER_BASE);
        return;
    }
    powerup_x -= 1;
    if (powerup_x < -8) {
        powerup_active = 0;
        UnsetSprite(SPR_POWER_BASE);
        return;
    }
    SetSprite(SPR_POWER_BASE, TILE_POWERUP, 0,
              (u8)powerup_x, (u8)powerup_y, 5);
}

/* ============================================================
 * Collisions
 * ============================================================ */
void check_collisions(void)
{
    u8 i, j;
    s16 dx, dy;

    /* Bullet vs enemy */
    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        for (j = 0; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;
            dx = bullets[i].x - enemies[j].x - 8;
            dy = bullets[i].y - enemies[j].y - 8;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx < 16 && dy < 16) {
                bullets[i].active = 0;
                UnsetSprite(SPR_BULLET_BASE + i);
                enemies[j].hp--;
                if (enemies[j].hp == 0) {
                    spawn_explosion(enemies[j].x + 8, enemies[j].y + 8);
                    enemies[j].active = 0;
                    { u8 bs;
                      if (j == 0) bs = SPR_ENEMY0_BASE;
                      else if (j == 1) bs = SPR_ENEMY1_BASE;
                      else if (j == 2) bs = SPR_ENEMY2_BASE;
                      else bs = SPR_ENEMY3_BASE;
                      { u8 _h; for(_h=0;_h<4;_h++) UnsetSprite(bs+_h); };
                    }
                    wave_killed++;
                    PlaySound(SFX_POP);
                    score += 100 + (u32)current_wave * 10;
                    /* Drones (types 0-3) drop powerups every 3rd drone kill */
                    if (enemies[j].type <= 3 && (wave_killed % 3) == 0 && !powerup_active) {
                        powerup_active = 1;
                        powerup_x = enemies[j].x;
                        powerup_y = enemies[j].y;
                    }
                }
                break;
            }
        }
    }

    /* Player vs enemy */
    if (invuln_timer == 0) {
        for (j = 0; j < MAX_ENEMIES; j++) {
            if (!enemies[j].active) continue;
            dx = player_x + 20 - enemies[j].x - 8;
            dy = player_y + 12 - enemies[j].y - 8;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx < 20 && dy < 16) {
                kill_player();
                break;
            }
        }
    }

    /* Player vs powerup */
    if (powerup_active && invuln_timer == 0) {
        dx = player_x + 20 - powerup_x;
        dy = player_y + 12 - powerup_y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx < 16 && dy < 12) {
            if (player_power < 2) player_power++;
            else if (player_speed < 4) player_speed++;
            PlaySound(SFX_POWERUP);
            score += 50;
            powerup_active = 0;
            UnsetSprite(SPR_POWER_BASE);
        }
    }
}

/* ============================================================
 * Player death
 * ============================================================ */
void kill_player(void)
{
    PlaySound(SFX_DEATH);
    spawn_explosion(player_x + 20, player_y + 12);
    player_lives--;
    player_power = 0;
    player_speed = PLAYER_SPEED;
    invuln_timer = 90;

    if (player_lives == 0) {
        game_state = STATE_GAMEOVER;
    } else {
        player_x = 20;
        player_y = 68;
    }
}

/* ============================================================
 * Scrolling
 * ============================================================ */
void scroll_level(void)
{
    scroll_x += scroll_speed;
    bg_scroll_x += 1;
    ShiftScroll(SCR_1_PLANE, (u8)scroll_x, 0);
    ShiftScroll(SCR_2_PLANE, (u8)(bg_scroll_x >> 1), 0);
}

/* ============================================================
 * Boss
 * ============================================================ */
void update_boss(void)
{
    u8 i;
    s16 dx, dy;

    if (!boss_active) return;
    boss_timer++;

    /* Clear WARNING text after 90 frames */
    if (boss_timer == 90) {
        PrintString(SCR_1_PLANE, 1, 5, 9, "        ");
    }

    /* Slide boss in from right */
    if (boss_x > SCRN_W - 56) {
        boss_x -= 1;
        draw_boss();
        return;
    }

    /* Vertical bobbing — faster in later phases */
    {
        u8 bob_speed;
        bob_speed = (boss_phase == 0) ? 0x3F : (boss_phase == 1) ? 0x1F : 0x0F;
        if ((boss_timer & bob_speed) < (bob_speed / 2)) {
            boss_y += 1;
            if (boss_y > 90) boss_y = 90;
        } else {
            boss_y -= 1;
            if (boss_y < 16) boss_y = 16;
        }
    }

    /* Horizontal weaving in phase 2+ */
    if (boss_phase >= 1) {
        if ((boss_timer & 0x3F) < 0x20) {
            boss_x -= 1;
            if (boss_x < SCRN_W - 72) boss_x = SCRN_W - 72;
        } else {
            boss_x += 1;
            if (boss_x > SCRN_W - 48) boss_x = SCRN_W - 48;
        }
    }

    if (boss_hit_flash > 0) boss_hit_flash--;

    /* Boss fires projectiles (uses enemy slots) */
    boss_fire_cd--;
    if (boss_fire_cd == 0) {
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].x = boss_x;
                enemies[i].y = boss_y + 12;
                enemies[i].type = 0;
                enemies[i].hp = 1;
                enemies[i].active = 1;
                enemies[i].timer = 0;
                enemies[i].vy = 0;
                break;
            }
        }
        /* Fire rate increases per phase */
        if (boss_phase == 0) boss_fire_cd = 45;
        else if (boss_phase == 1) boss_fire_cd = 30;
        else boss_fire_cd = 20;
    }

    /* Phase 2: spread fire pattern */
    if (boss_phase >= 1 && (boss_timer & 0x3F) == 0) {
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].x = boss_x + 20;
                enemies[i].y = boss_y + 4;
                enemies[i].type = 0;
                enemies[i].hp = 1;
                enemies[i].active = 1;
                enemies[i].timer = 0;
                enemies[i].vy = -1;
                break;
            }
        }
        for (i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].x = boss_x + 20;
                enemies[i].y = boss_y + 24;
                enemies[i].type = 0;
                enemies[i].hp = 1;
                enemies[i].active = 1;
                enemies[i].timer = 0;
                enemies[i].vy = 1;
                break;
            }
        }
    }

    /* Bullet vs boss collision */
    for (i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        dx = bullets[i].x - (boss_x + 20);
        dy = bullets[i].y - (boss_y + 12);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx < 24 && dy < 20) {
            bullets[i].active = 0;
            UnsetSprite(SPR_BULLET_BASE + i);
            boss_hp--;
            boss_hit_flash = 4;
            score += 10;
            PlaySound(SFX_BOSS_HIT);
            if (boss_hp == 0) {
                if (boss_phase < 2) {
                    /* Advance to next phase */
                    boss_phase++;
                    if (boss_phase == 1) boss_hp = 15;
                    else boss_hp = 10; /* Phase 1: 10 HP */
                    boss_fire_cd = 30;
                    PlaySound(SFX_EXPLODE);
                    /* Phase transition flash */
                    spawn_explosion(boss_x + 20, boss_y + 12);
                    spawn_explosion(boss_x, boss_y);
                    spawn_explosion(boss_x + 40, boss_y + 24);
                } else {
                    PlaySound(SFX_EXPLODE);
                    /* Boss defeated! */
                    spawn_explosion(boss_x + 10, boss_y + 8);
                    spawn_explosion(boss_x + 30, boss_y + 16);
                    spawn_explosion(boss_x + 20, boss_y + 24);
                    boss_active = 0;
                    score += 10000;
                    { u8 s; for (s = 0; s < 5; s++) UnsetSprite(SPR_BOSS_BASE + s); }
                    /* Clear boss body from scroll plane */
                    { u8 cx; u8 cy;
                      for (cy = 0; cy < 19; cy++)
                          for (cx = 8; cx < 32; cx++)
                              PutTile(SCR_1_PLANE, 0, cx, cy, WALL_OPEN);
                    }
                    game_state = STATE_COMPLETE;
                    return;
                }
            }
        }
    }

    /* Player vs boss collision */
    if (invuln_timer == 0) {
        dx = player_x + 20 - (boss_x + 24);
        dy = player_y + 12 - (boss_y + 16);
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx < 28 && dy < 24) kill_player();
    }

    draw_boss();
}

void draw_boss(void)
{
    u8 tx;
    u8 ty;
    u8 pal;
    if (!boss_active) return;
    pal = (boss_hit_flash > 0) ? 3 : 0;

    /* Erase previous body position if it moved (tile grid) */
    {
        u8 cur_tx, cur_ty;
        cur_tx = (u8)(boss_x / 8);
        cur_ty = (u8)(boss_y / 8);
        if (boss_prev_tx != 0xFF &&
            (boss_prev_tx != cur_tx || boss_prev_ty != cur_ty)) {
            for (tx = 0; tx < BOSS_W; tx++) {
                for (ty = 0; ty < BOSS_H; ty++) {
                    u8 ex, ey;
                    ex = (u8)((boss_prev_tx + tx) & 0x1F);
                    ey = (u8)(boss_prev_ty + ty);
                    if (ey < 19)
                        PutTile(SCR_1_PLANE, 0, ex, ey, WALL_OPEN);
                }
            }
        }
        boss_prev_tx = cur_tx;
        boss_prev_ty = cur_ty;
    }

    /* Draw boss body on scroll plane 1 (48x32 = 6x4 tiles) */
    for (tx = 0; tx < BOSS_W; tx++) {
        for (ty = 0; ty < BOSS_H; ty++) {
            if (boss_body_tiles[tx][ty]) {
                u8 bx_tile;
                u8 by_tile;
                bx_tile = (u8)((boss_x / 8 + tx) & 0x1F);
                by_tile = (u8)(boss_y / 8 + ty);
                if (by_tile < 19)
                    PutTile(SCR_1_PLANE, boss_body_pals[tx][ty], bx_tile, by_tile,
                            boss_body_tiles[tx][ty]);
            }
        }
    }

    /* Sprite weak points based on phase */
    if (boss_phase == 0) {
        /* Phase 1: 4 arm tips — destroyable */
        SetSprite(SPR_BOSS_BASE, TILE_BOSS_WP, 0,
                  (u8)(boss_x - 8), (u8)(boss_y + 12), 3);
        SetSprite(SPR_BOSS_BASE+1, TILE_BOSS_WP, 0,
                  (u8)(boss_x + 48), (u8)(boss_y + 12), 3);
        SetSprite(SPR_BOSS_BASE+2, TILE_BOSS_ARM, 0,
                  (u8)(boss_x - 4), (u8)(boss_y + 4), 2);
        SetSprite(SPR_BOSS_BASE+3, TILE_BOSS_ARM, 0,
                  (u8)(boss_x + 44), (u8)(boss_y + 20), 2);
    } else if (boss_phase == 1) {
        /* Phase 2: Core exposed, pulses */
        SetSprite(SPR_BOSS_BASE, TILE_BOSS_WP, 0,
                  (u8)(boss_x + 20), (u8)(boss_y + 12),
                  (boss_timer & 0x08) ? 3 : 2);
        UnsetSprite(SPR_BOSS_BASE+1);
        UnsetSprite(SPR_BOSS_BASE+2);
        UnsetSprite(SPR_BOSS_BASE+3);
    } else {
        /* Phase 3: Core + regenerating arms */
        SetSprite(SPR_BOSS_BASE, TILE_BOSS_WP, 0,
                  (u8)(boss_x + 20), (u8)(boss_y + 12),
                  (boss_timer & 0x04) ? 3 : 2);
        SetSprite(SPR_BOSS_BASE+1, TILE_BOSS_ARM, 0,
                  (u8)(boss_x - 4 + (boss_timer & 0x07)),
                  (u8)(boss_y + 8), 2);
        SetSprite(SPR_BOSS_BASE+2, TILE_BOSS_ARM, 0,
                  (u8)(boss_x + 44 - (boss_timer & 0x07)),
                  (u8)(boss_y + 16), 2);
        UnsetSprite(SPR_BOSS_BASE+3);
    }
}

/* ============================================================
 * Main loop
 * ============================================================ */
void main(void)
{
    InitNGPC();
    hiscore = 0;
    bw_mode = 0;
    continues = 2;

    while (1) {
        title_screen();
        ship_select();
        if (ship_type == 255) continue; /* B pressed, back to title */
        start_game();
        start_wave();

        game_state = STATE_WAVE_IN;
        state_timer = 0;
        if (konami_active) {
            player_power = 3;
            current_wave = 10;
            game_state = STATE_BOSS_IN;
            state_timer = 0;
        }

        for (;;) { /* session loop: gameplay + continue */

        while (game_state != STATE_GAMEOVER && game_state != STATE_COMPLETE) {
            WaitVsync();
            frame_count++;

            joy = JOYPAD & 0x7F;
            joy_edge = joy & ~joy_prev;
            joy_prev = joy;

            if (bw_mode) init_bw_palettes(); else init_palettes();

            switch (game_state) {
            case STATE_WAVE_IN:
                /* Show wave number for ~90 frames */
                state_timer++;
                if (state_timer == 1) {
                    if (current_wave <= MAX_WAVES) {
                        PrintString(SCR_1_PLANE, 1, 5, 9, "SECTOR");
                        draw_number(12, 9, (u32)current_wave, 2);
                    }
                }
                if (state_timer > 90) {
                    PrintString(SCR_1_PLANE, 1, 5, 9, "          ");
                    game_state = STATE_PLAY;
                }
                update_player();
                draw_ship();
                update_bullets();
                scroll_level();
                update_hud();
                break;

            case STATE_PLAY:
                /* Spawn enemies for this wave */
                spawn_timer++;
                if (wave_spawned < wave_total && spawn_timer >= spawn_delay) {
                    u8 burst;
                    u8 bsize;
                    spawn_timer = 0;
                    bsize = 3 + (current_wave / 3);
                    if (bsize > 5) bsize = 5;
                    for (burst = 0; burst < bsize && wave_spawned < wave_total; burst++)
                        spawn_enemy();
                }
                /* Also count off-screen enemies as "dealt with" */
                {
                    u8 active_count;
                    active_count = 0;
                    { u8 ei; for (ei = 0; ei < MAX_ENEMIES; ei++) {
                        if (enemies[ei].active) active_count++;
                    }}
                    /* If all spawned and none active, wave is done */
                    if (wave_spawned >= wave_total && active_count == 0) {
                        wave_killed = wave_total;
                    }
                }

                update_player();
                update_bullets();
                update_enemies();
                update_explosions();
                update_powerup();
                check_collisions();
                scroll_level();
                draw_ship();
                update_hud();

                /* Check wave complete: all spawned and none active */
                if (wave_spawned >= wave_total) {
                    u8 any_active;
                    any_active = 0;
                    { u8 ei; for (ei = 0; ei < MAX_ENEMIES; ei++) {
                        if (enemies[ei].active) any_active = 1;
                    }}
                    if (!any_active) wave_killed = wave_total;
                }
                if (wave_killed >= wave_total) {
                    game_state = STATE_WAVE_OUT;
                    state_timer = 0;
                }

                /* Pause */
                if (joy_edge & J_OPTION) {
                    PrintString(SCR_1_PLANE, 1, 6, 9, "PAUSED");
                    while (1) {
                        WaitVsync();
                        joy = JOYPAD & 0x7F;
                        joy_edge = joy & ~joy_prev;
                        joy_prev = joy;
                        if (joy_edge & J_OPTION) {
                            PrintString(SCR_1_PLANE, 1, 6, 9, "      ");
                            break;
                        }
                    }
                }
                break;

            case STATE_WAVE_OUT:
                state_timer++;
                if (state_timer == 1) {
                    PrintString(SCR_1_PLANE, 1, 3, 9, "SECTOR CLEAR");
                }
                if (state_timer > 120) {
                    PrintString(SCR_1_PLANE, 1, 3, 9, "            ");
                    current_wave++;
                    if (current_wave > MAX_WAVES) {
                        /* Boss time */
                        game_state = STATE_BOSS_IN;
                        state_timer = 0;
                    } else {
                        start_wave();
                        game_state = STATE_WAVE_IN;
                        state_timer = 0;
                    }
                }
                update_player();
                draw_ship();
                update_bullets();
                update_explosions();
                scroll_level();
                update_hud();
                break;

            case STATE_BOSS_IN:
                state_timer++;
                if (state_timer == 1) {
                    scroll_speed = 0;
                    scroll_x = 0;
                    ShiftScroll(SCR_1_PLANE, 0, 0);
                    ClearScreen(SCR_1_PLANE);
                    SysSetSystemFont();
                    init_tiles();
                    if (bw_mode) init_bw_palettes(); else init_palettes();
                    draw_hud();
                    PrintString(SCR_1_PLANE, 1, 5, 9, "WARNING!");
                    boss_active = 1;
                    PlaySound(SFX_BGM_BOSS);
                    boss_hp = 10; /* Phase 1: 10 HP */
                    boss_x = SCRN_W + 8;
                    boss_y = 50;
                    boss_phase = 0;
                    boss_timer = 0;
                    boss_fire_cd = 45;
                    boss_hit_flash = 0;
                    boss_prev_tx = 0xFF;
                    boss_prev_ty = 0xFF;
                }
                if (state_timer > 90) {
                    PrintString(SCR_1_PLANE, 1, 5, 9, "        ");
                    game_state = STATE_BOSS;
                }
                update_player();
                draw_ship();
                update_bullets();
                update_boss();
                update_explosions();
                update_hud();
                break;

            case STATE_BOSS:
                update_player();
                update_bullets();
                update_enemies();
                update_boss();
                update_explosions();
                check_collisions();
                draw_ship();
                update_hud();
                break;

            default:
                break;
            }
        }

        /* End screens */
        if (game_state == STATE_COMPLETE) {
            /* Victory */
            { u8 s; for (s = 0; s < 64; s++) UnsetSprite(s); }
            if (score > hiscore) hiscore = score;
            PrintString(SCR_1_PLANE, 1, 2, 8, "CHAPTER 1 COMPLETE");
            PrintString(SCR_1_PLANE, 1, 5, 10, "STAR RISE");
            {
                u8 t;
                for (t = 0; t < 240; t++) {
                    WaitVsync();
                    if (bw_mode) init_bw_palettes(); else init_palettes();
                    update_hud();
                }
            }
            break; /* leave session loop -> title */
        } else {
            /* Game over — offer continue */
            if (score > hiscore) hiscore = score;
            if (continues > 0) {
                u8 countdown;
                { u8 si; for (si = 0; si < 64; si++) UnsetSprite(si); }
                PrintString(SCR_1_PLANE, 1, 4, 9, "CONTINUE?");
                draw_number(14, 9, (u32)continues, 1);
                PrintString(SCR_1_PLANE, 1, 3, 11, "A=YES  B=NO");
                /* Debounce: wait for button release first */
                { u8 db; for (db = 0; db < 30; db++) WaitVsync(); }
                while (JOYPAD & 0x7F) WaitVsync();
                /* 10-second countdown */
                {
                u8 chose;
                countdown = 10;
                chose = 0;
                while (countdown > 0 && chose == 0) {
                    u16 ct;
                    draw_number(9, 13, (u32)countdown, 2);
                    for (ct = 0; ct < 60; ct++) {
                        WaitVsync();
                        joy = JOYPAD & 0x7F;
                        joy_edge = joy & ~joy_prev;
                        joy_prev = joy;
                        if (joy_edge & J_A) { chose = 1; break; }
                        if (joy_edge & J_B) { chose = 2; break; }
                    }
                    if (chose == 0) countdown--;
                }
                PrintString(SCR_1_PLANE, 1, 3, 9, "              ");
                PrintString(SCR_1_PLANE, 1, 3, 11, "           ");
                PrintString(SCR_1_PLANE, 1, 8, 13, "    ");
                if (chose == 1) {
                    continues--;
                    player_lives = 3;
                    player_power = 0;
                    player_speed = PLAYER_SPEED;
                    invuln_timer = 90;
                    player_x = 20;
                    player_y = 68;
                    if (boss_active) game_state = STATE_BOSS;
                    else game_state = STATE_PLAY;
                    continue; /* re-enter session loop */
                }
                /* B or timeout: debounce, fall through to game over */
                while (JOYPAD & 0x7F) WaitVsync();
                }
            }
            /* No continues left, declined, or timed out */
            { u8 si; for (si = 0; si < 64; si++) UnsetSprite(si); }
            PrintString(SCR_1_PLANE, 1, 5, 9, "GAME OVER");
            { u8 t; for (t = 0; t < 180; t++) WaitVsync(); }
            /* Debounce */
            while (JOYPAD & 0x7F) WaitVsync();
            { u8 t; for (t = 0; t < 30; t++) WaitVsync(); }
            PrintString(SCR_1_PLANE, 1, 3, 12, "PRESS START");
            while (1) {
                WaitVsync();
                joy = JOYPAD & 0x7F;
                joy_edge = joy & ~joy_prev;
                joy_prev = joy;
                if (joy_edge & (J_A | J_OPTION)) break;
            }
            break; /* leave session loop -> title */
        }

        } /* end session loop */
    }
}
