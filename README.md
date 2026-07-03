
# 🧠 Tremor.AI
### Parkinson's Disease Tremor Detection using STM32 + Embedded AI
 
*Real-time, on-device tremor classification — no cloud, no lag, no compromise.*
 
![Platform](https://img.shields.io/badge/Platform-STM32F401RE-03234B?style=flat-square&logo=stmicroelectronics)
![Language](https://img.shields.io/badge/Language-Embedded%20C-blue?style=flat-square)
![AI](https://img.shields.io/badge/AI-TensorFlow%20Lite-FF6F00?style=flat-square&logo=tensorflow)
![License](https://img.shields.io/badge/License-MIT-green?style=flat-square)
 
</div>
---
 
## 💡 Why This Project Exists
 
Over **10 million people worldwide** live with Parkinson's Disease, and tremor severity is one of the earliest, most telling indicators of disease progression. Clinical assessment is often subjective, infrequent, and confined to hospital visits.
 
**Tremor.AI** puts a neurologist-grade tremor classifier on a coin-sized microcontroller — turning a raw accelerometer signal into a real-time severity reading, right at the wrist, with zero internet dependency.
 
> Sensor data in. Clinical insight out. Entirely on-device.
 
---
 
## ⚙️ How It Works — At a Glance
 
```
   ┌──────────────┐      SPI       ┌──────────────────┐
   │   MPU9255    │ ─────────────► │   STM32F401RE     │
   │ Accelerometer│                │  (Nucleo Board)    │
   └──────────────┘                └─────────┬─────────┘
                                              │
                              Sliding Window (1 sec)
                                              │
                                   13 Statistical Features
                                              │
                                              ▼
                                  ┌───────────────────────┐
                                  │  X-CUBE-AI Neural Net  │
                                  │   (TFLite → STM32)     │
                                  └───────────┬───────────┘
                                              │
                                              ▼
                              NORMAL · MILD · MODERATE · SEVERE
                                              │
                                              ▼
                                    UART → Serial Output
```
 
---
 
## ✨ Highlights
 
| | |
|---|---|
| 🔋 **100% Offline** | All inference runs on-device — no cloud, no Wi-Fi, no latency |
| 🎯 **4-Class Precision** | Normal → Mild → Moderate → Severe |
| 📊 **13 Engineered Features** | Statistically rich window-based feature extraction |
| 🧠 **Real Neural Network** | Trained in TensorFlow, deployed via STM32 X-CUBE-AI |
| ⚡ **200Hz Sampling** | Captures the full 3–8Hz clinical tremor frequency band |
| 🔌 **SPI + UART** | Clean, minimal wiring — built for reproducibility |
 
---
 
## 🧩 System Architecture
 
**Signal Path:**
`MPU9255 (SPI)` → `STM32F401RE` → `Feature Extraction` → `AI Inference (X-CUBE-AI)` → `UART Output`
 
**ML Pipeline:**
`Raw Accelerometer Data` → `Feature Extraction` → `TensorFlow Training` → `TFLite Conversion` → `X-CUBE-AI Deployment` → `On-Device Inference`
 
---
 
## 🔬 The 13 Features Behind the Classification
 
Every 1-second window of raw accelerometer data is distilled into these statistical fingerprints:
 
<table>
<tr>
<td valign="top">
**Axis-wise Mean**
- Mean X
- Mean Y
- Mean Z
**Axis-wise Spread**
- Std Dev X
- Std Dev Y
- Std Dev Z
</td>
<td valign="top">
**Axis-wise Shape**
- Skewness X
- Skewness Y
- Skewness Z
**Magnitude-based**
- Mean Magnitude
- Std Dev Magnitude
- Maximum Magnitude
</td>
<td valign="top">
**Robust Statistic**
- Median Absolute Deviation
</td>
</tr>
</table>
---
 
## 🛠️ Build It Yourself
 
### Hardware
 
- STM32 Nucleo-F401RE
- MPU9255 Accelerometer
- USB Cable
- Breadboard + Jumper Wires
### Software Stack
 
| Embedded | AI / Data |
|---|---|
| STM32CubeIDE | TensorFlow |
| STM32CubeMX | TensorFlow Lite |
| STM32 X-CUBE-AI | NumPy / Pandas |
 
### Wiring
 
| MPU9255 Pin | STM32F401RE Pin |
|:---:|:---:|
| VCC | 3.3V |
| GND | GND |
| SCLK | PA5 |
| MISO | PA6 |
| MOSI | PA7 |
| CS | PA4 |
| INT | PA0 *(optional)* |
 
📖 Full setup walkthrough → **`STM_GUIDE`**
 
---
 
## 🖥️ Sample Serial Output
 
```
Pred: NORMAL
Pred: MILD
Pred: MODERATE
Pred: SEVERE
```
 
---
 
## 🗺️ Roadmap
 
- [ ] Bluetooth LE companion app for live visualization
- [ ] On-device model retraining support
- [ ] Battery-optimized low-power sampling mode
- [ ] Clinical validation study
---
 
## 👥 Team
 Harshini S · Akshaya H · Subiksha
 
---
 
## 📄 License
 
Released under the **MIT License** — free to use, modify, and build upon.
 
<div align="center">
*Built with 🧠 for a steadier tomorrow.*
 
</div>
 
