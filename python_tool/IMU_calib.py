import argparse
import struct
import sys
import time
from collections import deque

try:
    import serial
    from serial.tools import list_ports
except ImportError as exc:
    raise SystemExit(
        "Thieu thu vien pyserial. Cai dat bang lenh: pip install pyserial pyqtgraph PySide6"
    ) from exc

try:
    import pyqtgraph as pg
except ImportError as exc:
    raise SystemExit(
        "Thieu thu vien pyqtgraph. Cai dat bang lenh: pip install pyqtgraph"
    ) from exc

try:
    from PySide6.QtCore import QThread, Qt, Signal, QTimer
    from PySide6.QtWidgets import (
        QApplication,
        QComboBox,
        QGridLayout,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QSizePolicy,
        QSpinBox,
        QVBoxLayout,
        QWidget,
    )
except ImportError as exc:
    raise SystemExit(
        "Thieu thu vien PySide6. Cai dat bang lenh: pip install PySide6"
    ) from exc


DEFAULT_FIELDS = [
    "accel_x",
    "accel_y",
    "accel_z",
    "gyro_x",
    "gyro_y",
    "gyro_z",
    "temp",
]
DEFAULT_HEADER = 0xAA
DEFAULT_PERIOD_MS = 10
DEFAULT_REQUEST_BYTE = b"A"
CRC_SIZE = 2
FRAME_OVERHEAD = 4
IMU_STRUCT = struct.Struct("<hhhhhhh")
DEFAULT_PORT = "/dev/ttyUSB0"
UI_REFRESH_MS = 42
MAX_LOG_LINES = 300
PLOT_COLORS = [
    "#2563eb",
    "#dc2626",
    "#16a34a",
    "#9333ea",
    "#ea580c",
    "#0891b2",
    "#4f46e5",
    "#be123c",
    "#0f766e",
]


def crc16_modbus(data):
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def parse_imu_payload(payload):
    if len(payload) != IMU_STRUCT.size:
        return None

    values = IMU_STRUCT.unpack(payload)
    return {field: float(value) for field, value in zip(DEFAULT_FIELDS, values)}


class SerialReader(QThread):
    data_received = Signal(float, str, dict)
    raw_received = Signal(str)
    status_changed = Signal(str)
    error_occurred = Signal(str)

    def __init__(self, port, baudrate, timeout, header, period_ms):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.header = header
        self.period_ms = period_ms
        self.serial_conn = None
        self._running = True
        self._streaming = False
        self.rx_buffer = bytearray()

    def stop(self):
        self._running = False
        if self.serial_conn and self.serial_conn.is_open:
            try:
                self.serial_conn.close()
            except Exception:
                pass
        self.wait(1000)

    def set_streaming(self, enabled):
        self._streaming = enabled

    def run(self):
        try:
            self.serial_conn = serial.Serial(
                self.port,
                self.baudrate,
                timeout=self.timeout,
            )
            self.status_changed.emit(
                f"Da mo cong {self.port} @ {self.baudrate}, san sang polling"
            )
        except Exception as exc:
            self.error_occurred.emit(f"Khong mo duoc cong serial: {exc}")
            return

        next_request_time = time.monotonic()
        while self._running:
            try:
                now = time.monotonic()
                if self._streaming and now >= next_request_time:
                    self.serial_conn.write(DEFAULT_REQUEST_BYTE)
                    next_request_time = now + (self.period_ms / 1000.0)

                chunk = self.serial_conn.read(self.serial_conn.in_waiting or 1)
            except Exception as exc:
                if self._running:
                    self.error_occurred.emit(f"Loi serial: {exc}")
                break

            if not chunk:
                if not self._streaming:
                    self.msleep(20)
                continue

            self.rx_buffer.extend(chunk)
            self._consume_frames()

        self.status_changed.emit("Da dung doc serial")

    def _consume_frames(self):
        while True:
            if len(self.rx_buffer) < FRAME_OVERHEAD:
                return

            if self.rx_buffer[0] != self.header:
                header_pos = self.rx_buffer.find(bytes([self.header]))
                if header_pos == -1:
                    dropped = bytes(self.rx_buffer)
                    self.rx_buffer.clear()
                    self.raw_received.emit(f"Drop bytes: {dropped.hex(' ')}")
                    return

                if header_pos > 0:
                    dropped = bytes(self.rx_buffer[:header_pos])
                    del self.rx_buffer[:header_pos]
                    self.raw_received.emit(f"Drop bytes: {dropped.hex(' ')}")

            if len(self.rx_buffer) < FRAME_OVERHEAD:
                return

            payload_size = self.rx_buffer[1]
            frame_size = 1 + 1 + payload_size + CRC_SIZE
            if len(self.rx_buffer) < frame_size:
                return

            frame = bytes(self.rx_buffer[:frame_size])
            del self.rx_buffer[:frame_size]

            crc_received = int.from_bytes(frame[-CRC_SIZE:], byteorder="little")
            crc_expected = crc16_modbus(frame[:-CRC_SIZE])
            if crc_received != crc_expected:
                self.raw_received.emit(
                    f"CRC error: got 0x{crc_received:04X}, expected 0x{crc_expected:04X}, frame={frame.hex(' ')}"
                )
                continue

            payload = frame[2:-CRC_SIZE]
            parsed = parse_imu_payload(payload)
            if parsed is None:
                self.raw_received.emit(
                    f"Sai kich thuoc payload={payload_size}, can {IMU_STRUCT.size}, frame={frame.hex(' ')}"
                )
                continue

            self.data_received.emit(time.time(), frame.hex(" "), parsed)


