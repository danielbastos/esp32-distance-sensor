#ifndef ACCESS_POINT_H
#define ACCESS_POINT_H

class DistanceReader;

class AccessPoint {
public:
  explicit AccessPoint(DistanceReader& distanceReader);
  ~AccessPoint();

  bool begin();
  void handleClient();

private:
  struct Impl;
  Impl* _impl;

  bool connectSta();
  bool saveStaCredentials();
  bool startPortal();
  bool startWebServer();
  void stopPortal();
  void stopWebServer();
  void handleNetworks();
  void handleConnect();
  void handleDistance();
  void serveFile(const char* path, const char* contentType);

  AccessPoint(const AccessPoint&) = delete;
  AccessPoint& operator=(const AccessPoint&) = delete;

  DistanceReader& _distanceReader;
};

#endif
