/*
  WiFiEsp test: ClientTest
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#define DEBUG
//#define DEBUG_WIFI

#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

#define AP_SSID "embA"
#define AP_PASS "embA1234"
#define SERVER_NAME "10.10.14.53"
#define SERVER_PORT 5000
#define LOGID "KJK_ARD"
#define PASSWD "PASSWD"

#define CDS_PIN A0
#define BUTTON_PIN 2
#define LED_LAMP_PIN 3
#define DHTPIN 4
#define SERVO_PIN 5
#define WIFIRX 6  //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX
#define LED_BUILTIN_PIN 13

#define CMD_SIZE 50 // read, write 할 커맨드 사이즈 [수신자]LAMP@ON
#define ARR_CNT 5 // command에서 특수 문자 뺀 스트링 5개 보낼 수 있도록 지정
#define DHTTYPE DHT11 // 온습도 센서
bool timerIsrFlag = false;
boolean lastButton = LOW;     // 버튼의 이전 상태 저장
boolean currentButton = LOW;    // 버튼의 현재 상태 저장
boolean ledOn = false;      // LED의 현재 상태 (on/off)
boolean cdsFlag = false;

char sendBuf[CMD_SIZE];
char lcdLine1[17] = "Smart IoT By KJK"; // "" 묶으면 널 포함되어서 16+1
char lcdLine2[17] = "WiFi Connecting!";

int cds = 0;
unsigned int secCount;
unsigned int myservoTime = 0; // 2초 뒤에 꺼버려서 서보가 혼자 흔들리는 것 방지

char getSensorId[10]; // 파이에서 [KJK_ARD]GETSENSOR@5 하면 5초마다 데이터 보내줌(조도, 온습도 ...)
int sensorTime; // 위 명령어에서 지정한 초
float temp = 0.0; // 온도
float humi = 0.0; // 습도
bool updatTimeFlag = false; // RTC, 파이와 시간 동기화
typedef struct {
  int year;
  int month;
  int day;
  int hour;
  int min;
  int sec;
} DATETIME;
DATETIME dateTime = {0, 0, 0, 12, 0, 0}; // 년월일 시분초 초기화
DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo myservo;

void setup() {
  lcd.init();
  lcd.backlight(); // 백라이트 온
  lcdDisplay(0, 0, lcdLine1); // x축, y축
  lcdDisplay(0, 1, lcdLine2); // 2line

  pinMode(CDS_PIN, INPUT);    // 조도 핀을 입력으로 설정 (생략 가능)
  pinMode(BUTTON_PIN, INPUT);    // 버튼 핀을 입력으로 설정 (생략 가능)
  pinMode(LED_LAMP_PIN, OUTPUT);    // LED 핀을 출력으로 설정
  pinMode(LED_BUILTIN_PIN, OUTPUT); //D13

#ifdef DEBUG
  Serial.begin(115200); //DEBUG
#endif
  wifi_Setup();

  MsTimer2::set(1000, timerIsr); // 1000ms period 마다(1초) timerIsr 실행
  MsTimer2::start(); // main loop 가 돌다가 1초 되면 멈추고, timerIsr 실행
  // 부저는 타이머1

  myservo.attach(SERVO_PIN);
  myservo.write(0); // 서보 모터가 움직일 각도
  myservoTime = secCount; // 2초 뒤 디태치하여, 흔들림 제거
  dht.begin(); // 온습도 센서
}

void loop() {
  if (client.available()) { // 와이파이에 수신된 메시지가 있으면(서버가 보내준거)
    socketEvent(); // 문자열 파싱해서 주변 장치 동작 시킴
  }

  if (timerIsrFlag) // 1초에 한번씩 실행할 내용
  {
    timerIsrFlag = false;
    if (!(secCount % 5)) //5초에 한번씩 실행
    {

      cds = map(analogRead(CDS_PIN), 0, 1023, 0, 100); // 0~100%로 변환
      if ((cds >= 50) && cdsFlag)
      {
        cdsFlag = false;
        sprintf(sendBuf, "[%s]CDS@%d\n", LOGID, cds);
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        //        digitalWrite(LED_BUILTIN_PIN, HIGH);     // LED 상태 변경
      } 
      else if ((cds < 50) && !cdsFlag) // 50보다 떨어지면 50 넘기전까지 보내지마라
      {
        cdsFlag = true;
        sprintf(sendBuf, "[%s]CDS@%d\n", LOGID, cds);
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
        //        digitalWrite(LED_BUILTIN_PIN, LOW);     // LED 상태 변경
      }
      humi = dht.readHumidity(); // 5초마다 온도 읽기
      temp = dht.readTemperature();
#ifdef DEBUG
      /*      Serial.print("Cds: ");
            Serial.print(cds);
            Serial.print(" Humidity: ");
            Serial.print(humi);
            Serial.print(" Temperature: ");
            Serial.println(temp);
      */
