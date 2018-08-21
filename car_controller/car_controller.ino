/*
    car_controller.ino
      inlcude controller_helper.ino

    5/1/18
    marcus abate
    mabate@mit.edu
    Team 29

    Client side code for the robot. Controls robot movement, GET requests input from our server. Local commands via button press for testing.
    State machine diagram for this system can be viewed at the following link:
      https://drive.google.com/drive/u/1/folders/1piOiY-60U9nSa9TabEKwyLgaR48rEISD

    NOTES:
      Not super stateful atm because the states are mutually exclusive, so if you're not in the EXEC state then no command will be executed.
        This means that during GPS grabs, IMU grabs, GET and POST requests, the robot will be at a standstill. CONNECT has some statefulness implemented,
        but it still won't allow for command executions in the meantime.
      Still need to implement motor control using the Poloulou board.
      Still need to implement command acquisition from the server.
      Still need to implement gps, imu grabbing.
      Still need to implement coordiate POSTing.

    Library for the DRV8835MotorShield can be found here:
      https://github.com/pololu/drv8835-motor-shield.git
      Add it to you Documents/Arduino/Libraries folder and restart Arduino before compiling.


*/

/* Commands work under this scheme:
    0 no cmd
    1 right
    2 left
    3 forward
    4 reverse
*/

#include <adp5350.h>
#include <U8g2lib.h>
#include <mpu9255_esp32.h>
#include <WiFi.h>
#include <math.h>
#include <EEPROM.h>

#define SCREEN U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI
#define SCREEN_HEIGHT 64
#define SCREEN_WIDTH 128

SCREEN oled(U8G2_R0, 5, 17, 16);
ADP5350 adp;
MPU9255 imu;

int sys_state;
#define START 0
#define CALIB 1
#define CONNECT 2
#define GET_CMD 3
#define EXEC 4
#define GPS 5
#define IMU 6
#define POST 7

#define EEPROM_SIZE 512

const int uS_TO_S_FACTOR = 1000000;

unsigned int post_timer;
unsigned int gps_timer;
unsigned int imu_timer;
unsigned int cmd_timer;

// Timeouts all have units of ms
const int post_timeout = 10*1000;
const int gps_timeout = 120*1000;
const int imu_timeout = 1*1000;
const int cmd_timeout = 1*1000;
const int wifi_timeout = 500;
const int response_timeout = 6000;

// These bools determine state transitions
bool go_post;
bool get_gps;
bool get_cmd;
bool get_imu;
bool is_connected;
bool do_calibrate;
bool reset;
bool got_coords;
bool attempted_connect;

int current_command; // Holds the current command.
int past_command; // Holds the last command.

// The following are for the pwm signals to the motor channels on the motor board.
// Because ESP32 doesn't have analogWrite, we are using ledc
const int pwm_freq = 20000;
const int pwm_ch_1 = 0;
const int pwm_ch_2 = 1;
const int motor_pin_1 = 19;
const int motor_pin_2 = 15;
const int dir_pin_1 = 16;
const int dir_pin_2 = 4;
const int motor_mode_pin = 2;
const int pwm_resolution = 8; // bits
const int motor_speed = 200; // a pwm duty cycle. speed at which we run motors when executing commands.
const int motor_turning_speed = motor_speed/2;

// For testing:
const int loop_speed = 500;
unsigned int loop_timer;

void setup()
{
  Serial.begin(115200);
  Wire.begin(); //begin i2c comms

  // setup power board:
  adp.setCharger(1);
  adp.enableFuelGauge(1);
  adp.enableLDO(1, 1);
  adp.enableLDO(2, 1);
  adp.enableLDO(3, 0);
  delay(1000);

  // set up direction and pwm pins:
  pinMode(dir_pin_1, OUTPUT);
  pinMode(dir_pin_2, OUTPUT);
  pinMode(motor_mode_pin, OUTPUT);
  digitalWrite(motor_mode_pin, HIGH);

  ledcSetup(pwm_ch_1,pwm_freq,pwm_resolution);
  ledcSetup(pwm_ch_2,pwm_freq,pwm_resolution);
  ledcAttachPin(motor_pin_1,pwm_ch_1);
  ledcAttachPin(motor_pin_2,pwm_ch_2);

  // set up states:
  sys_state = START;

  go_post = false;
  get_gps = false;
  get_cmd = true;
  get_imu = false;
  is_connected = false;
  do_calibrate = false;
  reset = false;
  got_coords = false;
  attempted_connect = false;

  WiFi.mode(WIFI_OFF); //Turn off the wifi until we need it.

  Serial.println("Don't forget to use the correct wifi network and the correct acquire_cmd and exec functions.");

  loop_timer = millis();

}

void loop()
{
  Serial.println("state:"+String(sys_state));

  // First we check timers and connectivity to see if any time-based state transitions will be required at the end of this cycle.
  int time_now = millis();
  if (time_now-post_timer >= post_timeout)
    go_post = true;
  if (time_now-gps_timer >= gps_timeout)
    get_gps = true;
  if (time_now-imu_timer >= imu_timeout)
    get_imu = true;
  if (time_now-cmd_timer >= cmd_timeout)
    get_cmd = true;
  if (WiFi.status() == WL_CONNECTED)
    is_connected = true;

  // The switch statement handles the state machine of the system. State transitions are done at the end of each cycle.
  // One state execution occurs each cycle. One state transition occurs each cycle.
  switch (sys_state)
  {
    case START:
      reset = false; // If we've gone to start, we don't need to reset until a command is actually given.

      EEPROM.begin(EEPROM_SIZE);
      EEPROM.get(0,do_calibrate);

      // State transitions:
      if (do_calibrate)
        sys_state = CALIB;
      else
        sys_state = CONNECT;
      break;




    case CALIB:
      calibrate_system();

      // State transitions:
      sys_state = CONNECT;

      break;




    case CONNECT:
      connect_to_wifi();

      // State transitions:
      if (is_connected && get_gps)
        sys_state = GPS;
      else if (is_connected && !get_gps)
        sys_state = GET_CMD;
      else if (!is_connected)
        sys_state = CONNECT;




    case GET_CMD:
      acquire_cmd(); //Use when server GET is working.
      //acquire_cmd_local();

      // State transitions:
      if (get_gps)
        sys_state = GPS;
      else if (reset)
        sys_state = START;
      else if (!is_connected)
        sys_state = CONNECT;
      else
        sys_state = EXEC;
      break;




    case EXEC:
      execute_cmd(); //Use when server GET is working.
      //execute_cmd_local();

      // State transitions:
      if (get_cmd && !get_imu)
        sys_state = GET_CMD;
      else if (get_imu)
        sys_state = IMU;
      else if (!get_cmd)
        sys_state = EXEC;
      break;




    case GPS:
      acquire_gps();

      // State transitions:
      if (got_coords)
        sys_state = POST;
      else if (!got_coords)
        sys_state = GPS;
      sys_state = GET_CMD; //for testing without gps
      break;




    case IMU:
      acquire_imu();
      acquire_encoder();

      // State transitions:
      if (go_post)
        sys_state = POST;
      else if (!go_post)
        sys_state = GET_CMD;
      break;




    case POST:
      post_coords();

      // State transitions:
      sys_state = GET_CMD;
      break;
  }
  // Only for testing:
  while (millis()-loop_timer < loop_speed);
  loop_timer = millis();
}
