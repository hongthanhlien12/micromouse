#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// KIỂM TRA CẤU HÌNH BOARD TRÊN ARDUINO IDE:
// Vào menu Tools -> Board part number -> BẮT BUỘC chọn "BlackPill F411CE"
// ============================================================================
#if !defined(ARDUINO_BLACKPILL_F411CE)
  #error "Ban chua chon BlackPill F411CE! Hay vao menu Tools -> Board part number -> chon BlackPill F411CE"
#endif

// ============================================================================
// 1. ĐỊNH NGHĨA TOÀN BỘ CHÂN CẮM (KHỚP 100% BẢNG NỐI DÂY BREADBOARD & PCB)
// ============================================================================

// --- A. 4 Mắt Thu Hồng Ngoại (Phototransistor PT334-6B -> Chân Analog) ---
#define PIN_IR_THU_TRUOC_TRAI   PA0   // Cảm biến Trước Trái (FL)
#define PIN_IR_THU_CHEO_TRAI    PA1   // Cảm biến Chéo Trái  (DL)
#define PIN_IR_THU_CHEO_PHAI    PA2   // Cảm biến Chéo Phải  (DR)
#define PIN_IR_THU_TRUOC_PHAI   PA3   // Cảm biến Trước Phải (FR)

// --- B. 2 Nhánh Phát Hồng Ngoại (Điều khiển MOSFET Q1, Q2) ---
#define PIN_IR_PHAT_TRUOC       PB12  // Bật/tắt 2 bóng phát Trước (FL + FR)
#define PIN_IR_PHAT_CHEO        PB13  // Bật/tắt 2 bóng phát Chéo  (DL + DR)

// --- C. Cầu H Kép TB6612FNG (Điều khiển 2 Motor N20 6V 500RPM) ---
#define PIN_MOTOR_TRAI_PWM      PA8   // Chân PWMA
#define PIN_MOTOR_TRAI_IN1      PB14  // Chân AIN1
#define PIN_MOTOR_TRAI_IN2      PB15  // Chân AIN2

#define PIN_MOTOR_PHAI_PWM      PA9   // Chân PWMB
#define PIN_MOTOR_PHAI_IN1      PA10  // Chân BIN1
#define PIN_MOTOR_PHAI_IN2      PA12  // Chân BIN2

// --- D. 2 Bộ Đếm Xung Hall Encoder ---
#define PIN_ENC_TRAI_A          PB4   // Kênh A Encoder Trái
#define PIN_ENC_TRAI_B          PB5   // Kênh B Encoder Trái
#define PIN_ENC_PHAI_A          PB6   // Kênh A Encoder Phải
#define PIN_ENC_PHAI_B          PB7   // Kênh B Encoder Phải

// --- E. Đo Pin 2S, Biến Trở RV2, Công Tắc DIP 4-Bit, Nút Nhấn & Còi Buzzer ---
#define PIN_DO_PIN_2S           PB0   // Cầu phân áp 22k - 10k đo áp Pin 2S
#define PIN_BIEN_TRO_RV2        PB1   // Biến trở 10k chỉnh tốc độ Speed Run
#define PIN_DIP_1               PA4   // Công tắc gạt DIP bit 1
#define PIN_DIP_2               PA5   // Công tắc gạt DIP bit 2
#define PIN_DIP_3               PA6   // Công tắc gạt DIP bit 3
#define PIN_DIP_4               PA7   // Công tắc gạt DIP bit 4
#define PIN_NUT_START           PB10  // Nút bấm xuất phát (START)
#define PIN_NUT_CAL             PA15  // Nút bấm hiệu chuẩn / giữ 2s xóa Flash (CAL)
#define PIN_BUZZER              PB3   // Còi báo hiệu 5V (qua transistor Q3)

// ============================================================================
// 2. KHỞI TẠO MÀN HÌNH OLED I2C (Dùng chung chân PB8=SCL, PB9=SDA với MPU6050!)
// ============================================================================
#define OLED_RONG               128
#define OLED_CAO                64    // Đổi thành 32 nếu bạn dùng màn OLED 0.91" (128x32)
#define DIA_CHI_OLED            0x3C  // Địa chỉ I2C chuẩn của màn OLED SSD1306

