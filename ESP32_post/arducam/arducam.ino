      // ArduCAM Mini demo (C)2017 Lee
      // Web: http://www.ArduCAM.com
      // This program is a demo of how to use most of the functions
      // of the library with ArduCAM ESP32 2MP/5MP camera.
      // This demo was made for ArduCAM ESP32 2MP/5MP Camera.
      // It can take photo and send to the Web.
      // It can take photo continuously as video streaming and send to the Web.
      // The demo sketch will do the following tasks:
      // 1. Set the camera to JPEG output mode.
      // 2. if server receives "GET /capture",it can take photo and send to the Web.
      // 3.if server receives "GET /stream",it can take photo continuously as video
      //streaming and send to the Web.
      
      // This program requires the ArduCAM V4.0.0 (or later) library and ArduCAM ESP32 2MP/5MP camera
      // and use Arduino IDE 1.8.1 compiler or above
      
      #include <WiFi.h>
      #include <Wire.h>
      #include <ESP32WebServer.h>
      #include <ArduCAM.h>
      #include <SPI.h>
      #include <base64.h>
      #include "memorysaver.h"
      
      #if !(defined ESP32 )
      #error Please select the ArduCAM ESP32 UNO board in the Tools/Board
      #endif
      //This demo can only work on OV2640_MINI_2MP or ARDUCAM_SHIELD_V2 platform.
      #if !(defined (OV2640_MINI_2MP)||defined (OV5640_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP_PLUS) \
                    || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) \
                    ||(defined (ARDUCAM_SHIELD_V2) && (defined (OV2640_CAM) || defined (OV5640_CAM) || defined (OV5642_CAM))))
      #error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
      #endif
      
      // set GPIO17 as the slave select :
      const int CS = 17;
      const int CAM_POWER_ON = 10;
      #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
      ArduCAM myCAM(OV2640, CS);
      #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
      ArduCAM myCAM(OV5640, CS);
      #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
      ArduCAM myCAM(OV5642, CS);
      #endif
      
      //you can change the value of wifiType to select Station or AP mode.
      //Default is AP mode.
      int wifiType = 1; // 0:Station  1:AP
      
      //AP mode configuration
      //Default is arducam_esp8266.If you want,you can change the AP_aaid  to your favorite name
      const char *AP_ssid = "arducam_esp32";
      //Default is no password.If you want to set password,put your password here
      const char *AP_password = NULL;
      
      //Station mode you should put your ssid and password
      const char *ssid = "6s08"; // Put your SSID here
      const char *password = "iesc6s08"; // Put your PASSWORD here
      
      static const size_t bufferSize = 2048*4;
      static uint8_t buffer[bufferSize] = {0xFF};
      uint8_t temp = 0, temp_last = 0;
      int i = 0;
      bool is_header = false;
      const String PREFIX = "{\"content\":\"";
      const String SUFFIX = "\"}";
      String speech_data;
      ESP32WebServer server(80);
      
      void start_capture() {
        myCAM.clear_fifo_flag();
        myCAM.start_capture();
      }
      
      void camCapture(ArduCAM myCAM) {
        Serial.println("incamcapture");
        //WiFiClient client = server.client();
        uint32_t len  = myCAM.read_fifo_length();
        if (len >= MAX_FIFO_SIZE) //8M
        {
          Serial.println(F("Over size."));
        }
        if (len == 0 ) //0 kb
        {
          Serial.println(F("Size is 0."));
        }
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
        //if (!client.connected()) return;
        String response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: image/jpeg\r\n";
        response += "Content-len: " + String(len) + "\r\n\r\n";
        server.sendContent(response);
        Serial.println("response: "+response);
        i = 0;
        while ( len-- )
        {
          temp_last = temp;
          temp =  SPI.transfer(0x00);
          //Read JPEG data from FIFO
          if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
          {
            buffer[i++] = temp;  //save the last  0XD9
            //Write the remain bytes in the buffer
            //if (!client.connected()) break;
            //client.write(&buffer[0], i);
            is_header = false;
            //i = 0;
            myCAM.CS_HIGH();
            break;
          }
          if (is_header == true)
          {
            //Write image data to buffer if not full
            if (i < bufferSize)
              buffer[i++] = temp;
            else
            {
              //Write bufferSize bytes image data to file
              //if (!client.connected()) break;
             // client.write(&buffer[0], bufferSize);
//              i = 0;
//              buffer[i++] = temp;
            }
          }
          else if ((temp == 0xD8) & (temp_last == 0xFF))
          {
            is_header = true;
            buffer[i++] = temp_last;
            buffer[i++] = temp;
          }
        }
        
        long sample_num = 0;
        speech_data = PREFIX;
        speech_data += base64::encode(buffer, i);
         
        speech_data+=SUFFIX;
        Serial.println("speech_data"+speech_data);
//        for (int i  = 0; i<sizeof(buffer); i++){
//          Serial.print(buffer[i]);
//        }
        //Serial.println(buffer);
      }
      
      void serverCapture() {
        delay(1000);
        start_capture();
        Serial.println(F("CAM Capturing"));
      
        int total_time = 0;
      
        total_time = millis();
        while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
        total_time = millis() - total_time;
        Serial.print(F("capture total_time used (in miliseconds):"));
        Serial.println(total_time, DEC);
      
        total_time = 0;
      
        Serial.println(F("CAM Capture Done."));
        total_time = millis();
        camCapture(myCAM);
        total_time = millis() - total_time;
        Serial.print(F("send total_time used (in miliseconds):"));
        Serial.println(total_time, DEC);
        do_POST();
        Serial.println(F("CAM send Done."));
      }
      void do_POST(){
        
        WiFiClient client; //instantiate a client object
       //client.connect("iesc-s1.mit.edu", 80)
        if (client.connect("iesc-s1.mit.edu", 80)) { //try to connect to iesc-s1.mit.edu
          //if you used json, use the first body string. If you used form-encoded...use the second:
      
          client.println("POST /608dev/sandbox/emilycwx/608proj/image_post.py HTTP/1.1"); //CHANGE FOR YOUR SCRIPT
          client.println("Host: iesc-s1.mit.edu");
          //if you used json use the first Content-Type string:
          client.println("Content-Type: application/json");
        
          client.println("Content-Length: " + String(speech_data.length()));
          client.print("\r\n");
          client.print(speech_data);
          unsigned long count = millis();
          while (client.connected()) { //while we remain connected read out data coming back
            String line = client.readStringUntil('\n');
            Serial.println(line);
            if (line == "\r") { //found a blank line!
              //headers have been received! (indicated by blank line)
              break;
            }
            if (millis()-count>6000) break;
          }
          count = millis();
          String op; //create empty String object
          while (client.available()) { //read out remaining text (body of response)
            op+=(char)client.read();
          }
          Serial.println(op);
          client.stop();
     
          Serial.println();
          Serial.println("-----------");
        }else{
          Serial.println("connection failed");
          Serial.println("wait 0.5 sec...");
          client.stop();
          delay(300);
        }
      }   

      void handleNotFound() {
        String message = "Server is running!\n\n";
        message += "URI: ";
        message += server.uri();
        message += "\nMethod: ";
        message += (server.method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += server.args();
        message += "\n";
        server.send(200, "text/plain", message);
        Serial.println(message);
      
      
      
        if (server.hasArg("ql")) {
          int ql = server.arg("ql").toInt();
      #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
          myCAM.OV2640_set_JPEG_size(ql);
      #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
          myCAM.OV5640_set_JPEG_size(ql);
      #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
          myCAM.OV5642_set_JPEG_size(ql);
      #endif
      
          Serial.println("QL change to: " + server.arg("ql"));
        }
      }
      int state;
      int input_pin = 15;
      
      void setup() {
        uint8_t vid, pid;
        uint8_t temp;
        state = 0;
        Serial.begin(115200);
        
        //set the CS as an output:
        pinMode(CS, OUTPUT);
        pinMode(CAM_POWER_ON , OUTPUT);
        digitalWrite(CAM_POWER_ON, HIGH);
      #if defined(__SAM3X8E__)
        Wire1.begin();
      #else
        Wire.begin();
      #endif
        
        Serial.println(F("ArduCAM Start!"));
      
      
      
        // initialize SPI:
        SPI.begin();
        SPI.setFrequency(4000000); //4MHz
      
        //Check if the ArduCAM SPI bus is OK
        myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
        temp = myCAM.read_reg(ARDUCHIP_TEST1);
        if (temp != 0x55) {
          Serial.println(F("SPI1 interface Error!"));
          while (1);
        }
      
        //Check if the ArduCAM SPI bus is OK
        myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
        temp = myCAM.read_reg(ARDUCHIP_TEST1);
        if (temp != 0x55) {
          Serial.println(F("SPI1 interface Error!"));
          while (1);
        }
      #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
        //Check if the camera module type is OV2640
        myCAM.wrSensorReg8_8(0xff, 0x01);
        myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
        myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
        if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 )))
          Serial.println(F("Can't find OV2640 module!"));
        else
          Serial.println(F("OV2640 detected."));
      #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
        //Check if the camera module type is OV5640
        myCAM.wrSensorReg16_8(0xff, 0x01);
        myCAM.rdSensorReg16_8(OV5640_CHIPID_HIGH, &vid);
        myCAM.rdSensorReg16_8(OV5640_CHIPID_LOW, &pid);
        if ((vid != 0x56) || (pid != 0x40))
          Serial.println(F("Can't find OV5640 module!"));
        else
          Serial.println(F("OV5640 detected."));
      #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
        //Check if the camera module type is OV5642
        myCAM.wrSensorReg16_8(0xff, 0x01);
        myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
        myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
        if ((vid != 0x56) || (pid != 0x42)) {
          Serial.println(F("Can't find OV5642 module!"));
        }
        else
          Serial.println(F("OV5642 detected."));
      #endif
      
      
        //Change to JPEG capture mode and initialize the OV2640 module
        myCAM.set_format(JPEG);
        myCAM.InitCAM();
      #if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
        myCAM.OV2640_set_JPEG_size(OV2640_320x240);
      #elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
        myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
        myCAM.OV5640_set_JPEG_size(OV5640_320x240);
      #elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) ||(defined (OV5642_CAM))
        myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
        myCAM.OV5640_set_JPEG_size(OV5642_320x240);
      #endif
      
        myCAM.clear_fifo_flag();
