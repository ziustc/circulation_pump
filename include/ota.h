#ifndef OTA_H
#define OTA_H

class OTA
{
public:
    OTA();
    void init();
    void handle();
    void setPort(uint16_t port);
    void setHostname(const char *hostname);

private:
    uint16_t otaPort;
    char    *otaHostname;
};

#endif