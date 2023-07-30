#ifndef MARKER_H
#define MARKER_H

#include "host_comm.h"

#define MARKER_ENABLED 1


enum class MarkerCmd : uint8_t {
  enable = 0,
  disable,
  mark,
  end,
};


// in case of error, we may respond with just this struct,
// even if incoming struct had more data appeneded
struct MarkerData
{
  HostData header;
  MarkerCmd cmd;
};


struct MarkerDataEnable
{
  MarkerData header;
  uint32_t duration;
  uint8_t clock_pin;
  uint8_t data_pin;
};


struct MarkerDataItem
{
  MarkerData header;
  uint8_t marker;
};


class StreamMarker
{
  public:
    StreamMarker();

    void setup(HostComm* host_comm);
    void loop();

    HostError add_mark(uint8_t* mark);
    void host_msg(MarkerData* msg);

    bool inline is_enabled() {return _enabled; };

  private:
    HostComm* _host_comm;

    bool _enabled;
    uint32_t _duration;

    uint32_t _start_time;
    bool _sending;
    uint8_t _current_code;
    
    uint8_t _clock_pin;
    uint8_t _data_pin;

};


#endif
