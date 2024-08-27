int min_session_minutes = 60; // After this number of minutes, end session
int min_session_presses = 50; // After this number of lever presses, end session

int interval_schedule = 10;
char which_lever = 'l';



/*
These irValue comparisons (<10 being beam broken, >15 being beam unbroken) are
correct for new IR emitter/reciever pairs. Signals may begin to fluctuate around these 
values if the emitter/reciever pairs get degraded (e.g. chewed). This can appear as erratic 
nose in/out signals or as lagging in python recordings
*/

//IR detector pins and stuff
int ir_source = 9;
int detect_power = 8;
int detect_signal = A3;

bool ir_broken = false;
int no_single_ir = 0; //Sometimes IR measurements fluctuate to where there will randomly be a >10 value surrounded by zeros, so we want to eliminate these
int no_single_ir_in = 0; //But if the IR sensor is good, i.e. doesn't throw random large values out, we don't need this scheme and can get better temporal resolution

void measure_ir() {
  int irValue = analogRead(detect_signal);
  //Serial.println(irValue);
  if ((irValue < 20) and (ir_broken == false)) {
    if (no_single_ir_in > 5){
      Serial.print("nose_in ");
      send_report();
      ir_broken = true;
    }
    else {
      no_single_ir_in += 1;
    }
  }
  if (irValue < 20) {
    no_single_ir = 0;
  }
  if ((irValue > 25) and (ir_broken == true)) {
    if (no_single_ir > 5) {
      Serial.print("nose_out ");
      send_report();
      no_single_ir = 0;
      ir_broken = false;
    }
    else {
      no_single_ir += 1;
    }
  }
  if (irValue > 25) {
    no_single_ir_in = 0;
  }
}

/*
First checks for a signal that tells us to dispense food. If so, brings the dispenser 28v line to high and 
starts a timer. Once the timer passes a threshold, the 28v line is dropped back to low and the signal is reset.
*/

// Dispenser pins and stuff
int dispense_control = 11;
//int food_light = 4;
//unsigned long food_light_time;
bool food_signal = false;
unsigned long food_delay_timer;
unsigned long food_delay_thresh = 0; // How long we wait between turning dispenser signal line on and off, just a hardware specification hard-coded in
bool dispense_on = false;
unsigned long dispense_time;

void check_food() {
  if (food_signal) {
    if ((millis() - food_delay_timer) < food_delay_thresh) { // Dispenser requires a rising/falling edge so we time a quick HIGH/LOW switch
      return;
    }
    else if (dispense_on == false) {
      Serial.println("dispensing");
      digitalWrite(dispense_control, HIGH);
      //digitalWrite(food_light, HIGH);
      dispense_on = true;
      dispense_time = millis();
      //food_light_time = millis();  
    }    
    else if ((millis() - dispense_time) > 5){
      digitalWrite(dispense_control, LOW);
      dispense_on = false;    
      food_signal = false;  
    }
  }
}


/*
Toggles a lever_depressed_right/left state on or off
so that a mouse can continuously press a lever without
that registering as multiple presses
*/

// Lever pins and stuff
int left_lever_control = 12;
int left_lever_report = A5;
int left_led = 2;
int right_lever_control = 13;
int right_lever_report = A4;
int right_led = 3;

int num_presses = 0;
int cum_presses = 0;
bool lever_pressed = false;
bool lever_out = false;
int session_press = -1;
bool lever_depressed_right = false;
bool lever_depressed_left = false;

// Multi press scheme
int no_single_lever_right = 0;
int no_single_lever_left = 0;
int no_single_lever_off_right = 0;
int no_single_lever_off_left = 0;
int completed_cycles_right = 0;
int completed_cycles_left = 0;

char high_lever;
bool right_lever = false;
bool left_lever = false;
bool scrambled_state;

bool read_lever(char side){
  if (side == 'r') {
    int sensorValue = analogRead(right_lever_report);
    float voltage = sensorValue * (5.0 / 1023.0);
    if ((voltage < 0.5) && (not lever_depressed_right)) {
      if (no_single_lever_right > 5) {
        lever_depressed_right = true;
        Serial.print("r_pr "); // !!
        send_report();  // !!
        session_press = 1;
        return true;
      }
      else {
        no_single_lever_right += 1;
        no_single_lever_off_right = 0;
      }
    }
    else if ((voltage > 0.5) && (lever_depressed_right)) {
      if (no_single_lever_off_right > 5) {
        completed_cycles_right += 1;
        lever_depressed_right = false; 
      }
      else if (voltage > 0.5) {
      no_single_lever_right = 0;
      no_single_lever_off_right += 1;
      }
    }
  }
  else {
    int sensorValue = analogRead(left_lever_report);
    float voltage = sensorValue * (5.0 / 1023.0);
    if ((voltage < 0.5) && (not lever_depressed_left)) {
      if (no_single_lever_left > 5) {
        lever_depressed_left = true;
        Serial.print("l_pr ");
        send_report();
        session_press = 2;
          return true;
      }
      else {
        no_single_lever_left += 1;
        no_single_lever_off_left = 0;
      }
    }
    else if ((voltage > 0.5) && (lever_depressed_left)) {
      if (no_single_lever_off_left > 5) {
        completed_cycles_left += 1;
        lever_depressed_left = false; 
      }
      else if (voltage > 0.5) {
      no_single_lever_left = 0;
      no_single_lever_off_left += 1;
      }
    }
  }
  return false;
}

