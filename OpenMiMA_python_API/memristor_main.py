import serial
import time
import glob
import re
import numpy as np

# Try to automatically find the Arduino Due serial port
def find_arduino_port():
    ports = glob.glob('/dev/tty.usbmodem*')
    if not ports:
        raise IOError("Arduino Due port not found. Please check the connection.")
    return ports[0]

# Connect to serial port
SERIAL_PORT = find_arduino_port()
BAUDRATE = 115200
TIMEOUT = 2  # seconds

# Open serial connection
ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=TIMEOUT)
time.sleep(2)  # Give Arduino time to reboot after serial opens

def send_weight_modif(i, j, sign, pulses, v_plus_1 = 0.99, v_minus_1 = 0.3, v_plus_2 = 0.628, v_minus_2 = 1.4, delay = 15):
    if not (0 <= i <= 7 and 0 <= j <= 7):
        raise ValueError("i and j must be in range 0–7")
    if sign not in [-1, 1]:
        raise ValueError("sign must be -1 or 1")
    if pulses <= 0:
        raise ValueError("pulses must be positive")

    cmd = f"WEIGHTMODIF: {i} {j} {sign} {pulses} {v_plus_1} {v_minus_1} {v_plus_2} {v_minus_2} {delay}"
    _send_and_receive(cmd)
    
def send_weight_modif64(signs, pulses, voltages):
    """
    Send WEIGHTMODIF64 command for all 64 weights at once.
    
    Args:
        signs (list[int]): 64 elements, each -1 or 1
        pulses (list[int]): 64 elements, each > 0
    """
    if len(signs) != 64 or len(pulses) != 64:
        raise ValueError("Both 'signs' and 'pulses' must have length 64")
    if any(s not in (-1, 1, 0) for s in signs):
        raise ValueError("All signs must be -1 or 1")
    if any(p <= 0 for p in pulses):
        raise ValueError("All pulse counts must be positive")
        

    signs_str = " ".join(str(s) for s in signs)
    pulses_str = " ".join(str(p) for p in pulses)
    voltages_str = " ".join(str(v) for v in voltages)
    cmd = f"WEIGHTMODIFALL: {signs_str} {pulses_str} {voltages_str}"
    #print(cmd)
    _send_and_receive(cmd) 
    time.sleep(2)

def send_infere_multiple(dac_matrix):
    """
    Send INFEREMULTIPLE command to Arduino with multiple DAC vectors,
    and read back all the resulting ADC averages.

    Args:
        dac_matrix (np.ndarray): shape (N, 8) with DAC values in 0–4095.

    Returns:
        np.ndarray: shape (N, 8) with ADC averages as floats.
    """
    dac_matrix = np.asarray(dac_matrix, dtype=int)

    if dac_matrix.ndim != 2 or dac_matrix.shape[1] != 8:
        raise ValueError("dac_matrix must have shape (N, 8)")
    if np.any(dac_matrix < 0) or np.any(dac_matrix > 4095):
        raise ValueError("All DAC values must be between 0 and 4095")
        
    

    num_vectors = dac_matrix.shape[0]
    dac_flat = dac_matrix.flatten()
    
    #print(dac_flat.shape)

    cmd = "INFEREMULTIPLE: " + str(num_vectors) + " " + " ".join(str(val) for val in dac_flat)
    #print(cmd)

    ser.write((cmd + "\n").encode())
    time.sleep(0.1)
    TIMEOUT = 50

    results = []
    start = time.time()
    collecting = False

    while True:
        if ser.in_waiting:
            line = ser.readline().decode(errors='ignore').strip()
            #print(line)

            if "INFEREMULTIPLE ADC Averages:" in line:
                collecting = True
                continue
            elif "INFEREMULTIPLE complete." in line:
                break
            elif collecting and line:
                try:
                    values = [float(v) for v in line.split()]
                    if len(values) == 8:
                        results.append(values)
                except ValueError:
                    pass

        if time.time() - start > TIMEOUT:
            break

    return np.array(results, dtype=float)



def send_infere(dac_levels):
    if len(dac_levels) != 8:
        raise ValueError("You must provide exactly 8 DAC values")
    if any(not (0 <= val <= 4095) for val in dac_levels):
        raise ValueError("DAC values must be between 0 and 4095")

    cmd = "INFERE: " + " ".join(str(val) for val in dac_levels)
    #print(cmd)
    return _send_and_parse_infere(cmd)

def _send_and_receive(command):
    #print(f"\n>>> Sending: {command}")
    ser.write((command + "\n").encode())  # Send with newline
    time.sleep(0.6)

    start = time.time()
    
    while True:
        if ser.in_waiting:
            line = ser.readline().decode(errors='ignore').strip()
            if line:
                pass
                #print(line)
        elif time.time() - start > TIMEOUT:
            break
        


    #print("--- End of response ---\n")