class IMUCalibWindow(QMainWindow):
    def __init__(self, args):
        super().__init__()
        self.setWindowTitle("IMU Serial Monitor")
        self.resize(1280, 820)

        self.fields = list(DEFAULT_FIELDS)
        self.max_points = args.window
        self.header = args.header
        self.period_ms = args.period
        self.reader = None
        self.is_streaming = False
        self.series = {field: deque(maxlen=self.max_points) for field in self.fields}
        self.timestamps = deque(maxlen=self.max_points)
        self.value_labels = {}
        self.plot_configs = []
        self.selection_options = ["None"] + self.fields
        self.log_lines = deque(maxlen=MAX_LOG_LINES)
        self.latest_values = {field: 0.0 for field in self.fields}
        self.displayed_values = {field: None for field in self.fields}
        self.plot_dirty = False
        self.x_values_cache = []

        self.port_input = QComboBox()
        self.port_input.setEditable(True)
        self.port_input.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        self.port_input.setMinimumWidth(150)

        self.baud_input = QLineEdit(str(args.baudrate))
        self.baud_input.setFixedWidth(90)
        self.timeout_input = QLineEdit(str(args.timeout))
        self.timeout_input.setFixedWidth(70)
        self.period_input = QSpinBox()
        self.period_input.setRange(1, 1000)
        self.period_input.setSuffix(" ms")
        self.period_input.setValue(args.period)
        self.period_input.setFixedWidth(90)
        self.status_label = QLabel("Chua ket noi")
        self.status_label.setWordWrap(True)
        self.connect_button = QPushButton("Connect")
        self.stream_button = QPushButton("Start")
        self.stream_button.setEnabled(False)
        self.refresh_button = QPushButton("Refresh Port")

        default_port = args.port or DEFAULT_PORT
        self.port_input.addItem(default_port)
        self.port_input.setCurrentText(default_port)

        self._build_ui()
        self._create_plot()
        self._refresh_ports()
        self.ui_timer = QTimer(self)
        self.ui_timer.timeout.connect(self._flush_ui)
        self.ui_timer.start(UI_REFRESH_MS)

    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)

        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(8, 8, 8, 8)
        root_layout.setSpacing(6)

        toolbar_grid = QGridLayout()
        toolbar_grid.setHorizontalSpacing(8)
        toolbar_grid.setVerticalSpacing(6)
        toolbar_grid.addWidget(QLabel("Port"), 0, 0)
        toolbar_grid.addWidget(self.port_input, 0, 1)
        toolbar_grid.addWidget(QLabel("Baud"), 0, 2)
        toolbar_grid.addWidget(self.baud_input, 0, 3)
        toolbar_grid.addWidget(QLabel("Timeout"), 0, 4)
        toolbar_grid.addWidget(self.timeout_input, 0, 5)
        toolbar_grid.addWidget(QLabel("Period"), 0, 6)
        toolbar_grid.addWidget(self.period_input, 0, 7)
        toolbar_grid.addWidget(self.refresh_button, 0, 8)
        toolbar_grid.addWidget(self.connect_button, 0, 9)
        toolbar_grid.addWidget(self.stream_button, 0, 10)
        toolbar_grid.addWidget(self.status_label, 1, 0, 1, 11)
        toolbar_grid.setColumnStretch(11, 1)
        root_layout.addLayout(toolbar_grid)

        values_bar = QWidget()
        values_layout = QGridLayout(values_bar)
        values_layout.setContentsMargins(0, 0, 0, 0)
        values_layout.setHorizontalSpacing(10)
        values_layout.setVerticalSpacing(2)
        for index, field in enumerate(self.fields):
            col = index * 2
            name_label = QLabel(field.upper())
            name_label.setStyleSheet("font-size: 11px; color: #475569;")
            value_label = QLabel("0")
            value_label.setMinimumWidth(52)
            value_label.setStyleSheet("font-size: 11px; font-weight: 600;")
            values_layout.addWidget(name_label, 0, col)
            values_layout.addWidget(value_label, 0, col + 1)
            self.value_labels[field] = value_label
        values_layout.setColumnStretch(len(self.fields) * 2, 1)
        root_layout.addWidget(values_bar, 0)

        plot_panel = QWidget()
        self.right_layout = QVBoxLayout(plot_panel)
        self.right_layout.setContentsMargins(0, 0, 0, 0)
        self.right_layout.setSpacing(4)
        root_layout.addWidget(plot_panel, 1)

        self.refresh_button.setStyleSheet("background-color: #e2e8f0; color: #0f172a;")
        self.connect_button.setStyleSheet("background-color: #2563eb; color: white; font-weight: 600;")
        self.stream_button.setStyleSheet("background-color: #16a34a; color: white; font-weight: 600;")

        self.refresh_button.clicked.connect(self._refresh_ports)
        self.connect_button.clicked.connect(self._toggle_connection)
        self.stream_button.clicked.connect(self._toggle_streaming)

    def _create_plot(self):
        pg.setConfigOptions(antialias=False, background="w", foreground="k")

        default_selections = [
            ["accel_x", "accel_y", "accel_z"],
            ["gyro_x", "gyro_y", "gyro_z"],
            ["accel_x", "gyro_x", "None"],
        ]

        controls_layout = QHBoxLayout()
        controls_layout.setSpacing(4)
        for plot_index in range(3):
            controls_layout.addWidget(QLabel(f"P{plot_index + 1}"))
            combos = []
            for slot_index in range(3):
                combo = QComboBox()
                combo.addItems(self.selection_options)
                combo.setMinimumWidth(105)
                combo.setMaximumWidth(125)
                combo.setCurrentText(default_selections[plot_index][slot_index])
                combo.currentIndexChanged.connect(self._mark_plot_dirty)
                controls_layout.addWidget(combo)
                combos.append(combo)

            plot_widget = pg.PlotWidget()
            plot_item = plot_widget.getPlotItem()
            plot_item.setTitle(f"IMU Plot {plot_index + 1}")
            plot_item.showGrid(x=True, y=True, alpha=0.2)
            plot_item.setLabel("left", "Gia tri raw")
            if plot_index == 2:
                plot_item.setLabel("bottom", "Mau gan nhat")
            plot_item.setMenuEnabled(False)
            plot_item.hideButtons()
            plot_item.setClipToView(True)
            plot_item.setDownsampling(auto=True, mode="peak")
            legend = plot_item.addLegend(offset=(8, 8))

            curves = []
            for slot_index in range(3):
                curve = plot_item.plot(
                    pen=pg.mkPen(
                        PLOT_COLORS[(plot_index * 3 + slot_index) % len(PLOT_COLORS)],
                        width=2,
                    )
                )
                curves.append(curve)

            self.plot_configs.append(
                {
                    "plot_item": plot_item,
                    "combos": combos,
                    "curves": curves,
                    "legend": legend,
                    "last_selection": None,
                }
            )
            self.right_layout.addWidget(plot_widget, 1)

        controls_layout.addStretch(1)
        self.right_layout.insertLayout(0, controls_layout)

    def _refresh_ports(self):
        current = self.port_input.currentText().strip()
        ports = [port.device for port in list_ports.comports()]
        self.port_input.blockSignals(True)
        self.port_input.clear()
        self.port_input.addItems(ports)
        if current:
            if current not in ports:
                self.port_input.addItem(current)
            self.port_input.setCurrentText(current)
        elif ports:
            self.port_input.setCurrentIndex(0)
        else:
            self.port_input.setCurrentText(DEFAULT_PORT)
        self.port_input.blockSignals(False)

    def _toggle_connection(self):
        if self.reader and self.reader.isRunning():
            self._disconnect()
        else:
            self._connect()

    def _toggle_streaming(self):
        if not self.reader or not self.reader.isRunning():
            return

        self.period_ms = self.period_input.value()
        self.reader.period_ms = self.period_ms
        self.is_streaming = not self.is_streaming
        self.reader.set_streaming(self.is_streaming)
        self.stream_button.setText("Stop" if self.is_streaming else "Start")
        self.stream_button.setStyleSheet(
            "background-color: #dc2626; color: white; font-weight: 600;"
            if self.is_streaming
            else "background-color: #16a34a; color: white; font-weight: 600;"
        )
        action = "Bat dau gui 'A' dinh ky" if self.is_streaming else "Dung gui 'A' dinh ky"
        self.status_label.setText(action)
        self._append_log(action)

    def _connect(self):
        port = self.port_input.currentText().strip()
        if not port:
            QMessageBox.critical(self, "Loi", "Chua chon cong serial")
            return

        try:
            baudrate = int(self.baud_input.text().strip())
            timeout = float(self.timeout_input.text().strip())
        except ValueError:
            QMessageBox.critical(self, "Loi", "Baudrate hoac timeout khong hop le")
            return

        self.period_ms = self.period_input.value()

        self.reader = SerialReader(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            header=self.header,
            period_ms=self.period_ms,
        )
        self.reader.data_received.connect(self._handle_data)
        self.reader.raw_received.connect(self._handle_raw)
        self.reader.status_changed.connect(self._handle_status)
        self.reader.error_occurred.connect(self._handle_error)
        self.reader.start()

        self.connect_button.setText("Disconnect")
        self.stream_button.setEnabled(True)
        self.stream_button.setText("Start")
        self.stream_button.setStyleSheet("background-color: #16a34a; color: white; font-weight: 600;")
        self.is_streaming = False
        self.status_label.setText("Da connect, chua start")
        self._append_log(
            f"Da connect {port} @ {baudrate}, header=0x{self.header:02X}, request='A', period={self.period_ms} ms"
        )

    def _disconnect(self):
        if self.reader:
            self.reader.set_streaming(False)
            self.reader.stop()
            self.reader = None
        self.is_streaming = False
        self.connect_button.setText("Connect")
        self.stream_button.setEnabled(False)
        self.stream_button.setText("Start")
        self.stream_button.setStyleSheet("background-color: #16a34a; color: white; font-weight: 600;")
        self.status_label.setText("Da ngat ket noi")
        self._append_log("Da ngat ket noi serial")

    def _handle_status(self, status):
        self.status_label.setText(status)
        self._append_log(status)

    def _handle_error(self, error_text):
        self.status_label.setText("Loi")
        self.connect_button.setText("Connect")
        self.stream_button.setEnabled(False)
        self.stream_button.setText("Start")
        self.stream_button.setStyleSheet("background-color: #16a34a; color: white; font-weight: 600;")
        self.is_streaming = False
        self._append_log(error_text)
        if self.reader:
            self.reader.stop()
            self.reader = None
        QMessageBox.critical(self, "Serial error", error_text)

    def _handle_raw(self, raw_text):
        self._append_log(raw_text)

    def _handle_data(self, timestamp, frame_hex, parsed):
        self.timestamps.append(timestamp)

        for field in self.fields:
            value = parsed[field]
            self.series[field].append(value)
            self.latest_values[field] = value

        self.plot_dirty = True

    def _append_log(self, text):
        self.log_lines.append(f"{time.strftime('%H:%M:%S')}  {text}")

    def _mark_plot_dirty(self):
        self.plot_dirty = True

    def _flush_ui(self):
        for field in self.fields:
            value_text = str(int(self.latest_values[field]))
            if self.displayed_values[field] != value_text:
                self.value_labels[field].setText(value_text)
                self.displayed_values[field] = value_text

        if self.plot_dirty:
            self._update_plot()
            self.plot_dirty = False

    def _update_plot(self):
        if not self.timestamps:
            return

        point_count = len(self.timestamps)
        if len(self.x_values_cache) != point_count:
            self.x_values_cache = list(range(point_count))
        x_values = self.x_values_cache

        for plot_config in self.plot_configs:
            plot_item = plot_config["plot_item"]
            y_min = None
            y_max = None
            legend_curves = []
            legend_labels = []
            current_selection = tuple(combo.currentText() for combo in plot_config["combos"])
            selection_changed = current_selection != plot_config["last_selection"]

            for combo, curve in zip(plot_config["combos"], plot_config["curves"]):
                field = combo.currentText()
                if field == "None":
                    curve.setData([], [])
                    continue

                y_values = self.series[field]
                curve.setData(x_values, list(y_values))
                legend_curves.append(curve)
                legend_labels.append(field.upper())

                if y_values:
                    current_min = min(y_values)
                    current_max = max(y_values)
                    y_min = current_min if y_min is None else min(y_min, current_min)
                    y_max = current_max if y_max is None else max(y_max, current_max)

            plot_item.setXRange(0, max(point_count - 1, 1), padding=0.0)
            if y_min is None or y_max is None:
                plot_item.setYRange(-1, 1, padding=0.0)
            else:
                padding = 1.0 if y_min == y_max else max((y_max - y_min) * 0.1, 1.0)
                plot_item.setYRange(y_min - padding, y_max + padding, padding=0.0)

            if selection_changed:
                legend = plot_config["legend"]
                legend.clear()
                for curve, label in zip(legend_curves, legend_labels):
                    legend.addItem(curve, label)
                plot_config["last_selection"] = current_selection


    def closeEvent(self, event):
        if self.reader and self.reader.isRunning():
            self.reader.stop()
            self.reader = None
        super().closeEvent(event)


def build_arg_parser():
    parser = argparse.ArgumentParser(
        description="Gui 'A' dinh ky, doc frame IMU nhi phan va hien thi du lieu."
    )
    parser.add_argument("--port", default="", help="Cong serial, vi du /dev/ttyUSB0")
    parser.add_argument("--baudrate", type=int, default=115200, help="Baudrate serial")
    parser.add_argument("--timeout", type=float, default=0.02, help="Timeout doc serial")
    parser.add_argument(
        "--header",
        type=lambda value: int(value, 0),
        default=DEFAULT_HEADER,
        help="Header frame, mac dinh 0xAA",
    )
    parser.add_argument(
        "--period",
        type=int,
        default=DEFAULT_PERIOD_MS,
        help="Chu ky gui ky tu 'A' theo ms",
    )
    parser.add_argument(
        "--window",
        type=int,
        default=300,
        help="So mau giu lai tren bieu do",
    )
    return parser


def main():
    args = build_arg_parser().parse_args()
    app = QApplication(sys.argv)
    window = IMUCalibWindow(args)
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