void extend_lever(char side){
  if (side == 'r') {
    digitalWrite(right_lever_control, HIGH);
    digitalWrite(right_led, HIGH);
  }
  if (side == 'l') {
    digitalWrite(left_lever_control, HIGH);
    digitalWrite(left_led, HIGH);
  }
  lever_out = true;
}

void retract_lever(char side){
  if (side == 'r') {
    digitalWrite(right_lever_control, LOW);
    digitalWrite(right_led, LOW);
  }
  if (side == 'l') {
    digitalWrite(left_lever_control, LOW);
    digitalWrite(left_led, LOW);
  }
}

/*
Gives time since start of experiment in Min:Sec:Millis
*/
unsigned long start_time;
void send_report() {
  unsigned long initial_mils = millis() - start_time;
  int report_mils = initial_mils % 1000; //What remains after converting to seconds
  int initial_sec = initial_mils / 1000; //Seconds without minutes removed
  int report_min = initial_sec / 60; //Removing minutes
  int report_sec = initial_sec % 60; //What remains after converting to minutes
  char buf[11];
  sprintf(buf,"%02d:%02d:%03d",report_min,report_sec,report_mils);
  Serial.print("time_");
  Serial.println(buf);
}

/* 
Essentially a random number generator
Enter a value in seconds and it will generate
a random number of milliseconds between 0 and that value.
*/ 
long generate_wait_time(int interval_schedule) {
  //long milli_wait = random(interval_schedule * 1000);
  long milli_wait = 1;
  Serial.print("Wait time is ");
  Serial.print(milli_wait);
  Serial.println(" milliseconds");
  return milli_wait;
}

/*
Inactive period

Take a millis reading and use that to make sure we just hang
around waiting until wait_time milliseconds have passed. While
waiting, we just make sure to take some measurements and to reset
the food signal if it is still on.
*/
void waiting(long wait_time) {
  long start_waiting = millis();
  while ((millis() - start_waiting) < wait_time) {
    measure_ir();
    read_lever(which_lever);
    check_food();
    check_if_done();
  }
  //Serial.println("Done waiting");  // Remove before use
}

/*
We enter this active waiting loop once the inactive period finishes.
Once the lever is hit, we signal the dispenser and return to the start.
*/
void waiting_active() {
  Serial.print("Actively waiting...");  // Remove before use
  while (true) {
    bool lever_status = read_lever(which_lever);
    if (lever_status) {
      break;
    }
    measure_ir();
    check_food();
    check_if_done();
  }
  food_signal = true;
  food_delay_timer = millis();
  cum_presses += 1;
  check_food();
}

void check_if_done() {
  bool done = false;
  float mins_elapsed = (millis() - start_time) / 60000.0;
  if (mins_elapsed >= min_session_minutes) {
    Serial.println("END - session timed out");
    done = true;
  }
  else if (cum_presses >= min_session_presses) {
    Serial.print("END - max number of rewards delivered");
    done = true;
  }
  if (done) {
    retract_lever(which_lever);
    Serial.print("Finished at ");
    send_report();
    Serial.print("Number of rewards: ");
    Serial.println(cum_presses);
    Serial.println("FINISHED");
    while (true); { 
      bool i = true;
    }
  }
}


void setup() {
  Serial.begin(9600);

  //Lever and dispenser pins and stuff
  pinMode(left_lever_control, OUTPUT);
  pinMode(left_lever_report, INPUT);
  pinMode(left_led, OUTPUT);

  pinMode(right_lever_control, OUTPUT);
  pinMode(right_lever_report, INPUT);
  pinMode(right_led, OUTPUT);

  pinMode(dispense_control, OUTPUT);
  //IR detector pins and stuff
  pinMode(ir_source, OUTPUT);
  pinMode(detect_power, OUTPUT);
  pinMode(detect_signal, INPUT);
  digitalWrite(ir_source, HIGH);
  digitalWrite(detect_power, HIGH);
  /*  
  For the sake of trying to get video and arduino time together
  We wait until we get a ready to go signal
  */
  Serial.println("Waiting...");
  while (true) {
    char r_t_s = Serial.read();
    if (r_t_s == 'g') {
      Serial.println("ready");  
      break;
    }
  }

  start_time = millis();
  Serial.println("entering loop");
  extend_lever(which_lever);
}

void loop() {
  long wait_time = generate_wait_time(interval_schedule);
  waiting(wait_time);
  waiting_active();
}

