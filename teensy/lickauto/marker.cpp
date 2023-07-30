#include "Arduino.h"
#include "marker.h"
#include "host_comm.h"
#include "utils.h"


StreamMarker::StreamMarker()
{
  _enabled = false;
  _sending = false;
}


void StreamMarker::setup(HostComm* host_comm)
{
  _host_comm = host_comm;
}

void StreamMarker::loop()
{
  uint8_t bit;

  if (!_sending)
    return;

  if (_bit_state != -1 && micros() - _start_time < _duration)
    return;

  switch (_bit_state)
  {
    case -1:
    case 1:
    case 3:
    case 5:
    case 7:
    case 9:
    case 11:
    case 13:
      _bit_state++;
      bit = 0x80 >> (_bit_state / 2);

      if (_current_code & bit && _data_low)
        digitalWrite(_data_pin, HIGH);
      else if (!(_current_code & bit) && !_data_low)
        digitalWrite(_data_pin, LOW);
      _data_low = !(_current_code & bit);

      digitalWrite(_clock_pin, HIGH);

      _start_time = micros();
      break;

    case 0:
      // first bit is sent flipped on clock down
      if (_current_code & 0b10000000)
        digitalWrite(_data_pin, LOW);
      else
        digitalWrite(_data_pin, HIGH);
      _data_low = _current_code & 0b10000000;

      digitalWrite(_clock_pin, LOW);

      _start_time = micros();
      _bit_state++;
      break;

    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 14:
      digitalWrite(_clock_pin, LOW);

      _start_time = micros();
      _bit_state++;
      break;
  }

  if (_bit_state == 15)
    _sending = false;
}

HostError StreamMarker::add_mark(uint8_t* mark)
{
  if (!_enabled)
    return HostError::not_running;

  if (_sending)
  {
    *mark = _current_code;
    return HostError::no_error;
  }

  _current_code = get_next_code_val();
  *mark = _current_code;
  _sending = true;
  _bit_state = -1;

  return HostError::no_error;
}

void StreamMarker::host_msg(MarkerData* msg)
{
  // host validated that it's at least size MarkerData
  MarkerDataEnable* enable_msg;
  MarkerDataItem marker_item;

  bool respond = true;
  HostError err = HostError::no_error;

  switch (msg->cmd)
  {
    case MarkerCmd::enable:
      if (msg->header.len != sizeof(MarkerDataEnable))
      {
        err = HostError::bad_input;
        break;
      }
      if (_enabled)
      {
        err = HostError::bad_state;
        break;
      }

      enable_msg = (MarkerDataEnable*)msg;
      _duration = enable_msg->duration;
      _clock_pin = enable_msg->clock_pin;
      _data_pin = enable_msg->data_pin;
      _sending = false;
      _enabled = true;
      _data_low = true;
      _bit_state = -1;

      pinMode(_clock_pin, OUTPUT);
      digitalWrite(_clock_pin, LOW);
      pinMode(_data_pin, OUTPUT);
      digitalWrite(_data_pin, LOW);

      break;
    
    case MarkerCmd::disable:
      if (msg->header.len != sizeof(MarkerData))
      {
        err = HostError::bad_input;
        break;
      }
      if (!_enabled)
      {
        err = HostError::bad_state;
        break;
      }

      _sending = false;
      _enabled = false;

      pinMode(_clock_pin, INPUT);
      pinMode(_data_pin, INPUT);

      break;
    
    case MarkerCmd::mark:
      if (msg->header.len != sizeof(MarkerData))
      {
        err = HostError::bad_input;
        break;
      }
      err = add_mark(&marker_item.marker);
      if (err != HostError::no_error)
        break;

      memcpy(&marker_item, msg, sizeof(MarkerData));
      marker_item.header.header.len = sizeof(MarkerDataItem);
      _host_comm->send_to_host(&marker_item, sizeof(MarkerDataItem));
      respond = false;
      
      break;

    default:
      err = HostError::bad_input;
      break;

  }

  if (respond)
  {
    // sending back ack or with errors only have the basic headers
    msg->header.err = err;
    msg->header.len = sizeof(MarkerData);
    _host_comm->send_to_host(msg, sizeof(MarkerData));
  }
}
