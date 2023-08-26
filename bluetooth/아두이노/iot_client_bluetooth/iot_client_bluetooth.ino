/*
  blue test: 
  http://www.kccistc.net/
  작성일 : 2022.12.19
  작성자 : IoT 임베디드 KSH
*/
#include <SoftwareSerial.h>
#include <Wire.h>
// #include <DHT.h>
#include <MsTimer2.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// RFID
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9
 
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key; 

// Init array that will store new NUID 
byte nuidPICC[4];
char rfid_id[8+1];
char cardUid[] = "E1404A0D";


LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DEBUG
#define CDS_PIN A0
#define BUTTON_PIN 2
// #define DHTPIN 4 
#define LED_BUILTIN_PIN 13
#define SERVO_PIN 5
// 초음파
#define trigPin 2 // Trigger Pin
#define echoPin 3 // Echo Pin
#define minimumRange   1 // 최소 거리
#define maximumRange   30  // 최대 거리

// #define DHTTYPE DHT11
#define ARR_CNT 5
#define CMD_SIZE 60
char lcdLine1[17] = "Smart IoT By KJK";
char lcdLine2[17] = "";
char sendBuf[CMD_SIZE]; // 블루투스로 전송할 내용
char recvId[10] = "KJK_SQL";  // SQL 저장 클라이이언트 ID
bool lastButton = HIGH;       // 버튼의 이전 상태 저장
bool currentButton = HIGH;    // 버튼의 현재 상태 저장
bool ledOn = false;      // LED의 현재 상태 (on/off)
bool timerIsrFlag = false;
unsigned int secCount;
int cds=0;
float humi;
float temp;
bool cdsFlag = false;
int getSensorTime;
unsigned int myservoTime = 0; // 2초 뒤에 꺼버려서 서보가 혼자 흔들리는 것 방지
//초음파
long duration, distance; // 펄스 시간, 거리 측정용 변수

// 서보
int angle;

// int doorOpenTimeCnt = 0;
// char isDoorOpened = 0;

// DHT dht(DHTPIN, DHTTYPE);

// SoftwareSerial BTSerial(10, 11); // 10 ==> HC06:TX, TX ==> HC06:RX
SoftwareSerial BTSerial(8, 7); // D8 ==> HC06:TX, D7 ==> HC06:RX // RFID랑 10, 11 이 겹쳐서 7,8로 바꿈

Servo myservo;



void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("setup() start!");
#endif
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN_PIN, OUTPUT);
 
  BTSerial.begin(9600); // set the data rate for the SoftwareSerial port
  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();
  // dht.begin();

  // 서보
  #define DOOR_OPEN 90
  #define DOOR_CLOSE 0
  myservo.attach(SERVO_PIN);
  myservo.write(DOOR_CLOSE); // 서보 모터가 움직일 각도
  myservoTime = secCount; // 2초 뒤 디태치하여, 흔들림 제거

  // 초음파
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // RFID
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 

  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

}

