/*
9/30 あおむし　SSIDを番号選択にしました
10/1 あおむし　RSSI強度選別可能に。
10/3 RSSI強度ライブ出力を行う。
やること：


ESP単体...API関係
toGB...データ送受信

問題：
10/12 GB通信のためのピン設定をコピったら再起動を繰り返す無能になりおった、コイツ...!
また、while内で指示なし拘束が1秒程度あるとき再起動をすることがわかったので、delay(50)を挟んでいる。

10/25 久々に触った。パスワードを受け取った時の処理を作成。
コードを簡単にするため、SSIDとパスワードの対を改行記号で区切って受け取ることに。
SSID送付できない問題は、GB側で読み取りをループさせることで対応を図る。
バッファサイズは512バイトで統一。
10/26 微調整。Serialを無効化。接続エラー発生時の覚書を記入。
10/29 gb側からの信号でSSIDループを行う機能を実装。

*/

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

char dummyConn[] = "MYSSID\nMyPassword";

WiFiServer server(4242);
WiFiClient client;

struct ssids { //SSIDの構造体を作成。
  String id;
  int rssi;
};

struct ssids mySsids[50];//読み取り用SSID集。SSIDとRSSIから成る構造体。
String ssid;
String password;
int signal_strength; //信号強度を保存しておく。
boolean isFirsttime;//setup内に入れられない初期処理を行うためのフラグ。

//GB通信用変数群
const uint16_t dataMask = 0xf00f;
const byte pinEspRd = 4;
const uint16_t pinEspRdMask = 1 << pinEspRd;
const byte pinEspClk = 5;
const uint16_t pinEspClkMask = 1 << pinEspClk;
const byte pinLED = 16;

char outbuffer[512];
volatile int outindex;
volatile bool outFilled = false;
char inbuffer[512];
volatile int inindex;
volatile bool inFilled = false;


char serialbuffer[512];
int serialIndex = 0;

//GB通信用変数群　おわり


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
  //Serial.begin(74880);
  delay(500);//Just to avoid anything triggering during the somewhat "electrically turbulent" start.

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 
  isFirsttime=true;


  //バッファを末尾文字で初期化 Init buffers to terminal character
  outbuffer[0] = '\0';
  outindex = 0;
  outFilled = false;
  inbuffer[0] = '\0';
  inindex = 0;
  inFilled = false;
  
  //Setup pins
  for (int i = 0; i < 16; i++) {
    if ((0x0001 << i) & dataMask) {
      pinMode(i, INPUT_PULLUP);
    }
  }//*/
  
  ///*//デバッグ時の可読性のためのLED
  pinMode(pinEspRd, INPUT);
  pinMode(pinEspClk, INPUT);
  pinMode(pinLED, OUTPUT);
  //*/
  //Interrupts
  //DO NOT TOUCH espRd INTERRUPTS!
  //Especially the logic of enabling the GPIO output in response to the interrupts
  //must match the behavior of the Game Boy program. The Game Boy may never try to
  //write to the ESP while the GPIO output is enabled or we will get bus contention
  //(i.e. a short circuit that might damage something)
  
  attachInterrupt(digitalPinToInterrupt(pinEspClk), espClkFall, FALLING);
  attachInterrupt(digitalPinToInterrupt(pinEspRd), espRdFall, FALLING);
}

//Get CPU cycle, based on https://sub.nanona.fi/esp8266/timing-and-ticks.html
static inline int32_t asm_ccount(void) {
    int32_t r;
    asm volatile ("rsr %0, ccount" : "=r"(r));
    return r;
}

//Variables controlling interrupt logic
volatile int32_t lastRd = 0;  //Number of clock cycles on the last ESPRD fall used to reject triggers of espClkFall while reading espRdFall is doing its thing.
volatile int32_t lastClk = 0; //Number of clock cycles on the last ESPCLK fall used to reject the trigger on the second event.
volatile boolean readingFromESP = false;

