#ifndef ACCESS_POINT_H
#define ACCESS_POINT_H

class AccessPoint {
public:
  AccessPoint();
  ~AccessPoint();

  bool begin();
  void handleClient();

private:
  struct Impl;
  Impl* _impl;

  bool connectSta();
  bool saveStaCredentials();
  bool startPortal();
  void stopPortal();
  void handleNetworks();
  void handleConnect();
  void serveFile(const char* path, const char* contentType);

  AccessPoint(const AccessPoint&) = delete;
  AccessPoint& operator=(const AccessPoint&) = delete;
};

#endif
