#include <gb/gb.h>         // GBDKライブラリの基本的な機能を含むヘッダをインクルード
#include <gb/drawing.h>
#include <stdint.h>        // 標準整数型の定義を提供するヘッダをインクルード
#include <stdio.h>         // 標準入出力関数を提供するヘッダをインクルード
#include <string.h>         //文字列関数

#include "font.c"
#include "keyboard.c"       //キーボード用...
//#include "textEngine.c"     //文字列変換エンジン。最終的にはESPへ移管。


//ESP間通信用関数
unsigned char buffer[32];//バッファ長さ
const unsigned char * const fromESP = (unsigned char *)0x7ffe;//通信用固定コード。いじらないように！
unsigned char * const toESP = (unsigned char *)0x7fff;//通信用固定コード。いじらないように！
/*
ESPとの通信
mainループ内に設置して、必ず踏むようにする。
GB側が処理できるように、ESP側の通信には接頭語句が付く?switch構文で対応。関数とする。
例：SSIDリスト　->S とか。
*/


// SSIDリストを定義
char *ssids[] = {
    "AAAAAA", "BBBBBB", "CCCCCC", "DDDDDD", "EEEEEE", "FFFFFF", "GGGGGG", "HHHHHH", "IIIIII", "JJJJJJ", "KKKKKK", "LLLLLL", "MMMMMM", "NNNNNN"
};
const uint8_t num_ssids = sizeof(ssids) / sizeof(ssids[0]);  // SSIDの数を計算
const uint8_t ssidsPerPage = 11;  // 1ページあたりのSSID数
uint8_t currentSelection = 0;  // 現在選択されているSSIDのインデックス
uint8_t currentPage = 0;  // 現在のページ番号



//描画用関数:marginなど
unsigned char arrow_tiles[] = {
0x18,0x00,0x0c,0x00,
0x06,0x00,0x03,0x00,
0x06,0x00,0x0c,0x00,
0x18,0x00,0x00,0x00
};

// リロードアイコンのタイルデータ
unsigned char reload_arrow_tiles[] = {
0xda,0x00,0x92,0x00,
0xda,0x00,0x52,0x00,
0xdb,0x00,0x00,0x00,
0x01,0xc5,0x00,0x00,

0x6d,0x00,0x48,0x00,
0x68,0x00,0x48,0x00,
0x6c,0x00,0x00,0x01,
0x41,0x85,0x0e,0x07,

0xe4,0x00,0xb6,0x00,
0x94,0x00,0xb6,0x00,
0x80,0x00,0x82,0x73,
0x41,0x85,0x05,0x06
};


// スプライトをセットアップする関数
void setupArrowSprite() {
    set_sprite_data(0, 1, arrow_tiles);  // スプライトデータをメモリにロード
    set_sprite_tile(0, 0);               // スプライトタイルを設定
    move_sprite(0, 8, 48);               // スプライトの初期位置を設定
}
void setupReloadSprite() {
    set_sprite_data(1, 1, reload_arrow_tiles);  // スプライトデータをメモリにロード
    set_sprite_tile(1, 1);                      // スプライトタイルを設定（1番目のスプライトに1番目のタイルを使用）
    move_sprite(1, 100, 40);                    // スプライトの位置を設定（右上隅近く）
}

void hideAllSprites() {
    for (uint8_t i = 0; i < 40; i++) {  // Game Boyは最大で40個のスプライトを扱えます
        move_sprite(i, 0, 0);  // スプライトを画面外に移動
        hide_sprite(i);  // スプライトを非表示にする
    }
}
void drawKeyBoard(){
    HIDE_BKG; //背景を非表示に
    set_bkg_data(0, 1, gl_font);
    set_bkg_tiles(0, 0, 20, 18, gl_keyboard);
    SHOW_BKG; //背景表示   
}
//デバッグ用効果音
void debugSE(int sel){
    switch(sel){
        case 0:
            NR10_REG = 0x00; // 
            NR11_REG = 0x88; //
            NR12_REG = 0x45;
            NR13_REG = 0x73;
            NR14_REG = 0x86;
            break;
        case 1:
            NR10_REG = 0x15; // 00010101
            NR11_REG = 0x80; //
            NR12_REG = 0x94;
            NR13_REG = 0x67;
            NR14_REG = 0x86;            
            break;
        default:
            break;
    }

}    

