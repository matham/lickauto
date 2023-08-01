#include "Arduino.h"
#include "host_comm.h"
#include "i2c_board.h"
#include "marker.h"

// based on https://github.com/PaulStoffregen/cores/blob/5b6d81b05a5df51bb8b2734c2f5b4f55ba4f2af2/teensy4/usb_serial.h


HostComm::HostComm()
{
  _read_buff_n = 0;
}


void HostComm::setup(StreamMarker* marker)
{
  _marker = marker;

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  _led_high = false;
  _last_led_time = millis();
}

void HostComm::loop()
{
  HostData header;
  int n = Serial.available();
  int item;

  if (n <= 0)
  {
    // no data to read
    if (millis() - _last_led_time >= 500)
    {
      // switch LED if timed-out
      digitalWrite(LED_BUILTIN, _led_high ? LOW : HIGH);
      _led_high = !_led_high;
      _last_led_time = millis();
    }
    return;
  }

  // switch LED for every packet
  digitalWrite(LED_BUILTIN, _led_high ? LOW : HIGH);
  _led_high = !_led_high;
  _last_led_time = millis();

  header.len = sizeof(HostData);
  header.code = HostCode::comm;
  header.id = 0;
  header.err = HostError::bad_input;

  while (n)
  {
    // read single char
    item = Serial.read();
    if (item == -1)
      return;

    _read_buff[_read_buff_n++] = (uint8_t)item;
    n--;

    // check if we have full message
    if (_read_buff[0] != _read_buff_n)
      continue;

    if (_read_buff_n < sizeof(HostData))
    {
      send_to_host(&header, header.len);
      _read_buff_n = 0;
      continue;
    }

    switch (((HostData*)_read_buff)->code)
    {
      case HostCode::modio_board:
        if (_read_buff_n < sizeof(ModIOData))
          send_to_host(&header, header.len);
        else
          ModIOBoard::host_msg((ModIOData*)_read_buff, this, _marker);
        break;

      case HostCode::stream_marker:
        if (_read_buff_n < sizeof(MarkerData))
          send_to_host(&header, header.len);
        else
          _marker->host_msg((MarkerData*)_read_buff);
        break;

      case HostCode::echo:
        if (_read_buff_n != sizeof(HostData))
          send_to_host(&header, header.len);
        else
          send_to_host(_read_buff, _read_buff_n);
        break;

      default:
        send_to_host(&header, header.len);
        break;
    }

    _read_buff_n = 0;
  }
}

void HostComm::send_to_host(void* data, uint8_t len)
{
  HostData header;

  if (Serial.availableForWrite() < len)
  {
    // if not enough space, block the write and drop msg
    header.len = sizeof(HostData);
    header.code = ((HostData*)data)->code;
    header.id = ((HostData*)data)->id;
    header.err = HostError::dropping_data;

    len = sizeof(HostData);
    data = &header;
  }

  Serial.write((uint8_t*)data, len);
}
