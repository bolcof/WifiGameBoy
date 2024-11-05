#include <gb/gb.h>         // GBDKライブラリの基本的な機能を含むヘッダをインクルード
#include <gb/drawing.h>
#include <stdint.h>        // 標準整数型の定義を提供するヘッダをインクルード
#include <stdio.h>         // 標準入出力関数を提供するヘッダをインクルード
#include <string.h>         //文字列関数
#include <stdbool.h>

#include "data/music/song_1.c"//BGM

#include "font.c"
#include "keyboard.c"       //キーボード用...
#include "text/textEngine.c"     //文字列変換エンジン。最終的には一部機能をESPへ移管。


//ESP間通信用関数
unsigned char buffer[32];//バッファ長さ
const unsigned char * const fromESP = (unsigned char *)0x7ffe;//通信用固定コード。いじらないように！
unsigned char * const toESP = (unsigned char *)0x7fff;//通信用固定コード。いじらないように！
/*
ESPとの通信
mainループ内に設置して、必ず踏むようにする。
GB側が処理できるように、ESP側の通信には接頭語句が付く?switch構文で対応。関数とする。
例：SSIDリスト　->S とか。
原則１バイトデータでの送受信を前提とする。ESP側で翻訳を行う。
*/


// SSIDリストを定義
unsigned char ssids[20][32];//32文字データを20項目確保。
uint8_t num_ssids = 20;
/* = {
    "AAAAAAAAAA", "BBBBBBBBBB", "CCCCCCCCCC", "DDDDDDDDDD", "EEEEEEEEEE", "FFFFFFFFFF", "GGGGGGGGGG", "HHHHHHHH", "IIIIIII", "JJJJJJ", "KKKKKK", "LLLLLL", "MMMMMM", "NNNNNN"
};*/
unsigned char pass[32];//パスワード
uint8_t passIdx=0; //パスワード入力位置
///*//デバッグ用文字列(読み取り用なのでポインタでよい)
unsigned char *ssidsOverride={
    "Saqwsftasd\ndxfeAFWYU\n\n@kojiuhgya\n\n71934h2ibru\nuoqamvdzvg\nuohbiuawc\nsdf-qweafc\npingo!!asds\nchrs_2.4G\nGETOUT_PWR\nGETOUT_PWR\nGETOUT_PWR\nGETOUT_PWR"
};
unsigned char *UTF8sanpleText={
    "みんな、みえているか!?\nオレ ###は 2022ねん から タイムトリップ してきたのだが ココでは いったい ナニが おきているんだ...?"
};
//*/

const uint8_t ssidsPerPage = 11;  // 1ページあたりのSSID数
uint8_t currentSelection = 0;  // 現在選択されているSSIDのインデックス
uint8_t currentPage = 0;  // 現在のページ番号

#define MARGINX 8
#define MARGINY 8
#define TXTLENPL 18//１行当たり最大文字数
#define TXTSPRFRM 10//濁点スプライト開始位置
#define TYPEADLS 3 //入力末尾スプライト格納位置
#define KEYWDTH 14 //キーボード横文字数
#define KEYHGT 6 //キーボード縦文字数
#define KEYSTX 24//キーボード開始座標
#define KEYSTY 88//キーボード開始座標
#define TYPEINIX 32
#define TYPEINIY 64 //入力末尾スプライト初期位置
#define BGMNUM 1 //音楽の数


uint8_t spriteDebug =0; //デバッグ用。

uint8_t keyBoardState =0; //キーボードの状態。ブール値のように使う。
uint8_t joypadState;

typedef struct SpriteNorm{//通常スプライトの構造体
    uint8_t ID;
    uint8_t row;
    uint8_t col;    
    uint8_t x;
    uint8_t y;

} SpriteNorm;
typedef struct Blinker{//キーボード入力末尾スプライトの構造体
    uint8_t ID;
    uint8_t x;
    uint8_t y;
    uint8_t len;
} Blinker;
Blinker blinker ={TYPEADLS,0,0,100};//キーボード入力末尾スプライトの構造体インスタンス
SpriteNorm arrow ={0,0,0,KEYSTX,KEYSTY};//キーボード入力時のためのカーソル構造体インスタンス