// 画面を描画する関数
void drawMainMenu() {
    printf("\n");
    printf("New Connect\n\n");
    printf("SSID");
    for (uint8_t i = 0; i < 9; i++) { // Adjust spaces according to your display width
        printf(" ");
    }
    printf("Reload\n");

    
    for (uint8_t i = 0; i < ssidsPerPage; i++) {
        uint8_t index = i + currentPage * ssidsPerPage;  // 現在のページに応じたSSIDのインデックスを計算
        if (index < num_ssids) {
            printf("  %s\n", ssids[index]);  // SSIDを表示
        }
    }

    // ページ番号を表示
    uint8_t pageStrLen = 5 + (currentPage + 1 >= 10 ? 2 : 1);  // ページ番号表示の文字列長を計算
    uint8_t spaces = (20 - pageStrLen) / 2; // 中央揃えのためのスペース数を計算
    printf("\n");
    for (uint8_t i = 0; i < spaces; i++) {
        printf(" ");
    }
    printf("< %d >\n", currentPage + 1);

    SHOW_SPRITES;  // スプライトを表示
}

// 矢印の位置を更新する関数
void updateArrowPosition() {
    move_sprite(0, 8, 48 + 8 * (currentSelection % ssidsPerPage));  // 矢印スプライトの位置を更新
}

//キーボード処理
void KEYBOARD_INPUT(){
    //unsigned char * str;
    //str="unko";
    //return str;

}    

// 選択を更新する関数
void updateSelection(uint8_t joypadState) {
    uint8_t oldSelection = currentSelection;  // 現在の選択を保存

    // 現在のページの最初と最後のインデックスを計算
    uint8_t firstIndexOfPage = currentPage * ssidsPerPage;
    uint8_t lastIndexOfPage = firstIndexOfPage + ssidsPerPage - 1;
    if (currentPage == (num_ssids / ssidsPerPage)) {  // 最後のページの場合
        lastIndexOfPage = num_ssids - 1;
    }

    if (joypadState & J_UP) {
        if (currentSelection > firstIndexOfPage) {
            currentSelection--;  // 上ボタンが押されたら選択を一つ上に移動
        }
    }
    if (joypadState & J_DOWN) {
        if (currentSelection < lastIndexOfPage) {
            currentSelection++;  // 下ボタンが押されたら選択を一つ下に移動
        }
    }
    if (oldSelection != currentSelection) {
        updateArrowPosition();  // 矢印の位置を更新
    }
}


// ページ切り替え
void updatePage(uint8_t joypadState) {
    uint8_t oldPage = currentPage;
    if (joypadState & J_LEFT) {
        if (currentPage > 0) {
            currentPage--;  // 左ボタンが押されたら前のページに移動
        }
    }
    if (joypadState & J_RIGHT) {
        if (currentPage < (num_ssids / ssidsPerPage)) {
            currentPage++;  // 右ボタンが押されたら次のページに移動
        }
    }
    if (oldPage != currentPage) {
        cls();
        drawMainMenu();  // 画面を再描画
        currentSelection = currentPage * ssidsPerPage;  // 新しいページの最初のSSIDを選択
        updateArrowPosition();  // 矢印の位置を更新
    }
}

// 選択したSSIDを表示する関数
void drawSelectedSSID() {
    cls();
    // SSIDの所在ページを計算して現在のページを更新
    //printf("\nSSID: %s\n", ssids[currentSelection]);
    //printf("\nPASSWORD:");
    drawKeyBoard();
}

// 現在の画面状態を管理する変数
enum ScreenState {
    MAIN_MENU,
    SELECTEDSSID
} currentScreen = MAIN_MENU;

