/*
|このファイルでは文字列判定を行います。
| ほとんどASCIIコード(２バイト文字だけ独自の番号をつけます)に則って記述される文字について、
| 同じ見た目の文字列を我々のチップセット(font.c)で表示できるようにするためのものです。
| 今回は文字を符号なしcharとして取り扱うため、受信信号は0x00~0xffの値を取り得ます。
| (なので、ESP文字列受け渡し~GB内でただのcharが出るのはまずい！注意！！)


判定ルールはこちら：
0x80~0xffを和文使用領域とする！
128~186 (0x81~0xB8) はカタカナ(及び横棒)
187~239(0xB9~0xB8) はひらがな
240,241(0xB9,0xB10)は濁点及び半濁点。
ESP内で文字列を整形する。
case総当たりでやってもらう。
(容量が許せばgb側にも配置して概念実証を行う)

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
ESP：特殊文字(×,¥)を(*,\)に変形
ESP：独自文字コード変換実行
ESP：GBへ送信
✅GB：独自文字コードからタイルアドレスに変換(ここまでESPでもよい)
GB：タイルアドレスによる文字表示

*概念実証時は、全てをGB内で処理してよい。

*/



#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h> 


unsigned char textCodeMod(unsigned char chr){
   unsigned char c = 0; 

    //1.特殊文字(×,¥)の場合の変形
    if(chr == (uint8_t)165)c=92; //¥ => '\'
    if(chr == (uint8_t)215)c=42; //× => *

    switch(chr){
        //濁点除去済みの和文を変形
        case'。':c = (unsigned char)128;break;
        case'「':c = (unsigned char)129;break;
        case'」':c = (unsigned char)130;break;
        case'、':c = (unsigned char)131;break;
        case'・':c = (unsigned char)132;break;
        case'ヲ':c = (unsigned char)133;break;
        case'ァ':c = (unsigned char)134;break;
        case'ィ':c = (unsigned char)135;break;
        case'ゥ':c = (unsigned char)136;break;
        case'ェ':c = (unsigned char)137;break;
        case'ォ':c = (unsigned char)138;break;
        case'ャ':c = (unsigned char)139;break;
        case'ュ':c = (unsigned char)140;break;
        case'ョ':c = (unsigned char)141;break;
        case'ッ':c = (unsigned char)142;break;
        case'ー':c = (unsigned char)143;break;
        case'ア':c = (unsigned char)144;break;
        case'イ':c = (unsigned char)145;break;
        case'ウ':c = (unsigned char)146;break;
        case'エ':c = (unsigned char)147;break;
        case'オ':c = (unsigned char)148;break;
        case'カ':c = (unsigned char)149;break;
        case'キ':c = (unsigned char)150;break;
        case'ク':c = (unsigned char)151;break;
        case'ケ':c = (unsigned char)152;break;
        case'コ':c = (unsigned char)153;break;
        case'サ':c = (unsigned char)154;break;
        case'シ':c = (unsigned char)155;break;
        case'ス':c = (unsigned char)156;break;
        case'セ':c = (unsigned char)157;break;
        case'ソ':c = (unsigned char)158;break;
        case'タ':c = (unsigned char)159;break;
        case'チ':c = (unsigned char)160;break;
        case'ツ':c = (unsigned char)161;break;
        case'テ':c = (unsigned char)162;break;
        case'ト':c = (unsigned char)163;break;
        case'ナ':c = (unsigned char)164;break;
        case'ニ':c = (unsigned char)165;break;
        case'ヌ':c = (unsigned char)166;break;
        case'ネ':c = (unsigned char)167;break;
        case'ノ':c = (unsigned char)168;break;
        case'ハ':c = (unsigned char)169;break;
        case'ヒ':c = (unsigned char)170;break;
        case'フ':c = (unsigned char)171;break;
        case'ヘ':c = (unsigned char)172;break;
        case'ホ':c = (unsigned char)173;break;
        case'マ':c = (unsigned char)174;break;
        case'ミ':c = (unsigned char)175;break;
        case'ム':c = (unsigned char)176;break;
        case'メ':c = (unsigned char)177;break;
        case'モ':c = (unsigned char)178;break;
        case'ヤ':c = (unsigned char)179;break;
        case'ユ':c = (unsigned char)180;break;
        case'ヨ':c = (unsigned char)181;break;
        case'ラ':c = (unsigned char)182;break;
        case'リ':c = (unsigned char)183;break;
        case'ル':c = (unsigned char)184;break;
        case'レ':c = (unsigned char)185;break;
        case'ロ':c = (unsigned char)186;break;
        case'ワ':c = (unsigned char)187;break;
        case'ン':c = (unsigned char)188;break;

        case'を':c = (unsigned char)189;break;
        case'ぁ':c = (unsigned char)190;break;
        case'ぃ':c = (unsigned char)191;break;
        case'ぅ':c = (unsigned char)192;break;
        case'ぇ':c = (unsigned char)193;break;
        case'ぉ':c = (unsigned char)194;break;
        case'ゃ':c = (unsigned char)195;break;
        case'ゅ':c = (unsigned char)196;break;
        case'ょ':c = (unsigned char)197;break;
        case'っ':c = (unsigned char)198;break;

        case'あ':c = (unsigned char)200;break;
        case'い':c = (unsigned char)201;break;
        case'う':c = (unsigned char)202;break;
        case'え':c = (unsigned char)203;break;
        case'お':c = (unsigned char)204;break;
        case'か':c = (unsigned char)205;break;
        case'き':c = (unsigned char)206;break;
        case'く':c = (unsigned char)207;break;
        case'け':c = (unsigned char)208;break;
        case'こ':c = (unsigned char)209;break;
        case'さ':c = (unsigned char)210;break;
        case'し':c = (unsigned char)211;break;
        case'す':c = (unsigned char)212;break;
        case'せ':c = (unsigned char)213;break;
        case'そ':c = (unsigned char)214;break;
        case'た':c = (unsigned char)215;break;
        case'ち':c = (unsigned char)216;break;
        case'つ':c = (unsigned char)217;break;
        case'て':c = (unsigned char)218;break;
        case'と':c = (unsigned char)219;break;
        case'な':c = (unsigned char)220;break;
        case'に':c = (unsigned char)221;break;
        case'ぬ':c = (unsigned char)222;break;
        case'ね':c = (unsigned char)223;break;
        case'の':c = (unsigned char)224;break;
        case'は':c = (unsigned char)225;break;
        case'ひ':c = (unsigned char)226;break;
        case'ふ':c = (unsigned char)227;break;
        case'へ':c = (unsigned char)228;break;
        case'ほ':c = (unsigned char)229;break;
        case'ま':c = (unsigned char)230;break;
        case'み':c = (unsigned char)231;break;
        case'む':c = (unsigned char)232;break;
        case'め':c = (unsigned char)233;break;
        case'も':c = (unsigned char)234;break;
        case'や':c = (unsigned char)235;break;
        case'ゆ':c = (unsigned char)236;break;
        case'よ':c = (unsigned char)237;break;
        case'ら':c = (unsigned char)238;break;
        case'り':c = (unsigned char)239;break;
        case'る':c = (unsigned char)240;break;
        case'れ':c = (unsigned char)241;break;
        case'ろ':c = (unsigned char)242;break;
        case'わ':c = (unsigned char)243;break;
        case'ん':c = (unsigned char)244;break;
    }

}

uint8_t getTextIndex(unsigned char chr){
    unsigned char c = 0;
    //文字列判定: 自己定義ASCIIコードに基づいてフィルタをかける。
    //ここで吐き出した数値を参照して文字を描画する！
    //データ送受信上では、ひらがなとカタカナも8byteデータ内に収める。



    //2.和文の場合
    if(chr>= (uint8_t)128 ){
        if(chr<=(uint8_t)188 ){
            //2a. 記号及びカタカナの場合
            c = chr-(uint8_t)59;

        }else if(chr<=(uint8_t)239){
            //2b. ひらがなの場合
            c = chr-(uint8_t)57;
        }else{
            //2c. 濁点の場合
            c = chr-(uint8_t)110;
        }

    }
    else if(chr>= (uint8_t)123){
        //記号の場合
        c = chr-(uint8_t)58;

    }else if(chr>=(uint8_t)97){
        //小文字の場合
        c= chr+(uint8_t)92;
    }else{
        //数字、記号、大文字の場合
        c= chr-(uint8_t)32;
    }

}