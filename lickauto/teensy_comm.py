import serial
import struct
from enum import IntEnum


class HostError(IntEnum):
    no_error = 0
    already_exists = 1
    bad_input = 2
    no_resource = 3
    not_found = 4
    i2c_teensy_error = 5
    not_running = 6
    bad_state = 7
    program_error = 8
    dropping_data = 9
    _end = 10


class HostCode(IntEnum):
    modio_board = 0
    stream_marker = 1
    comm = 2
    echo = 3
    _end = 4


class ModIOCmd(IntEnum):
    create = 0
    remove = 1
    read_dig_cont_start = 2
    read_dig_cont_stop = 3
    read_dig = 4
    write_dig = 5
    _blank = 6
    _end = 7


class ModIOPullup(IntEnum):
    disabled = 0
    enabled_22k_ohm = 1
    enabled_47k_ohm = 2
    enabled_100k_ohm = 3
    _end = 4


class ModIOFreq(IntEnum):
    freq_100k = 0
    freq_400k = 1
    freq_1m = 2
    _end = 3


class MarkerCmd(IntEnum):
    enable = 0
    disable = 1
    mark = 2
    _end = 3


class TeensyComm:

    ser: serial.Serial = None

    _host_comm_f = 'BBBB'

    _modio_data_f = 'BBB'

    _modio_create_f = 'BB'

    _modio_data_buff_f = 'BB'

    _marker_data_f = 'B'

    _marker_enable_f = 'LBB'

    _marker_item_f = 'B'

    _buffer = b''

    def make_host_echo(self, id_val: int):
        fmt = '<' + self._host_comm_f
        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.echo.value, id_val,
            HostError.no_error.value
        )

    def make_modio_create(
            self, id_val: int, port: int, address: int, freq: ModIOFreq,
            pullup: ModIOPullup
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f + \
              self._modio_create_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.create.value,
            freq.value, pullup.value
        )

    def make_modio_remove(self, id_val: int, port: int, address: int):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.remove.value
        )

    def make_modio_read_digital(self, id_val: int, port: int, address: int):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.read_dig.value
        )

    def make_modio_read_digital_cont_start(
            self, id_val: int, port: int, address: int
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address,
            ModIOCmd.read_dig_cont_start.value
        )

    def make_modio_read_digital_cont_stop(
            self, id_val: int, port: int, address: int
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address,
            ModIOCmd.read_dig_cont_stop.value
        )

    def make_modio_write_digital(
            self, id_val: int, port: int, address: int, *relays: tuple[bool]):
        fmt = '<' + self._host_comm_f + self._modio_data_f + \
              self._modio_data_buff_f

        if len(relays) > 4:
            raise ValueError("There are only up to 4 relays per-board")

        value = 0
        for i, val in enumerate(relays):
            value |= int(bool(val)) << i

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.write_dig.value,
            0, value
        )

    def make_marker_enable(
            self, id_val: int, duration: int, clock_pin: int, data_pin: int
    ):
        fmt = '<' + self._host_comm_f + self._marker_data_f + \
              self._modio_create_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.stream_marker.value, id_val,
            HostError.no_error.value, MarkerCmd.enable.value,
            duration, clock_pin, data_pin
        )

    def make_marker_disable(self, id_val: int):
        fmt = '<' + self._host_comm_f + self._marker_data_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.stream_marker.value, id_val,
            HostError.no_error.value, MarkerCmd.disable.value
        )

    def make_marker_mark(self, id_val: int):
        fmt = '<' + self._host_comm_f + self._marker_data_f

        return struct.pack(
            fmt, struct.calcsize(fmt), HostCode.stream_marker.value, id_val,
            HostError.no_error.value, MarkerCmd.mark.value
        )

    def parse_buffer(self) -> list:
        buffer = self._buffer
        if not buffer:
            return []

        msgs = []
        n = len(buffer)
        i = 0
        msg_n = buffer[i]

        while i + msg_n <= n:
            msgs.append(self._parse_message(buffer[i:i + msg_n]))
            i += msg_n

            if i == n:
                break
            msg_n = buffer[i]

        if i != 0:
            self._buffer = buffer[i:]
        return msgs

    def _parse_message(self, data: bytes):
        pass
