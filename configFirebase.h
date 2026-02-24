#ifndef __CONFIGFIREBASE__H
#define __CONFIGFIREBASE__H

class CONFIGFIREBASE {
public:
  CONFIGFIREBASE();
  ~CONFIGFIREBASE();
  void initFirebase();
  bool WiFiError();
  void sendFirebaseData(float hr, float spo2, float temp);
};

#endif