void loop()
{
  if (BTSerial.available())
    bluetoothEvent();

  

  // 1초 마다 실행
  if (timerIsrFlag)
  {
    timerIsrFlag = false;

    // if (isDoorOpened) doorOpenTimeCnt++;
    // else if (!isDoorOpened) doorOpenTimeCnt = 0;


    // 조도, 온도, 습도 CLCD에 쓰기 & 블루투스로 보내기
//     cds = map(analogRead(CDS_PIN), 0, 1023, 0, 100);
//     humi = dht.readHumidity();
//     temp = dht.readTemperature();
    
//     sprintf(lcdLine2,"C:%d T:%d H:%d", cds, (int)temp, (int)humi);
// #ifdef DEBUG
//     //Serial.println(lcdLine2);
// #endif
//     lcdDisplay(0, 1, lcdLine2);

//     if(getSensorTime != 0 && !(secCount % getSensorTime)) {
//       sprintf(sendBuf,"[%s]SENSOR@%d@%d@%d\n",recvId,cds,(int)temp,(int)humi);
//       BTSerial.write(sendBuf);   
//     }    
    
//     // 조도
//     if ((cds >= 50) && cdsFlag)
//     {
//       cdsFlag = false;
//       sprintf(sendBuf, "[%s]CDS@%d\n", recvId, cds);
//       BTSerial.write(sendBuf, strlen(sendBuf));
//     } 
//     else if ((cds < 50) && !cdsFlag)
//     {
//       cdsFlag = true;
//       sprintf(sendBuf, "[%s]CDS@%d\n", recvId, cds);
//       BTSerial.write(sendBuf, strlen(sendBuf));
//     }

    // 서보
    if (myservo.attached() && myservoTime + 2 == secCount) // 서보 모터 구동한지 2초 지나면
    {
      myservo.detach(); // 서보 전원 차단(흔들림, 노이즈 차단)
    }
    if (angle == DOOR_OPEN) { // 문 열렸음 알림
      sprintf(sendBuf, "[%s]SERVO@%d\n", recvId, angle);
      BTSerial.write(sendBuf, strlen(sendBuf));
    }

    
    // if ( doorOpenTimeCnt % 10 == 0) { // 10초 동안 문 열려 있으면 자동닫기
    //   Serial.println(doorOpenTimeCnt);
    //   myservo.attach(SERVO_PIN);
    //   angle = DOOR_CLOSE;
    //   myservo.write(DOOR_CLOSE);
    // }

    // 초음파
    if (1) {
      digitalWrite(trigPin, LOW); 
      delayMicroseconds(2); 
      
      digitalWrite(trigPin, HIGH);
      delayMicroseconds(10); 
      
      digitalWrite(trigPin, LOW);
      duration = pulseIn(echoPin, HIGH);
      
      // 측정된 시간을 cm로 환산
      distance = duration/58.2;
      
      if (distance >= maximumRange || distance <= minimumRange)
      {
        Serial.println("out of range");
      }
      else 
      {
        Serial.println(distance);
      }

      if ( distance > 3 ) {
        // 문 열림
        // isDoorOpened = 1;
        

        // if (myservo.read() > DOOR_CLOSE) { // 1초마다 동작하기 때문에 명령어로 
        //   myservo.attach(SERVO_PIN);
        //   angle = DOOR_CLOSE;
        //   myservo.write(DOOR_CLOSE);
        // }
      }
      // else {
      //   isDoorOpened = 0;
      // }

    }

  }
  // End 1초마다 실행할 내용

  getRfidId();

//   // 빌트인 LED 디바운스
//   currentButton = debounce(lastButton);   // 디바운싱된 버튼 상태 읽기
//   if (lastButton == HIGH && currentButton == LOW)  // 버튼을 누르면...
//   {
//     ledOn = !ledOn;       // LED 상태 값 반전
//     digitalWrite(LED_BUILTIN_PIN, ledOn);     // LED 상태 변경
//     sprintf(sendBuf, "[%s]BUTTON@%s\n", recvId, ledOn ? "ON" : "OFF");
//     BTSerial.write(sendBuf);
// #ifdef DEBUG
//     Serial.println(sendBuf);
// #endif
//   }
//   lastButton = currentButton;     // 이전 버튼 상태를 현재 버튼 상태로 설정



#ifdef DEBUG
  if (Serial.available())
    BTSerial.write(Serial.read());
#endif
}


// 명령 처리
void bluetoothEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len = BTSerial.readBytesUntil('\n', recvBuf, sizeof(recvBuf) - 1); // \n 직전까지 저장

  if (len == 0) return;

#ifdef DEBUG
  Serial.print("Recv : ");
  Serial.println(recvBuf);
