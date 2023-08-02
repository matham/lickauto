import serial
from struct import unpack, calcsize, pack
from enum import IntEnum
from typing import Optional


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


class HostCode(IntEnum):
    modio_board = 0
    stream_marker = 1
    comm = 2
    echo = 3


class ModIOCmd(IntEnum):
    create = 0
    remove = 1
    read_dig_cont_start = 2
    read_dig_cont_stop = 3
    read_dig = 4
    write_dig = 5
    address_change = 6


class ModIOPullup(IntEnum):
    disabled = 0
    enabled_22k_ohm = 1
    enabled_47k_ohm = 2
    enabled_100k_ohm = 3


class ModIOFreq(IntEnum):
    freq_100k = 0
    freq_400k = 1
    freq_1m = 2


class MarkerCmd(IntEnum):
    enable = 0
    disable = 1
    mark = 2


class TeensyComm:

    _ser: Optional[serial.Serial] = None

    _host_comm_f = 'BBBB'

    _modio_data_f = 'BBB'

    _modio_create_f = 'BB'

    _modio_data_buff_f = 'BB'

    _marker_data_f = 'B'

    _marker_enable_f = 'LBB'

    _marker_item_f = 'B'

    _buffer = b''

    def create_serial_device(self, name: str):
        self._ser = serial.Serial(name)
        # self._ser.open()
        self._buffer = b''

    def close_serial_device(self):
        if self._ser is None:
            return
        self._ser.close()
        self._ser = None

    def write_serial(self, data: bytes):
        self._ser.write(data)

    def read_serial(self, size: Optional[int] = None):
        if size is None:
            data = self._ser.read_all()
        else:
            data = self._ser.read(size)

        if not len(data):
            return
        if len(self._buffer):
            self._buffer += data
        else:
            self._buffer = data

    def make_host_echo(self, id_val: int):
        fmt = '<' + self._host_comm_f
        return pack(
            fmt, calcsize(fmt), HostCode.echo.value, id_val,
            HostError.no_error.value
        )

    def make_modio_create(
            self, id_val: int, port: int, address: int, freq: ModIOFreq,
            pullup: ModIOPullup
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f + \
              self._modio_create_f

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.create.value,
            freq.value, pullup.value
        )

    def make_modio_remove(self, id_val: int, port: int, address: int):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.remove.value
        )

    def make_modio_read_digital(self, id_val: int, port: int, address: int):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.read_dig.value
        )

    def make_modio_read_digital_cont_start(
            self, id_val: int, port: int, address: int
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address,
            ModIOCmd.read_dig_cont_start.value
        )

    def make_modio_read_digital_cont_stop(
            self, id_val: int, port: int, address: int
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
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

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.write_dig.value,
            0, value
        )

    def make_modio_change_address(
            self, id_val: int, port: int, address: int, new_address: int
    ):
        fmt = '<' + self._host_comm_f + self._modio_data_f + \
              self._modio_data_buff_f

        return pack(
            fmt, calcsize(fmt), HostCode.modio_board.value, id_val,
            HostError.no_error.value, port, address, ModIOCmd.write_dig.value,
            0, new_address
        )

    def make_marker_enable(
            self, id_val: int, duration: int, clock_pin: int, data_pin: int
    ):
        fmt = '<' + self._host_comm_f + self._marker_data_f + \
              self._modio_create_f

        return pack(
            fmt, calcsize(fmt), HostCode.stream_marker.value, id_val,
            HostError.no_error.value, MarkerCmd.enable.value,
            duration, clock_pin, data_pin
        )

    def make_marker_disable(self, id_val: int):
        fmt = '<' + self._host_comm_f + self._marker_data_f

        return pack(
            fmt, calcsize(fmt), HostCode.stream_marker.value, id_val,
            HostError.no_error.value, MarkerCmd.disable.value
        )

    def make_marker_mark(self, id_val: int):
        fmt = '<' + self._host_comm_f + self._marker_data_f

        return pack(
            fmt, calcsize(fmt), HostCode.stream_marker.value, id_val,
            HostError.no_error.value, MarkerCmd.mark.value
        )

    def parse_buffer(self) -> list[dict]:
        buffer = self._buffer
        if not buffer:
            return []

        msgs = []
        n = len(buffer)
        i = 0
        msg_n, = unpack("<B", buffer[:1])

        while i + msg_n <= n:
            msgs.append(self._parse_message(buffer[i:i + msg_n]))
            i += msg_n

            if i == n:
                break
            msg_n, = unpack("<B", buffer[i:i + 1])

        if i != 0:
            self._buffer = buffer[i:]
        return msgs

    def _parse_message(self, data: bytes):
        host_fmt = self._host_comm_f
        host_n = calcsize(host_fmt)

        marker_data_f = self._marker_data_f
        marker_data_n = calcsize(marker_data_f)
        marker_item_f = self._marker_item_f
        marker_item_n = calcsize(marker_item_f)

        modio_data_f = self._modio_data_f
        modio_data_n = calcsize(modio_data_f)
        modio_data_buff_f = self._modio_data_buff_f
        modio_data_buff_n = calcsize(modio_data_buff_f)

        result = {}

        n, = unpack("<B", data[:1])
        if n != len(data) or n < host_n:
            raise ValueError("Read packet is too small for data")

        vals = unpack("<" + host_fmt, data[:host_n])
        code = HostCode(vals[1])
        result["src"] = code
        result["id_val"] = vals[2]
        result["error"] = HostError(vals[3])
        error = result["error"] != HostError.no_error

        start = host_n
        if code == HostCode.echo or code == HostCode.comm:
            if n != start:
                raise ValueError("Read packet has too much data")
            return result

        # with error, we can have no additional data
        if n == start and error:
            return result

        if code == HostCode.stream_marker:
            end = start + marker_data_n
            if n < end:
                raise ValueError("Read packet is too small for marker data")

            cmd, = unpack("<" + marker_data_f, data[start:end])
            cmd = MarkerCmd(cmd)
            result['cmd'] = cmd
            start = end

            # only mark sends back additional data
            if code == MarkerCmd.mark:
                if n == start and error:
                    return result

                end = start + marker_item_n
                if n < end:
                    raise ValueError("Read packet is too small for marker data")

                result["marker"], = unpack("<" + marker_item_f, data[start:end])
                start = end

        elif code == HostCode.modio_board:
            end = start + modio_data_n
            if n < end:
                raise ValueError("Read packet is too small for modio data")

            port, address, cmd, = unpack("<" + modio_data_f, data[start:end])
            cmd = ModIOCmd(cmd)
            result['cmd'] = cmd
            result['port'] = port
            result['address'] = address
            start = end

            # only mark sends back additional data
            if code in (
                    ModIOCmd.write_dig, ModIOCmd.read_dig,
                    ModIOCmd.read_dig_cont_start, ModIOCmd.address_change
            ):
                if n == start and error:
                    return result

                end = start + modio_data_buff_n
                if n < end:
                    raise ValueError("Read packet is too small for modio data")

                result['mark'], result['value'], = unpack(
                    "<" + marker_item_f, data[start:end])
                start = end

        if n != start:
            raise ValueError("Read packet has too much data")

        return result
