#include <Arduino.h>
#include <EEPROM.h>   // Thư viện có sẵn của STM32duino (tự động lưu vào Flash Sector cuối)

// Nhãn đánh dấu để biết trong Flash đã có bản đồ hợp lệ hay chưa
const uint32_t MA_HOP_LE = 0xABCD2026;

// Hai mảng bản đồ toàn cục 16x16 của xe khi đang chạy trong RAM:
// - tuongMeCung[hang][cot]: lưu 4 bức tường (Bit 0=Bắc, Bit 1=Đông, Bit 2=Nam, Bit 3=Tây)
// - oDaDiQua[hang][cot]   : 1 = xe đã trực tiếp đi vào và nhìn thấy, 0 = ô chưa từng đi tới
uint8_t tuongMeCung[16][16];
uint8_t oDaDiQua[16][16];

// Gom toàn bộ dữ liệu cần lưu thành 1 "hộp" (struct) để cất vào Flash chỉ bằng 1 lệnh
struct GoiDuLieuFlash {
    uint32_t nhanKiemTra;       // 4 bytes: luôn bằng 0xABCD2026 nếu đã lưu
    uint8_t  walls[16][16];     // 256 bytes: mảng tường
    uint8_t  visited[16][16];   // 256 bytes: mảng đánh dấu ô đã khám phá (chống đi xuyên tường!)
    uint32_t maBamChecksum;     // 4 bytes: tổng kiểm tra chống lỗi dữ liệu khi tắt nguồn đột ngột
};

// Hàm tính tổng kiểm tra (Checksum) bằng vòng lặp for 2 chiều cực dễ hiểu
uint32_t tinhChecksum(const GoiDuLieuFlash &goi) {
    uint32_t tong = goi.nhanKiemTra;
    for (int hang = 0; hang < 16; hang++) {
        for (int cot = 0; cot < 16; cot++) {
            tong = (tong * 31) + goi.walls[hang][cot];
            tong = (tong * 31) + goi.visited[hang][cot];
        }
    }
    return tong;
}

// ============================================================================
// 1. HÀM LƯU BẢN ĐỒ VÀO FLASH
// -> Gọi ngay khi xe vừa dừng yên trong Ô Đích (7,7)-(8,8) hoặc khi về lại (0,0)
// ============================================================================
void luuBanDoVaoFlash() {
    GoiDuLieuFlash goi;
    goi.nhanKiemTra = MA_HOP_LE;

    // Chép từng ô từ mảng RAM vào hộp dữ liệu
    for (int hang = 0; hang < 16; hang++) {
        for (int cot = 0; cot < 16; cot++) {
            goi.walls[hang][cot]   = tuongMeCung[hang][cot];
            goi.visited[hang][cot] = oDaDiQua[hang][cot];
        }
    }

    // Tính mã kiểm tra toàn vẹn
    goi.maBamChecksum = tinhChecksum(goi);

    // Lệnh thần thánh của Arduino STM32: tự động xóa Sector Flash và ghi toàn bộ struct vào địa chỉ 0!
    EEPROM.put(0, goi);

    Serial.println("[FLASH] Da luu thanh cong ban do 16x16 vao Flash STM32!");
}

// ============================================================================
// 2. HÀM ĐỌC BẢN ĐỒ TỪ FLASH RA RAM (Dùng ở đầu Lượt 2 - Speed Run)
// -> Trả về true nếu đọc thành công, false nếu Flash đang trống hoặc dữ liệu lỗi
// ============================================================================
bool docBanDoTuFlash() {
    GoiDuLieuFlash goiDocRa;

    // Đọc toàn bộ hộp dữ liệu từ địa chỉ 0 của Flash ra biến goiDocRa
    EEPROM.get(0, goiDocRa);

    // Kiểm tra 1: Nhãn có đúng là 0xABCD2026 không? (Nếu Flash mới tinh chưa lưu sẽ là 0xFFFFFFFF)
    if (goiDocRa.nhanKiemTra != MA_HOP_LE) {
        Serial.println("[FLASH] Chua co ban do trong Flash!");
        return false;
    }

    // Kiểm tra 2: Mã Checksum có khớp 100% không?
    if (goiDocRa.maBamChecksum != tinhChecksum(goiDocRa)) {
        Serial.println("[FLASH] Du lieu Flash bi loi Checksum!");
        return false;
    }

    // Dữ liệu chuẩn 100% -> Chép ngược từ hộp dữ liệu ra 2 mảng RAM để chạy Speed Run
    for (int hang = 0; hang < 16; hang++) {
        for (int cot = 0; cot < 16; cot++) {
            tuongMeCung[hang][cot] = goiDocRa.walls[hang][cot];
            oDaDiQua[hang][cot]    = goiDocRa.visited[hang][cot];
        }
    }

    Serial.println("[FLASH] Doc ban do Speed Run thanh cong!");
    return true;
}

// ============================================================================
// 3. HÀM XÓA TRẮNG BẢN ĐỒ CŨ (Gọi khi giữ nút CAL 2 giây trước trận đấu mới)
// ============================================================================
void xoaBanDoTrongFlash() {
    GoiDuLieuFlash goiRong = {0}; // Đặt nhanKiemTra = 0 (khác 0xABCD2026)
    EEPROM.put(0, goiRong);
    Serial.println("[FLASH] Da xoa trang ban do cu!");
}