#include <gb/gb.h>
#include <stdio.h>

#define BUFFER_SIZE 32

unsigned char buffer[BUFFER_SIZE];

void send_byte(unsigned char b) {
    // シリアル送信開始
    SB_REG = b;
    SC_REG = 0x81; // シリアル転送開始ビットをセット

    // 送信完了を待つ
    while (SC_REG & 0x80);
}

unsigned char receive_byte() {
    // シリアル受信待ち
    while (!(SC_REG & 0x80));
    return SB_REG;
}

void send_string(const char *str) {
    while (*str) {
        send_byte(*str);
        wait_vbl_done();  // シリアル送信が完了するまで待つ
        str++;
    }
}

void receive_string(unsigned char *buffer, size_t buffer_size) {
    size_t i = 0;
    unsigned char received_char;
    do {
        received_char = receive_byte();
        buffer[i++] = received_char;
        wait_vbl_done(); // シリアル受信の安定のために待機
    } while (received_char != '\0' && i < buffer_size - 1);
    buffer[i] = '\0'; // 受信した文字列を終端する
}

void main() {
    printf("Ready to send button presses...\n");

    while (1) {
        // ボタンの入力を検出
        UINT8 joypad_state = joypad();

        // 各ボタンが押された場合に文字列を送信
        if (joypad_state & J_A) {
            send_string("Button A pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_B) {
            send_string("Button B pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_UP) {
            send_string("Button Up pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_DOWN) {
            send_string("Button Down pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_LEFT) {
            send_string("Button Left pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_RIGHT) {
            send_string("Button Right pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_START) {
            send_string("Button Start pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }
        if (joypad_state & J_SELECT) {
            send_string("Button Select pressed\n");
            // ESPからの応答を受け取る
            receive_string(buffer, BUFFER_SIZE);
            printf("ESP: %s\n", buffer);
        }

        // 次のフレームまで待つ
        wait_vbl_done();
    }
}
