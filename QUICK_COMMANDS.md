# ESP32 AI Health Assistant Quick Commands

## 0) Paths
```powershell
$PROJ = "d:\Projects\ESP32 Projects\ESP32-AI-Health-Assistant"
$FW   = "$PROJ\firmware"
$BE   = "$PROJ\backend"
$PORT = "COM3"
```

## 1) Start Backend
```powershell
cd "$BE"
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8787 --reload
```

## 2) Build Firmware
```powershell
pio run -d "$FW" -e esp32-s3-devkitc-1
```

## 3) Upload Firmware (COM3)
```powershell
pio run -d "d:\Projects\ESP32 Projects\ESP32-AI-Health-Assistant\firmware" -e esp32-s3-devkitc-1
```

## 4) Serial Monitor (COM3, 115200)
```powershell
pio run -d "d:\Projects\ESP32 Projects\ESP32-AI-Health-Assistant\firmware" -e esp32-s3-devkitc-1 -t upload --upload-port COM3
```

## 5) One-Line Build + Upload
```powershell
pio device monitor -d "d:\Projects\ESP32 Projects\ESP32-AI-Health-Assistant\firmware" -e esp32-s3-devkitc-1 -p COM3 -b 115200
```

