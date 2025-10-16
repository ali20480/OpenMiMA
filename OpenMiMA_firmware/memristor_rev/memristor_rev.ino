//
//    FILE: AD5593R_test_dac.ino
//  AUTHOR: Rob Tillaart + modified by ChatGPT
// PURPOSE: test dac mode + command-driven parallel pulsing + ADC read command
//     URL: https://github.com/RobTillaart/AD5593R
//

#include "AD5593R.h"
#include "Wire.h"

AD5593R AD(0x11);

int out_1_sw = 5;
int out_2_sw = 8;
int out_3_sw = 7;
int out_4_sw = 6;
int out_5_sw = 9;
int out_6_sw = 12;
int out_7_sw = 11;
int out_8_sw = 10;

int in_1_sw = 30;
int in_2_sw = 32;
int in_3_sw = 34;
int in_4_sw = 36;
int in_5_sw = 40;
int in_6_sw = 42;
int in_7_sw = 46;
int in_8_sw = 44;

const int outSwitchPins[8] = {out_1_sw, out_2_sw, out_3_sw, out_4_sw, out_5_sw, out_6_sw, out_7_sw, out_8_sw};
const int inSwitchPins[8] = {in_1_sw, in_2_sw, in_3_sw, in_4_sw, in_5_sw, in_6_sw, in_7_sw, in_8_sw};

// Define analog pins for channels 1-8, adjust if needed
const int analogInputs[8] = {A1, A2, A3, A11, A10, A9, A8, A0};

float Vref_dac = 1.5;
float virt_gnd = 0.75;
float pulse_volt_dac = 1.2;
int pulse_dac_level = (int) ((pulse_volt_dac * 4095) / Vref_dac);
float read_volt_dac = 0.8;
int read_dac_level = (int) ((read_volt_dac * 4095) / Vref_dac);

float half_pos_pulse_volt_dac = 1.3; //0.975; 1.05
int half_pos_pulse_dac_level = (int) ((half_pos_pulse_volt_dac * 4095) / Vref_dac);

float half_neg_pulse_volt_dac = 0.525; //0.525; 0.5
int half_neg_pulse_dac_level = (int) ((half_neg_pulse_volt_dac * 4095) / Vref_dac);

float voltage_pos = 1.45; //1.45 1.9
int dacValue_pos = (int) ((voltage_pos * 2047) / 3.3);

float voltage_neg = 1; //2.2 0 
int dacValue_neg = (int) ((voltage_neg * 2047) / 3.3);

float min_due_volt = 0.545;

float max_due_volt = 2.739;

void setup()
{
  while (!Serial);
  Serial.begin(115200);
  Serial.println();
  Serial.println(__FILE__);
  Serial.print("AD5593R_LIB_VERSION: ");
  Serial.println(AD5593R_LIB_VERSION);
  Serial.println();

  analogWriteResolution(12);
  analogReadResolution(12);

  //float voltage = 2.2; //2.2
  //int dacValue = (int) ((voltage * 2047) / 3.3);
  //analogWrite(DAC1, dacValue);

  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  // Setup all switch pins
  for (int i = 0; i < 8; i++) {
    pinMode(outSwitchPins[i], OUTPUT);
    pinMode(inSwitchPins[i], OUTPUT);
    digitalWrite(outSwitchPins[i], LOW);
    digitalWrite(inSwitchPins[i], LOW);
  }

  Wire.begin();

  Serial.print("Connect: ");
  Serial.println(AD.isConnected());
  Serial.print("Address: ");
  Serial.println(AD.getAddress(), HEX);

  // DAC setup
  AD.setDACmode(0xFF);
  AD.setExternalReference(true, 1.5);
  AD.setDACRange2x(false);
  AD.setLDACmode(AD5593R_LDAC_DIRECT);

  // Read back default values
  for (int pin = 0; pin < 8; pin++)
  {
    Serial.print(AD.readDAC(pin), HEX);
    Serial.print("\t");
  }
  Serial.println();
  Serial.println();

  for (int pin = 0; pin < 8; pin++)
  {
    Serial.print(AD.writeDAC(pin, read_dac_level));
    Serial.print("\t");
  }
  Serial.println();

  Serial.println("Ready to receive commands:");
  Serial.println(" - INFEREMULTIPLE:");
  Serial.println(" - WEIGHTMODIF");
  Serial.println(" - WEIGHTMODIFALL");
  Serial.println();

}

