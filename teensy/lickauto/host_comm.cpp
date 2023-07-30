#include "Arduino.h"
#include "host_comm.h"
#include "i2c_board.h"
#include "marker.h"

// based on https://github.com/PaulStoffregen/cores/blob/5b6d81b05a5df51bb8b2734c2f5b4f55ba4f2af2/teensy4/usb_serial.h


HostComm::HostComm()
{

}


void HostComm::setup(StreamMarker* marker)
{
  _marker = marker;
}

void HostComm::loop()
{

}

void HostComm::send_to_host(void* data, uint8_t len)
{

}
