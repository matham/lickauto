#include "Arduino.h"
#include <i2c_driver.h>
#include "imx_rt1060/imx_rt1060_i2c_driver.h"
#include <string.h>

#include "i2c_board.h"
#include "host_comm.h"
#include "marker.h"

// based on https://github.com/Richard-Gemmell/teensy4_i2c/blob/v2.0.0-beta.2/src/i2c_driver.h


ModIOBoard* ModIOBoard::boards[NUM_MODIO_BOARDS_MAX] = {NULL};


void ModIOBoard::setup()
{

}

void ModIOBoard::loop()
{
  uint8_t i = 0;

  for (; i < NUM_MODIO_BOARDS_MAX && boards[i] != NULL; i++)
    boards[i]->loop_board();
}

void ModIOBoard::host_msg(ModIOData* msg, HostComm* host_comm, StreamMarker* marker)
{
  // host validated that it's at least size ModIOData
  ModIOBoard* board = locate_board(msg->port, msg->address);
  uint8_t i = 0;
  bool respond = true;
  HostError err = HostError::no_error;

  switch (msg->cmd)
  {
    case ModIOCmd::create:
      if (msg->header.len != sizeof(ModIODataCreate))
      {
        err = HostError::bad_input;
        break;
      }
      if (board != NULL)
      {
        err = HostError::already_exists;
        break;
      }

      for (i = 0; i < NUM_MODIO_BOARDS_MAX && boards[i] != NULL; i++);
      if (i == NUM_MODIO_BOARDS_MAX)
      {
        err = HostError::no_resource;
        break;
      }

      switch (((ModIODataCreate*)msg)->header.port)
      {
        // see https://github.com/Richard-Gemmell/teensy4_i2c/tree/master#ports-and-pins for def of ports
        case 0:
          boards[i] = new ModIOBoard((ModIODataCreate*) msg, host_comm, marker, Master, &err);
          break;
        case 1:
          boards[i] = new ModIOBoard((ModIODataCreate*) msg, host_comm, marker, Master1, &err);
          break;
        case 2:
          boards[i] = new ModIOBoard((ModIODataCreate*) msg, host_comm, marker, Master2, &err);
          break;
        default:
          boards[i] = NULL;
          err = HostError::bad_input;
          break;
      }

      if (err == HostError::no_error && boards[i] == NULL)
        err = HostError::no_resource;

      if (err != HostError::no_error && boards[i] != NULL)
      {
        delete boards[i];
        boards[i] = NULL;
        break;
      }
      
      break;

    case ModIOCmd::remove:
      if (msg->header.len != sizeof(ModIOData))
      {
        err = HostError::bad_input;
        break;
      }
      if (board == NULL)
      {
        err = HostError::not_found;
        break;
      }
      
      for (i = 0; boards[i] != board; i++);
      // move all boards up by one
      boards[i] = NULL;
      for (; i < NUM_MODIO_BOARDS_MAX - 1 && boards[i + 1] != NULL; i++)
      {
        boards[i] = boards[i + 1];
        boards[i + 1] = NULL;
      }

      board->delete_board();
      delete board;

      break;

    case ModIOCmd::address_change:
    case ModIOCmd::write_dig:
      if (msg->header.len != sizeof(ModIODataBuff))
      {
        err = HostError::bad_input;
        break;
      }
    case ModIOCmd::read_dig_cont_start:
    case ModIOCmd::read_dig:
    case ModIOCmd::read_dig_cont_stop:
      if (msg->cmd != ModIOCmd::write_dig && msg->cmd != ModIOCmd::address_change && msg->header.len != sizeof(ModIOData))
      {
        err = HostError::bad_input;
        break;
      }
      if (board == NULL)
      {
        err = HostError::not_found;
        break;
      }
      if (board->_buff_n == I2C_REQUEST_BUFF_N)
      {
        err = HostError::no_resource;
        break;
      }

      if (msg->cmd == ModIOCmd::read_dig_cont_start)
        board->_last_read_val = 0xFF;
      
      i = (board->_buff_start + board->_buff_n) % I2C_REQUEST_BUFF_N;
      // either it's of size ModIOData or ModIODataBuff, which is bigger. buff is of size ModIODataBuff
      memcpy(&board->_request_buff[i], msg, msg->header.len);
      board->_buff_n++;

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
    msg->header.len = sizeof(ModIOData);
    host_comm->send_to_host(msg, sizeof(ModIOData));
  }
}


