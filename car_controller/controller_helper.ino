/*
    controller_helper.ino

    5/1/18
    marcus abate
    mabate@mit.edu
    Team 29

    Helper code for the client side of the robot. Contains functions used to execute the state machine.
    See car_controller.ino for the state machine and higher-level organization, as well as function calls and notes.

*/


// Comes from remotetempmonitor, may need to be changed for our purposes. Sets up the imu module for our usage.
void setup_imu()
{
  while (imu.readByte(MPU9255_ADDRESS, WHO_AM_I_MPU9255) != 0x73)
    Serial.println("NOT FOUND");

  imu.initMPU9255();
  imu.MPU9255SelfTest(imu.selfTest);
  imu.calibrateMPU9255(imu.gyroBias, imu.accelBias);
  imu.initMPU9255();
  imu.initAK8963(imu.factoryMagCalibration);
  imu.getAres(); //call this so the IMU internally knows its range/resolution
}

// Calibrates the system to a fresh state by clearing EEPROM, resetting bools and timers, calibrating the imu.
void calibrate_system()
{
  setup_imu();

  /* //Grandfathered in from remotetempmonitor for manual calibrations. Use if you have a spare button:
  pinMode(input_pin,INPUT_PULLUP);
  if (digitalRead(input_pin) == 0)
  {
    Serial.println("calibrating");
    do_calibrate = true;
  }
  */

  Serial.println("calibrating...");
  //clear EEPROM:
  for (int i = 0 ; i < EEPROM_SIZE ; i++)
    EEPROM.write(i, 0);
  EEPROM.commit();

  post_timer = millis();
  gps_timer = millis();
  imu_timer = millis();
  cmd_timer = millis();

  do_calibrate = false;
  EEPROM.put(0,do_calibrate);
  EEPROM.commit();
}

// Connects to the control server.
void connect_to_wifi()
{
  if (!attempted_connect)
  {
    Serial.println("Attempting from scratch");
    WiFi.mode(WIFI_STA);
    //WiFi.begin("MIT"); //Use when at home
    WiFi.begin("6s08","iesc6s08"); //use when in class
    attempted_connect = true;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    is_connected = true;
    attempted_connect = false;
  }
  else
    is_connected = false;

   Serial.println("is_connected:" + String(is_connected));

}

// For testing. Uses buttons on the ESP32 board to simulate commands.
// To use, hold down a command button until it registers via serial.
/*
void acquire_cmd_local()
{
  int right_pin = 23;
  int left_pin = 21;
  int fwd_pin = 15;
  int rev_pin = 2;

  pinMode(right_pin,INPUT_PULLUP);
  pinMode(left_pin,INPUT_PULLUP);
  pinMode(fwd_pin,INPUT_PULLUP);
  pinMode(rev_pin,INPUT_PULLUP);

  past_command = current_command;

  if (digitalRead(right_pin) == 0)
    current_command = 1;
  else if (digitalRead(left_pin) == 0)
    current_command = 2;
  else if (digitalRead(fwd_pin) == 0)
    current_command = 3;
  else if (digitalRead(rev_pin) == 0)
    current_command = 4;
  else
    current_command = 0; //This shouldn't happen. Something's gone wrong if this has happened.
}

// For testing. Prints current command to serial based on what current_command is.
void execute_cmd_local()
{
  Serial.print("\n\ngitCurrent Command:  ");
  switch (current_command)
  {
    case 0 : Serial.print("STOP\n"); break;
    case 1 : Serial.print("RIGHT\n"); break;
    case 2 : Serial.print("LEFT\n"); break;
    case 3 : Serial.print("FWD\n"); break;
    case 4 : Serial.print("REV\n"); break;
  }
}
*/

// Sends a GET request to our server to acquire the latest command to be executed.
// This version of acquire_cmd() works with the control server.
void acquire_cmd()
{
  WiFiClient client; //instantiate a client object
  if (client.connect("iesc-s1.mit.edu", 80)) //try to connect
  {
    // This will send the request to the server
    // If connected, fire off HTTP GET:
    String get_url = "GET http://iesc-s1.mit.edu/608dev/sandbox/ch3nj/team29/commands.py?type=command HTTP/1.1"; /* FIX THIS */
    client.println(get_url);
    client.println("Host: iesc-s1.mit.edu");
    client.print("\r\n");
    unsigned long count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      String line = client.readStringUntil('\n');
      Serial.println(line);
      if (line == "\r") { //found a blank line!
        //headers have been received! (indicated by blank line)
        break;
      }
      if (millis()-count>response_timeout) break;
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

    // parsing the output to get our useful quantities:
    past_command = current_command;

    if (op == "stop")
      current_command = 0;
    if (op == "right")
      current_command = 1;
    if (op == "left")
      current_command = 2;
    if (op == "forward")
      current_command = 3;
    if (op == "back")
      current_command = 4;
  }
  else
  {
    Serial.println("connection failed");
    Serial.println("wait 0.5 sec...");
    client.stop();
    delay(500);
    /****************************************** IF this happens too often, neeed an out ******************************************/
  }
  //WiFi.mode(WiFi_OFF); // Probably never needs to be turned off for our application
}

// Sends signal to motor board to execute a command by instructing the motors appropriately.
// This version works with the server control.
void execute_cmd()
{
  switch (current_command)
  {
    case 0 : digitalWrite(dir_pin_1, HIGH);
             digitalWrite(dir_pin_2, LOW);
             ledcWrite(pwm_ch_1, 0);
             ledcWrite(pwm_ch_2, 0);
             Serial.println("Stopping");
             break;

    case 1 : digitalWrite(dir_pin_1, LOW);
             digitalWrite(dir_pin_2, HIGH);
             ledcWrite(pwm_ch_1, motor_turning_speed);
             ledcWrite(pwm_ch_2, motor_turning_speed);
             Serial.println("Going Right");
             break;

    case 2 : digitalWrite(dir_pin_1, HIGH);
             digitalWrite(dir_pin_2, LOW);
             ledcWrite(pwm_ch_1, motor_turning_speed);
             ledcWrite(pwm_ch_2, motor_turning_speed);
             Serial.println("Going Left");
             break;

    case 3 : digitalWrite(dir_pin_1, HIGH);
             digitalWrite(dir_pin_2, HIGH);
             ledcWrite(pwm_ch_1, motor_speed);
             ledcWrite(pwm_ch_2, motor_speed);
             Serial.println("Going Forward");
             break;

    case 4 : digitalWrite(dir_pin_1, LOW);
             digitalWrite(dir_pin_2, LOW);
             ledcWrite(pwm_ch_1, motor_speed);
             ledcWrite(pwm_ch_2, motor_speed);
             Serial.println("Going Backward");
             break;
  }
}

// Updates coordinates based on GPS fix.
void acquire_gps()
{
  /******************************** NEED TO FIGURE OUT HOW THIS WORKS ********************************/
}

// Updates coordinates based on IMU dead reckoning.
void acquire_imu()
{
  /******************************** NEED TO FIGURE OUT HOW THIS WORKS ********************************/
}

void acquire_encoder()
{
  /******************************** NEED TO FIGURE OUT HOW THIS WORKS ********************************/
}

// Posts coordinates and other info to the server for data processing.
void post_coords()
{
  /******************************** NEED TO FIGURE OUT HOW THIS WORKS ********************************/
}