//DO NOT TOUCH INTERRUPTS unless you know what you are doing! (see above)　通信用の割り込み処理です。触らないように！
//When the ESPRD input rises, the Game Boy wants to read from the ESP
//We are way to slow to respond to the first read request and the Game Boy
//will have to drop the first response as it will be undefined. However, we
//will use the read request as a trigger to output the next character and
//the Game Boy just needs to read slow enough so the ESP can keep up.
//When we set \0 as output, this is the last character of our data stream,
//but we disable the output when the rd line rises after that, because that
//is when the Game Boy has seen this \0 and also expects us to stop.
ICACHE_RAM_ATTR void espRdFall() {
  lastRd = asm_ccount();
  if (!readingFromESP) {
    //First read attempt. Enable output and start with first character
    readingFromESP = true;
    GPES = dataMask; //Enable all data pins
  }
  if (outFilled) {
    //Send current character and move to the next one
    char data = outbuffer[outindex++];
    uint32_t pinData = ((data & 0x00f0) << 8) | (data & 0x0f);
    GPOS = pinData & dataMask;
    GPOC = ~pinData & dataMask;
    if (data == '\0') {
      outFilled = false;
    }
  } else if (outindex == 0) {
    //There is no data to send, but we need to send \0 anyways, so the Game Boy knows that we are done.
    GPOC = dataMask;
    outindex++;
  } else {
    //The \0 has been sent last time. Let's turn off the output and end this transfer
    GPEC = dataMask; //Disable all data pins
    readingFromESP = false;
    outindex = 0;
    return;
  }
}

//DO NOT TOUCH INTERRUPTS unless you know what you are doing! (see above) 通信用の割り込み処理です。触らないように！
//The ESPCLK falls in the middle of a R/W operation. If the Game Boy is
//reading from the ESP, this is handled in espRdRise, which sets
//readingFromESP = true. In this case, this interrupt here will be ignored.
//If the Game Boy tries to write to the ESP, however, everything becomes a
//bit more tricky as the interrupt delay is a multiple of a single memory
//access cycle. Therefore, the Game Boy will do three consecutive write
//attempts at equidistant intervals. We will use the first two to figure out
//the timing of the write attempts (which will include our own interrupt
//delay), so we can try to exactly intercept the third attempt.
//We also need to ignore too fast interrupt triggers, because there is a
//short additional spike of ESPCLK at the end of each cycle (although I
//doubt that it can trigger the interrupt) and we will not try to directly
//catch subsequent write attempts as we can be lucky if this method is
//precise enough to capture the third write :)
ICACHE_RAM_ATTR void espClkFall() {
  int32_t clk = asm_ccount();
  if (readingFromESP)
    return;
  if ((uint32_t)(clk - lastRd) < 800) //Reject if (dt < 10µs), the ESP8266 has an 80MHz clock, so one cycle is 12.5ns
    //The thing is that even starting the interrupt routine can take several µs, so if this has been triggered together with the last call to espRdFall, an unwanted call might come in still a few µs after that
    return;
  if ((uint32_t)(clk - lastClk) < 2400) //Reject if (dt < 30µs), the ESP8266 has an 80MHz clock, so one cycle is 12.5ns
    //We don't want to trigger again on the second write
    return;

  //Wait for rising edge with a timeout, so this does not block forever if we miss it
  while ((uint32_t)(asm_ccount() - clk) < 1400) {
    if (GPI & pinEspClkMask)
      break;
  }
  //Wait for falling edge. (No need for a timeout. ESPRD only stays HIGH for a few 100ns.
  while (GPI & pinEspClkMask) {
    //NOOP
  }
  uint32_t gpi = GPI; //Catch the new value
  
  if (!inFilled) {
    //Only accept new data if the buffer is not already filled
    char c = ((gpi & 0xf000) >> 8) | (gpi & 0x0f);
    inbuffer[inindex] = c;
    if (c == '\0') {
      if (inindex > 0)
        inFilled = true;
    } else
      inindex++;
  }
  lastClk = clk;
  
}

