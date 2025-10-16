import numpy as np
from sklearn.datasets import load_digits
from sklearn.model_selection import train_test_split
from sklearn.decomposition import PCA
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

# --- Sigmoid function ---
def sigmoid(z):
    return 1 / (1 + np.exp(-z))

# --- Logistic loss function ---
def loss_fn(wb, X, y):
    w = wb[:-1]
    b = wb[-1]
    z = np.dot(X, w) + b
    probs = sigmoid(z)
    loss = -np.mean(y * np.log(probs + 1e-8) + (1 - y) * np.log(1 - probs + 1e-8))
    return loss

# --- Initial weights and bias ---
init_wb = np.zeros(X_train.shape[1] + 1)  # 8 weights + 1 bias

# --- Constraints: weights ≥ 0 (but not the bias) ---
constraints = [{'type': 'ineq', 'fun': lambda wb, i=i: wb[i]} for i in range(X_train.shape[1])]

# --- Optimize using SLSQP ---
result = minimize(loss_fn, init_wb, args=(X_train, y_train), constraints=constraints, method='SLSQP')

# --- Extract trained weights and bias ---
w_opt = result.x[:-1]
b_opt = result.x[-1]

# --- Predict on test data ---
y_probs = sigmoid(np.dot(X_test, w_opt) + b_opt)
y_pred = (y_probs >= 0.5).astype(int)

# --- Evaluate ---
accuracy = accuracy_score(y_test, y_pred)
print("Test Accuracy with PCA + Positive Weights:", accuracy)
print("Learned Positive Weights:", w_opt)
print("Bias Term:", b_opt)

np.savez("learned_weights.npz", data = w_opt)
np.savez("learned_bias.npz", data = b_opt)
