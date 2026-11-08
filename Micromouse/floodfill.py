"""
Micromouse Flood Fill Algorithm & Competition Simulator Core
Hỗ trợ:
1. Tạo mê cung ngẫu nhiên chuẩn thi đấu (Braided Maze & DFS Perfect Maze)
2. Hai biến thể Flood Fill:
   - Classic Flood Fill (Tối ưu số ô bước đi - Manhattan step cost)
   - Modified / Turn-Weighted Flood Fill (Tối ưu thời gian thực - Phạt trọng số khi rẽ góc)
3. Hai chế độ hoạt động:
   - Mode Test: Thử nghiệm tự do
   - Mode Giả lập thi đấu:
       + Bước 1: Robot dò map, cập nhật tường và trích xuất đường đi ngắn nhất
                 (chỉ đi trên các ô đã dò an toàn) lưu vào Cache
       + Bước 2: Robot chạy tốc độ cao (Fast Run) đọc trực tiếp lộ trình từ bộ nhớ Cache
"""

import random
from collections import deque
from typing import List, Tuple, Optional, Dict

# Hướng di chuyển: Bắc (0), Đông (1), Nam (2), Tây (3)
NORTH = 0
EAST = 1
SOUTH = 2
WEST = 3

WALL_NORTH = 1
WALL_EAST = 2
WALL_SOUTH = 4
WALL_WEST = 8

DX = [0, 1, 0, -1]
DY = [1, 0, -1, 0]  # (0,0) ở góc dưới trái, Y tăng khi đi lên hướng Bắc
OPPOSITE_DIR = [SOUTH, WEST, NORTH, EAST]
WALL_BITS = [WALL_NORTH, WALL_EAST, WALL_SOUTH, WALL_WEST]
DIR_SYMBOLS = ["^", ">", "V", "<"]
DIR_NAMES = ["Bắc (N)", "Đông (E)", "Nam (S)", "Tây (W)"]


class Maze:
    """Mô hình lưu trữ mê cung thực tế hoặc bản đồ nhớ của Robot"""
    def __init__(self, width: int = 16, height: int = 16):
        self.width = width
        self.height = height
        self.walls = [[0 for _ in range(height)] for _ in range(width)]
        self._init_outer_walls()

    def _init_outer_walls(self):
        """Khởi tạo 4 bức tường bao quanh mê cung và vách xuất phát (0,0)"""
        for x in range(self.width):
            self.add_wall(x, self.height - 1, NORTH)
            self.add_wall(x, 0, SOUTH)
        for y in range(self.height):
            self.add_wall(0, y, WEST)
            self.add_wall(self.width - 1, y, EAST)
        if self.width > 1 and self.height > 1:
            self.add_wall(0, 0, EAST)

    def clear_internal_walls(self):
        self.walls = [[0 for _ in range(self.height)] for _ in range(self.width)]
        self._init_outer_walls()

    def has_wall(self, x: int, y: int, direction: int) -> bool:
        if not (0 <= x < self.width and 0 <= y < self.height):
            return True
        return (self.walls[x][y] & WALL_BITS[direction]) != 0

    def add_wall(self, x: int, y: int, direction: int):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        self.walls[x][y] |= WALL_BITS[direction]
        nx, ny = x + DX[direction], y + DY[direction]
        if 0 <= nx < self.width and 0 <= ny < self.height:
            self.walls[nx][ny] |= WALL_BITS[OPPOSITE_DIR[direction]]

    def remove_wall(self, x: int, y: int, direction: int):
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        self.walls[x][y] &= ~WALL_BITS[direction]
        nx, ny = x + DX[direction], y + DY[direction]
        if 0 <= nx < self.width and 0 <= ny < self.height:
            self.walls[nx][ny] &= ~WALL_BITS[OPPOSITE_DIR[direction]]

    def toggle_wall(self, x: int, y: int, direction: int):
        if self.has_wall(x, y, direction):
            self.remove_wall(x, y, direction)
        else:
            self.add_wall(x, y, direction)

    def generate_random(self, goals: List[Tuple[int, int]], braid_ratio: float = 0.25):
        w, h = self.width, self.height
        for x in range(w):
            for y in range(h):
                for d in range(4):
                    self.add_wall(x, y, d)

        visited = [[False for _ in range(h)] for _ in range(w)]
        stack = [(0, 0)]
        visited[0][0] = True

        if h > 1:
            self.remove_wall(0, 0, NORTH)
            visited[0][1] = True
            stack.append((0, 1))

        while stack:
            cx, cy = stack[-1]
            neighbors = []
            for d in range(4):
                if cx == 0 and cy == 0 and d == EAST:
                    continue
                nx, ny = cx + DX[d], cy + DY[d]
                if 0 <= nx < w and 0 <= ny < h and not visited[nx][ny]:
                    if nx == 0 and ny == 0:
                        continue
                    neighbors.append((nx, ny, d))

            if neighbors:
                nx, ny, d = random.choice(neighbors)
                self.remove_wall(cx, cy, d)
                visited[nx][ny] = True
                stack.append((nx, ny))
            else:
                stack.pop()

        if braid_ratio > 0:
            for x in range(1, w - 1):
                for y in range(1, h - 1):
                    if random.random() < braid_ratio:
                        d = random.choice([NORTH, EAST])
                        self.remove_wall(x, y, d)

        goal_set = set(goals)
        for gx, gy in goals:
            for d in range(4):
                nx, ny = gx + DX[d], gy + DY[d]
                if (nx, ny) in goal_set:
                    self.remove_wall(gx, gy, d)

        self.add_wall(0, 0, EAST)
        self.add_wall(0, 0, SOUTH)
        self.add_wall(0, 0, WEST)
        if h > 1:
            self.remove_wall(0, 0, NORTH)