inline ModIOBoard* ModIOBoard::locate_board(uint8_t port, uint8_t address)
{
  uint8_t i = 0;

  for (; i < NUM_MODIO_BOARDS_MAX && boards[i] != NULL && (boards[i]->_port != port || boards[i]->_address != address); i++);

  if (i == NUM_MODIO_BOARDS_MAX || boards[i] == NULL)
    return NULL;
  return boards[i];
}

ModIOBoard::ModIOBoard(ModIODataCreate* data, HostComm* host_comm, StreamMarker* marker, I2CMaster& controller, HostError* err) : _controller(controller)
{
  _port = data->header.port;
  _address = data->header.address;
  _host_comm = host_comm;
  _marker = marker;
  _working = 0;

  _last_read_val = 0xFF;
  _buff_start = 0;
  _buff_n = 0;

  if (_address & 0b10000000)
  {
    *err = HostError::bad_input;
    return;
  }

  switch (data->pullup)
  {
    case ModIOPullup::disabled:
      _controller.set_internal_pullups(InternalPullup::disabled);
      break;
    case ModIOPullup::enabled_22k_ohm:
      _controller.set_internal_pullups(InternalPullup::enabled_22k_ohm);
      break;
    case ModIOPullup::enabled_47k_ohm:
      _controller.set_internal_pullups(InternalPullup::enabled_47k_ohm);
      break;
    case ModIOPullup::enabled_100k_ohm:
      _controller.set_internal_pullups(InternalPullup::enabled_100k_ohm);
      break;
    default:
      *err = HostError::bad_input;
      return;
  }

  switch (data->freq)
  {
    case ModIOFreq::freq_100k:
      _controller.begin(100000);
      break;
    case ModIOFreq::freq_400k:
      _controller.begin(400000);
      break;
    case ModIOFreq::freq_1m:
      _controller.begin(1000000);
      break;
    default:
      *err = HostError::bad_input;
      return;
  }

  if (_controller.has_error())
    {
      *err = HostError::i2c_teensy_error;
      return;
    }
    
}

void ModIOBoard::delete_board()
{
  // todo: not sure what happens if in the middle of request. Clean up waiting requests
  _controller.end();
}