void loop()
{
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.startsWith("INFEREMULTIPLE:")) {
      String dataLine = cmd.substring(strlen("INFEREMULTIPLE:"));
      dataLine.trim();

      // Parse first argument: number of vectors
      int startIdx = 0;
      while (startIdx < dataLine.length() && isspace(dataLine.charAt(startIdx))) startIdx++;
      int endIdx = startIdx;
      while (endIdx < dataLine.length() && !isspace(dataLine.charAt(endIdx))) endIdx++;
      int numVectors = dataLine.substring(startIdx, endIdx).toInt();
      startIdx = endIdx;

      if (numVectors <= 0) {
        Serial.println("Error: INFEREMULTIPLE requires N > 0");
        return;
      }

      int totalExpected = numVectors * 8;
      int dacValues[totalExpected];
      int count = 0;

      // Parse the DAC values
      while (startIdx < dataLine.length() && count < totalExpected) {
        while (startIdx < dataLine.length() && isspace(dataLine.charAt(startIdx))) startIdx++;
        if (startIdx >= dataLine.length()) break;
        endIdx = startIdx;
        while (endIdx < dataLine.length() && !isspace(dataLine.charAt(endIdx))) endIdx++;
        dacValues[count++] = constrain(dataLine.substring(startIdx, endIdx).toInt(), 0, 4095);
        startIdx = endIdx;
      }

      if (count != totalExpected) {
        Serial.print("Error: Expected ");
        Serial.print(totalExpected);
        Serial.println(" DAC values after N.");
        return;
      }

      Serial.print("Running inference for ");
      Serial.print(numVectors);
      Serial.println(" input vectors...");

      // Store all results here
      float results[numVectors][8];

      for (int v = 0; v < numVectors; v++) {
        // Apply one vector's DAC levels
        for (int ch = 0; ch < 8; ch++) {
          digitalWrite(inSwitchPins[ch], LOW);
          digitalWrite(outSwitchPins[ch], LOW);
        }

        delay(5);

        for (int ch = 0; ch < 8; ch++) {
          AD.writeDAC(ch, dacValues[v * 8 + ch]);
          delay(5);
        }

        float sumReadings[8] = {0};
        float N_avg = 20;

        for (int i = 0; i < N_avg; i++) {

          for (int ch = 0; ch < 8; ch++) {
            digitalWrite(outSwitchPins[ch], HIGH);
          }

          delay(5); //10

          for (int ch = 0; ch < 8; ch++) {
            int reading = analogRead(analogInputs[ch]);
            sumReadings[ch] += reading;
            delay(5);
          }


          for (int ch = 0; ch < 8; ch++) {
            digitalWrite(outSwitchPins[ch], LOW);
          }

          delay(5);


        }


        for (int ch = 0; ch < 8; ch++) {
          results[v][ch] = sumReadings[ch] / N_avg;
        }
      }

      // Print all results
      Serial.println("INFEREMULTIPLE ADC Averages:");
      for (int v = 0; v < numVectors; v++) {
        for (int ch = 0; ch < 8; ch++) {
          Serial.print(results[v][ch], 2);
          Serial.print("\t");
        }
        Serial.println();
      }
      Serial.println("INFEREMULTIPLE complete.");
    }


    else if (cmd.startsWith("WEIGHTMODIF:")) {
      String dataLine = cmd.substring(strlen("WEIGHTMODIF:"));
      dataLine.trim();

      int i, j, sign, pulses;
      float v_plus_1, v_minus_1, v_plus_2, v_minus_2;
      int delay_;
      int count = 0;
      float values[9];

      int startIdx = 0;
      int len = dataLine.length();

      while (startIdx < len && count < 9) {
        while (startIdx < len && isspace(dataLine.charAt(startIdx))) startIdx++;
        if (startIdx >= len) break;

        int endIdx = startIdx;
        while (endIdx < len && !isspace(dataLine.charAt(endIdx))) endIdx++;

        String token = dataLine.substring(startIdx, endIdx);

        if (count < 4){
          values[count++] = token.toInt();
        }
        else{
          values[count++] = token.toFloat();
        }
        

        startIdx = endIdx;
      }



      i = (int) values[0];
      j = (int) values[1];
      sign = (int) values[2];
      pulses = (int) values[3];
      v_plus_1 = values[4];
      v_minus_1 = values[5];
      v_plus_2 = values[6];
      v_minus_2 = values[7];
      delay_ = (int) values[8];

      /*
      Serial.println(i);
      Serial.println(j);
      Serial.println(sign);
      Serial.println(pulses);

      Serial.println(v_plus_1);
      Serial.println(v_minus_1);
      Serial.println(v_plus_2);
      Serial.println(v_minus_2);
      Serial.println(delay_);
      */
      

      if (i < 0 || i > 7 || j < 0 || j > 7 || (sign != 1 && sign != -1) || pulses <= 0) {
        Serial.println("Invalid arguments. Indices 0–7, sign 1/-1, pulses > 0");
        return;
      }


      if (sign == 1){

          float half_pos_pulse_volt_dac = v_plus_1; //0.99 ; //0.975; 1.05 0.85
          int half_pos_pulse_dac_level = (int) ((half_pos_pulse_volt_dac * 4095) / Vref_dac);

          float voltage_neg = (v_minus_1 - 0.545) / 0.665; //0.3; //2.2 0 
          int dacValue_neg = (int) ((voltage_neg * 4095) / 3.3);



          AD.writeDAC(i, half_pos_pulse_dac_level);
          delay(5); // small delay to settle DAC


          analogWrite(DAC1, dacValue_neg);
          delay(5);
          

      }
      else{

        float half_neg_pulse_volt_dac = v_plus_2; //0.628; //0.525; 0.5 64 0.643
        int half_neg_pulse_dac_level = (int) ((half_neg_pulse_volt_dac * 4095) / Vref_dac);

        float voltage_pos = (v_minus_2 - 0.545) / 0.665; //1.4; //1.45 1.9 94 1.35
        int dacValue_pos = (int) ((voltage_pos * 4095) / 3.3);

    

          AD.writeDAC(i, half_neg_pulse_dac_level);
          delay(5); // small delay to settle DAC

          analogWrite(DAC1, dacValue_pos);
          delay(5);

      }

      for (int ii = 0; ii < 7; ii++){
        digitalWrite(outSwitchPins[ii], LOW);
        digitalWrite(inSwitchPins[ii], LOW);
      }
      delay(5);

      fastPulse(outSwitchPins[i], inSwitchPins[j], pulses, delay_); 
      /*
      for (int p = 0; p < pulses; p++) {
        digitalWrite(outSwitchPins[i], HIGH);
        digitalWrite(inSwitchPins[j], HIGH);
        delayMicroseconds(10);
        digitalWrite(outSwitchPins[i], LOW);
        digitalWrite(inSwitchPins[j], LOW);
        delayMicroseconds(10);
      }
      */

    }

    else if (cmd.startsWith("WEIGHTMODIFALL:")) {
      String dataLine = cmd.substring(strlen("WEIGHTMODIFALL:"));
      dataLine.trim();

      int values[128];
      float voltages[64];
      int count = 0;
      int startIdx = 0;
      int len = dataLine.length();

      while (startIdx < len && count < 128) {
        while (startIdx < len && isspace(dataLine.charAt(startIdx))) startIdx++;
        if (startIdx >= len) break;

        int endIdx = startIdx;
        while (endIdx < len && !isspace(dataLine.charAt(endIdx))) endIdx++;

        String token = dataLine.substring(startIdx, endIdx);
        values[count++] = token.toInt();
        startIdx = endIdx;
      }

        // --- Step 2: Parse 64 floats (voltages) ---
      int voltageCount = 0;
      while (startIdx < len && voltageCount < 64) {
        while (startIdx < len && isspace(dataLine.charAt(startIdx))) startIdx++;
        if (startIdx >= len) break;

        int endIdx = startIdx;
        while (endIdx < len && !isspace(dataLine.charAt(endIdx))) endIdx++;

        String token = dataLine.substring(startIdx, endIdx);
        voltages[voltageCount++] = token.toFloat();
        startIdx = endIdx;
      }

      // --- Error Checking ---
      if (count != 128 || voltageCount != 64) {
        Serial.println("Error: WEIGHTMODIFALL requires 128 integers + 64 floats.");
        return;
      }

      // Load into 2D arrays
      int signs[8][8];
      int pulses[8][8];
      float voltMatrix[8][8];
      int idx = 0;
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          signs[i][j] = values[idx++];
          //pulses[i][j] = values[idx++];
        }
      }

      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          //signs[i][j] = values[idx++];
          pulses[i][j] = values[idx++];
        }
      }


      idx = 0;
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          voltMatrix[i][j] = voltages[idx++];
        }
      }

    int indices_i[8];
    int indices_j[8];

    // Initialize index arrays
    for (int idx = 0; idx < 8; idx++) {
      indices_i[idx] = idx;
      indices_j[idx] = idx;
    }

    // Shuffle index arrays once
    shuffleArray(indices_i, 8);
    shuffleArray(indices_j, 8);

      // Execute weight changes
      for (int i_dx = 0; i_dx < 8; i_dx++) {
        for (int j_dx = 0; j_dx < 8; j_dx++) {

          int i = indices_i[i_dx];
          int j = indices_j[j_dx];
          
          int sign = signs[i][j];
          int pulseCount = pulses[i][j];

          float voltadd = voltMatrix[i][j]; //((float) pulseCount / 1000); //0.25; //0.25

          int add_dac = 0; //(int) ((voltadd * 4095) / Vref_dac);

          int add_due = 0; //(int) ((voltage_neg * 2047) / 3.3);

          //if (pulseCount <= 0 || (sign != 1 && sign != -1)) continue;

          int switch_modif = 1;

          // Configure DACs for sign
          if (sign == 1) {

          float v_plus_1 = 0.75 + voltadd;
          float v_minus_1 = 0.75 - voltadd; 

          float half_pos_pulse_volt_dac = v_plus_1; //0.99 ; //0.975; 1.05 0.85
          int half_pos_pulse_dac_level = (int) ((half_pos_pulse_volt_dac * 4095) / Vref_dac);

          float voltage_neg = (v_minus_1 - 0.545) / 0.665; //0.3; //2.2 0 
          int dacValue_neg = (int) ((voltage_neg * 4095) / 3.3);

            AD.writeDAC(i, half_pos_pulse_dac_level);
            delay(5);

            analogWrite(DAC1, dacValue_neg);

            delay(5);
          } 
          else if(sign == -1) {

          float v_plus_2 = 0.75 - voltadd;
          float v_minus_2 = 0.75 + voltadd; 

          float half_neg_pulse_volt_dac = v_plus_2; //0.628; //0.525; 0.5 64 0.643
          int half_neg_pulse_dac_level = (int) ((half_neg_pulse_volt_dac * 4095) / Vref_dac);

          float voltage_pos = (v_minus_2 - 0.545) / 0.665; //1.4; //1.45 1.9 94 1.35
          int dacValue_pos = (int) ((voltage_pos * 4095) / 3.3);


            AD.writeDAC(i, half_neg_pulse_dac_level);
            delay(5);

            analogWrite(DAC1, dacValue_pos);
            
            delay(5);
          }
          else{
            switch_modif = 0;
          }

          // Set all switches LOW first
          for (int ch = 0; ch < 8; ch++) {
            digitalWrite(outSwitchPins[ch], LOW);
            digitalWrite(inSwitchPins[ch], LOW);
          }

          if (switch_modif == 1){
          // Apply pulses

          fastPulse(outSwitchPins[i], inSwitchPins[j], 1, pulseCount);
          
          }
        }
      }

      Serial.println("WEIGHTMODIFALL complete.");
    }

    else if (cmd.startsWith("SETVOLTAGES:")) {
      String dataLine = cmd.substring(strlen("SETVOLTAGES:"));
      dataLine.trim();

      float voltages[9];  // 8 for AD5593R, 1 for Due
      int count = 0;

      int startIdx = 0;
      int len = dataLine.length();

      while (startIdx < len && count < 9) {
        while (startIdx < len && isspace(dataLine.charAt(startIdx))) startIdx++;
        if (startIdx >= len) break;

        int endIdx = startIdx;
        while (endIdx < len && !isspace(dataLine.charAt(endIdx))) endIdx++;

        String token = dataLine.substring(startIdx, endIdx);
        voltages[count++] = token.toFloat();

        startIdx = endIdx;
      }

      if (count != 9) {
        Serial.println("Error: SETVOLTAGES requires 9 floats: v0 ... v7 due_voltage");
        return;
      }

      // Set AD5593R DAC voltages
      for (int ch = 0; ch < 8; ch++) {
        int dacValue = (int)((voltages[ch] * 4095) / Vref_dac);
        dacValue = constrain(dacValue, 0, 4095);
        AD.writeDAC(ch, dacValue);
        delay(2);
      }

      // Set Arduino Due DAC voltage
      voltages[8] = (voltages[8] - 0.545) / 0.665;
      int dueDAC = (int)((voltages[8] * 4095) / 3.3);  // Assuming 3.3V ref
      dueDAC = constrain(dueDAC, 0, 4095);
      analogWrite(DAC1, dueDAC);

      
      Serial.println("Voltages updated:");
      for (int i = 0; i < 8; i++) {
        Serial.print("AD5593R DAC"); Serial.print(i); Serial.print(": ");
        Serial.print(voltages[i], 3); Serial.print(" V\t");
      }
      Serial.print("Due DAC1: "); Serial.print(voltages[8], 3); Serial.println(" V");
      
    }
    


    else {
      //Serial.println("Unknown command, send OUTINPULSES: or READADC");
    }
  }
}