def _send_and_parse_infere(command):
    #print(f"\n>>> Sending: {command}")
    ser.write((command + "\n").encode())
    time.sleep(0.05)

    adc_values = []
    start = time.time()

    while True:
        if ser.in_waiting:
            line = ser.readline().decode(errors='ignore').strip()
            #print(line)

            # Look for the line that starts with "INFERE ADC Averages:"
            if "INFERE ADC Averages:" in line:
                # Next line should be the actual values
                values_line = ser.readline().decode(errors='ignore').strip()
                #print(values_line)

                # Extract 8 float values
                try:
                    adc_values = [float(val) for val in values_line.split()]
                    if len(adc_values) != 8:
                        raise ValueError("Expected 8 ADC values.")
                    #else:
                        #break
                except Exception as e:
                    print(f"Error parsing ADC values: {e}")
                    adc_values = []

        elif time.time() - start > TIMEOUT:
            break

    #print(adc_values)
    return adc_values


def convert_to_infere_volts(in_vec, max_volt_read = 0.05):
    #Assumes data scaled between 0 and 1
    Vref_dac = 1.5
    #in_vec = np.maximum(in_vec, 0.1)
    in_vec = in_vec * max_volt_read + 0.75 #0.7379
    to_send = (in_vec * 4095) / Vref_dac
    return to_send.astype(int)
    
def val_to_pulse(val, max_val = 1, max_pulse = 1000):
    if val < max_val:
        pulses = int(max_pulse * val / max_val) #+ 10
    else:
        pulses = int(max_pulse)
        
        
    return pulses
        

def modify_weight(W):
    signs = np.zeros(64).astype(int)
    pulses = np.zeros(64).astype(int)
    ii = 0

    for i in range(W.shape[0]):
        for j in range(W.shape[1]):
            if W[i,j] != 0:
                ss = int(np.sign(W[i,j]))
                val = np.abs(W[i,j])
                pulse_cnt = val * 1000
                if pulse_cnt == 0:
                    #ss = 0
                    pulse_cnt = 1
                    
                #print(pulse_cnt)
                #send_weight_modif(i, j, ss, pulse_cnt)
                signs[ii] = int(ss * 1)
                pulses[ii] = int(pulse_cnt * 1)
                ii += 1
            else:
                ss = 0
                val = np.abs(W[i,j])
                pulse_cnt = 1
                #print(pulse_cnt)
                #send_weight_modif(i, j, ss, pulse_cnt)
                signs[ii] = int(ss * 1)
                pulses[ii] = int(pulse_cnt * 1)
                ii += 1
      
    print(signs)
    print(pulses)
    pulses[pulses == 0] = 1
    send_weight_modif64(signs, pulses)
    
    
    
def modify_weight_pulse(W):
    signs = np.zeros(64).astype(int)
    pulses = np.zeros(64).astype(int)
    ii = 0

    for i in range(W.shape[0]):
        for j in range(W.shape[1]):
            if W[i,j] != 0:
                ss = int(np.sign(W[i,j]))
                val = np.abs(W[i,j])
                pulse_cnt = int(val)
                if pulse_cnt == 0:
                    pulse_cnt = 1
                    
                #print(pulse_cnt)
                #send_weight_modif(i, j, ss, pulse_cnt)
                signs[ii] = int(ss * 1)
                pulses[ii] = int(pulse_cnt * 1)
                ii += 1
            else:
                ss = 0
                val = np.abs(W[i,j])
                pulse_cnt = val_to_pulse(val)
                #print(pulse_cnt)
                #send_weight_modif(i, j, ss, pulse_cnt)
                signs[ii] = int(ss * 1)
                pulses[ii] = int(pulse_cnt * 1 + 1)
                ii += 1
      
    print(signs)
    print(pulses)
    send_weight_modif64(signs, pulses)
            
    
    
    
def mod_weights_all(W, sigma = 100):

    for i in range(W.shape[0]):
        for j in range(W.shape[1]):
            ss = int(np.sign(W[i,j]))
            val = np.abs(W[i,j])
            pulse_cnt = int(val*sigma)
            print(pulse_cnt)
            if pulse_cnt == 0:
                pulse_cnt = 1
                
                
            send_weight_modif(i,j,int(ss * 1),int(pulse_cnt * 1))


# Example usage
if __name__ == "__main__":
    for i in range(2):
        time_now = time.time()
        send_weight_modif(0, 7, 1, 10)
        # Send an INFERE command and retrieve ADC values
        tosend = np.zeros((8,8))
        tosend = tosend + 2100
        tosend = tosend.astype(int)
        adc_result = send_infere_multiple(tosend) #send_infere([2100, 2100, 2100, 2100, 2100, 2100, 2100, 2100])
        time_now2 = time.time()
        print(time_now2 - time_now)
        print("Extracted ADC Averages:")
        print(adc_result)
        
        
        
        
        
