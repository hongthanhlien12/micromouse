# Micromouse Simulator — Flood Fill & Competition Engine

Bộ mô phỏng giải mê cung **Micromouse** hỗ trợ tạo mê cung ngẫu nhiên chuẩn thi đấu, so sánh trực quan 2 loại thuật toán **Flood Fill** và mô phỏng quy trình thi đấu 2 giai đoạn (**Bước 1: Dò Map & Tìm Đường ➔ Bước 2: Chạy Tốc Độ Cao Từ Bộ Nhớ Cache**).

---

## 1. Cách Khởi Chạy Nhanh

- **Bản Web Interactive (Khuyên dùng):** Mở trực tiếp file [`index.html`](./index.html) trên trình duyệt hoặc nhấp đúp vào [`start_web.bat`](./start_web.bat).
- **Bản Desktop GUI (Python Tkinter):** Chạy lệnh `python micromouse_gui.py` hoặc nhấp đúp vào [`start_gui.bat`](./start_gui.bat).

---

## 2. Hai Chế Độ Hoạt Động (2 Modes)

### 🏆 Mode 1: Chế Độ Giả Lập Thi Đấu (Competition Mode — 2 Bước)
Mô phỏng quy trình thi đấu Micromouse thực tế gồm 2 bước rõ rệt:
1. **Bước 1 — Robot Dò Map & Tìm Đường Ngắn Nhất (`Exploration Phase`):**
   - Ban đầu Robot đứng tại ô xuất phát `(0, 0)` và chưa biết cấu trúc tường bên trong mê cung (các tường chưa quét hiển thị mờ dưới dạng sương mù).
   - Mỗi khi di chuyển, cảm biến trên Robot quét các bức tường xung quanh, cập nhật bản đồ bộ nhớ và tính lại ma trận khoảng cách Flood Fill.
   - Sau khi chạm đích (và tùy chọn dò thêm lượt về `(0, 0)` để mở rộng tầm nhìn), Robot trích xuất **Lộ trình ngắn nhất (Shortest Path)** từ `(0, 0)` đến Đích và **lưu toàn bộ chuỗi tọa độ + lệnh điều hướng vào Bộ Nhớ Cache (`Route Cache`)**.
2. **Bước 2 — Chạy Theo Lộ Trình Trong Cache (`Fast / Speed Run Phase`):**
   - Robot xuất phát lại từ `(0, 0)` và **đọc trực tiếp chuỗi lệnh từ Bộ Nhớ Cache** (`Tiến`, `Rẽ Phải`, `Rẽ Trái`...).
   - Không còn rẽ vào ngõ cụt và không cần tính toán lại Flood Fill, giúp Robot băng thẳng về Đích với số bước tối ưu và tốc độ cao nhất.

### 🧪 Mode 2: Chế Độ Test (Sandbox Mode)
- Cho phép thử nghiệm thuật toán từng bước (`Bước Đơn / Step`), chạy tự động, tính `Fill` tới Đích hoặc `Fill từ Start`.
- Hỗ trợ điều khiển thủ công bằng cụm phím D-Pad hoặc các phím mũi tên trên bàn phím (`W/A/S/D` hoặc `↑/↓/←/→`).
- Click chuột trực tiếp vào bất kỳ cạnh ô nào trên mê cung để **thêm hoặc xóa tường** theo thời gian thực.

---

## 3. Chức Năng Tạo Map Ngẫu Nhiên Chuẩn Micromouse

Hỗ trợ 4 chế độ sinh bản đồ tự động:
1. **Map Thi Đấu (Braided Maze - Đa lộ trình):** Khởi tạo bằng DFS Backtracker sau đó mở ngẫu nhiên các vách ngăn nội bộ để tạo ra nhiều tuyến đường ngắn/dài khác nhau tới phòng Đích $2 \times 2$ ở trung tâm (đúng chuẩn đề thi Micromouse quốc tế).
2. **Map Độc Đạo (Perfect DFS Maze):** Mê cung cây khung chỉ có duy nhất một đường dẫn tới Đích và rất nhiều nhánh cụt đánh lừa.
3. **Map Thông Thoáng (Open Maze):** Ít tường chắn, nhiều hành lang thẳng dài để kiểm nghiệm tốc độ.
4. **Map Trống (Empty Maze):** Chỉ có tường bao ngoài và vách xuất phát để người dùng tự thiết kế mê cung.

---

## 4. Phân Biệt 2 Loại Thuật Toán Flood Fill

| Tiêu chí | 1. Classic Flood Fill (Cổ điển) | 2. Modified / Turn-Weighted Flood Fill (Cải tiến) |
| :--- | :--- | :--- |
| **Cách tính chi phí** | Mỗi ô liền kề cộng đúng `+1` đơn vị ($D(u) = D(v) + 1$). | Đi thẳng cùng hướng cộng `+1`, nhưng nếu phải **rẽ góc 90°** thì bị cộng phạt thêm trọng số (ví dụ `+2`). |
| **Mục tiêu tối ưu** | Tìm đường đi qua **ít ô vuông nhất**. | Tìm đường đi có **ít khúc cua nhất (nhiều đoạn thẳng dài nhất)** để đạt **thời gian chạy thực tế nhanh nhất**. |
| **Đặc điểm đường đi** | Có thể chọn đường zích-zắc liên tục nếu số ô bằng hoặc ít hơn 1 ô so với đường thẳng. | Ưu tiên các hành lang thẳng dài vì ngoài đời thực Robot đi thẳng có thể tăng tốc cao, còn khi rẽ phải phanh giảm tốc. |
| **Cơ chế cập nhật bộ nhớ** | Chạy lan truyền sóng BFS từ các ô Đích ($D = 0$) ra toàn bộ các ô liên thông. | Cập nhật lan truyền có xét hướng đi vào mỗi ô (hoặc chỉ đẩy các ô bị thay đổi vào Queue động). |
