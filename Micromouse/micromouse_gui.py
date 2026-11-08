"""
Micromouse Flood Fill & Competition Simulator (Python Tkinter Desktop GUI)
Hỗ trợ:
- Tạo Map Ngẫu Nhiên chuẩn thi đấu Micromouse (Đa lộ trình / Độc đạo / Trống)
- 2 loại thuật toán Flood Fill: Classic Flood Fill & Turn-Weighted (Modified) Flood Fill
- 2 Chế độ:
    1. Mode Test: Điều khiển tự do, bước đơn, sửa tường trực tiếp
    2. Mode Giả Lập Thi Đấu:
       + Bước 1: Robot dò map tìm đường ngắn nhất và nạp vào Bộ nhớ Cache
       + Bước 2: Robot đọc lộ trình từ Cache và chạy tốc độ cao về Đích
"""

import tkinter as tk
from tkinter import ttk, messagebox
from floodfill import (
    Maze, FloodFillSolver,
    NORTH, EAST, SOUTH, WEST,
    DX, DY, DIR_SYMBOLS
)


class MicromouseApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Micromouse Simulator — Flood Fill & Competition Mode")
        self.geometry("1120x760")
        self.minsize(920, 640)

        self.maze_size = 16
        self.speed_ms = 100
        self.timer_id = None
        self.app_mode = "competition"  # "competition" hoặc "test"
        # Trạng thái thi đấu: IDLE | STEP1_EXPLORE | CACHE_READY | STEP2_CACHE | DONE
        self.comp_state = "IDLE"
        self.explore_steps = 0
        self.cache_steps = 0

        self.real_maze = Maze(self.maze_size, self.maze_size)
        self.solver = FloodFillSolver(self.maze_size, self.maze_size, algo_type="classic")

        self.create_widgets()
        self.on_generate_random()

    def create_widgets(self):
        # 1. Thanh công cụ trên cùng
        top_bar = tk.Frame(self, pady=8, padx=12, bd=1, relief=tk.RAISED)
        top_bar.pack(side=tk.TOP, fill=tk.X)

        tk.Label(top_bar, text="Chế độ:", font=("Segoe UI", 9, "bold")).pack(side=tk.LEFT, padx=(0, 4))
        self.mode_combo = ttk.Combobox(
            top_bar,
            values=["Mode Giả Lập Thi Đấu (2 Bước)", "Mode Test (Tự Do)"],
            width=26,
            state="readonly"
        )
        self.mode_combo.current(0)
        self.mode_combo.bind("<<ComboboxSelected>>", self.on_mode_change)
        self.mode_combo.pack(side=tk.LEFT, padx=(0, 12))

        tk.Label(top_bar, text="Cỡ Map:", font=("Segoe UI", 9, "bold")).pack(side=tk.LEFT, padx=(0, 4))
        self.size_entry = tk.Entry(top_bar, width=4, font=("Segoe UI", 9), justify="center")
        self.size_entry.insert(0, str(self.maze_size))
        self.size_entry.pack(side=tk.LEFT, padx=(0, 4))
        tk.Button(top_bar, text="Đặt Cỡ", command=self.on_apply_size).pack(side=tk.LEFT, padx=(0, 10))

        self.map_type_combo = ttk.Combobox(
            top_bar,
            values=["Map Thi Đấu (Đa lộ trình)", "Map Độc Đạo (DFS)", "Map Trống"],
            width=22,
            state="readonly"
        )
        self.map_type_combo.current(0)
        self.map_type_combo.pack(side=tk.LEFT, padx=(0, 6))

        tk.Button(
            top_bar,
            text="🎲 Tạo Map Ngẫu Nhiên",
            bg="#d97706",
            fg="white",
            font=("Segoe UI", 9, "bold"),
            command=self.on_generate_random
        ).pack(side=tk.LEFT, padx=(0, 14))

        # Các nút điều khiển Bước 1 & Bước 2
        self.btn_step1 = tk.Button(
            top_bar,
            text="🔍 Bước 1: Dò Map",
            bg="#2563eb",
            fg="white",
            font=("Segoe UI", 9, "bold"),
            command=self.on_start_step1
        )
        self.btn_step1.pack(side=tk.LEFT, padx=3)

        self.btn_step2 = tk.Button(
            top_bar,
            text="🚀 Bước 2: Chạy Cache",
            bg="#7c3aed",
            fg="white",
            font=("Segoe UI", 9, "bold"),
            state=tk.DISABLED,
            command=self.on_start_step2
        )
        self.btn_step2.pack(side=tk.LEFT, padx=3)

        self.btn_step_single = tk.Button(
            top_bar,
            text="Bước Đơn (Step)",
            command=self.on_single_step
        )
        self.btn_step_single.pack(side=tk.LEFT, padx=3)

        tk.Button(
            top_bar,
            text="🔄 Reset",
            bg="#dc2626",
            fg="white",
            font=("Segoe UI", 9, "bold"),
            command=self.on_reset
        ).pack(side=tk.LEFT, padx=4)

        # 2. Vùng chính: Canvas bên trái, Sidebar bên phải
        main_content = tk.Frame(self, padx=10, pady=10)
        main_content.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        canvas_frame = tk.Frame(main_content, bd=2, relief=tk.SUNKEN)
        canvas_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(canvas_frame, bg="white", cursor="crosshair")
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda e: self.update_canvas())
        self.canvas.bind("<Button-1>", self.on_canvas_click)

        # Sidebar bên phải
        sidebar = tk.Frame(main_content, width=320, padx=10)
        sidebar.pack(side=tk.RIGHT, fill=tk.Y)
        sidebar.pack_propagate(False)

        # Trạng thái & Thông số
        stats_frame = tk.LabelFrame(sidebar, text="Thông Số Robot & Giai Đoạn", font=("Segoe UI", 9, "bold"), padx=8, pady=8)
        stats_frame.pack(side=tk.TOP, fill=tk.X, pady=(0, 8))

        self.lbl_state = tk.Label(stats_frame, text="Trạng thái: Sẵn sàng", fg="#2563eb", font=("Segoe UI", 9, "bold"), anchor="w")
        self.lbl_state.pack(fill=tk.X, pady=2)

        self.lbl_pos = tk.Label(stats_frame, text="Tọa độ (X, Y): (0, 0) | Hướng: ^", font=("Segoe UI", 9), anchor="w")
        self.lbl_pos.pack(fill=tk.X, pady=2)

        self.lbl_dist = tk.Label(stats_frame, text="Giá trị Floodfill tại ô: 0", font=("Segoe UI", 9), anchor="w")
        self.lbl_dist.pack(fill=tk.X, pady=2)

        self.lbl_steps = tk.Label(stats_frame, text="Bước Dò (B1): 0 | Bước Cache (B2): 0", font=("Segoe UI", 9, "bold"), anchor="w")
        self.lbl_steps.pack(fill=tk.X, pady=2)

        # Lựa chọn loại thuật toán Flood Fill
        algo_frame = tk.LabelFrame(sidebar, text="Loại Thuật Toán Flood Fill", font=("Segoe UI", 9, "bold"), padx=8, pady=8)
        algo_frame.pack(side=tk.TOP, fill=tk.X, pady=(0, 8))

        self.algo_combo = ttk.Combobox(
            algo_frame,
            values=[
                "1. Classic Flood Fill (Đếm số ô)",
                "2. Modified Flood Fill (Ưu tiên đi thẳng)"
            ],
            state="readonly"
        )
        self.algo_combo.current(0)
        self.algo_combo.bind("<<ComboboxSelected>>", self.on_algo_change)
        self.algo_combo.pack(fill=tk.X, pady=(0, 6))

        self.auto_step2_var = tk.BooleanVar(value=True)
        tk.Checkbutton(
            algo_frame,
            text="Tự động chạy Bước 2 sau khi nạp Cache",
            variable=self.auto_step2_var
        ).pack(anchor="w")

        self.fog_var = tk.BooleanVar(value=True)
        tk.Checkbutton(
            algo_frame,
            text="Làm mờ tường chưa quét (Sương mù)",
            variable=self.fog_var,
            command=self.update_canvas
        ).pack(anchor="w")

        # Bộ nhớ Cache Lộ Trình
        cache_frame = tk.LabelFrame(sidebar, text="💾 Bộ Nhớ Cache Lộ Trình Ngắn Nhất", font=("Segoe UI", 9, "bold"), padx=8, pady=8)
        cache_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, pady=(0, 8))

        self.cache_listbox = tk.Listbox(cache_frame, font=("Consolas", 9), height=10)
        self.cache_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar = tk.Scrollbar(cache_frame, orient="vertical", command=self.cache_listbox.yview)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.cache_listbox.config(yscrollcommand=scrollbar.set)

        # Tốc độ
        speed_frame = tk.LabelFrame(sidebar, text="Tốc Độ Chạy Mô Phỏng", font=("Segoe UI", 9, "bold"), padx=8, pady=4)
        speed_frame.pack(side=tk.BOTTOM, fill=tk.X)
        self.speed_scale = tk.Scale(speed_frame, from_=1, to_=35, orient=tk.HORIZONTAL, command=self.on_speed_change)
        self.speed_scale.set(10)
        self.speed_scale.pack(fill=tk.X)

    def get_selected_algo(self) -> str:
        return "classic" if self.algo_combo.current() == 0 else "modified"

    def on_mode_change(self, event=None):
        self.app_mode = "competition" if self.mode_combo.current() == 0 else "test"
        self.on_reset()

    def on_algo_change(self, event=None):
        self.solver.algo_type = self.get_selected_algo()
        self.solver.update_flood()
        if self.solver.route_cache:
            self.solver.build_shortest_path_cache()
            self.refresh_cache_listbox()
        self.update_canvas()

    def on_apply_size(self):
        try:
            val = int(self.size_entry.get())
            if 6 <= val <= 32:
                self.maze_size = val
                self.on_generate_random()
            else:
                messagebox.showwarning("Lỗi", "Kích thước hợp lệ từ 6 đến 32!")
        except ValueError:
            messagebox.showerror("Lỗi", "Vui lòng nhập số nguyên!")

    def on_generate_random(self):
        self.stop_timer()
        self.real_maze = Maze(self.maze_size, self.maze_size)
        self.solver = FloodFillSolver(self.maze_size, self.maze_size, algo_type=self.get_selected_algo())

        choice = self.map_type_combo.current()
        if choice == 0:
            self.real_maze.generate_random(self.solver.goals, braid_ratio=0.24)
        elif choice == 1:
            self.real_maze.generate_random(self.solver.goals, braid_ratio=0.0)
        else:
            self.real_maze.clear_internal_walls()

        self.on_reset()

    def on_reset(self):
        self.stop_timer()
        self.solver = FloodFillSolver(self.maze_size, self.maze_size, algo_type=self.get_selected_algo())
        self.solver.sense_and_update(self.real_maze)
        self.comp_state = "IDLE"
        self.explore_steps = 0
        self.cache_steps = 0
        self.btn_step2.config(state=tk.DISABLED)
        self.refresh_cache_listbox()
        self.update_ui_stats()
        self.update_canvas()

    def on_start_step1(self):
        self.on_reset()
        self.comp_state = "STEP1_EXPLORE"
        self.lbl_state.config(text="Bước 1: Đang dò map & tìm đường ngắn nhất...", fg="#2563eb")
        self.run_loop()

    def on_start_step2(self):
        if not self.solver.route_cache:
            return
        self.stop_timer()
        self.solver.mouse_x = 0
        self.solver.mouse_y = 0
        self.solver.mouse_dir = NORTH
        self.solver.cache_step_index = 0
        self.cache_steps = 0
        self.comp_state = "STEP2_CACHE"
        self.lbl_state.config(text="Bước 2: Đang chạy theo lộ trình trong Cache...", fg="#7c3aed")
        self.update_canvas()
        self.run_loop()

    def on_single_step(self):
        self.stop_timer()
        if self.comp_state in ("IDLE", "STEP1_EXPLORE"):
            self.comp_state = "STEP1_EXPLORE"
            done = self.solver.step_explore(self.real_maze)
            self.explore_steps += 1
            if done:
                self.finish_step1()
        elif self.comp_state in ("CACHE_READY", "STEP2_CACHE"):
            self.comp_state = "STEP2_CACHE"
            done = self.solver.step_from_cache()
            self.cache_steps = self.solver.cache_step_index
            self.refresh_cache_listbox()
            if done:
                self.comp_state = "DONE"
                self.lbl_state.config(text="🏆 Đã hoàn thành lộ trình từ Cache!", fg="#16a34a")
        self.update_ui_stats()
        self.update_canvas()

    def run_loop(self):
        if self.comp_state == "STEP1_EXPLORE":
            done = self.solver.step_explore(self.real_maze)
            self.explore_steps += 1
            self.update_ui_stats()
            self.update_canvas()
            if done:
                self.finish_step1()
            else:
                self.timer_id = self.after(self.speed_ms, self.run_loop)

        elif self.comp_state == "STEP2_CACHE":
            done = self.solver.step_from_cache()
            self.cache_steps = self.solver.cache_step_index
            self.refresh_cache_listbox()
            self.update_ui_stats()
            self.update_canvas()
            if done:
                self.comp_state = "DONE"
                self.lbl_state.config(text="🏆 Hoàn tất chạy tốc độ cao từ Cache!", fg="#16a34a")
                self.stop_timer()
            else:
                self.timer_id = self.after(max(20, int(self.speed_ms * 0.7)), self.run_loop)

    def finish_step1(self):
        self.stop_timer()
        self.solver.build_shortest_path_cache()
        self.solver.mouse_x = 0
        self.solver.mouse_y = 0
        self.solver.mouse_dir = NORTH
        self.comp_state = "CACHE_READY"
        self.btn_step2.config(state=tk.NORMAL)
        self.lbl_state.config(text="✅ Đã lưu đường ngắn nhất vào Cache! Sẵn sàng Bước 2.", fg="#16a34a")
        self.refresh_cache_listbox()
        self.update_ui_stats()
        self.update_canvas()

        if self.auto_step2_var.get() and self.app_mode == "competition":
            self.timer_id = self.after(600, self.on_start_step2)

    def refresh_cache_listbox(self):
        self.cache_listbox.delete(0, tk.END)
        if not self.solver.route_cache:
            self.cache_listbox.insert(tk.END, " [Cache trống - Hãy chạy Bước 1]")
            return
        for idx, item in enumerate(self.solver.route_cache):
            line = f"#{idx:02d}: ({item['x']},{item['y']}) {DIR_SYMBOLS[item['dir']]} | {item['cmd']}"
            self.cache_listbox.insert(tk.END, line)
            if self.comp_state in ("STEP2_CACHE", "DONE") and idx == self.solver.cache_step_index:
                self.cache_listbox.selection_clear(0, tk.END)
                self.cache_listbox.selection_set(idx)
                self.cache_listbox.see(idx)

    def stop_timer(self):
        if self.timer_id:
            self.after_cancel(self.timer_id)
            self.timer_id = None

    def on_speed_change(self, val):
        steps_sec = int(val)
        self.speed_ms = max(20, int(1000 / steps_sec))

    def update_ui_stats(self):
        x, y = self.solver.mouse_x, self.solver.mouse_y
        d_sym = DIR_SYMBOLS[self.solver.mouse_dir]
        dist = self.solver.distances[x][y]
        self.lbl_pos.config(text=f"Tọa độ (X, Y): ({x}, {y}) | Hướng: {d_sym}")
        self.lbl_dist.config(text=f"Giá trị Floodfill tại ô: {dist}")
        self.lbl_steps.config(text=f"Bước Dò (B1): {self.explore_steps} | Bước Cache (B2): {self.cache_steps}")

    def on_canvas_click(self, event):
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        n = self.maze_size
        cell_size = min(w, h) / n
        offset_x = (w - cell_size * n) / 2
        offset_y = (h - cell_size * n) / 2

        cx = int((event.x - offset_x) // cell_size)
        cy = n - 1 - int((event.y - offset_y) // cell_size)
        if not (0 <= cx < n and 0 <= cy < n):
            return

        in_x = (event.x - offset_x) - cx * cell_size
        in_y = (event.y - offset_y) - (n - 1 - cy) * cell_size
        d_north = in_y
        d_south = cell_size - in_y
        d_west = in_x
        d_east = cell_size - in_x

        min_d = min(d_north, d_south, d_west, d_east)
        if min_d == d_north:
            target_dir = NORTH
        elif min_d == d_south:
            target_dir = SOUTH
        elif min_d == d_west:
            target_dir = WEST
        else:
            target_dir = EAST

        if (target_dir == NORTH and cy == n - 1) or \
           (target_dir == SOUTH and cy == 0) or \
           (target_dir == WEST and cx == 0) or \
           (target_dir == EAST and cx == n - 1):
            return

        self.real_maze.toggle_wall(cx, cy, target_dir)
        self.solver.known_maze.toggle_wall(cx, cy, target_dir)
        self.solver.update_flood()
        if self.solver.route_cache:
            self.solver.build_shortest_path_cache()
            self.refresh_cache_listbox()
        self.update_canvas()

    def update_canvas(self):
        self.canvas.delete("all")
        w = self.canvas.winfo_width()
        h = self.canvas.winfo_height()
        if w < 50 or h < 50:
            return

        n = self.maze_size
        cell_size = min(w, h) / n
        offset_x = (w - cell_size * n) / 2
        offset_y = (h - cell_size * n) / 2
        use_fog = self.fog_var.get()

        # 1. Vẽ nền các ô và số Floodfill
        for x in range(n):
            for y in range(n):
                sx = offset_x + x * cell_size
                sy = offset_y + (n - 1 - y) * cell_size

                if (x, y) in self.solver.goals:
                    bg_color = "#fde047"
                elif self.solver.visited[x][y]:
                    bg_color = "#dcfce7"
                else:
                    bg_color = "#ffffff"

                self.canvas.create_rectangle(sx, sy, sx + cell_size, sy + cell_size, fill=bg_color, outline="#e2e8f0")

                dist = self.solver.distances[x][y]
                if dist != 9999:
                    f_size = max(8, int(cell_size * 0.27))
                    self.canvas.create_text(sx + cell_size / 2, sy + cell_size / 2, text=str(dist), font=("Segoe UI", f_size, "bold"), fill="#1e293b")

        # 2. Vẽ lộ trình lưu trong Cache
        if len(self.solver.route_cache) > 1:
            coords = []
            for pt in self.solver.route_cache:
                px = offset_x + pt["x"] * cell_size + cell_size / 2
                py = offset_y + (n - 1 - pt["y"]) * cell_size + cell_size / 2
                coords.extend([px, py])
            self.canvas.create_line(*coords, fill="#f59e0b", width=max(4, int(cell_size * 0.22)), capstyle=tk.ROUND, joinstyle=tk.ROUND)

        # 3. Vẽ tường
        wall_w = max(2, int(cell_size * 0.08))
        for x in range(n):
            for y in range(n):
                sx = offset_x + x * cell_size
                sy = offset_y + (n - 1 - y) * cell_size
                for d in range(4):
                    if self.real_maze.has_wall(x, y, d):
                        is_known = self.solver.known_maze.has_wall(x, y, d)
                        color = "#1d4ed8" if (not use_fog or is_known) else "#cbd5e1"
                        if d == NORTH:
                            self.canvas.create_line(sx, sy, sx + cell_size, sy, fill=color, width=wall_w)
                        elif d == EAST:
                            self.canvas.create_line(sx + cell_size, sy, sx + cell_size, sy + cell_size, fill=color, width=wall_w)
                        elif d == SOUTH:
                            self.canvas.create_line(sx, sy + cell_size, sx + cell_size, sy + cell_size, fill=color, width=wall_w)
                        elif d == WEST:
                            self.canvas.create_line(sx, sy, sx, sy + cell_size, fill=color, width=wall_w)

        # 4. Vẽ đinh chốt
        peg_r = max(2, int(cell_size * 0.05))
        for x in range(n + 1):
            for y in range(n + 1):
                px = offset_x + x * cell_size
                py = offset_y + y * cell_size
                self.canvas.create_oval(px - peg_r, py - peg_r, px + peg_r, py + peg_r, fill="#ef4444", outline="")

        # 5. Vẽ Robot
        mx, my = self.solver.mouse_x, self.solver.mouse_y
        msx = offset_x + mx * cell_size
        msy = offset_y + (n - 1 - my) * cell_size
        m_pad = cell_size * 0.16
        m_color = "#7c3aed" if self.comp_state in ("STEP2_CACHE", "DONE") else "#dc2626"

        self.canvas.create_rectangle(msx + m_pad, msy + m_pad, msx + cell_size - m_pad, msy + cell_size - m_pad, fill=m_color, outline="white", width=1)
        f_size_m = max(10, int(cell_size * 0.42))
        self.canvas.create_text(msx + cell_size / 2, msy + cell_size / 2, text=DIR_SYMBOLS[self.solver.mouse_dir], font=("Consolas", f_size_m, "bold"), fill="white")


if __name__ == "__main__":
    app = MicromouseApp()
    app.mainloop()