Adafruit_SSD1306 oled(OLED_RONG, OLED_CAO, &Wire, -1);
bool coManHinhOLED = false;           // Tự động nhận biết đang cắm hay rút OLED
String thongBaoTrangThai = "READY!";  // Dòng chữ trạng thái cuối màn hình OLED

// ============================================================================
// 3. CẤU TRÚC LƯU BẢN ĐỒ MÊ CUNG VÀO FLASH (Qua thư viện EEPROM.h)
// ============================================================================
const uint32_t MA_HOP_LE = 0xABCD2026;

uint8_t tuongMeCung[16][16]; // Bit 0=Bắc, Bit 1=Đông, Bit 2=Nam, Bit 3=Tây
uint8_t oDaDiQua[16][16];    // 1 = đã đi qua, 0 = chưa đi qua (chặn ở Speed Run!)
bool daCoBanDoFlash = false;

struct GoiDuLieuFlash {
    uint32_t nhanKiemTra;
    uint8_t  walls[16][16];
    uint8_t  visited[16][16];
    uint32_t maBamChecksum;
};

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

void luuBanDoVaoFlash() {
    GoiDuLieuFlash goi;
    goi.nhanKiemTra = MA_HOP_LE;
    for (int hang = 0; hang < 16; hang++) {
        for (int cot = 0; cot < 16; cot++) {
            goi.walls[hang][cot]   = tuongMeCung[hang][cot];
            goi.visited[hang][cot] = oDaDiQua[hang][cot];
        }
    }
    goi.maBamChecksum = tinhChecksum(goi);

    EEPROM.put(0, goi); // Tự động xóa Sector 7 và lưu toàn bộ struct vào Flash!
    daCoBanDoFlash = true;
    thongBaoTrangThai = "FLASH SAVED!";
}

bool docBanDoTuFlash() {
    GoiDuLieuFlash goiDocRa;
    EEPROM.get(0, goiDocRa);

    if (goiDocRa.nhanKiemTra != MA_HOP_LE) return false;
    if (goiDocRa.maBamChecksum != tinhChecksum(goiDocRa)) return false;

    for (int hang = 0; hang < 16; hang++) {
        for (int cot = 0; cot < 16; cot++) {
            tuongMeCung[hang][cot] = goiDocRa.walls[hang][cot];
            oDaDiQua[hang][cot]    = goiDocRa.visited[hang][cot];
        }
    }
    daCoBanDoFlash = true;
    thongBaoTrangThai = "MAP LOADED OK";
    return true;
}

void xoaBanDoTrongFlash() {
    GoiDuLieuFlash goiRong = {0};
    EEPROM.put(0, goiRong);
    daCoBanDoFlash = false;
    thongBaoTrangThai = "MAP CLEARED!";
}

// ============================================================================
// 4. BIẾN TRẠNG THÁI CẢM BIẾN, ENCODER & GYRO MPU6050
// ============================================================================
int irFL = 0, irDL = 0, irDR = 0, irFR = 0;
volatile long xungEncTrai = 0;
volatile long xungEncPhai = 0;

float gocYaw = 0.0f;
float saiSoTinhGyroZ = 0.0f;
unsigned long mocThoiGianGyro = 0;
bool dangChayMeCung = false; // Khi = true thì TẮT vẽ OLED để không làm trễ PID!

void ngatEncTraiA() {
    if (digitalRead(PIN_ENC_TRAI_A) == digitalRead(PIN_ENC_TRAI_B)) xungEncTrai++;
    else xungEncTrai--;
}

void ngatEncPhaiA() {
    if (digitalRead(PIN_ENC_PHAI_A) == digitalRead(PIN_ENC_PHAI_B)) xungEncPhai++;
    else xungEncPhai--;
}

