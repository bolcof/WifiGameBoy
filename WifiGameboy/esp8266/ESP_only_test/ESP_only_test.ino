/*
9/30 あおむし　SSIDを番号選択にしました
10/1 あおむし　RSSI強度選別可能に。
10/3 RSSI強度ライブ出力を行う。
10/29 ESP頑張るマン
やること：
ESP単体...API関係
*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#include "secrets.h"


struct ssids { //SSIDの構造体を作成。
  String id;
  int rssi;
};

struct ssids mySsids[50];//読み取り用SSID集。SSIDとRSSIから成る構造体。
String ssid =SECRET_SSID;
String password =SECRET_PASS;
int signal_strength; //信号強度を保存しておく。

const char * apiURL = "https://en.wikipedia.org/w/api.php";

char resultData[4096];
char * resultPosition;
boolean resultPending = false;

char serialBuffer[256];
byte serialIndex = 0;


//クイックソート(拾い物)
int pivot(int i, int j) {
    int k = i + 1;
    while (k <= j && mySsids[i].rssi == mySsids[k].rssi) k++;
    if (k > j) return -1;
    if (mySsids[i].rssi >= mySsids[k].rssi) return i;
    return k;
}
int partition(int i, int j, int x) {
    int l = i, r = j;
    while (l <= r) {
        while (l <= j && mySsids[l].rssi < x)  l++;
        while (r >= i && mySsids[r].rssi >= x) r--;
        if (l > r) break;
        String s = mySsids[l].id;
        int rs = mySsids[l].rssi;
        mySsids[l].id = mySsids[r].id;
        mySsids[l].rssi = mySsids[r].rssi;
        mySsids[r].id = s;
        mySsids[r].rssi = rs;
        l++; r--;
    }
    return l;
}
void quickSort(int i, int j) {
    if (i == j) return;
    int p = pivot(i, j);
    if (p != -1) {
        int k = partition(i, j, mySsids[p].rssi);
        quickSort(i, k - 1);
        quickSort(k, j);
    }
}
void sort(int num_data) {
    quickSort(0, num_data - 1);
}
//*/

void setup() {
  Serial.begin(74880);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // Wi-Fiネットワークのスキャンを開始
  Serial.println("Wi-Fiネットワークのスキャンを開始します...");
  int networkCount = WiFi.scanNetworks();
  if (networkCount == 0) {
    Serial.println("利用可能なネットワークが見つかりませんでした。");
  } else {
    Serial.print(networkCount);
    Serial.println(" 個のネットワークを検出しました。");
    //各SSID表示。ここで名前を得られれば見やすい。コマンドの返り値がStringなので期待したい。
    for (int i = 0; i < networkCount; ++i) {
      if(i<50){
        mySsids[i].id=  WiFi.SSID(i); 
        mySsids[i].rssi=WiFi.RSSI(i); 
      }   
    }
    //SSIDをソートする。 拾ってきたソートではindexの若い順に小さい値が入るようなので、GB送付時はindexの末尾から送る。
    Serial.println("Sorting...");    
    sort(networkCount);
    for (int i=0;i<networkCount;i++){
        Serial.printf("ネットワーク %02d : %s %ddB\n", i,mySsids[i].id.c_str(),mySsids[i].rssi);       
    }
    
  }
 // GB側で画像マップを用いた表示を行う場合、それ用に変換した文字列を出す必要がある？
 // wifi gameboyのwilipedia表示部分を参考にする。


  // ユーザーにSSIDとパスワードの入力を促す
  Serial.println("接続したいSSIDを入力してください:");
   while (Serial.available() == 0) {}  // ユーザー入力を待機
    int ssidIndex = Serial.parseInt();   // ユーザー入力を数値として取得
    if (ssidIndex >= 0 && ssidIndex < networkCount) {
      ssid = mySsids[ssidIndex].id;
      Serial.printf("Selected SSID: %s\n", ssid.c_str());
    }
  delay(500);
  Serial.println("パスワードを入力してください:");
  while (Serial.available() == 0) {} // ユーザーの入力を待つ
  password ="" //Serial.readStringUntil('\n'); // 改行まで読み込む
  password.trim(); // 改行文字を削除
  Serial.printf("PASSWORD: %s\n", password.c_str());

  // Wi-Fiに接続
  Serial.println("指定されたネットワークに接続を試みます...");
  WiFi.begin(ssid.c_str(), password.c_str());
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("接続されました!");
  Serial.print("IPアドレス: ");
  Serial.println(WiFi.localIP());
}


char url[256];
void APIparse(){//API接続・解析用関数

      
      
      if(!resultPending){
        Serial.println("検索したい文字を入れてください");
        while (Serial.available() == 0) {} // ユーザーの入力を待つ
        const char *word = Serial.readStringUntil('\n').c_str(); // 改行まで読み込む
        
        strcpy(url, apiURL);
        strcat(url, "?action=query&format=json&titles=");
        strcat(url, word);
        strcat(url, "&prop=extracts&exintro&explaintext");        
      }


      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      http.useHTTP10(true);
      http.begin(client, url);
      int httpCode = http.GET();
      if (httpCode == 200) {
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, http.getStream());
        if (error) {
          Serial.printf("JSON ERROR: %s\n",error.c_str());//エラー文字列出力
          
        } else {
          //情報取得
          JsonObject pages = doc["query"]["pages"];
          JsonObject::iterator it = pages.begin();
          if (it->key() == "-1")
            //strcpy(outbuffer, "Not found.");
            Serial.printf("Not Found\n");
          else {
            strncpy(resultData, it->value()["extract"], 4096);
            resultPending = true;
            resultPosition = resultData;
            strncpy(serialBuffer,resultPosition, 51); //240+null, use multiple of 20 to align with screen width
            if (serialBuffer[50] != '\0') {
              resultPosition += 50;
              serialBuffer[50] = '\0';
            } else
              resultPending = false;
          }
          //出力
          printf("%s\n",serialBuffer);
          //outindex = 0;
          //outFilled = true;
        }
      } else {
        char code[4];
        itoa(httpCode, code, 10);
        printf("ERROR: %s\n",code);
        //strcpy(outbuffer, "Error: ");
        //strcat(outbuffer, code);
        //outindex = 0;
        //outFilled = true;
      }
      http.end();

}

void loop() {
  
  APIparse();

   //接続中のSSIDについて、信号強度を取得する。この処理重そうだからできるだけ避けたい。数値とアンテナの本数をどのように扱うかは今後検討。
  if(millis()%5000==0){//時間変数、使えるのか...？
    int networkCount = WiFi.scanNetworks();
    for (int i = 0; i < networkCount; ++i) {
      String ssid_sample=WiFi.SSID(i);
      if(ssid == ssid_sample){
        signal_strength=WiFi.RSSI(i);
        Serial.printf("%dBm",signal_strength);
        break;
      }
    } 
  } 
}