#endif
      if (!client.connected()) {
        lcdDisplay(0, 1, "Server Down");
        server_Connect(); // 서버가 꺼지면 클라는 5초마다 접속 시도
      }
    }
    if (myservo.attached() && myservoTime + 2 == secCount) // 서보 모터 구동한지 2초 지나면
    {
      myservo.detach(); // 서보 전원 차단(흔들림, 노이즈 차단)
    }
    if (sensorTime != 0 && !(secCount % sensorTime ))
    {
      sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi);
      /*      char tempStr[5];
            char humiStr[5];
            dtostrf(humi, 4, 1, humiStr);  //50.0 4:전체자리수,1:소수이하 자리수
            dtostrf(temp, 4, 1, tempStr);  //25.1
            sprintf(sendBuf,"[%s]SENSOR@%d@%s@%s\r\n",getSensorId,cdsValue,tempStr,humiStr);
      */
      client.write(sendBuf, strlen(sendBuf));
      client.flush();
    }
    sprintf(lcdLine1, "%02d.%02d  %02d:%02d:%02d", dateTime.month, dateTime.day, dateTime.hour, dateTime.min, dateTime.sec );
    lcdDisplay(0, 0, lcdLine1); // 갱싱된 시간을 1초마다 출력
    if (updatTimeFlag)
    {
      client.print("[GETTIME]\n");
      updatTimeFlag = false;
    }
  }

  // 스위치 누를 때 => 센서 값으로 처리하도록 변경
  currentButton = debounce(lastButton);   // 디바운싱된 버튼 상태 읽기
  if (lastButton == LOW && currentButton == HIGH)  // 버튼을 누르면...
  {
    ledOn = !ledOn;       // LED 상태 값 반전
    digitalWrite(LED_BUILTIN_PIN, ledOn);     // LED 상태 변경
    //    sprintf(sendBuf,"[%s]BUTTON@%s\n",LOGID,ledOn?"ON":"OFF");
    sprintf(sendBuf, "[HM_CON]GAS%s\n", ledOn ? "ON" : "OFF");
    client.write(sendBuf, strlen(sendBuf));
    client.flush();
  }
  lastButton = currentButton;     // 이전 버튼 상태를 현재 버튼 상태로 설정

}