#endif

  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }

  //recvBuf : [XXX_BTM]LED@ON
  //pArray[0] = "XXX_LIN"   : 송신자 ID
  //pArray[1] = "LED"
  //pArray[2] = "ON"
  //pArray[3] = 0x0

  // 수신 값 LCD에 출력
  if ((strlen(pArray[1]) + strlen(pArray[2])) < 16)
  {
    sprintf(lcdLine2, "%s %s", pArray[1], pArray[2]);
    lcdDisplay(0, 1, lcdLine2);
  }

  // LED
  if (!strcmp(pArray[1], "LED")) {
    if (!strcmp(pArray[2], "ON")) {
      digitalWrite(LED_BUILTIN_PIN, HIGH);
    }
    else if (!strcmp(pArray[2], "OFF")) {
      digitalWrite(LED_BUILTIN_PIN, LOW);
    }
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  }
  // GETSENSOR => 조도, 온도, 습도
  // else if(!strcmp(pArray[1],"GETSENSOR"))
  // {
  //   if(pArray[2] == NULL){
  //     getSensorTime = 0;
  //   }else {
  //     getSensorTime = atoi(pArray[2]);
  //     strcpy(recvId,pArray[0]);
  //   }
  //   sprintf(sendBuf,"[%s]SENSOR@%d@%d@%d\n",pArray[0],cds,(int)temp,(int)humi);
  // }

  // Servo
  else if (!strcmp(pArray[1], "SERVO"))
  {
    myservo.attach(SERVO_PIN);
    myservoTime = secCount;
    
    if ( !strcmp(pArray[2], "ON") ) {
      angle = DOOR_OPEN;
      myservo.write(DOOR_OPEN);
    }      
    else if ( !strcmp(pArray[2], "Off") ) {
      angle = DOOR_CLOSE;
      myservo.write(DOOR_CLOSE);
    }
    
    sprintf(sendBuf, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
  }

  // 초음파
  else if (!strcmp(pArray[1], "ULTRASONIC")) {
    sprintf(sendBuf, "[%s]ULTRASONIC@%d\n", recvId, distance);
    BTSerial.write(sendBuf, strlen(sendBuf));
  }
  

  else if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
    return ;
  }
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
    return ;
  }

#ifdef DEBUG
  Serial.print("Send : ");
  Serial.print(sendBuf);
#endif
  BTSerial.write(sendBuf);
}


void timerIsr()
{
  timerIsrFlag = true;
  secCount++;
}


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

/**
RFID
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void getCardIdCharArray(byte *buffer, byte bufferSize) {

  // Serial.print("bufferSize : "); 4 byte
  // Serial.println(bufferSize);  
  for (byte i = 0; i < bufferSize; i++) {
    // Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    // Serial.print(buffer[i], HEX);
    sprintf(rfid_id + (2*i), "%02X", buffer[i]);
  }
  rfid_id[8] = '\0';
  // Serial.print("rfid_id : ");
  Serial.println(rfid_id);
}

void getRfidId() {

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  // Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  // Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&  
    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }

  
  getCardIdCharArray(rfid.uid.uidByte, rfid.uid.size);
  int speakerPin = 6;
  if ( !strcmp(rfid_id, cardUid) ) {
    sprintf(lcdLine2,"%s", "Door Open!");
    lcdDisplay(0, 1, lcdLine2);

    // int tones[] = {261, 277, 294, 311, 330, 349, 370, 392};
    int tones[] = {400, 420, 440};
    for(int i = 0; i < sizeof(tones)/sizeof(tones[0]); i++)
    {
      tone(speakerPin, tones[i]);
      delay(100);
    }
    noTone(speakerPin);

  } 
  else {
    sprintf(lcdLine2,"%s", "house breaking!");
    lcdDisplay(0, 1, lcdLine2);
    tone(speakerPin, 392);
    delay(300);
    noTone(speakerPin);
  }

  // if (rfid.uid.uidByte[0] != nuidPICC[0] || 
  //   rfid.uid.uidByte[1] != nuidPICC[1] || 
  //   rfid.uid.uidByte[2] != nuidPICC[2] || 
  //   rfid.uid.uidByte[3] != nuidPICC[3] ) {
  //   Serial.println(F("A new card has been detected."));

  //   // Store NUID into nuidPICC array
  //   for (byte i = 0; i < 4; i++) {
  //     nuidPICC[i] = rfid.uid.uidByte[i];
  //   }
   
  //   // Serial.println(F("The NUID tag is:"));
  //   // Serial.print(F("In hex: "));
  //   getCardIdCharArray(rfid.uid.uidByte, rfid.uid.size);
  //   // Serial.println();    
  // }
  // else Serial.println(F("Card read previously."));

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}