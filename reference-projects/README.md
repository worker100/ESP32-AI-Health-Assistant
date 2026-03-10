# Reference Projects

This folder is reserved for external reference implementations, algorithm notes, and source links used during development.

Current references:

1. `MAXREFDES117`
   - Official page: <https://www.analog.com/en/resources/reference-designs/maxrefdes117.html>
   - Purpose:
     - Official Maxim/ADI reference design for heart-rate and SpO2.
     - Reference for `25 sps`, `4-second window`, and quality-gated pulse oximetry flow.

2. `MAX30102_by_RF`
   - Repository: <https://github.com/aromring/MAX30102_by_RF>
   - Purpose:
     - Community implementation derived from the Maxim reference design.
     - Reference for `autocorrelation` and `Pearson correlation` quality gates.

3. `SparkFun MAX3010x Sensor Library`
   - Repository: <https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library>
   - Purpose:
     - Driver layer and Arduino/ESP32 integration baseline.
     - Used for stable FIFO access to MAX30102.

Notes:
- The main firmware should keep only project code.
- External reference code, copied snippets, or comparison notes should be stored here.
- Future downloaded repos can be placed under subfolders in this directory.