unsigned char signal_classification(int sig){//アンテナのための、RSSI信号強度振り分け。
  unsigned char c = 0;
  if(sig>-50){
    c=4;
  }else if(sig>-80){
    c=3;
  }else if(sig>-100){
    c=2;
  }else{
    c=1;
  }
  return c;
}
void RSSIfeed(){//RSSI送信

    int networkCount = WiFi.scanNetworks();
    for (int i = 0; i < networkCount; ++i) {
      String ssid_sample=WiFi.SSID(i);
      if(ssid == ssid_sample){
        signal_strength=WiFi.RSSI(i);
        //Serial.printf("%dBm",signal_strength);
        outbuffer[0]=signal_classification(signal_strength);
        outbuffer[1]='\0';
        outFilled=true;
        break;
      }
    } 
}


void maintainWifi() { //wifi維持機能
  int status = WiFi.status();
  while (status != WL_CONNECTED) {//接続遮断時、同じSSIDで再接続を行う。
    digitalWrite(pinLED, LOW);

    status = WiFi.begin(ssid, password);

    int pauses = 0;
    while (status != WL_CONNECTED && pauses < 20) {
      delay(500);
      pauses++;
      if (pauses % 2 == 0)
        digitalWrite(pinLED, LOW);
      else
        digitalWrite(pinLED, HIGH);
      status = WiFi.status();
    }

    digitalWrite(pinLED, LOW);

    server.begin();
    client = server.available();
  }
}

void handleDataFromGameBoy() {//受信データの整形。
  if (inFilled) {
    uint8_t i = 0;
    while (inbuffer[i] != '\0') {
      serialbuffer[i]=inbuffer[i];//cilentに依存せず、変数を保存できる。
      client.write(inbuffer[i]);
      i++;
      if (i == 0)
        break;
    }

    i++;
    serialbuffer[i]=('\0');//終端文字設定。
    client.write('\n');//こっちはTELNETコンソールへの送信用なので改行で終わる。

    inindex = 0;
    inFilled = false;
  }
}

void handleDataFromTcp() {//送信データの整形
  if (outFilled || outindex > 0)//出力データがある、または出力中の場合。
    return; //There already is a message we need to print first.

  while (client.available() > 0) {//出力バッファへの書き込み
    outbuffer[outindex++] = client.read();//クライアントという言葉が見えるだろうか？この関数はネット接続後のみ使えるようだ。
  }
  outbuffer[outindex] = '\0';
  outindex = 0;
  outFilled = true;
}
void writetoGB(char s[]){//gbに任意の文字列を送信する関数。行儀が悪いので使い所注意
  int i=0;
  while(s[i] != '\0'){
    outbuffer[outindex++]=s[i++];
  }
    outbuffer[outindex] = '\0';
    outindex = 0;
    outFilled = true;
}

String textExtractor(char s[]){//文字列抽出装置
  int i=0;
  String tex;
  while(s[i]!='\0'){
    tex+=s[i];
    i++;
  }
  return tex;
}

void connenctionInfoExtractor(char s[]){//パス対抽出装置
  bool isPass =false;
  int i;
  while(s[serialIndex]!='\0'){//文字列受信中の処理
    if(s[serialIndex]=='\n'){//改行文字を挟んで二つのデータが来る。その先頭がSSID。ここではSSIDの終端設定処理を行い、パスワード文字列へ入力対象を変更する。
      isPass=true;
      serialIndex++;
      ssid[i]='\0'; //SSID終端文字記入
      i=0;
    }

    if(!isPass){ //SSID
      ssid[i]=s[serialIndex];
    }else{  //pass
      password[i]=s[serialIndex];
    } 
    serialIndex++;
    i++;
  }
  password[i]='\0'; //パスワードに終端文字書き込み
  serialIndex=0; //読み取りインデックスの初期化
}

