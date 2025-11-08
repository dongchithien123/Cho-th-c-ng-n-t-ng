// ================================================
//  CHƯƠNG TRÌNH: HỆ THỐNG CHO ĂN TỰ ĐỘNG CHO THÚ CƯNG
//  - Phát hiện vật thể (mèo) bằng cảm biến HC-SR04
//  - Điều khiển servo mở khay thức ăn
//  - Tự động khóa 30 phút sau 5 lần cho ăn liên tục
//  - Tự động reset đếm sau 15 phút nếu chưa bị khóa
//  - Hiển thị trạng thái lên LCD và ứng dụng Blynk
// ================================================


// ====== 1️⃣ Khai báo thông tin Blynk Template ======
#define BLYNK_TEMPLATE_ID "TMPL6wqkjqeaS"     // Mã định danh template trên Blynk
#define BLYNK_TEMPLATE_NAME "Pet Feeder"       // Tên template hiển thị trên web / app


// ====== 2️⃣ Nạp các thư viện cần thiết ======
#include <ESP8266WiFi.h>          // Kết nối WiFi cho NodeMCU
#include <BlynkSimpleEsp8266.h>   // Giao tiếp với nền tảng Blynk
#include <Wire.h>                 // Giao tiếp I2C (RTC & LCD)
#include <LiquidCrystal_I2C.h>    // Điều khiển LCD 1602 qua I2C
#include <RTClib.h>               // Thư viện thời gian thực DS3231
#include <Servo.h>                // Thư viện điều khiển Servo SG90


// ====== 3️⃣ Thông tin WiFi & Tài khoản Blynk ======
char auth[] = "p0UBLN1tKPW5ZTqT3grYmVrtqKHnJgui"; // Token thiết bị trên Blynk Console
char ssid[] = "mt";                               // Tên mạng WiFi
char pass[] = "mothaibabon";                      // Mật khẩu WiFi


// ====== 4️⃣ Cấu hình các chân kết nối ======
#define TRIG_PIN D6   // Chân Trigger của cảm biến HC-SR04
#define ECHO_PIN D7   // Chân Echo của cảm biến HC-SR04
#define SERVO_PIN D5  // Servo điều khiển nắp khay
#define SDA_PIN D2    // I2C SDA (LCD + RTC)
#define SCL_PIN D1    // I2C SCL (LCD + RTC)


// ====== 5️⃣ Khai báo thiết bị ======
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 16x2, địa chỉ I2C 0x27
RTC_DS3231 rtc;                     // Module thời gian thực DS3231
Servo servo;                        // Động cơ servo SG90
BlynkTimer timer;                   // Bộ hẹn giờ cập nhật hệ thống


// ====== 6️⃣ Biến điều khiển và cấu hình logic ======
bool servoActive = false;           // Cờ trạng thái servo đang mở
unsigned long servoStartTime = 0;   // Thời điểm bắt đầu mở servo
const int threshold_distance_cm = 20; // Khoảng cách phát hiện mèo (cm)

int feedCount = 0;                  // Biến đếm số lần cho ăn
unsigned long lastFeedTime = 0;     // Thời điểm cuối cùng cho ăn
bool isLocked = false;              // Trạng thái khóa hệ thống

const unsigned long lockDuration = 30UL * 60UL * 1000UL;  // 30 phút khóa
const unsigned long resetDuration = 15UL * 60UL * 1000UL; // Reset đếm sau 15 phút

int feedHours[4] = {7, 11, 15, 19}; // Giờ tự động cho ăn
int feedMinutes[4] = {0, 0, 0, 0};  // Phút tự động cho ăn
unsigned long lastFeedCountTime = 0; // Thời điểm ghi nhận feedCount gần nhất


// ====== 7️⃣ HÀM ĐỌC KHOẢNG CÁCH (CẢM BIẾN HC-SR04) ======
// Dùng xung Trigger và Echo để đo khoảng cách (cm)
long readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return -1;
  return duration / 58; // Quy đổi thời gian thành khoảng cách (cm)
}


// ====== 8️⃣ HÀM CHO ĂN (KÍCH HOẠT SERVO) ======
// Điều kiện: Không bị khóa -> mở servo -> tăng đếm -> gửi dữ liệu
void activateServo() {
  if (isLocked) return; // Nếu hệ thống đang khóa, không cho ăn

  servo.write(180); // Mở nắp khay
  servoActive = true;
  servoStartTime = millis();

  feedCount++; // Tăng số lần cho ăn
  lastFeedCountTime = millis(); // Ghi thời gian cho ăn

  // Gửi dữ liệu lên Blynk và LCD
  Blynk.virtualWrite(V2, feedCount);
  Blynk.virtualWrite(V1, "Feeding...");
  lcd.setCursor(0, 1);
  lcd.print("Feeding...      ");

  // Nếu cho ăn >= 5 lần => tự động khóa 30 phút
  if (feedCount >= 5) {
    isLocked = true;
    lastFeedTime = millis();
    lcd.setCursor(0, 1);
    Blynk.virtualWrite(V1, "LOCKED (30m)");
  }
}


// ====== 9️⃣ NÚT ĐIỀU KHIỂN THỦ CÔNG TRÊN APP (V0) ======
BLYNK_WRITE(V0) {
  int feedCommand = param.asInt();
  if (feedCommand == 1 && !isLocked) {
    activateServo(); // Gọi hàm cho ăn
  }
}


