/**
 * ============================================================================
 * MICROMOUSE EMBEDDED C FIRMWARE REFERENCE (STM32F411CEU6 BLACKPILL)
 * ============================================================================
 * Tệp này tổng hợp toàn bộ cấu trúc chương trình C chuẩn cho chuột Micromouse
 * sử dụng bộ linh kiện phần cứng thực tế:
 *  1. MCU        : WeAct Blackpill STM32F411CEU6 (100 MHz, 128 KB RAM)
 *  2. Driver     : TB6612FNG hoặc DRV8833 (2 kênh PWM + Direction)
 *  3. Động cơ    : 2x GA12-N20 có Encoder 2 kênh A/B (gắn vào TIM2 & TIM3)
 *  4. Cảm biến   : 3x Cặp hồng ngoại Analog (SFH4545 + TEFT4300) đọc qua ADC1
 * ============================================================================
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* --------------------------------------------------------------------------
 * PHẦN 1: CẤU TRÚC DỮ LIỆU MÊ CUNG & THUẬT TOÁN FLOOD FILL + BỘ NHỚ CACHE
 * -------------------------------------------------------------------------- */
#define MAZE_SIZE   16
#define INF_DIST    9999

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

#define WALL_N (1 << 0)
#define WALL_E (1 << 1)
#define WALL_S (1 << 2)
#define WALL_W (1 << 3)

const int8_t  DX[4]            = {0, 1, 0, -1};
const int8_t  DY[4]            = {1, 0, -1, 0};
const uint8_t WALL_MASK[4]     = {WALL_N, WALL_E, WALL_S, WALL_W};
const uint8_t OPPOSITE_WALL[4] = {WALL_S, WALL_W, WALL_N, WALL_E};

uint8_t  known_walls[MAZE_SIZE][MAZE_SIZE];
uint8_t  visited_cell[MAZE_SIZE][MAZE_SIZE];
uint16_t dist[MAZE_SIZE][MAZE_SIZE];

typedef enum {
    CMD_START = 0,
    CMD_MOVE_FORWARD,
    CMD_TURN_RIGHT_AND_MOVE,
    CMD_TURN_LEFT_AND_MOVE,
    CMD_TURN_BACK_AND_MOVE
} MoveCommand_t;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t dir;
    MoveCommand_t cmd;
} CacheNode_t;

CacheNode_t route_cache[MAZE_SIZE * MAZE_SIZE];
uint16_t    cache_length = 0;

typedef struct {
    uint8_t x, y;
} Cell_t;

static Cell_t bfs_queue[MAZE_SIZE * MAZE_SIZE];

void maze_set_wall(uint8_t x, uint8_t y, uint8_t dir) {
    known_walls[x][y] |= WALL_MASK[dir];
    int8_t nx = x + DX[dir];
    int8_t ny = y + DY[dir];
    if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
        known_walls[nx][ny] |= OPPOSITE_WALL[dir];
    }
}

void maze_init_outer_walls(void) {
    for (uint8_t i = 0; i < MAZE_SIZE; i++) {
        maze_set_wall(i, MAZE_SIZE - 1, NORTH);
        maze_set_wall(i, 0, SOUTH);
        maze_set_wall(0, i, WEST);
        maze_set_wall(MAZE_SIZE - 1, i, EAST);
    }
    maze_set_wall(0, 0, EAST); // Vách xuất phát chuẩn bên phải ô (0,0)
}

/**
 * Cập nhật ma trận khoảng cách Flood Fill
 * - only_visited = false: Dùng trong Bước 1 (Dò đường mở rộng)
 * - only_visited = true : Dùng khi khóa sổ tạo Cache cho Bước 2
 *                         (Chỉ đi qua các ô ĐÃ DÒ, không bao giờ cắt qua ô tối/tường ẩn)
 */
void update_floodfill(bool only_visited) {
    uint16_t head = 0, tail = 0;

    for (uint8_t x = 0; x < MAZE_SIZE; x++) {
        for (uint8_t y = 0; y < MAZE_SIZE; y++) {
            dist[x][y] = INF_DIST;
        }
    }

    const uint8_t goals[4][2] = {{7,7}, {7,8}, {8,7}, {8,8}};
    for (int i = 0; i < 4; i++) {
        uint8_t gx = goals[i][0], gy = goals[i][1];
        if (!only_visited || visited_cell[gx][gy]) {
            dist[gx][gy] = 0;
            bfs_queue[tail++] = (Cell_t){gx, gy};
        }
    }

    while (head < tail) {
        Cell_t c = bfs_queue[head++];
        uint16_t cur_d = dist[c.x][c.y];

        for (uint8_t d = 0; d < 4; d++) {
            if ((known_walls[c.x][c.y] & WALL_MASK[d]) == 0) {
                int8_t nx = c.x + DX[d];
                int8_t ny = c.y + DY[d];
                if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
                    if (only_visited && !visited_cell[nx][ny]) continue;
                    if (dist[nx][ny] > cur_d + 1) {
                        dist[nx][ny] = cur_d + 1;
                        bfs_queue[tail++] = (Cell_t){nx, ny};
                    }
                }
            }
        }
    }
}

/**
 * Tạo lộ trình ngắn nhất an toàn sau Bước 1 và lưu vào mảng route_cache[]
 */
void build_safe_route_cache(void) {
    update_floodfill(true);

    cache_length = 0;
    uint8_t cx = 0, cy = 0, cdir = NORTH;
    route_cache[cache_length++] = (CacheNode_t){cx, cy, cdir, CMD_START};

    while (dist[cx][cy] > 0 && cache_length < MAZE_SIZE * MAZE_SIZE) {
        uint16_t min_d = dist[cx][cy];
        int8_t best_dir = -1;
        uint8_t pref[4] = {cdir, (cdir + 1) & 3, (cdir + 3) & 3, (cdir + 2) & 3};

        for (int i = 0; i < 4; i++) {
            uint8_t d = pref[i];
            if ((known_walls[cx][cy] & WALL_MASK[d]) == 0) {
                int8_t nx = cx + DX[d];
                int8_t ny = cy + DY[d];
                if (nx >= 0 && nx < MAZE_SIZE && ny >= 0 && ny < MAZE_SIZE) {
                    if (visited_cell[nx][ny] && dist[nx][ny] < min_d) {
                        min_d = dist[nx][ny];
                        best_dir = d;
                    }
                }
            }
        }

        if (best_dir < 0) break;

        MoveCommand_t cmd = CMD_MOVE_FORWARD;
        if (best_dir == ((cdir + 1) & 3))      cmd = CMD_TURN_RIGHT_AND_MOVE;
        else if (best_dir == ((cdir + 3) & 3)) cmd = CMD_TURN_LEFT_AND_MOVE;
        else if (best_dir == ((cdir + 2) & 3)) cmd = CMD_TURN_BACK_AND_MOVE;

        cx += DX[best_dir];
        cy += DY[best_dir];
        cdir = best_dir;

        route_cache[cache_length++] = (CacheNode_t){cx, cy, cdir, cmd};
    }
}
