"""
Mackorone MMS (Micromouse Simulator) Python Driver
Tệp này tương thích hoàn toàn 100% với trình giả lập mã nguồn mở mackorone/mms (trong Video 1).
Nếu bạn tải MMS về chạy, chỉ cần trỏ lệnh chạy đến: python mms_mouse.py
"""

import sys
from collections import deque

class API:
    @staticmethod
    def mazeWidth():
        sys.stdout.write("mazeWidth\n")
        sys.stdout.flush()
        return int(sys.stdin.readline().strip())

    @staticmethod
    def mazeHeight():
        sys.stdout.write("mazeHeight\n")
        sys.stdout.flush()
        return int(sys.stdin.readline().strip())

    @staticmethod
    def wallFront():
        sys.stdout.write("wallFront\n")
        sys.stdout.flush()
        return sys.stdin.readline().strip() == "true"

    @staticmethod
    def wallRight():
        sys.stdout.write("wallRight\n")
        sys.stdout.flush()
        return sys.stdin.readline().strip() == "true"

    @staticmethod
    def wallLeft():
        sys.stdout.write("wallLeft\n")
        sys.stdout.flush()
        return sys.stdin.readline().strip() == "true"

    @staticmethod
    def moveForward(distance=None):
        cmd = f"moveForward {distance}\n" if distance else "moveForward\n"
        sys.stdout.write(cmd)
        sys.stdout.flush()
        res = sys.stdin.readline().strip()
        return res != "crash"

    @staticmethod
    def turnRight():
        sys.stdout.write("turnRight\n")
        sys.stdout.flush()
        sys.stdin.readline()

    @staticmethod
    def turnLeft():
        sys.stdout.write("turnLeft\n")
        sys.stdout.flush()
        sys.stdin.readline()

    @staticmethod
    def setWall(x, y, direction):
        sys.stdout.write(f"setWall {x} {y} {direction}\n")
        sys.stdout.flush()

    @staticmethod
    def setColor(x, y, color):
        sys.stdout.write(f"setColor {x} {y} {color}\n")
        sys.stdout.flush()

    @staticmethod
    def setText(x, y, text):
        sys.stdout.write(f"setText {x} {y} {text}\n")
        sys.stdout.flush()

    @staticmethod
    def log(text):
        sys.stderr.write(f"{text}\n")
        sys.stderr.flush()

# Hướng: 0: North, 1: East, 2: South, 3: West
NORTH, EAST, SOUTH, WEST = 0, 1, 2, 3
DIR_CHARS = ['n', 'e', 's', 'w']
DX = [0, 1, 0, -1]
DY = [1, 0, -1, 0]

def main():
    API.log("Khởi động chuột thuật toán Flood Fill cho MMS...")
    w = API.mazeWidth()
    h = API.mazeHeight()

    # Xác định các ô đích ở giữa
    goals = set()
    if w % 2 == 0 and h % 2 == 0:
        cx, cy = w // 2, h // 2
        goals = {(cx - 1, cy - 1), (cx - 1, cy), (cx, cy - 1), (cx, cy)}
    else:
        goals = {(w // 2, h // 2)}

    # Bản đồ tường nội bộ của chuột
    walls = [[[False]*4 for _ in range(h)] for _ in range(w)]
    distances = [[9999]*h for _ in range(w)]

    # Tường ngoài
    for x in range(w):
        walls[x][h-1][NORTH] = True
        walls[x][0][SOUTH] = True
    for y in range(h):
        walls[0][y][WEST] = True
        walls[w-1][y][EAST] = True

    # Trạng thái chuột
    x, y, direction = 0, 0, NORTH

    def update_flood():
        for i in range(w):
            for j in range(h):
                distances[i][j] = 9999
        queue = deque()
        for gx, gy in goals:
            distances[gx][gy] = 0
            queue.append((gx, gy))
            API.setText(gx, gy, "0")

        while queue:
            cx, cy = queue.popleft()
            cd = distances[cx][cy]
            for d in range(4):
                if not walls[cx][cy][d]:
                    nx, ny = cx + DX[d], cy + DY[d]
                    if 0 <= nx < w and 0 <= ny < h:
                        if distances[nx][ny] > cd + 1:
                            distances[nx][ny] = cd + 1
                            queue.append((nx, ny))
                            API.setText(nx, ny, str(cd + 1))

    def add_internal_wall(cx, cy, d):
        walls[cx][cy][d] = True
        API.setWall(cx, cy, DIR_CHARS[d])
        nx, ny = cx + DX[d], cy + DY[d]
        if 0 <= nx < w and 0 <= ny < h:
            walls[nx][ny][(d + 2) % 4] = True

    # Cập nhật lần đầu
    update_flood()

    while True:
        API.setColor(x, y, "G") # Tô màu xanh lá các ô đã đi qua như Video 1

        if (x, y) in goals:
            API.log("Đã đến đích thành công!")
            break

        # Quét cảm biến tường
        # Tường phía trước
        if API.wallFront():
            add_internal_wall(x, y, direction)
        # Tường bên phải
        if API.wallRight():
            add_internal_wall(x, y, (direction + 1) % 4)
        # Tường bên trái
        if API.wallLeft():
            add_internal_wall(x, y, (direction + 3) % 4)

        # Cập nhật khoảng cách Floodfill
        update_flood()

        # Chọn hướng đi có chi phí nhỏ nhất
        min_cost = 9999
        best_dir = direction

        for d in [direction, (direction + 1) % 4, (direction + 3) % 4, (direction + 2) % 4]:
            if not walls[x][y][d]:
                nx, ny = x + DX[d], y + DY[d]
                if 0 <= nx < w and 0 <= ny < h:
                    if distances[nx][ny] < min_cost:
                        min_cost = distances[nx][ny]
                        best_dir = d

        # Xoay chuột theo hướng tốt nhất
        if best_dir == (direction + 1) % 4:
            API.turnRight()
        elif best_dir == (direction + 3) % 4:
            API.turnLeft()
        elif best_dir == (direction + 2) % 4:
            API.turnRight()
            API.turnRight()

        direction = best_dir
        API.moveForward()
        x += DX[direction]
        y += DY[direction]

if __name__ == "__main__":
    main()