//        if (wifiType == 0) {
//          if (!strcmp(ssid, "SSID")) {
//            Serial.println(F("Please set your SSID"));
//            while (1);
//          }
//          if (!strcmp(password, "PASSWORD")) {
//            Serial.println(F("Please set your PASSWORD"));
//            while (1);
//          }
//          // Connect to WiFi network
//          Serial.println();
//          Serial.println();
//          Serial.print(F("Connecting to "));
//          Serial.println(ssid);
//      
//          WiFi.mode(WIFI_STA);
//          WiFi.begin(ssid, password);
//          while (WiFi.status() != WL_CONNECTED) {
//            delay(500);
//            Serial.print(F("."));
//          }
//          Serial.println(F("WiFi connected"));
//          Serial.println("");
//          Serial.println(WiFi.localIP());
//        } else if (wifiType == 1) {
//          Serial.println();
//          Serial.println();
//          Serial.print(F("Share AP: "));
//          Serial.println(AP_ssid);
//          Serial.print(F("The password is: "));
//          Serial.println(AP_password);
//      
//          WiFi.mode(WIFI_AP);
//          WiFi.softAP(AP_ssid, AP_password);
//          Serial.println("");
//          Serial.println(WiFi.softAPIP());
//        }
      
        // Start the server
//        server.on("/capture", HTTP_GET, serverCapture);
//        server.on("/stream", HTTP_GET, serverStream);
//        server.onNotFound(handleNotFound);
//        server.begin();
        start_wifi();
        Serial.println("should have connected");
          pinMode(input_pin,INPUT_PULLUP);
