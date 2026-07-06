import os  
import sys
import serial
import struct
import csv
import threading
import socket
import time
import signal
import queue
from datetime import datetime
import collections
import numpy as np
import pyqtgraph as pg

# --- PYQT5 INTERFACE TOOLS ---
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QRadioButton, QLabel
from PyQt5.QtGui import QFont
from PyQt5.QtCore import Qt

# Use OpenGL for GPU rendering
pg.setConfigOptions(useOpenGL=True, antialias=False)

# --- COUPLING & CONFIGURATION SWITCHES ---
INPUT_MODE = 'BOTH'        # Options: 'UART', 'WIFI', or 'BOTH'
DEFAULT_SOURCE = 1         # 0 for UART, 1 for WIFI

# --- Serial Config ---
SERIAL_PORT = 'COM5'
BAUD_RATE = 115200

# --- Wi-Fi UDP Config ---
UDP_IP = '0.0.0.0'         
UDP_PORT = 12345           

# --- Protocol Packet Config ---
PACKET_LEN = 26            # Đã tăng từ 25 lên 26 để thêm seq_num
HEADER = bytes([0xAA, 0x55])

# --- Sampling Rates ---
FS_PPG = 100               
FS_ECG = 700               

# --- GUI Chart Config ---
WINDOW_SECONDS = 5
ECG_MAX_SAMPLES = FS_ECG * WINDOW_SECONDS
PPG_MAX_SAMPLES = FS_PPG * WINDOW_SECONDS


def calculate_checksum(payload_bytes):
    return sum(payload_bytes) & 0xFF

def parse_packet(pkt):
    if len(pkt) != PACKET_LEN:
        return None
    # Checksum tích lũy từ byte chứa seq_num (idx 2) đến byte dữ liệu cuối cùng (idx 24)
    if calculate_checksum(pkt[2:25]) != pkt[25]:
        return None
    
    # Trích xuất dữ liệu dựa theo offset mới (dịch sau seq_num)
    seq_num = pkt[2]
    ecg_samples = struct.unpack('>7H', pkt[3:17])
    red_val = struct.unpack('>I', pkt[17:21])[0] & 0x03FFFF
    ir_val = struct.unpack('>I', pkt[21:25])[0] & 0x03FFFF
    return (seq_num, ecg_samples, red_val, ir_val)


class CsvLogger(threading.Thread):
    """ Dedicated background thread to handle batched CSV writing for both streams. """
    def __init__(self, log_queue):
        super().__init__(daemon=True)
        self.log_queue = log_queue
        self.running = True
        current_time = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.filename = f"sensor_data_unified_{current_time}.csv"
        # Đã thêm cột Seq_Num vào tiêu đề file CSV
        self.headers = ["Timestamp", "Mode", "Seq_Num", "ECG_1", "ECG_2", "ECG_3", "ECG_4", "ECG_5", "ECG_6", "ECG_7", "PPG_RED", "PPG_IR"]

    def run(self):
        abs_path = os.path.abspath(self.filename)
        print(f"\n[SYSTEM] Logging Unified Data to: {abs_path}\n")
        
        with open(self.filename, mode='a', newline='') as csv_file:
            writer = csv.writer(csv_file)
            writer.writerow(self.headers)
            
            while self.running:
                try:
                    item = self.log_queue.get(timeout=0.5)
                    batch = [item]
                    while not self.log_queue.empty() and len(batch) < 1000:
                        batch.append(self.log_queue.get_nowait())
                    
                    rows = []
                    for (ts, mode, seq_num, ecg, red, ir) in batch:
                        ts_str = datetime.fromtimestamp(ts).strftime("%H:%M:%S.%f")[:-3]
                        # Ghi nhận thêm giá trị seq_num vào hàng
                        rows.append([ts_str, mode, seq_num] + list(ecg) + [red, ir])
                        
                    writer.writerows(rows)
                    csv_file.flush()
                except queue.Empty:
                    continue
                except Exception as e:
                    print(f"[CSV Logger] Error: {e}")