// Đọc 4 mắt IR có trừ nhiễu ánh sáng đèn hội trường (Ambient Subtraction)
void doc4MatIR() {
    // 1. Nhóm Trước (FL + FR)
    int nenFL = analogRead(PIN_IR_THU_TRUOC_TRAI);
    int nenFR = analogRead(PIN_IR_THU_TRUOC_PHAI);
    digitalWrite(PIN_IR_PHAT_TRUOC, HIGH);
    delayMicroseconds(60);
    int sangFL = analogRead(PIN_IR_THU_TRUOC_TRAI);
    int sangFR = analogRead(PIN_IR_THU_TRUOC_PHAI);
    digitalWrite(PIN_IR_PHAT_TRUOC, LOW);
    irFL = max(0, sangFL - nenFL);
    irFR = max(0, sangFR - nenFR);

    // 2. Nhóm Chéo (DL + DR)
    int nenDL = analogRead(PIN_IR_THU_CHEO_TRAI);
    int nenDR = analogRead(PIN_IR_THU_CHEO_PHAI);
    digitalWrite(PIN_IR_PHAT_CHEO, HIGH);
    delayMicroseconds(60);
    int sangDL = analogRead(PIN_IR_THU_CHEO_TRAI);
    int sangDR = analogRead(PIN_IR_THU_CHEO_PHAI);
    digitalWrite(PIN_IR_PHAT_CHEO, LOW);
    irDL = max(0, sangDL - nenDL);
    irDR = max(0, sangDR - nenDR);
}

// Điều khiển 2 Motor qua TB6612FNG (-255 đến +255, 0 = phanh ngắn mạch dừng ngay)
void dieuKhienMotor(int tocDoTrai, int tocDoPhai) {
    tocDoTrai = constrain(tocDoTrai, -255, 255);
    tocDoPhai = constrain(tocDoPhai, -255, 255);

    if (tocDoTrai > 0) {
        digitalWrite(PIN_MOTOR_TRAI_IN1, HIGH); digitalWrite(PIN_MOTOR_TRAI_IN2, LOW);
        analogWrite(PIN_MOTOR_TRAI_PWM, tocDoTrai);
    } else if (tocDoTrai < 0) {
        digitalWrite(PIN_MOTOR_TRAI_IN1, LOW);  digitalWrite(PIN_MOTOR_TRAI_IN2, HIGH);
        analogWrite(PIN_MOTOR_TRAI_PWM, -tocDoTrai);
    } else {
        digitalWrite(PIN_MOTOR_TRAI_IN1, HIGH); digitalWrite(PIN_MOTOR_TRAI_IN2, HIGH);
        analogWrite(PIN_MOTOR_TRAI_PWM, 0);
    }

    if (tocDoPhai > 0) {
        digitalWrite(PIN_MOTOR_PHAI_IN1, HIGH); digitalWrite(PIN_MOTOR_PHAI_IN2, LOW);
        analogWrite(PIN_MOTOR_PHAI_PWM, tocDoPhai);
    } else if (tocDoPhai < 0) {
        digitalWrite(PIN_MOTOR_PHAI_IN1, LOW);  digitalWrite(PIN_MOTOR_PHAI_IN2, HIGH);
        analogWrite(PIN_MOTOR_PHAI_PWM, -tocDoPhai);
    } else {
        digitalWrite(PIN_MOTOR_PHAI_IN1, HIGH); digitalWrite(PIN_MOTOR_PHAI_IN2, HIGH);
        analogWrite(PIN_MOTOR_PHAI_PWM, 0);
    }
}

// Giao tiếp Gyro MPU6050 (địa chỉ 0x68)
const uint8_t DIA_CHI_MPU6050 = 0x68;

