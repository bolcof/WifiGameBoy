/*

***
UTF-8平文->1バイトコード変換フィルタ部分は、後でESPに移動
参考：https://qiita.com/t-yama-3/items/7cba573b4cf23322dfc8
***

|このファイルでは1️⃣文字列判定及び2️⃣表示を行います。UTF-8での取り扱いと想定。
| ほとんどASCIIコード(２バイト文字だけ独自の番号をつけます)に則って記述される文字について、
| 同じ見た目の文字列を我々のチップセット(font.c)で表示できるようにするためのものです。



判定ルールはこちら：
0x80~0xffを和文使用領域とする！
128~186 (0x81~0xB8) はカタカナ(及び横棒)
187~239(0xB9~0xB8) はひらがな
240,241(0xB9,0xB10)は濁点及び半濁点。
ESP内で文字列を整形する。
if文総当たりでやってもらう。
(容量が許せばgb側にも配置して概念実証を行う)
->アホ難しいぞ！そもそもchar型で複数バイト文字をなんとかするのが間違い。

濁点、半濁点は濁点抜きの文字と共に２文字で送る。
GB上ではコードが濁点の場合同じ部分に２度描いてもらう。
(テキスト多い時どうする？画面更新しないでなんとかならんかな？)


篩の詳細はこちら：
uint8_t sample = 入力文字
uint8_t code=判別コード
case 32-96: code -=32;
//記号、数字及び大文字
case 97-122: code +=92;
//小文字
case 123-127: code -= 58
//記号
case 128-188 code-=59
case 189-239 code-=57
case 240-241 code-=110
//ひらがなとカタカナ


判定用経路はこちら：
ESP：サーバから平文を受領
ESP：独自文字コード変換実行(この段階で１バイト文字にする)
ESP：GBへ送信
✅GB：独自文字コードからタイルアドレスに変換(ここまでESPでもよい)
GB：タイルアドレスによる文字表示

*概念実証時は、全てをGB内で処理してよい。

*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h> 

char* sampleTex = "ABCDEF あーいうえ・お アイウエオ abcdef";//デバッグ用



//構造体作成
typedef struct TextParseContext{
    uint8_t *buffer;
    uint8_t bufferPos;

    const char* inText;
    uint8_t textPos;
} TextParseContext;




/*コード文への変換関数(1バイトデータを返す。有限長byteとして定義)
uint8_t *u8to1byteCode(uint8_t *buf, const char *str){
    int count=0;
    uint8_t *p=buf; // returnするポインタを格納。
    uint8_t filter[3];//判別用配列
    uint8_t dFlg=0; //濁点フラグ。1=濁点、2=半濁点。
    
    while (*str != '\0') {

        if ((*str & 0xC0) != 0x80) {//先頭文字であるか参照
            //フィルタに当該バイトから３バイト分割り当て
            filter[0]=*str; filter[1]=*(str+1); filter[2]=*(str+2);
            if((filter[0] & 0x80) == 0x00){ //１バイト文字の場合の処理
                *buf = *str;//元文字列からバッファに直接アドレス転写
                buf++;
            } 
            //特殊文字判定(2byte)
            else if(filter[0] ==0xC2 && filter[1] ==0xA5 ){*buf = '\\'; buf++; str++;}//"¥" ->"\"
            else if(filter[0] ==0xC3 && filter[1] ==0x97 ){*buf = '*';  buf++; str++;}//"×" ->"*"
            //ひらがなとカタカナの処理(3byte)
            else{
                if(filter[0] ==0xE3){ //E3
                    if(filter[1] ==0x80){//E380:和文記号
                         if(filter[2] ==0x81){ *buf=0x83; buf++;  }//E38081 "、"
                    else if(filter[2] ==0x82){ *buf=0x80; buf++;  }//E38082 "。"
                    else if(filter[2] ==0x8C){ *buf=0x81; buf++;  }//E3808C "「"
                    else if(filter[2] ==0x8D){ *buf=0x82; buf++;  }//E3808D "」" 
                    else if(filter[2] ==0x9C){ *buf=0x7E; buf++;  }//E3809C "〜"、'~'として解釈                   
                    }
                else if(filter[1] ==0x81){ //E381:ひらがな1️⃣
                         if(filter[2] ==0x80){ *buf=0x20; buf++;  }//E38181 "　" 全角スペースのつもりだがあまりうまく動いてないかも
                    else if(filter[2] ==0x81){ *buf=0xBE; buf++;  }//E38182 "ぁ"                        
                    else if(filter[2] ==0x82){ *buf=0xC8; buf++;  }//E38182 "あ"
                    else if(filter[2] ==0x83){ *buf=0xBF; buf++;  }//E38183 "ぃ"
                    else if(filter[2] ==0x84){ *buf=0xC9; buf++;  }//E38184 "い"
                    else if(filter[2] ==0x85){ *buf=0xC0; buf++;  }//E38185 "ぅ"
                    else if(filter[2] ==0x86){ *buf=0xCA; buf++;  }//E38186 "う"
                    else if(filter[2] ==0x87){ *buf=0xC1; buf++;  }//E38187 "ぇ"
                    else if(filter[2] ==0x88){ *buf=0xCB; buf++;  }//E38188 "え"
                    else if(filter[2] ==0x89){ *buf=0xC2; buf++;  }//E38189 "ぉ"
                    else if(filter[2] ==0x8A){ *buf=0xCC; buf++;  }//E3818A "お"
                    else if(filter[2] ==0x8B){ *buf=0xCD; buf++;  }//E3818B "か"
                    else if(filter[2] ==0x8C){ *buf=0xCD; buf++; dFlg=1; }//E3818C "が"　濁点処理フラグ添付
                    else if(filter[2] ==0x8D){ *buf=0xCE; buf++;  }//E3818D "き"
                    else if(filter[2] ==0x8E){ *buf=0xCE; buf++; dFlg=1; }//E3818E "ぎ"　
                    else if(filter[2] ==0x8F){ *buf=0xCF; buf++;  }//E3818F "く"
                    else if(filter[2] ==0x90){ *buf=0xCF; buf++; dFlg=1; }//E38190 "ぐ"　
                    else if(filter[2] ==0x91){ *buf=0xD0; buf++;  }//E38191 "け"
                    else if(filter[2] ==0x92){ *buf=0xD0; buf++; dFlg=1; }//E38192 "げ"　
                    else if(filter[2] ==0x93){ *buf=0xD1; buf++;  }//E38193 "こ"
                    else if(filter[2] ==0x94){ *buf=0xD1; buf++; dFlg=1; }//E38194 "ご"
                    else if(filter[2] ==0x95){ *buf=0xD2; buf++;  }//E38195 "さ"                    
                    else if(filter[2] ==0x96){ *buf=0xD2; buf++; dFlg=1; }//E38196 "ざ"　
                    else if(filter[2] ==0x97){ *buf=0xD3; buf++;  }//E38197 "し"
                    else if(filter[2] ==0x98){ *buf=0xD3; buf++; dFlg=1; }//E38198 "じ"　
                    else if(filter[2] ==0x99){ *buf=0xD4; buf++;  }//E38199 "す"
                    else if(filter[2] ==0x9A){ *buf=0xD4; buf++; dFlg=1; }//E3819A "ず"　
                    else if(filter[2] ==0x9B){ *buf=0xD5; buf++;  }//E3819B "せ"
                    else if(filter[2] ==0x9C){ *buf=0xD5; buf++; dFlg=1; }//E3819C "ぜ"　
                    else if(filter[2] ==0x9D){ *buf=0xD6; buf++;  }//E3819D "そ"
                    else if(filter[2] ==0x9E){ *buf=0xD6; buf++; dFlg=1; }//E3819E "ぞ"
                    else if(filter[2] ==0x9F){ *buf=0xD7; buf++;  }//E38195 "た"                    
                    else if(filter[2] ==0xA0){ *buf=0xD7; buf++; dFlg=1; }//E38196 "だ"　
                    else if(filter[2] ==0xA1){ *buf=0xD8; buf++;  }//E38197 "ち"
                    else if(filter[2] ==0xA2){ *buf=0xD8; buf++; dFlg=1; }//E38198 "ぢ"　
                    else if(filter[2] ==0xA3){ *buf=0xC6; buf++;  }//E38199 "っ"
                    else if(filter[2] ==0xA4){ *buf=0xD9; buf++;  }//E38199 "つ"
                    else if(filter[2] ==0xA5){ *buf=0xD9; buf++; dFlg=1; }//E3819A "づ"　
                    else if(filter[2] ==0xA6){ *buf=0xDA; buf++;  }//E3819B "て"
                    else if(filter[2] ==0xA7){ *buf=0xDA; buf++; dFlg=1; }//E3819C "で"　
                    else if(filter[2] ==0xA8){ *buf=0xDB; buf++;  }//E3819D "と"
                    else if(filter[2] ==0xA9){ *buf=0xDB; buf++; dFlg=1; }//E3819E "ど"   
                    else if(filter[2] ==0xAA){ *buf=0xDC; buf++;  }//E38199 "な"
                    else if(filter[2] ==0xAB){ *buf=0xDD; buf++;  }//E3819A "に"　
                    else if(filter[2] ==0xAC){ *buf=0xDE; buf++;  }//E3819B "ぬ"
                    else if(filter[2] ==0xAD){ *buf=0xDF; buf++;  }//E3819C "ね"　
                    else if(filter[2] ==0xAE){ *buf=0xE0; buf++;  }//E3819D "の"
                    else if(filter[2] ==0xAF){ *buf=0xE1; buf++;  }//E3819E "は" 
                    else if(filter[2] ==0xB0){ *buf=0xE1; buf++; dFlg=1; }//"ば" 
                    else if(filter[2] ==0xB1){ *buf=0xE1; buf++; dFlg=2; }//"ぱ"
                    else if(filter[2] ==0xB2){ *buf=0xE2; buf++;  }//       "ひ" 
                    else if(filter[2] ==0xB3){ *buf=0xE2; buf++; dFlg=1; }//"び" 
                    else if(filter[2] ==0xB4){ *buf=0xE2; buf++; dFlg=2; }//"ぴ"                                                          
                    else if(filter[2] ==0xB5){ *buf=0xE3; buf++;  }//       "ふ" 
                    else if(filter[2] ==0xB6){ *buf=0xE3; buf++; dFlg=1; }//"ぶ" 
                    else if(filter[2] ==0xB7){ *buf=0xE3; buf++; dFlg=2; }//"ぷ" 
                    else if(filter[2] ==0xB8){ *buf=0xE4; buf++;  }//       "へ" 
                    else if(filter[2] ==0xB9){ *buf=0xE4; buf++; dFlg=1; }//"べ" 
                    else if(filter[2] ==0xBA){ *buf=0xE4; buf++; dFlg=2; }//"ぺ" 
                    else if(filter[2] ==0xBB){ *buf=0xE5; buf++;  }//       "ほ" 
                    else if(filter[2] ==0xBC){ *buf=0xE5; buf++; dFlg=1; }//"ぼ" 
                    else if(filter[2] ==0xBD){ *buf=0xE5; buf++; dFlg=2; }//"ぽ" 
                    else if(filter[2] ==0xBE){ *buf=0xE6; buf++;  }//       "ま" 
                    else if(filter[2] ==0xBF){ *buf=0xE7; buf++;  }//       "み" 
                    }
                else if(filter[1] ==0x82){ //E382:ひらがな2️⃣~カタカナ1️⃣。
                         if(filter[2] ==0x80){ *buf=0xE8; buf++;  }//"む"
                    else if(filter[2] ==0x81){ *buf=0xE9; buf++;  }//"め"　
                    else if(filter[2] ==0x82){ *buf=0xEA; buf++;  }//"も"
                    else if(filter[2] ==0x83){ *buf=0xC3; buf++;  }//"ゃ"　
                    else if(filter[2] ==0x84){ *buf=0xEB; buf++;  }//"や"
                    else if(filter[2] ==0x85){ *buf=0xC4; buf++;  }//"ゅ"
                    else if(filter[2] ==0x86){ *buf=0xEC; buf++;  }//"ゆ"　
                    else if(filter[2] ==0x87){ *buf=0xC5; buf++;  }//"ょ"
                    else if(filter[2] ==0x88){ *buf=0xED; buf++;  }//"よ"　
                    else if(filter[2] ==0x89){ *buf=0xEE; buf++;  }//"ら"
                    else if(filter[2] ==0x8A){ *buf=0xEF; buf++;  }//"り"
                    else if(filter[2] ==0x8B){ *buf=0xF0; buf++;  }//"る"　
                    else if(filter[2] ==0x8C){ *buf=0xF1; buf++;  }//"れ"
                    else if(filter[2] ==0x8D){ *buf=0xF2; buf++;  }//"ろ"　
                    else if(filter[2] ==0x8F){ *buf=0xF3; buf++;  }//"わ"
                    else if(filter[2] ==0x92){ *buf=0xBD; buf++;  }//"を"
                    else if(filter[2] ==0x93){ *buf=0xF4; buf++;  }//"ん"
                    else if(filter[2] ==0x94){ *buf=0xCA; buf++; dFlg=1; }//"ゔ"

                    else if(filter[2] ==0xA0){ *buf=0x3D; buf++;  }//"="
                    else if(filter[2] ==0xA1){ *buf=0x86; buf++;  }//"ァ"　
                    else if(filter[2] ==0xA2){ *buf=0x90; buf++;  }//"ア"
                    else if(filter[2] ==0xA3){ *buf=0x87; buf++;  }//"ィ"　
                    else if(filter[2] ==0xA4){ *buf=0x91; buf++;  }//"イ"
                    else if(filter[2] ==0xA5){ *buf=0x88; buf++;  }//"ゥ"
                    else if(filter[2] ==0xA6){ *buf=0x92; buf++;  }//"ウ"　
                    else if(filter[2] ==0xA7){ *buf=0x89; buf++;  }//"ェ"　
                    else if(filter[2] ==0xA8){ *buf=0x93; buf++;  }//"エ"
                    else if(filter[2] ==0xA9){ *buf=0x8A; buf++;  }//"ォ"
                    else if(filter[2] ==0xAA){ *buf=0x94; buf++;  }//"オ"　
                    else if(filter[2] ==0xAB){ *buf=0x95; buf++;  }//"カ"
                    else if(filter[2] ==0xAC){ *buf=0x95; buf++; dFlg=1; }//"ガ"　
                    else if(filter[2] ==0xAD){ *buf=0x96; buf++;  }//"キ"
                    else if(filter[2] ==0xAE){ *buf=0x96; buf++; dFlg=1; }//"ギ"
                    else if(filter[2] ==0xAF){ *buf=0x97; buf++;  }//"ク"

                    else if(filter[2] ==0xB0){ *buf=0x97; buf++; dFlg=1; }//"グ"
                    else if(filter[2] ==0xB1){ *buf=0x98; buf++;  }//"ケ"　
                    else if(filter[2] ==0xB2){ *buf=0x98; buf++; dFlg=1; }//"ゲ"
                    else if(filter[2] ==0xB3){ *buf=0x99; buf++;  }//"コ"　
                    else if(filter[2] ==0xB4){ *buf=0x99; buf++; dFlg=1; }//"ゴ"
                    else if(filter[2] ==0xB5){ *buf=0x9A; buf++;  }//"サ"
                    else if(filter[2] ==0xB6){ *buf=0x9A; buf++; dFlg=1; }//"ザ"　
                    else if(filter[2] ==0xB7){ *buf=0x9B; buf++;  }//"シ"　
                    else if(filter[2] ==0xB8){ *buf=0x9B; buf++; dFlg=1; }//"ジ"
                    else if(filter[2] ==0xB9){ *buf=0x9C; buf++;  }//"ス"
                    else if(filter[2] ==0xBA){ *buf=0x9C; buf++; dFlg=1; }//"ズ"　
                    else if(filter[2] ==0xBB){ *buf=0x9D; buf++;  }//"セ"
                    else if(filter[2] ==0xBC){ *buf=0x9D; buf++; dFlg=1; }//"ゼ"　
                    else if(filter[2] ==0xBD){ *buf=0x9E; buf++;  }//"ソ"
                    else if(filter[2] ==0xBE){ *buf=0x9E; buf++; dFlg=1; }//"ゾ"
                    else if(filter[2] ==0xBF){ *buf=0x9F; buf++;  }//"タ"
                    }
                else if(filter[1] ==0x83){ //E383:カタカナ2️⃣、和文記号。
                         if(filter[2] ==0x80){ *buf=0x9F; buf++; dFlg=1; }//"ダ"
                    else if(filter[2] ==0x81){ *buf=0xA0; buf++;  }//"チ"　
                    else if(filter[2] ==0x82){ *buf=0xA0; buf++; dFlg=1; }//"ヂ"
                    else if(filter[2] ==0x83){ *buf=0x8E; buf++;  }//"ッ"　
                    else if(filter[2] ==0x84){ *buf=0xA1; buf++;  }//"ツ"
                    else if(filter[2] ==0x85){ *buf=0xA1; buf++; dFlg=1; }//"ヅ"
                    else if(filter[2] ==0x86){ *buf=0xA2; buf++;  }//"テ"　
                    else if(filter[2] ==0x87){ *buf=0xA2; buf++; dFlg=1; }//"デ"
                    else if(filter[2] ==0x88){ *buf=0xA3; buf++;  }//"ト"　
                    else if(filter[2] ==0x89){ *buf=0xA3; buf++; dFlg=1; }//"ド"
                    else if(filter[2] ==0x8A){ *buf=0xA4; buf++;  }//"ナ"
                    else if(filter[2] ==0x8B){ *buf=0xA5; buf++;  }//"ニ"　
                    else if(filter[2] ==0x8C){ *buf=0xA6; buf++;  }//"ヌ"
                    else if(filter[2] ==0x8D){ *buf=0xA7; buf++;  }//"ネ"　
                    else if(filter[2] ==0x8E){ *buf=0xA8; buf++;  }//"ノ"
                    else if(filter[2] ==0x8F){ *buf=0xA9; buf++;  }//"ハ"

                    else if(filter[2] ==0x90){ *buf=0xA9; buf++; dFlg=1; }//"バ"
                    else if(filter[2] ==0x91){ *buf=0xA9; buf++; dFlg=2; }//"パ"　
                    else if(filter[2] ==0x92){ *buf=0xAA; buf++;  }//"ヒ"
                    else if(filter[2] ==0x93){ *buf=0xAA; buf++; dFlg=1; }//"ビ"　
                    else if(filter[2] ==0x94){ *buf=0xAA; buf++; dFlg=2; }//"ピ"
                    else if(filter[2] ==0x95){ *buf=0xAB; buf++;  }//"フ"
                    else if(filter[2] ==0x96){ *buf=0xAB; buf++; dFlg=1; }//"ブ"　
                    else if(filter[2] ==0x97){ *buf=0xAB; buf++; dFlg=2; }//"プ"
                    else if(filter[2] ==0x98){ *buf=0xAC; buf++;  }//"ヘ"　
                    else if(filter[2] ==0x99){ *buf=0xAC; buf++; dFlg=1; }//"ベ"
                    else if(filter[2] ==0x9A){ *buf=0xAC; buf++; dFlg=2; }//"ペ"
                    else if(filter[2] ==0x9B){ *buf=0xAD; buf++;  }//"ホ"　
                    else if(filter[2] ==0x9C){ *buf=0xAD; buf++; dFlg=1; }//"ボ"
                    else if(filter[2] ==0x9D){ *buf=0xAD; buf++; dFlg=2; }//"ポ"　
                    else if(filter[2] ==0x9E){ *buf=0xAE; buf++;  }//"マ"
                    else if(filter[2] ==0x9F){ *buf=0xAF; buf++;  }//"ミ"             

                    else if(filter[2] ==0xA0){ *buf=0xB0; buf++;  }//"ム"
                    else if(filter[2] ==0xA1){ *buf=0xB1; buf++;  }//"メ"　
                    else if(filter[2] ==0xA2){ *buf=0xB2; buf++;  }//"モ"
                    else if(filter[2] ==0xA3){ *buf=0x8B; buf++;  }//"ャ"　
                    else if(filter[2] ==0xA4){ *buf=0xB3; buf++;  }//"ヤ"
                    else if(filter[2] ==0xA5){ *buf=0x8C; buf++;  }//"ュ"
                    else if(filter[2] ==0xA6){ *buf=0xB4; buf++;  }//"ユ"　
                    else if(filter[2] ==0xA7){ *buf=0x8D; buf++;  }//"ョ"　
                    else if(filter[2] ==0xA8){ *buf=0xB5; buf++;  }//"ヨ"
                    else if(filter[2] ==0xA9){ *buf=0xB6; buf++;  }//"ラ"
                    else if(filter[2] ==0xAA){ *buf=0xB7; buf++;  }//"リ"　
                    else if(filter[2] ==0xAB){ *buf=0xB8; buf++;  }//"ル"
                    else if(filter[2] ==0xAC){ *buf=0xB9; buf++;  }//"レ"　
                    else if(filter[2] ==0xAD){ *buf=0xBA; buf++;  }//"ロ"
                    else if(filter[2] ==0xAF){ *buf=0xBB; buf++;  }//"ワ"
                    else if(filter[2] ==0xB2){ *buf=0x85; buf++;  }//"ヲ"
                    else if(filter[2] ==0xB3){ *buf=0xBC; buf++;  }//"ン" 

                    else if(filter[2] ==0xBB){ *buf=0x84; buf++;  }//"・"   
                    else if(filter[2] ==0xBC){ *buf=0x8F; buf++;  }//"ー"                       
                    }        
                }


                str+=2;
            }
        count++; 
        }
        //濁点処理。
        if(dFlg!=0){
            if(dFlg==1){//濁点
                *buf=0xF5; buf++;
            }else if(dFlg==2){//半濁点
                *buf=0xF6; buf++;
            }
            dFlg=0;
        }



        str++;
    }
    *buf='\0';
    //printf("TEXT LENGTH= %d\n",count);
    return p;
}
//*/

