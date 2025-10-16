"""
Write a weight matrix to the memristor board

Author: Dr. Ali Safa
College of Science and Engineering
Hamad Bin Khalifa University
"""
import numpy as np
from sklearn import datasets
from sklearn.decomposition import PCA
from sklearn.preprocessing import OneHotEncoder
from sklearn.model_selection import train_test_split
from scipy.special import softmax
import matplotlib.pyplot as plt
from memristor_main import *

plt.close("all")


"""
1) Find Maximum values reachable by each memristors
"""

WEIGHT = np.load("learned_weights.npz", allow_pickle = True)['data']

target_weights = WEIGHT.astype(int) #np.array([200, 100, 150, 60, 120, 250, 180, 40])

print(target_weights)

#array([163,  39,  17,  11,   0,  11,   1,  14])

iy = 1 #1 best one

# =============================================================================
# initial_values = np.zeros(8)
# for ix in range(8):
#     input_ = np.zeros((1,8)) 
#     input_[0,ix] = 1
#     input_volt = convert_to_infere_volts(input_)
#     adc_result = send_infere_multiple(input_volt)
#     read = adc_result[0,iy]
#     initial_values[ix] = read
# 
# 
# delta_init = target_weights - initial_values
# 
# =============================================================================

    
    

for ix in range(8):
    
    
    N_iter = 100
    
    L_check = 20
    
    center_voltage = 0.75
    
    Kp_plus = 7
    
    Kp_minus = 0.007
    
    Ki = 3
    
    ERR_acc = 0
    
    limit_err = 3000
    
    target = target_weights[ix] #500
    
    volt_add_more = 0
    
    
    output_hist = []
    for n_iter in range(N_iter):
        print(str(ix) + "----" + str(n_iter + 1) + "/" + str(N_iter))
        
        
        input_ = np.zeros((1,8)) 
        input_[0,ix] = 1 #0.5
        input_volt = convert_to_infere_volts(input_)
        adc_result = send_infere_multiple(input_volt)
        read = adc_result[0,iy]
        
        print(read)
            
        
        LOSS_grad = (target - read) * 2
        
        ERR_acc += LOSS_grad
        
        if np.abs(ERR_acc) > limit_err:
            ERR_acc = np.sign(ERR_acc) * limit_err
            
        
        output_hist.append(read * 1)
        
        CTRL = Kp_plus * LOSS_grad + Ki * ERR_acc
        
                
        if n_iter % L_check == 0:
            if n_iter > 0:
                volt_add_more += 0.1 * 0
                print("ADD MORE")
                

        
        #deadzone
        if np.abs(target - read) < 15:
            #ERR_acc = 0
            CTRL = 0
            break
            
        else:
        
            sign_ch = np.sign(CTRL)
            
            ctrl = 1
            
            if read < 1:
                # Get out of Low Resistance State
            
                if sign_ch == 1:
                    volt_delta = 0.11 + volt_add_more/2 #np.minimum(np.abs(CTRL), 0.2) #0.35
                else:
                    volt_delta = 0.11 + volt_add_more/2 #np.minimum(np.abs(CTRL), 0.2) #0.28
                    
                DEL = 50 #5000
                    
            else:
                if sign_ch == 1:
                    volt_delta = 0.081 + volt_add_more #np.minimum(np.abs(CTRL), 0.2) #0.35
                else:
                    volt_delta = 0.08 + volt_add_more #np.minimum(np.abs(CTRL), 0.2) #0.28
                
                
                DEL = int(np.abs(CTRL)) * 60
            
            #print(DEL)
            
            
            send_weight_modif(ix, iy, sign_ch, ctrl, v_plus_1 = center_voltage + volt_delta, v_minus_1 = np.maximum(center_voltage - volt_delta, 0.57), v_plus_2 = center_voltage - volt_delta, v_minus_2 = center_voltage + volt_delta, delay = DEL)
            
            


print("Final weights: ")

total = 0
for ix in range(8):
    input_ = np.zeros((1,8)) 
    input_[0,ix] = 1
    input_volt = convert_to_infere_volts(input_)
    adc_result = send_infere_multiple(input_volt)
    read = adc_result[0,iy]

    
    print(read)
    
print("Chip-in-the-loop fine-tuning...")   


import numpy as np
from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score
from scipy.optimize import minimize

# --- Load digits and filter for 0 and 1 ---
digits = load_digits()
X = digits.data
y = digits.target

# Only keep digits 0 and 1
mask = (y == 0) | (y == 1)
X = X[mask]
y = y[mask]

# Normalize inputs to [0, 1]
X = X / 16.0

# --- Apply PCA to reduce to 8 dimensions ---
pca = PCA(n_components=8)
X_reduced = pca.fit_transform(X)

X_reduced = (X_reduced - np.min(X_reduced)) / (np.max(X_reduced) - np.min(X_reduced))

# --- Train/Test split ---
X_train, X_test, y_train, y_test = train_test_split(X_reduced, y, test_size=0.2, random_state=42)


    
def sigmoid(x):
    return 1 / (1 + np.exp(-x))

# Initialize bias
bias_ = 0.0

# Learning rate for gradient descent
lr = 0.001

# Number of epochs to iterate over the data
epochs = 10

for epoch in range(epochs):
    total_loss = 0
    for idx in range(0, X_train.shape[0] - 5, 5):
        input_ = X_train[idx:idx+5, :]
        label_ = y_train[idx:idx+5]

        input_volt = convert_to_infere_volts(input_)
        adc_result = send_infere_multiple(input_volt)
        read = adc_result[:, iy]  # Shape: (5,)
        

        # Forward pass
        Y = 0.005 * read + bias_  # Shape: (5,)
        probs = sigmoid(Y)  # Predicted probabilities
        
        print(probs)
        print(label_)

        # Compute binary cross-entropy loss
        loss = -np.mean(label_ * np.log(probs + 1e-8) + (1 - label_) * np.log(1 - probs + 1e-8))
        total_loss += loss
        print(loss)

        # Compute gradient of loss w.r.t. bias_
        grad = np.mean(probs - label_)  # Derivative of BCE loss wrt bias

        # Update bias using gradient descent
        bias_ -= lr * grad

    print(f"Epoch {epoch+1}, Loss: {total_loss:.4f}, Bias: {bias_:.4f}")
    
  
N_p = 10
all_accs = []
threshs = np.linspace(0.58, 0.69, N_p)
for c in range(N_p):
    test_acc = 0  
    iter_ = 0
    for idx in range(0, X_test.shape[0] - 5, 5):
        input_ = X_test[idx:idx+5, :]
        label_ = y_test[idx:idx+5]
    
        input_volt = convert_to_infere_volts(input_)
        adc_result = send_infere_multiple(input_volt)
        read = adc_result[:, iy]  # Shape: (5,)
        
    
        # Forward pass
        Y = 0.005 * read + bias_  # Shape: (5,)
        probs = sigmoid(Y)  # Predicted probabilities
        
        probs[probs >= threshs[c]] = 1
        probs[probs < threshs[c]] = 0
        
        acc_loc = np.sum(probs == label_) / 5
        
        test_acc += acc_loc
        
        iter_ += 1
        
        print(test_acc / iter_)
        
        
    final_test_acc = test_acc / iter_
    all_accs.append(final_test_acc)
    
    
plt.figure(3)
plt.plot(threshs, all_accs, ".-", color = "orange")
plt.ylim(0.485, 1)
plt.ylabel("Accuracy")
plt.xlabel("Probability Threshold")
plt.grid("on")
    

np.savez("without_vipi.npz", data = [all_accs, threshs])    
    
    