void khoiTaoMPU6050() {
    Wire.beginTransmission(DIA_CHI_MPU6050);
    Wire.write(0x6B); Wire.write(0x00); // Đánh thức MPU6050
    Wire.endTransmission();

    Wire.beginTransmission(DIA_CHI_MPU6050);
    Wire.write(0x1B); Wire.write(0x08); // Thang đo ±500 độ/giây (65.5 LSB/độ/s)
    Wire.endTransmission();

    // Lấy 500 mẫu lúc đứng yên để tìm độ trôi tĩnh
    long tong = 0;
    for (int i = 0; i < 500; i++) {
        Wire.beginTransmission(DIA_CHI_MPU6050);
        Wire.write(0x47);
        Wire.endTransmission(false);
        Wire.requestFrom(DIA_CHI_MPU6050, (uint8_t)2);
        int16_t rawZ = (Wire.read() << 8) | Wire.read();
        tong += rawZ;
        delay(2);
    }
    saiSoTinhGyroZ = tong / 500.0f;
    mocThoiGianGyro = micros();
}

void capNhatGocQuay() {
    unsigned long bayGio = micros();
    float dt = (bayGio - mocThoiGianGyro) / 1000000.0f;
    mocThoiGianGyro = bayGio;

    Wire.beginTransmission(DIA_CHI_MPU6050);
    Wire.write(0x47);
    Wire.endTransmission(false);
    Wire.requestFrom(DIA_CHI_MPU6050, (uint8_t)2);
    int16_t rawZ = (Wire.read() << 8) | Wire.read();

    float tocDoGocZ = (rawZ - saiSoTinhGyroZ) / 65.5f;
    if (abs(tocDoGocZ) > 0.3f) {
        gocYaw += tocDoGocZ * dt;
    }
}

float docDienApPin2S() {
    int adc = analogRead(PIN_DO_PIN_2S);
    return (adc / 4095.0f) * 3.3f * 3.2f; // Cầu phân áp (22k + 10k)/10k = 3.2
}

void keuBip(int soLan, int ms) {
    for (int i = 0; i < soLan; i++) {
        digitalWrite(PIN_BUZZER, HIGH); delay(ms);
        digitalWrite(PIN_BUZZER, LOW);  delay(ms);
    }
}

// ============================================================================
// 5. HÀM HIỂN THỊ BẢNG ĐIỀU KHIỂN LÊN MÀN HÌNH OLED I2C (Chỉ vẽ khi xe đứng yên!)
// ============================================================================
void capNhatManHinhOLED() {
    if (!coManHinhOLED || dangChayMeCung) return;

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);

    // Dòng 1: Điện áp Pin 2S + Góc Yaw hiện tại
    oled.setCursor(0, 0);
    oled.print("Pin:"); oled.print(docDienApPin2S(), 1); oled.print("V ");
    oled.print("Yaw:"); oled.print(gocYaw, 1); oled.print("d");

    // Dòng 2 & 3: Giá trị ADC 12-bit của 4 mắt IR (FL, DL, DR, FR)
    oled.setCursor(0, 13);
    oled.print("FL:"); oled.print(irFL);
    oled.setCursor(64, 13);
    oled.print("FR:"); oled.print(irFR);

    oled.setCursor(0, 25);
    oled.print("DL:"); oled.print(irDL);
    oled.setCursor(64, 25);
    oled.print("DR:"); oled.print(irDR);

    // Dòng 4: Xung Hall Encoder Trái & Phải
    oled.setCursor(0, 38);
    oled.print("EncL:"); oled.print(xungEncTrai);
    oled.setCursor(64, 38);
    oled.print("R:");    oled.print(xungEncPhai);

    // Dòng 5: Trạng thái bản đồ Flash & Nút bấm
    oled.setCursor(0, 52);
    oled.print(daCoBanDoFlash ? "[MAP]" : "[---]");
    oled.print(" ");
    oled.print(thongBaoTrangThai);

    oled.display();
}

// ============================================================================
// 6. THUẬT TOÁN FLOOD FILL & DỪNG NGAY LẬP TỨC KHI TỚI ĐÍCH (7,7)-(8,8)
//    (Không cần chạy lượt tự tìm đường quay về (0,0)!)
// ============================================================================
#define NGUONG_TUONG_TRUOC      300   // Ngưỡng ADC nhận biết có tường phía trước
#define NGUONG_TUONG_HAI_BEN    400   // Ngưỡng ADC nhận biết có tường Trái / Phải
#define XUNG_MOI_O_90MM         573   // Số xung Encoder cho 1 ô 90mm (bánh Ø21mm, 420 xung/vòng)