void SSIDlookup(){
//初期処理,SSIDの獲得含む。
  int networkCount = WiFi.scanNetworks(); // Wi-Fiネットワークのスキャンを開始
  if (networkCount == 0) {
    //Serial.println("利用可能なネットワークが見つかりませんでした。");
  } else {
    //Serial.print(networkCount);
    //Serial.println(" 個のネットワークを検出しました。");
    for (int i = 0; i < networkCount; ++i) {//SSID読み取り
      if(i<50){
        mySsids[i].id=  WiFi.SSID(i); 
        mySsids[i].rssi=WiFi.RSSI(i); 
      }   
    }
    //SSIDをソートする。 拾ってきたソートではindexの若い順に小さい値が入るようなので、GB送付時はindexの末尾から送る。 
    sort(networkCount);
    /*
    for (int i=0;i<networkCount;i++){
        Serial.printf("ネットワーク %02d : %s %ddB\n", i,mySsids[i].id.c_str(),mySsids[i].rssi);
    }//*/

    //送信文字列作成処理。clientを介さないで送るためこの形式。 
     outindex = 0;//念のため出力文字列index初期化   
     outbuffer[outindex++]== 'S';//先頭に接頭詞添付
     for(int i=networkCount-1; i>networkCount-11; i--){//末尾が強いので末尾から送る。
          int j=0;
          while(mySsids[i].id[j]!='\0'){//String終端文字'¥0'まで文字列を呼び出す
            //Serial.print(mySsids[i].id[j]);
            outbuffer[outindex++]=mySsids[i].id[j++]; 
          }
           //Serial.print('\n');
           outbuffer[outindex++]='\n';//改行文字挿入       
     }
    //送信文字列作成終了処理。この後インタラプトから送信が行われる。
      outbuffer[outindex] = '\0';
      outindex = 0;
      outFilled = true;
  }
  // ユーザーにSSIDとパスワードの入力を促す
  //Serial.println("接続したいSSIDを入力してください:");
  bool LTIKA=false;
   while (inbuffer[0]=='\0') {// 今回はGBからの文字列情報の完パケ待機
    delay(600);
    ///*デバッグ用
    if(!LTIKA){
      digitalWrite(pinLED, HIGH); LTIKA=true;
    }else{
      digitalWrite(pinLED, LOW); LTIKA=false;
    }//*/
  }  

   handleDataFromGameBoy();//受け取りデータを内部バッファに回収
   connenctionInfoExtractor( serialbuffer );//接続情報獲得
   
  //Serial.printf("SSID: %s\n", ssid.c_str()); //デバッグ
  //Serial.printf("PASSWORD: %s\n", password.c_str());//デバッグ

  // Wi-Fiに接続
  //Serial.println("指定されたネットワークに接続を試みます...");
  WiFi.begin(ssid.c_str(), password.c_str());
  byte counter =0;
  while (WiFi.status() != WL_CONNECTED) {
    counter++;
    delay(500);
    //Serial.print(".");
    //ここにエラー処理を入れる。タイムアウト10秒以上とかで再接続を要求する。エラー種類がわかればそれを送る。
    //この場合、wifiリストの取得からやり直しさせる。Wi-Fiに接続以前の部分はメソッドとして用意した方がいいかもしれない。
    if(counter>20){
        writetoGB("CONNECTION FAILED");
        return;
    }
  }
  //Serial.println("接続されました!");
  //Serial.print("IPアドレス: ");
  //Serial.println(WiFi.localIP());   
}

void loop() {
  if(isFirsttime){
    //SSIDlookup();
    isFirsttime=false;
  }
  
  handleDataFromTcp();
  handleDataFromGameBoy(); 

  //この辺API叩きマン介入

  //リクエスト受信時のSSID作成
  if(serialbuffer[0] =='S'){
    char sigF[4]={serialbuffer[0],serialbuffer[1], serialbuffer[2],serialbuffer[3]};
    if(sigF[0]=='S'&&sigF[1]=='S'&&sigF[2]=='I'&&sigF[3]=='D'){
      digitalWrite(pinLED, HIGH);
      SSIDlookup();
      digitalWrite(pinLED, LOW);
    }
  }

  //接続中のSSIDについて、信号強度を取得する。この処理重そうだからできるだけ避けたい。数値とアンテナの本数をどのように扱うかは今後検討。
  if(WiFi.status() == WL_CONNECTED && millis()%5000==0){//時間変数、使えるのか...？
      RSSIfeed();
  }




  if(password[0]!='\0')maintainWifi(); //接続維持処理:WiFi
  //TELNET用処理
    if (!client) {
    client = server.available();
  } else if (!client.connected()) {
    client.stop();
    client = server.available();
  }
  //*/
}