// Helper function to shuffle an array
void shuffleArray(int* array, int size) {
  for (int i = size - 1; i > 0; i--) {
    int j = random(i + 1); // random integer from 0 to i
    // Swap elements
    int temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}


bool parseCombinedLine(const String& line, int *outVec, int *inVec, int *pulseVec) {
  int values[24];
  int count = 0;

  int startIdx = 0;
  int len = line.length();

  while (startIdx < len && count < 24) {
    // skip leading spaces
    while (startIdx < len && isspace(line.charAt(startIdx))) startIdx++;

    if (startIdx >= len) break;

    // find next space or end
    int endIdx = startIdx;
    while (endIdx < len && !isspace(line.charAt(endIdx))) endIdx++;

    String token = line.substring(startIdx, endIdx);
    values[count] = token.toInt();
    count++;

    startIdx = endIdx;
  }

  if (count != 24) return false;

  for (int i = 0; i < 8; i++) {
    outVec[i] = values[i];
    inVec[i] = values[i + 8];
    pulseVec[i] = values[i + 16];
  }

  return true;
}


void printVector(const char *name, int *vec) {
  Serial.print(name);
  Serial.print(": ");
  for (int i = 0; i < 8; i++) {
    Serial.print(vec[i]);
    Serial.print(" ");
  }
  Serial.println();
}

void sendParallelPulses(int *outStates, int *inStates, int *pulseCounts) {
  int maxPulses = 0;
  for (int i = 0; i < 8; i++) {
    if (pulseCounts[i] > maxPulses) maxPulses = pulseCounts[i];
  }
  Serial.print("Max pulses to send: ");
  Serial.println(maxPulses);

  // Set all pins LOW initially
  for (int i = 0; i < 8; i++) {
    digitalWrite(outSwitchPins[i], LOW);
    digitalWrite(inSwitchPins[i], LOW);
  }

  for (int pulse = 0; pulse < maxPulses; pulse++) {
    for (int ch = 0; ch < 8; ch++) {
      if (pulse < pulseCounts[ch]) {
        digitalWrite(outSwitchPins[ch], outStates[ch]);
        digitalWrite(inSwitchPins[ch], inStates[ch]);
      } else {
        digitalWrite(outSwitchPins[ch], LOW);
        digitalWrite(inSwitchPins[ch], LOW);
      }
    }

    delay(1); // pulse HIGH time

    // Turn all LOW to complete pulse
    for (int ch = 0; ch < 8; ch++) {
      digitalWrite(outSwitchPins[ch], LOW);
      digitalWrite(inSwitchPins[ch], LOW);
    }

    delay(1); // pulse LOW time
  }
}

void fastPulse(int outPin, int inPin, int pulses, int delay_) {
  // Get port registers and masks dynamically
  Pio* outPort = digitalPinToPort(outPin);
  Pio* inPort = digitalPinToPort(inPin);
  uint32_t outMask = digitalPinToBitMask(outPin);
  uint32_t inMask = digitalPinToBitMask(inPin);
  
  // Configure as outputs (faster than pinMode)
  outPort->PIO_OER = outMask;
  inPort->PIO_OER = inMask;
  outPort->PIO_PUDR = outMask; // Disable pull-up
  inPort->PIO_PUDR = inMask;   // Disable pull-up

  for (int p = 0; p < pulses; p++) {
    // Set pins HIGH
    outPort->PIO_SODR = outMask;
    inPort->PIO_SODR = inMask;

    delayMicroseconds(delay_);
    
    // 50ns delay (4 NOPs @ 84MHz)
    //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n"); 
    
    // Set pins LOW
    outPort->PIO_CODR = outMask;
    inPort->PIO_CODR = inMask;

    delayMicroseconds(delay_);
    
    // 50ns delay between pulses
    //asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop\n");
  }
}