uint8_t khoangCach[16][16];           // Bảng khoảng cách Flood Fill tới Đích (7,7)-(8,8)
int hangHienTai = 0, cotHienTai = 0;  // Tọa độ ô hiện tại của xe (Bắt đầu từ (0,0))
int huongHienTai = 0;                 // 0 = Bắc (+hàng), 1 = Đông (+cột), 2 = Nam (-hàng), 3 = Tây (-cột)

// Kiểm tra xem xe đã vào 1 trong 4 ô Đích trung tâm (7,7), (7,8), (8,7), (8,8) chưa
bool laODich(int hang, int cot) {
    return (hang == 7 || hang == 8) && (cot == 7 || cot == 8);
}

// Ghi nhận tường ở ô hiện tại từ 4 mắt IR vào mảng tuongMeCung[16][16]
void capNhatTuongTaiOHienTai() {
    oDaDiQua[hangHienTai][cotHienTai] = 1;

    bool coTuongTruoc = ((irFL + irFR) / 2) > NGUONG_TUONG_TRUOC;
    bool coTuongTrai  = (irDL > NGUONG_TUONG_HAI_BEN);
    bool coTuongPhai  = (irDR > NGUONG_TUONG_HAI_BEN);

    const int dHang[4] = { +1,  0, -1,  0 };
    const int dCot[4]  = {  0, +1,  0, -1 };

    // Đánh dấu tường theo hướng tuyệt đối (0=Bắc, 1=Đông, 2=Nam, 3=Tây)
    int cacHuongCanKiemTra[3] = {
        huongHienTai,                 // Phía trước mặt xe
        (huongHienTai + 3) % 4,       // Bên trái xe
        (huongHienTai + 1) % 4        // Bên phải xe
    };
    bool ketQuaCamBien[3] = { coTuongTruoc, coTuongTrai, coTuongPhai };

    for (int i = 0; i < 3; i++) {
        if (ketQuaCamBien[i]) {
            int h = cacHuongCanKiemTra[i];
            tuongMeCung[hangHienTai][cotHienTai] |= (1 << h);
            int hangKe = hangHienTai + dHang[h];
            int cotKe  = cotHienTai  + dCot[h];
            if (hangKe >= 0 && hangKe < 16 && cotKe >= 0 && cotKe < 16) {
                int huongNguoc = (h + 2) % 4;
                tuongMeCung[hangKe][cotKe] |= (1 << huongNguoc);
            }
        }
    }
}

// Tính bảng khoảng cách Flood Fill từ 4 ô Đích (7,7)-(8,8) lan ra toàn sân 16x16
void tinhFloodFillDenDich(bool chiDiODaQua) {
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            khoangCach[r][c] = 255;
        }
    }

    uint8_t hangDoiR[256], hangDoiC[256];
    int dau = 0, cuoi = 0;

    // Đặt 4 ô Đích trung tâm có khoảng cách = 0
    const int oDich[4][2] = { {7, 7}, {7, 8}, {8, 7}, {8, 8} };
    for (int i = 0; i < 4; i++) {
        int gr = oDich[i][0], gc = oDich[i][1];
        if (!chiDiODaQua || oDaDiQua[gr][gc] == 1) {
            khoangCach[gr][gc] = 0;
            hangDoiR[cuoi] = gr;
            hangDoiC[cuoi] = gc;
            cuoi++;
        }
    }

    const int dHang[4] = { +1,  0, -1,  0 };
    const int dCot[4]  = {  0, +1,  0, -1 };

    while (dau < cuoi) {
        int r = hangDoiR[dau];
        int c = hangDoiC[dau];
        dau++;
        uint8_t kcHienTai = khoangCach[r][c];

        for (int h = 0; h < 4; h++) {
            if (tuongMeCung[r][c] & (1 << h)) continue; // Có tường chắn -> bỏ qua
            int nr = r + dHang[h];
            int nc = c + dCot[h];
            if (nr < 0 || nr >= 16 || nc < 0 || nc >= 16) continue;
            if (chiDiODaQua && oDaDiQua[nr][nc] == 0) continue; // Speed Run chỉ đi ô đã xác thực!
            if (khoangCach[nr][nc] == 255) {
                khoangCach[nr][nc] = kcHienTai + 1;
                hangDoiR[cuoi] = nr;
                hangDoiC[cuoi] = nc;
                cuoi++;
            }
        }
    }
}

