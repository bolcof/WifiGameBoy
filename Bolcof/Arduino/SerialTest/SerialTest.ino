#include <Arduino.h>

void setup() {
    Serial.begin(9600); // シリアル通信の初期化
    、9600bpsに設定
    Serial.println("ESP ready to receive data from Game Boy");
}

void loop() {
    if (Serial.available()) {
        String received = Serial.readStringUntil('\n'); // 受信データを読み取る
        Serial.print("Received: ");                     // 受信データの前にラベルを追加
        Serial.println(received);                       // 受信データを表示

        // 受け取ったデータをGame Boyに返す
        Serial.print("Echo: ");
        Serial.println(received);
    }
}