//        Serial.println(F("Server started"));
      }
      
      void loop() {
//        server.handleClient();
          int value = digitalRead(input_pin);
//          if(state==0){
//            if(!value){//pushed
//              state = 1;
//            }
//          }
//          else if(state == 1){
//            if(value){//unpushed
//              state = 2;
//            }
//          }
//          else if(state==2){
//            
//            //   do_POST();
//            serverCapture();
//            state = 0;
//          }
//          Serial.println(state);
          serverCapture();
          delay(2000);
          
      }
      void start_wifi(){
        WiFi.begin("6s08","iesc6s08");
        //WiFi.begin("MIT"); //attempt to connect to wifi

        int count = 0; //count used for Wifi check times
        while (WiFi.status() != WL_CONNECTED && count<6) {
          delay(500);
          Serial.print(".");
          count++;
        }
        delay(2000);
        if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
          Serial.println(WiFi.localIP().toString() + " (" + WiFi.macAddress() + ") (" + WiFi.SSID() + ")");
          delay(500);
        } else { //if we failed to connect just ry again.
          Serial.println(WiFi.status());
          ESP.restart(); // restart the ESP
        }
//        timer= millis();
//        WiFi.begin("MIT"); //attempt to connect to wifi 
//        Serial.println("trying to connect");
//        int count = 0; //count used for Wifi check times
//        while (WiFi.status() != WL_CONNECTED && count<6) {
//          delay(500);
//          Serial.print(".");
//          count++;
//        }
//        delay(2000);
//        if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
//          Serial.println(WiFi.localIP().toString() + " (" + WiFi.macAddress() + ") (" + WiFi.SSID() + ")");
//          delay(500);
//        } else { //if we failed to connect just try again.
//          Serial.println(WiFi.status());
//          ESP.restart(); // restart the ESP
//        }
      }

      