//BGM管理用変数
    const uint16_t EndofNote[BGMNUM] ={2304};//2304 forSONG1
    volatile uint8_t currentBGM =0;
    volatile uint16_t currentNoteIndex[BGMNUM][4];//if +8 >EON 
    volatile bool isFinishedNote[4] ={false,false,false,false};
    volatile bool isPlayingBGM =true;
    volatile uint16_t noteDurationCounter[4];//0,0,0,0
    volatile uint8_t noteSpeed=1;
    

  
void readFromESP(unsigned char * result) {//ESPから受信を行う関数  
    uint8_t i = 0;
    result[0] = *fromESP; //First read only triggers the read mode on the ESP. The result is garbage and will be overwriten in the first run of the loop.　通信成立のため、同じ情報を2度受信する。
    do {
        int burn_a_few_cycles = i*i*i;
        result[i] = *fromESP;
    } while (result[i++] != '\0');
}    

void sendToESP(unsigned char * s) {//ESPへ送信を行う関数 
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
//描画用変数:marginなど
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

//melody
// Example music data for each channel. 8*4
const uint8_t channels_data[] = {0x44, 0x10, 0x48, 0x10, 0x4C, 0x10, 0x00, 0x00,
                                0x40, 0x10, 0x43, 0x10, 0x47, 0x10, 0x00, 0x00,
                                0x00, 0x10, 0x02, 0x10, 0x04, 0x10, 0x00, 0x00,
                                0x22, 0x10, 0x23, 0x10, 0x24, 0x10, 0x00, 0x00};


// スプライトをセットアップする関数
void setupArrowSprite() {
    set_sprite_data(0, 1, arrow_tiles);  // スプライトデータをメモリにロード
    set_sprite_tile(arrow.ID, 0);               // スプライトタイルを設定
    move_sprite(arrow.ID, 8, 48);               // スプライトの初期位置を設定
}
void setupReloadSprite() {
    set_sprite_data(1, 1, reload_arrow_tiles);  // スプライトデータをメモリにロード
    set_sprite_tile(1, 1);                      // スプライトタイルを設定（1番目のスプライトに1番目のタイルを使用）
    move_sprite(1, 92, 40);                    // スプライトの位置を設定（右上隅近く）
}

void setupFont(){
    set_bkg_data(0, 216, gl_font);
    set_sprite_data(2,216,gl_font);
    set_sprite_tile(TYPEADLS,63+2);//キーボード末尾スプライトを設定。
    move_sprite(TYPEADLS,blinker.x,blinker.y);//一旦画面外へ
}

void hideAllSprites() {
    for (uint8_t i = 0; i < 40; i++) {  // Game Boyは最大で40個のスプライトを扱えます
        move_sprite(i, 0, 0);  // スプライトを画面外に移動
        hide_sprite(i);  // スプライトを非表示にする
    }
}
void hideTextSprites() {
    for (uint8_t i =TXTSPRFRM ; i < 40; i++) {  // Game Boyは最大で40個のスプライトを扱えます
        move_sprite(i, 0, 0);  // スプライトを画面外に移動
        hide_sprite(i);  // スプライトを非表示にする
    }
}
void drawLoading(int i){//なんか。
    int x = 60;
    int y = 120;
}

//文書表示時のテキスト描画について、濁点例外処理。
uint8_t sprADDR=0;
void drawTextMod(const uint8_t x, const uint8_t y, const uint8_t Code){
    uint8_t sprn=TXTSPRFRM+sprADDR;
    set_sprite_tile(sprn,Code+2);
    move_sprite(sprn,x+10, y+14); //濁点

    sprADDR++;
}
//文書表示時のテキスト描画。
void drawText(const uint8_t x, const uint8_t y, const uint8_t *text){

    uint8_t count=x;
    uint8_t lines=y;
    uint8_t realX=x*8;
    uint8_t realY=y*8;
    

    while(*text!='\0'){//文字コードから表示への変換
        
        if(count>=18||*text=='\n'){//改行処理判定
            count=x;
            realX=x*8;
            lines+=2; 
            realY+=16;
            if(*text=='\n')text++;//改行文字による改行の場合、消化。
        }
        uint8_t chs= *text++ ; 
        uint8_t chr=getTextIndex(chs);//文字を取得
        set_bkg_tile_xy(count, lines, chr);//描画

        if(*text==0xF5 || *text ==0xF6){//濁点等の場合、同じ場所にもう一度文字を書く(できる？sprite対応が良いと思う。ここでdTSを呼び出そう。)
            chs=*text++;
            chr=getTextIndex(chs);
            drawTextMod(realX, realY, chr);    //濁点と半濁点だけの処理。
            
        }
        realX+=8;
        count++;
    }  
    SHOW_BKG;
    SHOW_SPRITES;
    sprADDR=0;
}

//キーボード描画。カーソルを合わせて。
void drawKeyBoard(){
    HIDE_BKG; //背景を非表示に
    //set_bkg_data(0, 188, gl_font);
    set_bkg_tiles(0, 0, 20, 18, gl_keyboard);
    //濁点表示。{24,24,24}{24,56,112}
    uint8_t dtp[3][3]={{24,56,112},{24,24,24},{0x83,0x82,0x82}}; //濁点表示用情報
    for(int dti=0; dti<3; dti++){
        drawTextMod(dtp[0][dti],dtp[1][dti],dtp[2][dti]); 
    }
    sprADDR=0;

    SHOW_BKG; //背景表示
    move_sprite(arrow.ID,arrow.x,arrow.y);//カーソル移動   
}
//デバッグ用効果音
void debugSE(int  sel){
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

//音楽関係
void playNote(uint8_t note, uint8_t chnl) {

        switch (chnl)
        {
        case 0:
            // Set sound registers for Channel 1
            NR10_REG = 0x16; // Sweep settings
            NR11_REG = 0x40; // Sound length/wave pattern duty
            NR12_REG = 0x73; // Volume envelope
            NR13_REG = note; // Frequency low
            NR14_REG = 0x86; // Frequency high + start
            break;
        case 1:
            // Set sound registers for Channel 2
            NR21_REG = 0x40; // Sound length/wave pattern duty
            NR22_REG = 0x73; // Volume envelope
            NR23_REG = note; // Frequency low
            NR24_REG = 0x86; // Frequency high + start
            break;
        case 2:
            // Set sound registers for Channel 3
            NR30_REG = 0x80; // Channel on
            NR31_REG = 0xFF; // Sound length
            NR32_REG = 0x20; // Select output level
            NR33_REG = note; // Frequency low
            NR34_REG = 0xC0; // Frequency high + start
            break;
        case 3:
            // Set sound registers for Channel 4
            NR41_REG = 0x30; // Sound length
            NR42_REG = 0x73; // Volume envelope
            NR43_REG = note; // Polynomial counter
            NR44_REG = 0xC0; // Start sound
            break;                                
        default:
            break;
        }

 

}
void updateMusic() {//driven as VBLank_ISR
    if (!isPlayingBGM) return; // Do nothing if paused
    
    for(int i=0; i<4; i++){
        set_sprite_tile(i+2,currentNoteIndex[currentBGM][i]%128);
        move_sprite(i+2,16+currentNoteIndex[currentBGM][i]/100,132+i*8);
        if (noteDurationCounter[i] == 0) {
            uint8_t note = SONG_1[currentNoteIndex[currentBGM][i]+(i*2)];
            uint8_t duration = SONG_1[currentNoteIndex[currentBGM][i]+(i*2)+1];

            if (currentNoteIndex[currentBGM][i]+8 >= EndofNote[currentBGM]) {//compare currentNoteIndex to length of the SONG. 
                isFinishedNote[i] =true;
                currentNoteIndex[currentBGM][i] = i*2; // Reset to loop the melody
            } else {
                playNote(note,i);
                noteDurationCounter[i] = duration * noteSpeed; // Adjust duration scale
                currentNoteIndex[currentBGM][i] += 8;
            }
        } else {
            noteDurationCounter[i]--;
        }
    }
    if(isFinishedNote[0]&&isFinishedNote[1]&&isFinishedNote[2]&&isFinishedNote[3]){//reset for loop
        debugSE(1);
        for(int i=0;i<4;i++){
            
            isFinishedNote[i] =false;
            currentNoteIndex[currentBGM][i]=0;
            noteDurationCounter[i]=0;
        }
    } 

}
void stopMusic() {
    NR12_REG = 0x00; // Stop sound on Channel 1
    NR22_REG = 0x00; // Stop sound on Channel 2
    NR30_REG = 0x00; // Stop sound on Channel 3
    NR42_REG = 0x00; // Stop sound on Channel 4
}
void pauseMusic() {
    //stopMusic(); // Silence the music
    NR12_REG = 0x00;
    isPlayingBGM = false;
}

void resumeMusic() {
    isPlayingBGM = true;
}
void resetMusic(){
    isPlayingBGM=true;//場合によって消す

    for(int i=0; i<4; i++){
        currentNoteIndex[currentBGM][i]=0;
        noteDurationCounter[i]=0;
        isFinishedNote[i]=false;
    }

}
void controlMusicSpeed(uint8_t jps){
    if((jps & J_LEFT) && noteSpeed>1) noteSpeed--;
    if((jps & J_RIGHT) && noteSpeed<=20) noteSpeed= noteSpeed+1 ;
}





//SSID送信系をまとめた関数
void sendConnectingInfo(char* password){
    unsigned char s[128];
    //strcpy(s,ssids[currentSelection]);
    sprintf(s,"SPAS%s\n%s",ssids[currentSelection],password);//接頭記号"SPAS"に続いて、ssid,passの順でまとめる
    //drawText(1,2,s);delay(1000);//デバッグ用
    //送信処理
    sendToESP(s);

}

//キーボード処理. 
void keyBoardOpr(uint8_t js){
    if(keyBoardState == 0)return; //何もない時はリセット。

            //カーソル移動系
            if (js & (J_UP | J_DOWN)) {//上下移動
                debugSE(0);
                arrow.col= (js==J_DOWN)? arrow.col+1: arrow.col-1;
                if(arrow.col<0)arrow.col=0;
                if(arrow.col>KEYHGT)arrow.col =KEYHGT; //一番下は決定ボタンを指す。
            }
            if (js & (J_LEFT | J_RIGHT)) {
                debugSE(0);
                arrow.row= (js==J_RIGHT)? arrow.row+1: arrow.row-1;
                if(arrow.row<0)arrow.row=0;
                if(arrow.row>=KEYWDTH)arrow.row =KEYWDTH-1;   
            }
            //カーソル表示系
            if(arrow.col == KEYHGT){//決定ボタンにある時
                arrow.x=KEYSTX + 80; //8*10=80
                arrow.y=KEYSTY + (KEYHGT+1)*8;
            }else{//通常時
                arrow.x=KEYSTX + 8* arrow.row;
                arrow.y=KEYSTY + 8* arrow.col;
            }
            move_sprite(arrow.ID,arrow.x,arrow.y);//カーソル移動   

            //文字入力系
            if (js & J_A) {
                debugSE(1);
                //add character OR send password カーソルの位置による
                if(arrow.col == KEYHGT){//文字入力を確定する
                    blinker.x=0; blinker.y=0;//ブリンカを隠す
                    move_sprite(TYPEADLS,blinker.x,blinker.y);//ブリンカの移動
                    keyBoardState=0;
                    hideTextSprites();cls();
                    drawMainMenu();
                    // send password
                    sendConnectingInfo(pass);

                }else{
                    if(passIdx<32-1){//文字記入が可能なとき
                        pass[passIdx] = getText(arrow.row, arrow.col,KEYWDTH);//キーボード位置に応じて文字を取得
                        drawText(3,6,pass);
                        passIdx++;
                        blinker.x+=8;
                        move_sprite(TYPEADLS,blinker.x,blinker.y);
                    }
                }
            }
            if (js & J_B) {//キー入力キャンセルしてSSID選択に戻るのもやらせたい。
                debugSE(1);
                //delete character
                if(passIdx>0){
                    passIdx--;
                    pass[passIdx]=' ';//末尾をスペースにする
                    drawText(3,6,pass);//同じ長さの文字で上書き
                    pass[passIdx]='\0';//該当文字を消去
                    blinker.x-=8;
                    move_sprite(TYPEADLS,blinker.x,blinker.y);
                }               

            }
            if (js & J_START){
                debugSE(1);
                blinker.x=0; blinker.y=0;//ブリンカを隠す
                move_sprite(TYPEADLS,blinker.x,blinker.y);//ブリンカの移動
                keyBoardState=0;
    
                hideTextSprites();cls();
                drawMainMenu();
                // send password
                sendConnectingInfo(pass);

            }


}

// 画面を描画する関数
void drawMainMenu() {
    drawText(0,1,"New Connect\nSSID");
    drawText(12,3,"Reload");

    
    for (uint8_t i = 0; i < ssidsPerPage; i++) {
        uint8_t index = i + currentPage * ssidsPerPage;  // 現在のページに応じたSSIDのインデックスを計算
        if (index < num_ssids) {
            drawText(1,4+i,ssids[index]);
        }
    }

    // ページ番号を表示
    uint8_t pageStrLen = 5 + (currentPage + 1 >= 10 ? 2 : 1);  // ページ番号表示の文字列長を計算
    uint8_t spaces = (20 - pageStrLen) / 2; // 中央揃えのためのスペース数を計算
    //printf("\n");
    //for (uint8_t i = 0; i < spaces; i++) {
    //    printf(" ");
    //}
    //printf("< %d >\n", currentPage + 1);
    uint8_t mgn= spaces;
    uint8_t hgt= 5+ssidsPerPage;
    uint8_t pnum[2]= {currentPage+1+0x30,0}; //convert to ascll

    drawText(mgn,hgt,"<");drawText(mgn+1,hgt,pnum);drawText(mgn+2,hgt,">");

    SHOW_SPRITES;  // スプライトを表示
}

// 矢印の位置を更新する関数
void updateArrowPosition() {
    move_sprite(0, 8, 48 + 8 * (currentSelection % ssidsPerPage));  // 矢印スプライトの位置を更新
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
        uint8_t newPageFlag =num_ssids/ssidsPerPage;
        if(num_ssids==ssidsPerPage)newPageFlag=0;

        if (currentPage < newPageFlag) {
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
    drawKeyBoard();
    keyBoardState=1;

    //drawText(3,1,"SSID");
    drawText(3,1,ssids[currentSelection]);  //SSID表示 
    blinker.x=TYPEINIX; blinker.y=TYPEINIY;//ブリンカ初期位置設定
    blinker.x+=8*passIdx;//デバッグ時はパスワードの文字列末尾をブリンカ位置とする。

    move_sprite(TYPEADLS,blinker.x,blinker.y);//ブリンカの移動
    
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
    
    

    
void writeToSsids(unsigned char* rawData){//SSIDに投入する処理
    //0文字目は判別文字で使用済み！１文字目から文字列処理する。
    uint8_t index=0; //SSID書き込み用index
    uint8_t i=1;//読み出し用。０文字目無視
    uint8_t j=0;//書き込み用。
    do {//文字列がある間処理を行う。
         if(rawData[i]=='\n'){//改行で更新処理.
        
            //次の文字も確認した上で、終了処理をするかどうか判断。
            if(rawData[i+1]!='\n'){//改行1回のみ。通常処理。
                ssids[index][j]='\0';//終了文字添付
                j=0; index++; //通常の終了処理
            }//改行２回以上の場合は、１ループ後に再びこの検査を受けてもらう。 
        }else{
            //通常の文字の場合、SSIDに書き込み。改行文字は決してここに入れない！
            ssids[index][j]=rawData[i];j++;
        }     
        i++; 
    }while(rawData[i]!='\0');
    num_ssids=index-1;//読み取ったSSIDの数でnum_ssidsを再定義。
    
     
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
//ESPへのリクエスト関数。
void requestToESP(unsigned char *s){
    sendToESP(s);
}

/*//テキスト表示テスト用関数(スプライト番号順)。
uint8_t showTextSpriteTest(uint8_t sprcode_head, uint8_t JP){
    uint8_t sps = sprcode_head;
    uint8_t sph = sprcode_head*16;
    if(JP & J_UP){sps--;if(sps<=0)sps=0;}
    if(JP & J_DOWN){sps++;if(sps>13)sps=0;}
    set_bkg_tile_xy(0, 7, 0);set_bkg_tile_xy(1, 7, 0);
    for(int i=0; i<16; i++){
        set_bkg_tile_xy(i+2, 7, sph+i);//描画
        set_bkg_tile_xy(i+2, 8, sph+16+i);//描画
        set_bkg_tile_xy(i+2, 9, sph+32+i);//描画
    } 
    set_bkg_tile_xy(19, 7, 0);set_bkg_tile_xy(20, 7, 0);
    set_bkg_tile_xy(19, 8, 0);set_bkg_tile_xy(20, 8, 0);
    return sps;
}
//*/


//キー入力周りはinputだけ引き継いで関数内で処理した方が見やすいと思う...
//ステージ管理関数。十分短ければmain()管理で良いだろう。
void handleStages(enum ScreenState newScreen){

}

// メイン関数
void main(void) {
///*//デバッグ用効果音設定
    NR52_REG = 0x80; // サウンドを有効化    
    NR50_REG = 0x77; // 左右チャンネルの音量を MAX にする (01110111)
    NR51_REG = 0xFF; // 全てのチャンネルのパンを振る (11111111)    
//*/
/*///BGM用インタラプト設定
    disable_interrupts();
    add_VBL(updateMusic);
    enable_interrupts();   
//*/

    DISPLAY_ON;  // ディスプレイをオンにする
    setupFont();　//キーボードスプライトをセットアップ


    unsigned char buffer[256];
    
    //*/SSID読み取り部分。読めるまで無理やり継続する。

    drawText(1,6,"LOADING SSIDS\nPLEASE WAIT...");
    //delay(1000);//ESP立ち上げを待つ。必要なら入れる
    char *sendSignal = "SSID";//送信は一度きりにする。
    sendToESP(sendSignal);
    debugSE(1);
    delay(2000);//デバッグのためこうしているが、何らかの動きを見せても良い。
    debugSE(0);
    
    int delll =0;
    while(delll==0){
        readFromESP(buffer);
        
        if(buffer[0]=='S'){
            decode(buffer);//SSID一覧を取得する。 
            delll=1;
        }else{
            if(buffer[0]!='\0'){//文字列は受け取っているが、不正な値の場合。やり直しを要求...今回は手動にする
                //sendToESP(sendSignal);
                drawText(0,1,buffer);delay(2000);//デバッグ用。
            }
            
        }
        //デバッグ用ブレーカ
        joypadState = joypad();
        if(joypadState & J_A)delll=1;
        delay(100);
    }
    debugSE(0);
    //*/
    //decode(ssidsOverride);//表示用ダミーSSID。
    
    cls();//ローディング画面初期化
    setupArrowSprite();  // 矢印スプライトをセットアップ
    setupReloadSprite();  // リロードスプライトをセットアップ
    drawMainMenu();  // 初期画面を描画,テストでは非表示。
    updateArrowPosition();  // 矢印の位置を更新

    /*//文字表示テスト用。
    unsigned char buf[256];
    uint8_t *ans = u8to1byteCode(buf, UTF8sanpleText);//文字コード化。ここはESP側の処理。
    drawText(MARGINX,MARGINY, ans );//*/
   
    while (1) {
        
        joypadState = joypad();  // ジョイパッドの状態を取得

        //通信機能仮取付。
        readFromESP(buffer);
        ///*
        if (buffer[0] != '\0') {//受け取りがNULLではない：何か受け取った場合の処理
            //ここに文字列判別メソッドを入れる。
            //debugSE(0);    
            decode(buffer);
            drawText(0,1,buffer);//デバッグ用。
            buffer[0] ='\0'; //文字列処理後は受け取り文字先頭を空白とし、二重処理を避ける。
        }//*/       
        
        



        //ボタン入力に呼応した動きの記述。将来的にゲームステージで管理する。ゲームステージはenumで管理。
        if(keyBoardState!=0){
            keyBoardOpr(joypadState);
        }else{
            if (joypadState & (J_UP | J_DOWN)) {
                updateSelection(joypadState);  // 選択を更新
                debugSE(1);
                //spriteDebug = showTextSpriteTest(spriteDebug,joypadState);
            }
            if (joypadState & (J_LEFT | J_RIGHT)) {
                updatePage(joypadState);  // ページを更新
                controlMusicSpeed(joypadState);//デバッグ用、音楽速度調整
                debugSE(0);
            }
            if (joypadState & J_A) {
                switchScreen(SELECTEDSSID);
            }if (joypadState & J_SELECT){//SSID再送の場合。連打を避けるためキーが離れるまで待つ。
                //drawText(1,1,u8to1byteCode(buffer,UTF8sanpleText));
                char *sendSignal = "SSID";
                //sendToESP(sendSignal);
                debugSE(1);
                waitpadup();
            }if(joypadState & J_START){
                    //resetMusic();
            }
        }
        

        
        delay(100);  // 100ミリ秒待つ
    }

}