// Quay xe tại chỗ bằng Gyro MPU6050 (soDoQuay: +90 = rẽ trái, -90 = rẽ phải, 180 = quay đầu)
void quayXeTheoGyro(float soDoQuay) {
    float gocMucTieu = gocYaw + soDoQuay;
    unsigned long batDau = millis();
    while (millis() - batDau < 1200) { // Timeout an toàn 1.2 giây
        capNhatGocQuay();
        float saiSoGoc = gocMucTieu - gocYaw;
        if (abs(saiSoGoc) < 1.5f) break; // Đã quay đủ góc chuẩn!
        int xungQuay = constrain((int)(saiSoGoc * 2.2f), -120, 120);
        if (xungQuay > 0 && xungQuay < 55) xungQuay = 55;
        if (xungQuay < 0 && xungQuay > -55) xungQuay = -55;
        dieuKhienMotor(-xungQuay, +xungQuay);
    }
    dieuKhienMotor(0, 0);
    delay(60);
}

// Tiến thẳng 1 ô mê cung (90 mm) kết hợp giữ thẳng bằng Gyro + cảm biến tường
void diThangMotO90mm(int tocDoCoBan) {
    long mocEncDau = (abs(xungEncTrai) + abs(xungEncPhai)) / 2;
    float gocGiuThang = gocYaw;
    unsigned long batDau = millis();

    while (millis() - batDau < 1500) {
        capNhatGocQuay();
        doc4MatIR();

        // Nếu bất ngờ sát tường phía trước thì dừng ngay để không chạm tường
        if (((irFL + irFR) / 2) > 1800) break;

        long quangDuongXung = ((abs(xungEncTrai) + abs(xungEncPhai)) / 2) - mocEncDau;
        if (quangDuongXung >= XUNG_MOI_O_90MM) break;

        int buLaiGyro = (int)((gocGiuThang - gocYaw) * 3.0f);
        dieuKhienMotor(tocDoCoBan - buLaiGyro, tocDoCoBan + buLaiGyro);
    }
    dieuKhienMotor(0, 0);
}