// 画面を切り替える関数
void switchScreen(enum ScreenState newScreen) {
    hideAllSprites();
    currentScreen = newScreen;
    switch (currentScreen) {
        case MAIN_MENU:
            drawMainMenu();
            break;
        case SELECTEDSSID:
            drawSelectedSSID();
            break;
    }
}
    
    
//ESPから受信を行う関数    
void readFromESP(unsigned char * result) {
    uint8_t i = 0;
    result[0] = *fromESP; //First read only triggers the read mode on the ESP. The result is garbage and will be overwriten in the first run of the loop.　通信成立のため、同じ情報を2度受信する。
    debugSE(0);
    do {
        int burn_a_few_cycles = i*i*i;
        result[i] = *fromESP;
    } while (result[i++] != '\0');
}    
//ESPへ送信を行う関数   
    
void sendToESP(unsigned char * s) {
    unsigned char * i = s;
    do {
        uint8_t burn_a_few_cycles;
        //We need to send each character twice as the first write triggers the interrupt on the ESP, which will then wait and catch the second write　通信を成立させるために同じ情報を2度送信する。
        *toESP = *i;
        for (burn_a_few_cycles = 0; burn_a_few_cycles < 1; burn_a_few_cycles++) {
            //NOOP - otherwise we will be too fast for the ESP to catch the second write
        }
        *toESP = *i;
        for (burn_a_few_cycles = 0; burn_a_few_cycles < 3; burn_a_few_cycles++) {
            //NOOP - a little longer as the ESP needs to store the previous value
        }
    } while (*(i++) != '\0');
}
    
    
void writeToSsids(unsigned char* rawData){//SSIDに投入する処理
    //0文字目は判別文字で使用済み！１文字目から文字列処理する。
    uint8_t index=0; //SSID書き込み用index
    uint8_t i=1;
    uint8_t j=0;
    do {//文字列がある間処理を行う。  
        ssids[index][j]=rawData[i];//SSIDに書き込み
        i++;
        j++;
        if(rawData[i]=='\n'){//改行で更新処理
            index++;
            j=0;    
        }

    }while(rawData[i]!='\0');
    
    //SSID所得は一回性イベントなので、ここで再描画を行う。
    drawMainMenu();  // 初期画面を描画
    updateArrowPosition();  // 矢印の位置を更新    
}   

// ESPから読み取った関数を変更する処理
void decode(unsigned char* rawData){
    //１文字目で性質を判別
    switch(rawData[0]){
        case'S':
            writeToSsids(rawData);
            break;
        deafault://接頭何もない場合、バッファにぶち込む
            
            break;
    }
    
}
 

// メイン関数
void main(void) {
///*//デバッグ用効果音設定
    NR52_REG = 0x80; // サウンドを有効化    
    NR50_REG = 0x77; // 左右チャンネルの音量を MAX にする (01110111)
    NR51_REG = 0xFF; // 全てのチャンネルのパンを振る (11111111)
    
//*/

    DISPLAY_ON;  // ディスプレイをオンにする
    setupArrowSprite();  // 矢印スプライトをセットアップ
    setupReloadSprite();  // リロードスプライトをセットアップ
    drawMainMenu();  // 初期画面を描画
    updateArrowPosition();  // 矢印の位置を更新

    
    while (1) {
        
        uint8_t joypadState = joypad();  // ジョイパッドの状態を取得
        
        //通信機能仮取付。
        unsigned char buffer[512];
        readFromESP(buffer);
        ///*
        if (buffer[0] != '\0') {//受け取りがNULLではない：何か受け取った場合の処理
            //puts("   --Receiving--");//GB上デバッグ用文字列
            //puts(buffer);
            //ここに文字列判別メソッドを入れる。
            decode(buffer);
        }//*/       
        
        if (joypadState & (J_UP | J_DOWN)) {
            updateSelection(joypadState);  // 選択を更新
            debugSE(1);
        }
        if (joypadState & (J_LEFT | J_RIGHT)) {
            updatePage(joypadState);  // ページを更新
            debugSE(0);
        }
        if (joypadState & J_A) {
            switchScreen(SELECTEDSSID);
            //puts("    --Sending--");//GB上デバッグ用文字列
            //gets(buffer);
            //sendToESP(buffer);
        }
        

        
        delay(100);  // 100ミリ秒待つ
    }

}