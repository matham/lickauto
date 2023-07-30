#ifndef HOST_COMM_H
#define HOST_COMM_H

class StreamMarker;


enum class HostError : uint8_t {
  no_error = 0,
  already_exists,
  bad_input,
  no_resource,
  not_found,
  i2c_teensy_error,
  not_running,
  bad_state,
  program_error,
  end,
};


enum class HostCode : uint8_t {
  modio_board = 0,
  stream_marker,
  end,
};


struct HostData
{
  uint8_t len;
  HostCode code;
  uint8_t id;
  HostError err;
};


class HostComm
{
  public:
    HostComm();
    void setup(StreamMarker* marker);
    void loop();

    void send_to_host(void* data, uint8_t len);
  
  private:
    StreamMarker* _marker;

};

#endif
