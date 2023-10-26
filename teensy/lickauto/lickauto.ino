// to install, you need to install the arduino IDE
// then install Teensyduino from https://www.pjrc.com/teensy/td_download.html
// then install https://github.com/Richard-Gemmell/teensy4_i2c
// by downloading https://github.com/Richard-Gemmell/teensy4_i2c/archive/refs/tags/v2.0.0-beta.2.zip
// and installing it as a zip library


#include "Arduino.h"
#include "i2c_board.h"
#include "host_comm.h"
#include "marker.h"
#include "utils.h"


static HostComm host_comm;
static StreamMarker marker;


void setup() {
#if MARKER_ENABLED
  marker.setup(&host_comm);
#endif
  host_comm.setup(&marker);
  ModIOBoard::setup();
}


void loop() {
#if MARKER_ENABLED
  marker.loop();
#endif
  host_comm.loop();
  ModIOBoard::loop();
}