// 명령 처리
void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0}; // 문자열을 포인터 배열에 저장, 원소하나가 문자열의 주소(원소 하나다 어절 하나), 총 5 단어
  char recvBuf[CMD_SIZE] = {0}; // 수신된 명령어
  int len;

  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE); // 백슬레시 n 이전까지 문자열 50바트 저장(\n은 저장안된다는 뜻)
  client.flush(); // 남은 메시지가 있으면 전부 보내라
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]"); // [, @, ] => 3문자로 분리
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  //[KSH_ARD]LED@ON : pArray[0] = "KSH_ARD", pArray[1] = "LED", pArray[2] = "ON"
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16)
  {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2); // 수신된 명령어 출력
  }
  if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    strcpy(lcdLine2, "Server Connected");
    lcdDisplay(0, 1, lcdLine2);
    updatTimeFlag = true;
    return ;
  }
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop(); // 또 접속하면 기존 소켓 닫고, 새로 접속
    server_Connect();
    return ;
  }
  else if (!strcmp(pArray[1], "LED")) { // 여기서부터 명령어 시작
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_BUILTIN_PIN, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_BUILTIN_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  } 
  // LAMP
  else if (!strcmp(pArray[1], "LAMP")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_LAMP_PIN, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_LAMP_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  } 
  else if (!strcmp(pArray[1], "GETSTATE")) { // GETSTATE@LED : LED 상태 체크
    if (!strcmp(pArray[2], "LED")) {
      sprintf(sendBuf, "[%s]LED@%s\n", pArray[0], digitalRead(LED_BUILTIN_PIN) ? "ON" : "OFF");
    }
  }
  // Servo
  else if (!strcmp(pArray[1], "SERVO"))
  {
    myservo.attach(SERVO_PIN);
    myservoTime = secCount;
    if (!strcmp(pArray[2], "ON"))
      myservo.write(180);
    else
      myservo.write(0);
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);

  }
  // GETSENSOR // loop에서 시간마다 보내는 처리
  else if (!strncmp(pArray[1], "GETSENSOR", 9)) {
    if (pArray[2] != NULL) {
      sensorTime = atoi(pArray[2]);
      strcpy(getSensorId, pArray[0]);
      return;
    } else {
      sensorTime = 0; // 0초마다 => 1번 보내고 종료
      sprintf(sendBuf, "[%s]%s@%d@%d@%d\n", pArray[0], pArray[1], cds, (int)temp, (int)humi);
    }
  }
  // GETTIME
  else if(!strcmp(pArray[0],"GETTIME")) {  // 모두 2자리로 처리
    dateTime.year = (pArray[1][0]-0x30) * 10 + pArray[1][1]-0x30 ;
    dateTime.month =  (pArray[1][3]-0x30) * 10 + pArray[1][4]-0x30 ;
    dateTime.day =  (pArray[1][6]-0x30) * 10 + pArray[1][7]-0x30 ;
    dateTime.hour = (pArray[1][9]-0x30) * 10 + pArray[1][10]-0x30 ;
    dateTime.min =  (pArray[1][12]-0x30) * 10 + pArray[1][13]-0x30 ;
    dateTime.sec =  (pArray[1][15]-0x30) * 10 + pArray[1][16]-0x30 ;
#ifdef DEBUG
    sprintf(sendBuf,"\nTime %02d.%02d.%02d %02d:%02d:%02d\n\r",dateTime.year,dateTime.month,dateTime.day,dateTime.hour,dateTime.min,dateTime.sec );
    Serial.println(sendBuf);
#endif
    return;
  } 
  else
    return; // 여기서 없는 명령어는 무시하고 return

  client.write(sendBuf, strlen(sendBuf)); // 와이파이 모듈이 느려서 client.write는 최소 0.5초 간격이 필요(시리얼 와이파이 특성)
  // 메시지가 길면 @로 한번에 보내라
  client.flush();

#ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}
void timerIsr()
{
  timerIsrFlag = true;
  secCount++;
  clock_calc(&dateTime); // 시계 카운트 증가
}
void clock_calc(DATETIME *dateTime) // 연월일시 들어있는 구조체
{
  int ret = 0;
  dateTime->sec++;          // increment second // 60이 되면 분 증가

  if(dateTime->sec >= 60)                              // if second = 60, second = 0
  { 
      dateTime->sec = 0;
      dateTime->min++; 
             
      if(dateTime->min >= 60)                          // if minute = 60, minute = 0
      { 
          dateTime->min = 0;
          dateTime->hour++;                               // increment hour
          if(dateTime->hour == 24) 
          {
            dateTime->hour = 0;
            updatTimeFlag = true; // main에서 true면 서버에 겟타임 호출(자정에 호출)
          }
       }
    }
}

void wifi_Setup() {
  wifiSerial.begin(19200); // ESP 8266의 baud rate
  wifi_Init();
  server_Connect();
}
void wifi_Init()
{
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) { // 통신이 안된다면
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present"); // 통신이 안된다는 문자열
#endif
    }
    else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
  sprintf(lcdLine1, "ID:%s", LOGID); // 16문자만 써야함
  lcdDisplay(0, 0, lcdLine1);
  sprintf(lcdLine2, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  lcdDisplay(0, 1, lcdLine2);

#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}


int server_Connect()
{
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("["LOGID":"PASSWD"]");
  }
  else
  {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}


void printWifiStatus()
{
  // print the SSID of the network you're attached to

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// 기존값 지우고 새로 쓰기
void lcdDisplay(int x, int y, char * str)
{
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}
boolean debounce(boolean last)
{
  boolean current = digitalRead(BUTTON_PIN);  // 버튼 상태 읽기
  if (last != current)      // 이전 상태와 현재 상태가 다르면...
  {
    delay(5);         // 5ms 대기
    current = digitalRead(BUTTON_PIN);  // 버튼 상태 다시 읽기
  }
  return current;       // 버튼의 현재 상태 반환
}
