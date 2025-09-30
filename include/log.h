#ifndef LOG_H
#define LOG_H

// Activa o desactiva logs
#define ENABLE_LOGS 1   // 1 = activats, 0 = desactivats

#if ENABLE_LOGS
  #define LOG_PRINT(x)    Serial.print(x)
  #define LOG_PRINTLN(x)  Serial.println(x)
#else
  #define LOG_PRINT(x)
  #define LOG_PRINTLN(x)
#endif

#endif