// ====== 🔟 NÚT MỞ KHÓA THỦ CÔNG (V3) ======
BLYNK_WRITE(V3) {
  int unlockCmd = param.asInt();
  if (unlockCmd == 1 && isLocked) {
    isLocked = false;
    feedCount = 0;
    lcd.setCursor(0, 1);
    lcd.print("Unlocked        ");
    Blynk.virtualWrite(V1, "Unlocked");
  }
}


// ====== 11️⃣ CẬP NHẬT LỊCH TỪ ỨNG DỤNG (V4) ======
// Cho phép người dùng thay đổi giờ cho ăn qua app
BLYNK_WRITE(V4) {
  String schedule = param.asStr();
  Serial.println("New schedule: " + schedule);

  int index = 0;
  int lastIndex = 0;
  for (int i = 0; i < 4 && lastIndex < schedule.length(); i++) {
    int commaIndex = schedule.indexOf(',', lastIndex);
    if (commaIndex == -1) commaIndex = schedule.length();

    String timeStr = schedule.substring(lastIndex, commaIndex);
    int colonIndex = timeStr.indexOf(':');

    if (colonIndex != -1) {
      feedHours[i] = timeStr.substring(0, colonIndex).toInt();
      feedMinutes[i] = timeStr.substring(colonIndex + 1).toInt();
    }
    lastIndex = commaIndex + 1;
  }
}


// ====== 12️⃣ HÀM CẬP NHẬT TOÀN BỘ HỆ THỐNG ======
void updateSystem() {
  DateTime now = rtc.now();         // Lấy thời gian thực
  long distance = readDistanceCM(); // Đọc khoảng cách

  // --- HIỂN THỊ GIỜ + SỐ LẦN ĂN TRÊN LCD ---
  lcd.setCursor(0, 0);
  lcd.print(String(now.hour()) + ":" +
            (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" +
            (now.second() < 10 ? "0" : "") + String(now.second()) + " F:" + String(feedCount));

  // --- 1. TỰ ĐỘNG ĐÓNG SERVO SAU 3 GIÂY ---
  if (servoActive && (millis() - servoStartTime >= 3000)) {
    servo.write(0);
    servoActive = false;
  }

  // --- 2. XỬ LÝ TRẠNG THÁI KHÓA / RẢNH / ĐANG CHO ĂN ---
  if (isLocked) {
    unsigned long timeElapsed = millis() - lastFeedTime;
    if (timeElapsed >= lockDuration) {
      // Hết thời gian khóa
      isLocked = false;
      feedCount = 0;
      lcd.setCursor(0, 1);
      lcd.print("Unlocked        ");
      Blynk.virtualWrite(V1, "Unlocked");
    } else {
      // Hiển thị thời gian còn lại khi đang bị khóa
      unsigned long timeRemaining = lockDuration - timeElapsed;
      int minutesRemaining = timeRemaining / 60000;
      int secondsRemaining = (timeRemaining % 60000) / 1000;

      lcd.setCursor(0, 1);
      lcd.print("LOCK: ");
      if (minutesRemaining < 10) lcd.print("0");
      lcd.print(minutesRemaining);
      lcd.print(":");
      if (secondsRemaining < 10) lcd.print("0");
      lcd.print(secondsRemaining);
    }
  } else if (servoActive) {
    // Đang cho ăn
    // (Giữ chữ "Feeding..." sẵn có)
  } else {
    // Khi hệ thống rảnh
    lcd.setCursor(0, 1);
    lcd.print("Idle            ");
    Blynk.virtualWrite(V1, "Idle");
  }

  // --- 3. RESET SỐ LẦN ĂN NẾU 15 PHÚT KHÔNG CÓ HOẠT ĐỘNG ---
  if (!isLocked && feedCount > 0 && feedCount < 5 && (millis() - lastFeedCountTime >= resetDuration)) {
    feedCount = 0;
    Blynk.virtualWrite(V2, feedCount);
    Serial.println("Feed count reset after 15 mins");
  }

  // --- 4. TỰ ĐỘNG CHO ĂN KHI MÈO ĐẾN GẦN ---
  if (distance > 0 && distance <= threshold_distance_cm && !servoActive && !isLocked) {
    activateServo();
    Serial.println("Detected object -> Feeding");
  }

  // --- 5. TỰ ĐỘNG CHO ĂN THEO GIỜ CỐ ĐỊNH ---
  for (int i = 0; i < 4; i++) {
    if (now.hour() == feedHours[i] && now.minute() == feedMinutes[i] && now.second() == 0 && !servoActive && !isLocked) {
      activateServo();
    }
  }
}


// ====== 13️⃣ HÀM KHỞI TẠO HỆ THỐNG ======
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Connecting...");

  if (!rtc.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("RTC Error");
    while (1);
  }

  servo.attach(SERVO_PIN);
  servo.write(0);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Blynk.begin(auth, ssid, pass);        // Kết nối WiFi & Blynk Cloud
  timer.setInterval(1000L, updateSystem); // Cập nhật hệ thống mỗi 1 giây

  lcd.clear();
  lcd.print("Pet Feeder OK");
  Blynk.virtualWrite(V1, "Idle");
  Blynk.virtualWrite(V2, 0);
}


// ====== 14️⃣ HÀM LẶP CHÍNH ======
void loop() {
  Blynk.run();  // Duy trì kết nối Blynk
  timer.run();  // Gọi hàm updateSystem() mỗi 1 giây
}