class SerialReader(threading.Thread):
    def __init__(self, data_queue, log_queue):
        super().__init__(daemon=True)
        self.data_queue = data_queue
        self.log_queue = log_queue
        self.running = True
        self.mode_tag = 0  # 0 = UART
        
    def run(self):
        buf = bytearray()
        ser = None
        
        while self.running:
            if ser is None or not ser.is_open:
                try:
                    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
                    ser.setDTR(False)
                    ser.setRTS(False)
                    print(f"\n[UART] Successfully Connected to {SERIAL_PORT} @ {BAUD_RATE} baud.")
                except Exception:
                    time.sleep(2)
                    continue
            
            try:
                data = ser.read(1024)
                if data:
                    buf.extend(data)
                
                while True:
                    idx = buf.find(HEADER)
                    if idx == -1:
                        if len(buf) > 2:
                            buf = buf[-2:] 
                        break
                    
                    if len(buf) - idx < PACKET_LEN:
                        if idx > 0:
                            buf = buf[idx:]
                        break
                    
                    pkt = bytes(buf[idx:idx+PACKET_LEN])
                    buf = buf[idx+PACKET_LEN:] 
                    
                    result = parse_packet(pkt)
                    if result is None:
                        continue
                    
                    seq_num, ecg_samples, red_val, ir_val = result
                    ts = time.time()
                    
                    self.log_queue.put((ts, self.mode_tag, seq_num, ecg_samples, red_val, ir_val))
                    self.data_queue.append((self.mode_tag, seq_num, ecg_samples, red_val, ir_val))
                        
            except serial.SerialException:
                print(f"\n[UART] Sensor disconnected from {SERIAL_PORT}. Waiting for reconnection...")
                if ser:
                    ser.close()
                ser = None
            except Exception as e:
                print(f"[UART] Read loop error: {e}")
                time.sleep(1)
                
        if ser and ser.is_open:
            ser.close()