///*文字列を判定する(英字のみの場合はそのままこのフィルタを適用可能)
uint8_t getTextIndex(uint8_t chr){
    uint8_t c = 0;
    //文字列判定: 自己定義ASCIIコードに基づいてフィルタをかける。
    //ここで吐き出した数値を参照して文字を描画する！
    //データ送受信上では、ひらがなとカタカナも8byteデータ内に収める。

    //2.和文の場合
    if(chr>= 0x80 ){//>=128
        if(chr<=0xBC ){//<=188
            //2a. 記号及びカタカナの場合
            c = chr-59;

        }else if(chr<=0xF4){//<=244
            //2b. ひらがなの場合
            c = chr-55;
        }else{
            //2c. 濁点の場合
            c = chr-115;
        }

    }
    else if(chr>= 123){
        //記号の場合
        c = chr-58;

    }else if(chr>=97){
        //小文字の場合
        c= chr+94;
    }else{
        //数字、記号、大文字の場合
        c= chr-32;
    }

    return c;

}//*/

char getText(uint8_t x, uint8_t y,uint8_t w){//キーボード入力から文字を返す変数
    uint8_t code = y * w +x; //1次元情報に還元
    char chr = 0;

    //ASCII文字列を使用。
    //元のデータは、0からABC...XYZabc...xyz012...89!#$%&()*+-./:;=?@][_{}
    //ASCIIコードでは、大文字(0~25,+65)/小文字(26~51, +71)/数字(52~61,-4)
    /*特殊記号(厄介！): 
    62,!,33  
    63~66,#$%&,-28  
    67~70,()*+,-27 
    71~72, -. ,-26   
    73, / , 47
    74~75 , :; , -16
    76, =, 61
    77~78, ?@ , -14
    79 , ] , 91
    80, [ , 93
    81, _ , 95
    82,{, 123
    83, }, 125
    */

    if(code <= 25){//大文字
        chr=code+65;
    }else if(code<=51){//小文字
        chr=code+71;
    }else if(code<=61){//数字
        chr=code-4;
    }else{
        switch(code){
            case 62: chr=33; break;
            case 63:
            case 64:
            case 65: 
            case 66:
                chr=code-28;
                break;
            case 67:
            case 68:
            case 69:
            case 70:
                chr=code-27;
                break;
            case 71:
            case 72: 
                chr=code-26; 
                break; 
            case 73: chr=47; break; 
            case 74:
            case 75: 
                chr=code-16; 
                break; 
            case 76: chr=61; break; 
            case 77:
            case 78: 
                chr=code-14; 
                break; 
            case 79: chr=91; break; 
            case 80: chr=93; break; 
            case 81: chr=95; break; 
            case 82: chr=123; break; 
            case 83: chr=125; break;
            default:
                chr=0;
            break;
        }
    }
    return chr;


}


/*概念実証用。GBに焼くときは不使用。
int main(void){

    uint8_t buf[256];//文字コード格納用バッファ
    uint8_t bufs[256];//スプライトコード格納用バッファ

    uint8_t *ans = u8to1byteCode(buf, sampleTex);
    printf("text:%s\n", sampleTex);  // bufに格納された文字列の確認
    int p=0;
    while(p<=255 && buf[p]!='\0'){
        uint8_t put = buf[p];
        if(put<0x80){
            printf("%c ,",put); 
        }else{
            printf("%d ,",put); 
        }
        
        p++;
    }
    printf("\n");
    printf("EOT\n");
    p=0;
     while(p<=255 && buf[p]!='\0'){

        bufs[p]=getTextIndex(buf[p]);
        printf("%03d,",bufs[p]);
        
        p++;
    }   
    printf("\n");
    printf("EOS\n");

    return 0;
    

}//*/