void ModIOBoard::loop_board()
{
  uint8_t last_i;
  uint8_t i;
  bool last_read_same = false;

  if (!_buff_n)
    return;
  
  if (_working)
  {
    if (!_controller.finished())
    {
      if (millis() - _last_msg_ts >= 500)
      {
        _request_buff[_buff_start].header.header.err = HostError::timed_out;
        _request_buff[_buff_start].header.header.len = sizeof(ModIOData);

        _host_comm->send_to_host(&_request_buff[_buff_start], sizeof(ModIOData));

        _buff_n--;
        _buff_start++;
        _buff_start = _buff_start % I2C_REQUEST_BUFF_N;

        _working = 0;
      }
      return;
    }
    
    // do read stage if we're reading
    if (
        (_request_buff[_buff_start].header.cmd == ModIOCmd::read_dig
         || _request_buff[_buff_start].header.cmd == ModIOCmd::read_dig_cont_start)
        && _working == 1
        && !_controller.has_error()
       )
    {
      _controller.read_async(_request_buff[_buff_start].header.address, &_request_buff[_buff_start].value, 1, true);
      _working++;
      return;
    }
    
    // now we're finished reading or writing
    last_i = _buff_start;
    _request_buff[last_i].header.header.err = HostError::no_error;
    _request_buff[last_i].header.header.len = sizeof(ModIODataBuff);

    // data was read into the buff directly if reading
    // check if data is unchanged for cont. reading
    if (_request_buff[last_i].header.cmd == ModIOCmd::read_dig_cont_start)
    {
      if (_last_read_val == _request_buff[last_i].value)
        last_read_same = true;
      else
        _last_read_val = _request_buff[last_i].value;
    }

    switch (_request_buff[last_i].header.cmd)
    {
      case ModIOCmd::address_change:
      case ModIOCmd::write_dig:
      case ModIOCmd::read_dig:
      case ModIOCmd::read_dig_cont_start:
        if (_controller.has_error())
          _request_buff[last_i].header.header.err = HostError::i2c_teensy_error;
        else
        {
#if MARKER_ENABLED
          if (_marker->is_enabled() && !last_read_same)
            _request_buff[last_i].header.header.err = _marker->add_mark(&_request_buff[last_i].marker);
#endif
        }
        break;
        
      default:
        // shouldn't get here
        _request_buff[last_i].header.header.err = HostError::program_error;
        break;
    }

    // queue it for reading again
    if (_request_buff[last_i].header.cmd == ModIOCmd::read_dig_cont_start && _request_buff[last_i].header.header.err == HostError::no_error)
    {
      if (_buff_n != 1)
      {
        _buff_start++;
        _buff_start = _buff_start % I2C_REQUEST_BUFF_N;
        
        // put it at the end
        if (_buff_n != I2C_REQUEST_BUFF_N)
          // don't copy if full and it's in place
          memcpy(&_request_buff[(_buff_start - 1 + _buff_n) % I2C_REQUEST_BUFF_N], &_request_buff[last_i], sizeof(ModIODataBuff));
      }

      // only send if it's unchanged
      if (!last_read_same)
        _host_comm->send_to_host(&_request_buff[last_i], sizeof(ModIODataBuff));
    }
    else
    {
      _buff_n--;
      _buff_start++;
      _buff_start = _buff_start % I2C_REQUEST_BUFF_N;

      _host_comm->send_to_host(&_request_buff[last_i], sizeof(ModIODataBuff));
    }

    _working = 0;
  }

  if (!_buff_n)
    return;
  
  switch (_request_buff[_buff_start].header.cmd)
  {
    case ModIOCmd::address_change:
      _dev_buff[0] = 0xF0;
      _dev_buff[1] = _request_buff[_buff_start].value;
      _controller.write_async(_request_buff[_buff_start].header.address, _dev_buff, 2, true);

      _last_msg_ts = millis();
      _working = 1;
      break;

    case ModIOCmd::write_dig:
      _dev_buff[0] = 0x10;
      _dev_buff[1] = _request_buff[_buff_start].value;
      _controller.write_async(_request_buff[_buff_start].header.address, _dev_buff, 2, true);

      _last_msg_ts = millis();
      _working = 1;
      break;

    case ModIOCmd::read_dig:
    case ModIOCmd::read_dig_cont_start:
      _dev_buff[0] = 0x20;
      _controller.write_async(_request_buff[_buff_start].header.address, _dev_buff, 1, true);
      
      _last_msg_ts = millis();
      _working = 1;
      break;

    case ModIOCmd::read_dig_cont_stop:
      for (i = 0; i < _buff_n; i++)
      {
        if (_request_buff[(_buff_start + i) % I2C_REQUEST_BUFF_N].header.cmd == ModIOCmd::read_dig_cont_start)
        {
          _request_buff[(_buff_start + i) % I2C_REQUEST_BUFF_N].header.cmd = ModIOCmd::blank;
          break;
        }
      }

      _request_buff[_buff_start].header.header.err = HostError::no_error;
      _request_buff[_buff_start].header.header.len = sizeof(ModIOData);
      _host_comm->send_to_host(&_request_buff[_buff_start], sizeof(ModIOData));

      _buff_n--;
      _buff_start++;
      _buff_start = _buff_start % I2C_REQUEST_BUFF_N;
      break;
    
    case ModIOCmd::blank:
      // nothing to do, this msg was blanked earlier to be skipped
      _buff_n--;
      _buff_start++;
      _buff_start = _buff_start % I2C_REQUEST_BUFF_N;
      break;

    default:
      // shouldn't get here
      _request_buff[_buff_start].header.header.err = HostError::program_error;
      _request_buff[_buff_start].header.header.len = sizeof(ModIOData);
      _host_comm->send_to_host(&_request_buff[_buff_start], sizeof(ModIOData));
      
      _buff_n--;
      _buff_start++;
      _buff_start = _buff_start % I2C_REQUEST_BUFF_N;
      break;
  }
}