class UdpReader(threading.Thread):
    def __init__(self, data_queue, log_queue):
        super().__init__(daemon=True)
        self.data_queue = data_queue
        self.log_queue = log_queue
        self.running = True
        self.mode_tag = 1  # 1 = WIFI

    def run(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind((UDP_IP, UDP_PORT))
            sock.settimeout(0.2)
            print(f"[Wi-Fi] UDP socket listening on port {UDP_PORT}...")
        except Exception as e:
            print(f"[Wi-Fi] Socket binding failed: {e}")
            self.running = False
            return

        while self.running:
            try:
                data, addr = sock.recvfrom(1024)
                result = parse_packet(data)
                if result is None:
                    continue

                seq_num, ecg_samples, red_val, ir_val = result
                ts = time.time()

                self.log_queue.put((ts, self.mode_tag, seq_num, ecg_samples, red_val, ir_val))
                self.data_queue.append((self.mode_tag, seq_num, ecg_samples, red_val, ir_val))

            except socket.timeout:
                continue  
            except Exception as e:
                print(f"[Wi-Fi] Read loop error: {e}")
                break
        sock.close()


class RealTimePlot:
    def __init__(self, data_queue):
        self.data_queue = data_queue
        self.active_mode = DEFAULT_SOURCE
        
        self.ecg_data = np.zeros(ECG_MAX_SAMPLES)
        self.red_data = np.zeros(PPG_MAX_SAMPLES)
        self.ir_data = np.zeros(PPG_MAX_SAMPLES)
        
        self.ecg_sample_counter = 0
        self.ppg_sample_counter = 0
        self.last_frequency_check_time = time.time()
        
        # --- KHỞI TẠO BỘ ĐẾM SỐ THỨ TỰ & PACKET LOSS ---
        # Chỉ số 0 đại diện cho UART, Chỉ số 1 đại diện cho Wi-Fi
        self.last_seq = [None, None]
        self.lost_counter = [0, 0]
        self.total_counter = [0, 0]
        
        self.app = QApplication(sys.argv)
        self.main_win = QWidget()
        self.main_win.setWindowTitle("Biometric Stream Analytics Platform")
        self.main_win.resize(1200, 750)
        
        self.setup_ui()
        
        self.timer_chart = pg.QtCore.QTimer()
        self.timer_chart.timeout.connect(self.update_charts)
        self.timer_chart.start(33) 
        
        self.timer_dsp = pg.QtCore.QTimer()
        self.timer_dsp.timeout.connect(self.calculate_biometrics)
        self.timer_dsp.start(1000)
        
        self.main_win.show()

    def setup_ui(self):
        master_layout = QVBoxLayout()
        self.main_win.setLayout(master_layout)
        
        dashboard_layout = QHBoxLayout()
        
        self.lbl_ecg_bpm = QLabel("ECG BPM: --")
        self.lbl_ecg_bpm.setFont(QFont("Arial", 20, QFont.Bold))
        self.lbl_ecg_bpm.setStyleSheet("color: #4CAF50; padding: 10px; background-color: #1E1E1E; border-radius: 5px;")
        self.lbl_ecg_bpm.setAlignment(Qt.AlignCenter)
        
        self.lbl_ppg_bpm = QLabel("PPG BPM: --")
        self.lbl_ppg_bpm.setFont(QFont("Arial", 20, QFont.Bold))
        self.lbl_ppg_bpm.setStyleSheet("color: #00BCD4; padding: 10px; background-color: #1E1E1E; border-radius: 5px;")
        self.lbl_ppg_bpm.setAlignment(Qt.AlignCenter)
        
        self.lbl_spo2 = QLabel("SpO₂: -- %")
        self.lbl_spo2.setFont(QFont("Arial", 20, QFont.Bold))
        self.lbl_spo2.setStyleSheet("color: #FF5722; padding: 10px; background-color: #1E1E1E; border-radius: 5px;")
        self.lbl_spo2.setAlignment(Qt.AlignCenter)
        
        self.lbl_freq = QLabel("Telemetry Stats: Tracking...")
        self.lbl_freq.setFont(QFont("Arial", 12, QFont.Bold))
        self.lbl_freq.setStyleSheet("color: #E0E0E0; padding: 10px; background-color: #2D2D2D; border-radius: 5px;")
        self.lbl_freq.setAlignment(Qt.AlignCenter)
        
        dashboard_layout.addWidget(self.lbl_ecg_bpm)
        dashboard_layout.addWidget(self.lbl_ppg_bpm)
        dashboard_layout.addWidget(self.lbl_spo2)
        dashboard_layout.addWidget(self.lbl_freq)
        master_layout.addLayout(dashboard_layout)
        
        control_layout = QHBoxLayout()
        control_layout.addWidget(QLabel("<b>Active Live Plot Source:</b>"))
        
        self.btn_uart = QRadioButton("Hardware Serial (UART)")
        self.btn_wifi = QRadioButton("Wireless Network (Wi-Fi)")
        
        if self.active_mode == 0:
            self.btn_uart.setChecked(True)
        else:
            self.btn_wifi.setChecked(True)
            
        if INPUT_MODE == 'WIFI': self.btn_uart.setEnabled(False)
        if INPUT_MODE == 'UART': self.btn_wifi.setEnabled(False)
            
        control_layout.addWidget(self.btn_uart)
        control_layout.addWidget(self.btn_wifi)
        control_layout.addStretch() 
        master_layout.addLayout(control_layout)
        
        self.win = pg.GraphicsLayoutWidget()
        master_layout.addWidget(self.win)
        
        self.plot_ecg = self.win.addPlot()
        self.curve_ecg = self.plot_ecg.plot(pen=pg.mkPen('#4CAF50', width=1.5))
        
        self.win.nextRow()
        
        self.plot_ppg = self.win.addPlot()
        self.curve_red = self.plot_ppg.plot(pen=pg.mkPen('#FF5722', width=2), name="Red")
        self.curve_ir = self.plot_ppg.plot(pen=pg.mkPen('#00BCD4', width=2), name="IR")
        
        self.btn_uart.toggled.connect(self.handle_source_toggle)
        self.btn_wifi.toggled.connect(self.handle_source_toggle)
        
        self.update_plot_titles()

    def handle_source_toggle(self):
        self.active_mode = 0 if self.btn_uart.isChecked() else 1
        self.update_plot_titles()
        
        self.ecg_data.fill(0)
        self.red_data.fill(0)
        self.ir_data.fill(0)
        
        self.data_queue.clear()
        
        self.lbl_ecg_bpm.setText("ECG BPM: --")
        self.lbl_ppg_bpm.setText("PPG BPM: --")
        self.lbl_spo2.setText("SpO₂: -- %")
        self.lbl_freq.setText("Telemetry Stats: Resetting...")
        self.ecg_sample_counter = 0
        self.ppg_sample_counter = 0
        
        # Reset bộ đếm phân tích gói khi chuyển kênh
        self.last_seq = [None, None]
        self.lost_counter = [0, 0]
        self.total_counter = [0, 0]
        self.last_frequency_check_time = time.time()
        
    def update_plot_titles(self):
        src_label = "WIRED CABLE (UART)" if self.active_mode == 0 else "WIRELESS AIR (Wi-Fi)"
        self.plot_ecg.setTitle(f"ECG Signal — [Viewing: {src_label}]")
        self.plot_ppg.setTitle(f"PPG Signal — [Viewing: {src_label}]")

    def update_charts(self):
        new_ecg, new_red, new_ir = [], [], []
        
        n_items = len(self.data_queue)
        for _ in range(n_items):
            mode, seq_num, ecg_samples, red_val, ir_val = self.data_queue.popleft()
            
            # --- LOGIC TÍNH PACKET LOSS DỰA TRÊN SEQUENCE NUMBER (MODULO 256) ---
            if self.last_seq[mode] is not None:
                diff = (seq_num - self.last_seq[mode]) % 256
                if diff > 0:
                    if diff > 1:
                        self.lost_counter[mode] += (diff - 1)
                    self.total_counter[mode] += diff
            else:
                self.total_counter[mode] += 1
            
            self.last_seq[mode] = seq_num

            # Tách riêng dữ liệu của luồng hiển thị đang kích hoạt để vẽ đồ thị
            if mode == self.active_mode:
                new_ecg.extend(ecg_samples)
                new_red.append(red_val)
                new_ir.append(ir_val)
                self.ecg_sample_counter += len(ecg_samples)
                self.ppg_sample_counter += 1
            
        if not new_ecg:
            return
            
        ecg_len = len(new_ecg)
        ppg_len = len(new_red)
        
        if ecg_len > 0:
            if ecg_len >= ECG_MAX_SAMPLES:
                self.ecg_data[:] = new_ecg[-ECG_MAX_SAMPLES:]
            else:
                self.ecg_data[:-ecg_len] = self.ecg_data[ecg_len:]
                self.ecg_data[-ecg_len:] = new_ecg
            self.curve_ecg.setData(self.ecg_data)
            
        if ppg_len > 0:
            if ppg_len >= PPG_MAX_SAMPLES:
                self.red_data[:] = new_red[-PPG_MAX_SAMPLES:]
                self.ir_data[:] = new_ir[-PPG_MAX_SAMPLES:]
            else:
                self.red_data[:-ppg_len] = self.red_data[ppg_len:]
                self.red_data[-ppg_len:] = new_red
                self.ir_data[:-ppg_len] = self.ir_data[ppg_len:]
                self.ir_data[-ppg_len:] = new_ir
            
            self.curve_red.setData(self.red_data)
            self.curve_ir.setData(self.ir_data)

    def calculate_biometrics(self):
        right_now = time.time()
        duration = right_now - self.last_frequency_check_time
        
        # --- HIỂN THỊ ĐỒNG THỜI TẦN SỐ VÀ % PACKET LOSS LÊN UI ---
        uart_loss = 0.0
        if self.total_counter[0] > 0:
            uart_loss = (self.lost_counter[0] / self.total_counter[0]) * 100
            
        wifi_loss = 0.0
        if self.total_counter[1] > 0:
            wifi_loss = (self.lost_counter[1] / self.total_counter[1]) * 100

        if duration > 0:
            current_fs_ecg = self.ecg_sample_counter / duration
            current_fs_ppg = self.ppg_sample_counter / duration
            self.lbl_freq.setText(
                f"UART Loss: {uart_loss:.1f}% | Wi-Fi Loss: {wifi_loss:.1f}% | "
                f"Freq: ECG {current_fs_ecg:.1f}Hz | PPG {current_fs_ppg:.1f}Hz"
            )
        
        # Reset các biến đếm chu kỳ
        self.ecg_sample_counter = 0
        self.ppg_sample_counter = 0
        self.lost_counter = [0, 0]
        self.total_counter = [0, 0]
        self.last_frequency_check_time = right_now

        # ECG Calculation
        if not np.all(self.ecg_data == 0):
            ecg_window = self.ecg_data.copy()
            v_min, v_max, v_mean = np.min(ecg_window), np.max(ecg_window), np.mean(ecg_window)
            threshold = v_mean + 0.5 * (v_max - v_min)
            
            ecg_peaks = []
            min_ecg_distance = int(FS_ECG * 0.35) 
            last_ecg_idx = -min_ecg_distance
            
            for i in range(1, len(ecg_window) - 1):
                if ecg_window[i] > threshold and ecg_window[i] >= ecg_window[i-1] and ecg_window[i] > ecg_window[i+1]:
                    if (i - last_ecg_idx) > min_ecg_distance:
                        ecg_peaks.append(i)
                        last_ecg_idx = i
                        
            if len(ecg_peaks) >= 2:
                bpm_ecg = (60.0 * FS_ECG) / np.mean(np.diff(ecg_peaks))
                if 40 <= bpm_ecg <= 220: 
                    self.lbl_ecg_bpm.setText(f"ECG BPM: {int(round(bpm_ecg))}")
                else:
                    self.lbl_ecg_bpm.setText("ECG BPM: Noise")
            else:
                self.lbl_ecg_bpm.setText("ECG BPM: Syncing...")
        else:
            self.lbl_ecg_bpm.setText("ECG BPM: No Lead")

        # PPG Calculation
        if not np.all(self.ir_data == 0):
            ir_window = self.ir_data.copy()
            p_min, p_max, p_mean = np.min(ir_window), np.max(ir_window), np.mean(ir_window)
            ir_threshold = p_mean + 0.3 * (p_max - p_min)
            
            ppg_peaks = []
            min_ppg_distance = int(FS_PPG * 0.4) 
            last_ppg_idx = -min_ppg_distance
            
            for i in range(1, len(ir_window) - 1):
                if ir_window[i] > ir_threshold and ir_window[i] >= ir_window[i-1] and ir_window[i] > ir_window[i+1]:
                    if (i - last_ppg_idx) > min_ppg_distance:
                        ppg_peaks.append(i)
                        last_ppg_idx = i
                        
            if len(ppg_peaks) >= 2:
                bpm_ppg = (60.0 * FS_PPG) / np.mean(np.diff(ppg_peaks))
                if 40 <= bpm_ppg <= 180:
                    self.lbl_ppg_bpm.setText(f"PPG BPM: {int(round(bpm_ppg))}")
                else:
                    self.lbl_ppg_bpm.setText("PPG BPM: Noise")
            else:
                self.lbl_ppg_bpm.setText("PPG BPM: Syncing...")
        else:
            self.lbl_ppg_bpm.setText("PPG BPM: Clip Off")

        # SpO2 Calculation
        if not np.all(self.red_data == 0) and not np.all(self.ir_data == 0):
            red_window = self.red_data.copy()
            ir_window = self.ir_data.copy()
            
            red_dc = np.mean(red_window)
            ir_dc = np.mean(ir_window)
            
            if red_dc > 0 and ir_dc > 0:
                red_ac = np.max(red_window) - np.min(red_window)
                ir_ac = np.max(ir_window) - np.min(ir_window)
                
                if ir_ac > 0 and red_ac > 0:
                    ratio_of_ratios = (red_ac / red_dc) / (ir_ac / ir_dc)
                    spo2 = 110.0 - (25.0 * ratio_of_ratios)
                    
                    if 50 <= spo2 <= 100:  
                        self.lbl_spo2.setText(f"SpO₂: {int(round(spo2))}%")
                    else:
                        self.lbl_spo2.setText("SpO₂: Unstable")
                else:
                    self.lbl_spo2.setText("SpO₂: Flatline")
            else:
                self.lbl_spo2.setText("SpO₂: No Pulse")
        else:
            self.lbl_spo2.setText("SpO₂: Clip Off")

    def start(self):
        sys.exit(self.app.exec_())


def main():
    signal.signal(signal.SIGINT, signal.SIG_DFL)  
    
    data_queue = collections.deque()
    log_queue = queue.Queue() 
    
    threads_to_manage = []

    csv_thread = CsvLogger(log_queue)
    threads_to_manage.append(csv_thread)

    if INPUT_MODE in ['UART', 'BOTH']:
        uart_thread = SerialReader(data_queue, log_queue)
        threads_to_manage.append(uart_thread)

    if INPUT_MODE in ['WIFI', 'BOTH']:
        wifi_thread = UdpReader(data_queue, log_queue)
        threads_to_manage.append(wifi_thread)

    for thread in threads_to_manage:
        thread.start()
    
    try:
        plotter = RealTimePlot(data_queue)
        plotter.start()
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()