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
