#include <Arduino.h>

#if !defined(ARDUINO_BLACKPILL_F411CE)
  #error "Ban chua chon BlackPill F411CE! Hay vao menu Tools -> Board part number -> chon BlackPill F411CE"
#endif

// Chân điều khiển 2 nhánh MOSFET phát hồng ngoại
#define PIN_IR_PHAT_TRUOC       PB12  // Bật/tắt 2 đèn IR Trước (FL + FR)
#define PIN_IR_PHAT_CHEO        PB13  // Bật/tắt 2 đèn IR Chéo  (DL + DR)

// Chân đọc 4 bóng thu Phototransistor PT334-6B (khớp với bảng nối dây Breadboard & PCB)
#define PIN_IR_THU_TRUOC_TRAI   PA0   // Mắt Trước Trái (FL)
#define PIN_IR_THU_CHEO_TRAI    PA1   // Mắt Chéo Trái  (DL)
#define PIN_IR_THU_CHEO_PHAI    PA2   // Mắt Chéo Phải  (DR)
#define PIN_IR_THU_TRUOC_PHAI   PA3   // Mắt Trước Phải (FR)

// 4 biến lưu độ mạnh phản xạ của tường (từ 0 đến 4095)
int s_fl = 0, s_dl = 0, s_dr = 0, s_fr = 0;

void doc4MatIR() {
    // --- 1. Nhóm 2 mắt TRƯỚC (FL và FR) ---
    // Bước 1a: Đọc ánh sáng phòng khi đèn IR đang TẮT
    int fl_off = analogRead(PIN_IR_THU_TRUOC_TRAI);
    int fr_off = analogRead(PIN_IR_THU_TRUOC_PHAI);

    // Bước 1b: Bật đèn IR Trước, chờ 60 micro-giây cho PT334-6B ổn định rồi đọc lại
    digitalWrite(PIN_IR_PHAT_TRUOC, HIGH);
    delayMicroseconds(60);
    int fl_on = analogRead(PIN_IR_THU_TRUOC_TRAI);
    int fr_on = analogRead(PIN_IR_THU_TRUOC_PHAI);
    digitalWrite(PIN_IR_PHAT_TRUOC, LOW); // Tắt đèn ngay để bóng phát luôn mát!

    // Bước 1c: Lấy (Khi Bật - Khi Tắt) để loại bỏ hoàn toàn ánh sáng đèn hội trường
    s_fl = max(0, fl_on - fl_off);
    s_fr = max(0, fr_on - fr_off);

    // --- 2. Nhóm 2 mắt CHÉO (DL và DR) ---
    int dl_off = analogRead(PIN_IR_THU_CHEO_TRAI);
    int dr_off = analogRead(PIN_IR_THU_CHEO_PHAI);

    digitalWrite(PIN_IR_PHAT_CHEO, HIGH);
    delayMicroseconds(60);
    int dl_on = analogRead(PIN_IR_THU_CHEO_TRAI);
    int dr_on = analogRead(PIN_IR_THU_CHEO_PHAI);
    digitalWrite(PIN_IR_PHAT_CHEO, LOW);

    s_dl = max(0, dl_on - dl_off);
    s_dr = max(0, dr_on - dr_off);
}

void setup() {
    Serial.begin(115200);
    analogReadResolution(12); // Đọc ADC độ phân giải 12-bit (0 -> 4095) của STM32F411

    pinMode(PIN_IR_PHAT_TRUOC, OUTPUT);
    pinMode(PIN_IR_PHAT_CHEO,  OUTPUT);
    digitalWrite(PIN_IR_PHAT_TRUOC, LOW);
    digitalWrite(PIN_IR_PHAT_CHEO,  LOW);
}

void loop() {
    doc4MatIR();

    // In ra Serial Monitor (hoặc Serial Plotter) mỗi 100 ms để xem số đo thực tế
    Serial.print("FL:"); Serial.print(s_fl);
    Serial.print("\tDL:"); Serial.print(s_dl);
    Serial.print("\tDR:"); Serial.print(s_dr);
    Serial.print("\tFR:"); Serial.println(s_fr);
    delay(100);
}