// HÀM CHÍNH: Chạy từ (0,0) tới Đích (7,7)-(8,8) -> LẬP TỨC DỪNG HẲN & LƯU FLASH!
void chayDenDichVaDungNgay(bool laSpeedRun) {
    hangHienTai = 0;
    cotHienTai  = 0;
    huongHienTai = 0; // Đặt mũi xe hướng Bắc (+hàng) tại ô (0,0)
    gocYaw = 0.0f;
    dangChayMeCung = true; // Tắt vẽ OLED khi đang chạy để vòng lặp PID mượt 100%

    int tocDoChay = laSpeedRun ? 165 : 110;
    const int dHang[4] = { +1,  0, -1,  0 };
    const int dCot[4]  = {  0, +1,  0, -1 };

    while (true) {
        capNhatGocQuay();
        doc4MatIR();
        capNhatTuongTaiOHienTai();

        // ====================================================================
        // KIỂM TRA ĐÍCH: VỪA VÀO Ô ĐÍCH (7,7)..(8,8) LÀ DỪNG NGAY LẬP TỨC!
        // (Không chạy lượt tự tìm đường quay về (0,0))
        // ====================================================================
        if (laODich(hangHienTai, cotHienTai)) {
            dieuKhienMotor(0, 0);          // 1. Phanh dừng cứng ngay tại ô Đích!
            dangChayMeCung = false;        // 2. Bật lại màn hình OLED
            if (!laSpeedRun) {
                luuBanDoVaoFlash();        // 3. Lưu bản đồ vừa dò vào Sector 7 Flash!
                thongBaoTrangThai = "GOAL! SAVED FLASH";
            } else {
                thongBaoTrangThai = "SPEED RUN DONE!";
            }
            capNhatManHinhOLED();          // 4. Hiển thị kết quả lên OLED
            keuBip(3, 150);                // 5. Kêu 3 tiếng Bíp báo hiệu để bạn nhặt xe về (0,0)
            return;                        // 6. Thoát hàm ngay, xe đứng yên tại Đích!
        }

        // Nếu chưa tới Đích -> Tính Flood Fill để chọn ô kế tiếp gần Đích nhất
        tinhFloodFillDenDich(laSpeedRun);

        int huongTotNhat = -1;
        uint8_t kcNhoNhat = 255;

        for (int h = 0; h < 4; h++) {
            if (tuongMeCung[hangHienTai][cotHienTai] & (1 << h)) continue;
            int nr = hangHienTai + dHang[h];
            int nc = cotHienTai  + dCot[h];
            if (nr < 0 || nr >= 16 || nc < 0 || nc >= 16) continue;
            if (laSpeedRun && oDaDiQua[nr][nc] == 0) continue;

            if (khoangCach[nr][nc] < kcNhoNhat ||
               (khoangCach[nr][nc] == kcNhoNhat && h == huongHienTai)) {
                kcNhoNhat = khoangCach[nr][nc];
                huongTotNhat = h;
            }
        }

        // Nếu không tìm được đường hợp lệ hoặc bấm nút CAL để hủy khẩn cấp -> Dừng xe
        if (huongTotNhat == -1 || kcNhoNhat >= 255 || digitalRead(PIN_NUT_CAL) == LOW) {
            dieuKhienMotor(0, 0);
            dangChayMeCung = false;
            thongBaoTrangThai = "STOPPED SAFE!";
            capNhatManHinhOLED();
            keuBip(2, 200);
            return;
        }

        // Quay xe sang hướng huongTotNhat rồi tiến 1 ô (90 mm)
        int doLechHuong = (huongTotNhat - huongHienTai + 4) % 4;
        if (doLechHuong == 1)      quayXeTheoGyro(-90.0f);  // Rẽ phải 90 độ
        else if (doLechHuong == 2) quayXeTheoGyro(180.0f);  // Quay đầu 180 độ
        else if (doLechHuong == 3) quayXeTheoGyro(+90.0f);  // Rẽ trái 90 độ

        huongHienTai = huongTotNhat;
        diThangMotO90mm(tocDoChay);
        hangHienTai += dHang[huongHienTai];
        cotHienTai  += dCot[huongHienTai];
    }
}