class FloodFillSolver:
    """Bộ giải mê cung Micromouse hỗ trợ cả Mode Test và Mode Giả Lập Thi Đấu"""
    def __init__(self, width: int = 16, height: int = 16, algo_type: str = "classic"):
        self.width = width
        self.height = height
        self.algo_type = algo_type  # "classic" hoặc "modified"
        self.goals = self._default_goals()

        self.known_maze = Maze(width, height)
        self.distances = [[0 for _ in range(height)] for _ in range(width)]
        self.visited = [[False for _ in range(height)] for _ in range(width)]

        self.mouse_x = 0
        self.mouse_y = 0
        self.mouse_dir = NORTH
        self.visited[0][0] = True

        self.route_cache: List[Dict] = []
        self.cache_step_index = 0

        self.update_flood()

    def _default_goals(self) -> List[Tuple[int, int]]:
        w, h = self.width, self.height
        if w % 2 == 0 and h % 2 == 0:
            cx, cy = w // 2, h // 2
            return [(cx - 1, cy - 1), (cx - 1, cy), (cx, cy - 1), (cx, cy)]
        else:
            return [(w // 2, h // 2)]

    def init_distances(self, target_goals: List[Tuple[int, int]]):
        self.update_flood(target_goals, only_visited=False)

    def update_flood(self, target_goals: Optional[List[Tuple[int, int]]] = None, only_visited: bool = False):
        """
        Cập nhật ma trận khoảng cách Flood Fill.
        Nếu only_visited=True (dùng khi tạo Cache cho Bước 2), sóng chỉ lan truyền
        trên các ô Robot ĐÃ DÒ (visited=True) để đảm bảo lộ trình Cache tuyệt đối
        không đi xuyên qua các ô chưa khám phá hay tường ẩn.
        """
        if target_goals is None:
            target_goals = self.goals

        for x in range(self.width):
            for y in range(self.height):
                self.distances[x][y] = 9999

        if self.algo_type == "classic":
            queue = deque()
            for gx, gy in target_goals:
                if not only_visited or self.visited[gx][gy]:
                    self.distances[gx][gy] = 0
                    queue.append((gx, gy))

            while queue:
                cx, cy = queue.popleft()
                curr_dist = self.distances[cx][cy]
                for d in range(4):
                    if not self.known_maze.has_wall(cx, cy, d):
                        nx, ny = cx + DX[d], cy + DY[d]
                        if 0 <= nx < self.width and 0 <= ny < self.height:
                            if only_visited and not self.visited[nx][ny]:
                                continue
                            if self.distances[nx][ny] > curr_dist + 1:
                                self.distances[nx][ny] = curr_dist + 1
                                queue.append((nx, ny))
        else:
            queue = deque()
            for gx, gy in target_goals:
                if not only_visited or self.visited[gx][gy]:
                    self.distances[gx][gy] = 0
                    queue.append((gx, gy, -1))

            while queue:
                cx, cy, prev_dir = queue.popleft()
                curr_dist = self.distances[cx][cy]
                for d in range(4):
                    if not self.known_maze.has_wall(cx, cy, d):
                        nx, ny = cx + DX[d], cy + DY[d]
                        if 0 <= nx < self.width and 0 <= ny < self.height:
                            if only_visited and not self.visited[nx][ny]:
                                continue
                            step_cost = 1 if (prev_dir == -1 or prev_dir == d) else 2
                            new_cost = curr_dist + step_cost
                            if self.distances[nx][ny] > new_cost:
                                self.distances[nx][ny] = new_cost
                                queue.append((nx, ny, d))

    def sense_and_update(self, real_maze: Maze, target_goals: Optional[List[Tuple[int, int]]] = None):
        x, y = self.mouse_x, self.mouse_y
        self.visited[x][y] = True
        walls_changed = False

        for d in range(4):
            wall_present = real_maze.has_wall(x, y, d)
            if wall_present != self.known_maze.has_wall(x, y, d):
                if wall_present:
                    self.known_maze.add_wall(x, y, d)
                else:
                    self.known_maze.remove_wall(x, y, d)
                walls_changed = True

        if walls_changed:
            self.update_flood(target_goals, only_visited=False)

    def get_best_next_direction(self, prefer_unvisited: bool = True) -> Optional[int]:
        x, y = self.mouse_x, self.mouse_y
        min_dist = 9999
        best_dir = None

        turn_preference = [
            self.mouse_dir,
            (self.mouse_dir + 1) % 4,
            (self.mouse_dir + 3) % 4,
            (self.mouse_dir + 2) % 4
        ]

        for d in turn_preference:
            if not self.known_maze.has_wall(x, y, d):
                nx, ny = x + DX[d], y + DY[d]
                if 0 <= nx < self.width and 0 <= ny < self.height:
                    dist = self.distances[nx][ny]
                    if dist < min_dist or (prefer_unvisited and dist == min_dist and not self.visited[nx][ny]):
                        min_dist = dist
                        best_dir = d

        return best_dir

    def step(self, real_maze: Maze) -> bool:
        return self.step_explore(real_maze, self.goals)

    def step_explore(self, real_maze: Maze, target_goals: Optional[List[Tuple[int, int]]] = None) -> bool:
        if target_goals is None:
            target_goals = self.goals

        self.sense_and_update(real_maze, target_goals)
        if (self.mouse_x, self.mouse_y) in target_goals:
            return True

        best_dir = self.get_best_next_direction(prefer_unvisited=True)
        if best_dir is None:
            return True

        self.mouse_dir = best_dir
        self.mouse_x += DX[best_dir]
        self.mouse_y += DY[best_dir]
        self.visited[self.mouse_x][self.mouse_y] = True
        self.sense_and_update(real_maze, target_goals)
        return (self.mouse_x, self.mouse_y) in target_goals

    def build_shortest_path_cache(self) -> List[Dict]:
        """
        Trích xuất đường đi ngắn nhất từ (0,0) tới Đích CHỈ ĐI QUA CÁC Ô ĐÃ DÒ (only_visited=True)
        và lưu chuỗi lệnh vào bộ nhớ Cache. Đảm bảo 100% không đi xuyên tường chưa quét.
        """
        self.update_flood(self.goals, only_visited=True)
        self.route_cache = []

        cx, cy = 0, 0
        cdir = NORTH
        self.route_cache.append({
            "x": cx,
            "y": cy,
            "dir": cdir,
            "cmd": "START"
        })

        visited_in_path = {(cx, cy)}
        max_steps = self.width * self.height

        for _ in range(max_steps):
            if (cx, cy) in self.goals:
                break

            min_dist = 9999
            best_d = None
            turn_pref = [cdir, (cdir + 1) % 4, (cdir + 3) % 4, (cdir + 2) % 4]

            for d in turn_pref:
                if not self.known_maze.has_wall(cx, cy, d):
                    nx, ny = cx + DX[d], cy + DY[d]
                    if 0 <= nx < self.width and 0 <= ny < self.height:
                        if not self.visited[nx][ny] or (nx, ny) in visited_in_path:
                            continue
                        if self.distances[nx][ny] < min_dist:
                            min_dist = self.distances[nx][ny]
                            best_d = d

            if best_d is None:
                break

            if best_d == cdir:
                cmd = "F (Tiến thẳng)"
            elif best_d == (cdir + 1) % 4:
                cmd = "R + F (Rẽ phải & Tiến)"
            elif best_d == (cdir + 3) % 4:
                cmd = "L + F (Rẽ trái & Tiến)"
            else:
                cmd = "B + F (Quay đầu & Tiến)"

            cx += DX[best_d]
            cy += DY[best_d]
            cdir = best_d
            visited_in_path.add((cx, cy))

            self.route_cache.append({
                "x": cx,
                "y": cy,
                "dir": cdir,
                "cmd": cmd
            })

        self.cache_step_index = 0
        return self.route_cache

    def step_from_cache(self) -> bool:
        """Di chuyển Robot theo đúng lộ trình đã lưu trong Cache (Bước 2)"""
        if not self.route_cache:
            return True

        if self.cache_step_index < len(self.route_cache) - 1:
            self.cache_step_index += 1
            node = self.route_cache[self.cache_step_index]
            self.mouse_x = node["x"]
            self.mouse_y = node["y"]
            self.mouse_dir = node["dir"]
            self.visited[self.mouse_x][self.mouse_y] = True

        return self.cache_step_index >= len(self.route_cache) - 1
