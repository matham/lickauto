#ifndef I2C_BOARD_H
#define I2C_BOARD_H

// uses https://github.com/Richard-Gemmell/teensy4_i2c
#include <i2c_driver.h>
#include "imx_rt1060/imx_rt1060_i2c_driver.h"
#include "host_comm.h"
#include "marker.h"


#define NUM_MODIO_BOARDS_MAX 32
#define I2C_REQUEST_BUFF_N 32


enum class ModIOCmd : uint8_t {
  create = 0,
  remove,
  read_dig_cont_start,
  read_dig_cont_stop,
  read_dig,
  write_dig,
  address_change,
  blank, // nothing, just a placeholder internally - should not be used externally
  end,
};


enum class ModIOPullup : uint8_t {
  disabled = 0,
  enabled_22k_ohm,
  enabled_47k_ohm,
  enabled_100k_ohm,
  end,
};


enum class ModIOFreq : uint8_t {
  freq_100k = 0,
  freq_400k,
  freq_1m,
  end,
};


// in case of error, we may respond with just this struct,
// even if incoming struct had more data appeneded
struct ModIOData
{
  HostData header;
  uint8_t port;
  uint8_t address;
  ModIOCmd cmd;
};

struct ModIODataCreate
{
  ModIOData header;
  ModIOFreq freq;
  ModIOPullup pullup;
};

struct ModIODataBuff
{
  ModIOData header;
  uint8_t marker;
  uint8_t value;
};


class ModIOBoard
{
  public:
    ModIOBoard(ModIODataCreate* data, HostComm* host_comm, StreamMarker* marker, HostError* err);
    void delete_board();
    void loop_board();

    static void setup();
    static void loop();
    
    static void host_msg(ModIOData* msg, HostComm* host_comm, StreamMarker* marker);

  private:
    static inline ModIOBoard* locate_board(uint8_t port, uint8_t address);

    uint8_t _port;
    uint8_t _address;
    uint8_t _last_read_val;
    
    ModIODataBuff _request_buff[I2C_REQUEST_BUFF_N];
    uint8_t _buff_start;
    uint8_t _buff_n;
    uint8_t _working;
    uint _last_msg_ts;
    uint8_t _dev_buff[4];
  
    HostComm* _host_comm;
    I2CMaster& _controller;
    StreamMarker* _marker;

    static ModIOBoard* boards[];

};

#endif