// ============================================================================
// 7. SETUP() & LOOP()
// ============================================================================
void setup() {
    Serial.begin(115200);
    analogReadResolution(12);     // ADC 12-bit (0 -> 4095)
    analogWriteFrequency(20000);  // Xung PWM 20 kHz êm ái cho TB6612

    pinMode(PIN_IR_PHAT_TRUOC, OUTPUT); digitalWrite(PIN_IR_PHAT_TRUOC, LOW);
    pinMode(PIN_IR_PHAT_CHEO,  OUTPUT); digitalWrite(PIN_IR_PHAT_CHEO,  LOW);

    pinMode(PIN_MOTOR_TRAI_PWM, OUTPUT);
    pinMode(PIN_MOTOR_TRAI_IN1, OUTPUT);
    pinMode(PIN_MOTOR_TRAI_IN2, OUTPUT);
    pinMode(PIN_MOTOR_PHAI_PWM, OUTPUT);
    pinMode(PIN_MOTOR_PHAI_IN1, OUTPUT);
    pinMode(PIN_MOTOR_PHAI_IN2, OUTPUT);
    dieuKhienMotor(0, 0);

    pinMode(PIN_BUZZER, OUTPUT);

    pinMode(PIN_DIP_1,     INPUT_PULLUP);
    pinMode(PIN_DIP_2,     INPUT_PULLUP);
    pinMode(PIN_DIP_3,     INPUT_PULLUP);
    pinMode(PIN_DIP_4,     INPUT_PULLUP);
    pinMode(PIN_NUT_START, INPUT_PULLUP);
    pinMode(PIN_NUT_CAL,   INPUT_PULLUP);

    pinMode(PIN_ENC_TRAI_A, INPUT_PULLUP);
    pinMode(PIN_ENC_TRAI_B, INPUT_PULLUP);
    pinMode(PIN_ENC_PHAI_A, INPUT_PULLUP);
    pinMode(PIN_ENC_PHAI_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_TRAI_A), ngatEncTraiA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_PHAI_A), ngatEncPhaiA, CHANGE);

    // Khởi tạo bus I2C1 trên chân PB8 (SCL) và PB9 (SDA) cho cả MPU6050 và OLED
    Wire.setSDA(PB9);
    Wire.setSCL(PB8);
    Wire.begin();
    Wire.setClock(400000);

    // Kiểm tra và khởi động màn hình OLED (địa chỉ 0x3C)
    if (oled.begin(SSD1306_SWITCHCAPVCC, DIA_CHI_OLED)) {
        coManHinhOLED = true;
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(10, 24);
        oled.println("CALIBRATING GYRO...");
        oled.display();
    }

    // Đo bù trôi Gyro MPU6050 (giữ xe đứng im 1 giây lúc mới bật nguồn)
    khoiTaoMPU6050();

    // Khởi tạo tường bao quanh biên sân 16x16 và kiểm tra bản đồ trong Flash
    for (int i = 0; i < 16; i++) {
        tuongMeCung[15][i] |= (1 << 0); // Biên Bắc
        tuongMeCung[i][15] |= (1 << 1); // Biên Đông
        tuongMeCung[0][i]  |= (1 << 2); // Biên Nam
        tuongMeCung[i][0]  |= (1 << 3); // Biên Tây
    }
    docBanDoTuFlash();
    keuBip(2, 80);
}

void loop() {
    capNhatGocQuay();
    doc4MatIR();

    // 1. Nếu giữ nút CAL (PA15):
    //    - Bấm nhả nhanh (< 1 giây): Đặt lại góc Yaw = 0 và xung Encoder = 0
    //    - Giữ lì đủ 2 giây: Xóa trắng bản đồ trong Flash!
    if (digitalRead(PIN_NUT_CAL) == LOW) {
        unsigned long lucBam = millis();
        while (digitalRead(PIN_NUT_CAL) == LOW) {
            if (millis() - lucBam > 2000) {
                xoaBanDoTrongFlash();
                keuBip(3, 100);
                break;
            }
        }
        if (millis() - lucBam <= 2000) {
            gocYaw = 0.0f;
            xungEncTrai = 0;
            xungEncPhai = 0;
            thongBaoTrangThai = "ZEROED YAW/ENC!";
            keuBip(1, 60);
        }
        delay(200);
    }

    // 2. Nếu bấm nút START (PB10):
    //    - Gạt DIP 1 OFF (mặc định): Chạy Lượt 1 (Dò đường tới Đích -> DỪNG NGAY & Lưu Flash!)
    //    - Gạt DIP 1 ON  (khi đã có Map): Chạy Lượt 2 (Speed Run tới Đích -> DỪNG NGAY!)
    if (digitalRead(PIN_NUT_START) == LOW) {
        bool chonSpeedRun = (digitalRead(PIN_DIP_1) == LOW) && daCoBanDoFlash;
        thongBaoTrangThai = chonSpeedRun ? "FAST RUN..." : "SEARCHING...";
        capNhatManHinhOLED();
        keuBip(1, 100);
        delay(600); // Chờ bạn rút tay khỏi nút bấm trước khi xe xuất phát

        chayDenDichVaDungNgay(chonSpeedRun);
    }

    // 3. Cập nhật màn hình OLED mỗi 150 ms khi xe đang đứng yên
    static unsigned long lanCapNhatCu = 0;
    if (millis() - lanCapNhatCu >= 150) {
        lanCapNhatCu = millis();
        capNhatManHinhOLED();
    }
}
