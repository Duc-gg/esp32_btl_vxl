import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import os
# Thêm iirnotch để lọc nhiễu điện lưới, find_peaks để tìm đỉnh chính xác cho báo cáo
from scipy.signal import butter, iirnotch, filtfilt, find_peaks

# --- CẤU HÌNH ĐƯỜNG DẪN FILE ---
FILE_PATH = r"E:\pySerial\sensor_data_unified_20260707_172743.csv"  

# Tần số lấy mẫu cấu hình gốc
FS_ECG = 700  # Hz
FS_PPG = 100  # Hz

# --- 1. BỘ LỌC THÔNG DẢI (BANDPASS) ---
def butter_bandpass_filter(data, lowcut, highcut, fs, order=4):
    nyq = 0.5 * fs
    low = lowcut / nyq
    high = highcut / nyq
    b, a = butter(order, [low, high], btype='band')
    return filtfilt(b, a, data)

# --- 2. BỘ LỌC CHẮN DẢI HẸP (NOTCH FILTER) ---
def butter_notch_filter(data, notch_freq, fs, q=30):
    nyq = 0.5 * fs
    w0 = notch_freq / nyq
    b, a = iirnotch(w0, q)
    return filtfilt(b, a, data)

def analyze_biometric_csv(file_path):
    if not os.path.exists(file_path):
        print(f"[LỖI] Không tìm thấy file tại đường dẫn: {os.path.abspath(file_path)}")
        return

    print(f"[SYSTEM] Đang đọc và xử lý file: {file_path}...\n")
    
    # Đọc dữ liệu từ file CSV
    df = pd.read_csv(file_path)
    df.columns = df.columns.str.strip()
    
    # Trích xuất dữ liệu thô
    ecg_raw = df[['ECG_1', 'ECG_2', 'ECG_3', 'ECG_4', 'ECG_5', 'ECG_6', 'ECG_7']].values.flatten()
    ppg_ir_raw = df['PPG_IR'].values
    ppg_red_raw = df['PPG_RED'].values  
    
    # --- XỬ LÝ LỌC ECG BIÊN ĐỘ CAO ---
    ecg_bandpassed = butter_bandpass_filter(ecg_raw, lowcut=0.5, highcut=100.0, fs=FS_ECG, order=4)
    ecg_filtered = butter_notch_filter(ecg_bandpassed, notch_freq=50.0, fs=FS_ECG, q=35)

    # --- XỬ LÝ LỌC PPG IR & RED ---
    ppg_ir_filtered = butter_bandpass_filter(ppg_ir_raw, lowcut=0.5, highcut=4.0, fs=FS_PPG, order=4)
    ppg_red_filtered = butter_bandpass_filter(ppg_red_raw, lowcut=0.5, highcut=4.0, fs=FS_PPG, order=4)

    # Thống kê số liệu hệ thống
    total_rows = len(df)
    total_ecg_samples = len(ecg_raw)
    total_ppg_samples = len(ppg_ir_raw)
    time_ecg = np.arange(total_ecg_samples) / FS_ECG
    time_ppg = np.arange(total_ppg_samples) / FS_PPG

    # Tính toán thời gian đo đạc thực tế từ cột Timestamp
    try:
        timestamps = pd.to_datetime(df['Timestamp'].str.strip(), format='%H:%M:%S.%f')
        total_seconds = (timestamps.iloc[-1] - timestamps.iloc[0]).total_seconds()
    except Exception:
        total_seconds = total_ppg_samples / FS_PPG

    hours = int(total_seconds // 3600)
    minutes = int((total_seconds % 3600) // 60)
    seconds = total_seconds % 60
    
    avg_fs_ecg = total_ecg_samples / total_seconds if total_seconds > 0 else 0.0
    avg_fs_ppg = total_ppg_samples / total_seconds if total_seconds > 0 else 0.0

    # Thống kê bộ lọc tần số lấy mẫu (FS_Status)
    status_series = df['FS_Status'].str.strip().str.upper() if 'FS_Status' in df.columns else pd.Series()
    sync_count = (status_series == 'SYNCING').sum()
    pass_count = (status_series == 'PASS').sum()
    fail_count = (status_series == 'FAIL').sum()
    
    total_status = sync_count + pass_count + fail_count
    pass_rate = (pass_count / total_status * 100) if total_status > 0 else 0.0

    # --- THUẬT TOÁN TÍNH PACKET LOSS CHÍNH XÁC (ĐÃ SỬA LỖI GỘP NGUỒN) ---
    lost_packets = 0
    total_expected_packets = 0
    loss_rate = 0.0
    
    if 'Seq_Num' in df.columns and 'Source' in df.columns:
        sources = df['Source'].str.strip().unique()
        print("------------------------------------------------------------------")
        print(" * Thống kê truyền nhận dữ liệu chi tiết theo từng nguồn:")
        
        for src in sources:
            df_src = df[df['Source'].str.strip() == src]
            src_rows = len(df_src)
            
            if src_rows > 1:
                seq_nums_src = df_src['Seq_Num'].values.astype(int)
                diffs_src = np.diff(seq_nums_src) % 256
                lost_per_step_src = np.maximum(0, diffs_src - 1)
                lost_packets_src = int(np.sum(lost_per_step_src))
                
                # 1. Tính toán riêng từng nguồn để in ra kiểm tra nhanh
                total_expected_src = src_rows + lost_packets_src
                loss_rate_src = (lost_packets_src / total_expected_src * 100) if total_expected_src > 0 else 0.0
                print(f"   - Nguồn [{src}]: Nhận {src_rows} gói | Mất thật {lost_packets_src} gói -> Tỷ lệ mất: {loss_rate_src:.2f}%")
                
                # 2. Cộng dồn vào biến tổng để nuôi đoạn in báo cáo (Bảng thống kê thực nghiệm) ở cuối file
                lost_packets += lost_packets_src
                total_expected_packets += total_expected_src
                
        # Tính tỷ lệ mất gói tổng hợp cuối cùng
        if total_expected_packets > 0:
            loss_rate = (lost_packets / total_expected_packets) * 100
    # Hàm tính FFT
    def calculate_fft(signal, fs):
        signal_detrend = signal - np.mean(signal)
        N = len(signal_detrend)
        fft_values = np.fft.fft(signal_detrend)
        amplitudes = (2.0 / N) * np.abs(fft_values[:N // 2])
        amplitudes[0] = amplitudes[0] / 2.0
        frequencies = np.fft.fftfreq(N, 1/fs)[:N // 2]
        return frequencies, amplitudes

    freq_ecg_raw, amp_ecg_raw = calculate_fft(ecg_raw, FS_ECG)
    freq_ecg_filt, amp_ecg_filt = calculate_fft(ecg_filtered, FS_ECG)
    freq_ppg_ir_raw, amp_ppg_ir_raw = calculate_fft(ppg_ir_raw, FS_PPG)
    freq_ppg_ir_filt, amp_ppg_ir_filt = calculate_fft(ppg_ir_filtered, FS_PPG)
    freq_ppg_red_raw, amp_ppg_red_raw = calculate_fft(ppg_red_raw, FS_PPG)
    freq_ppg_red_filt, amp_ppg_red_filt = calculate_fft(ppg_red_filtered, FS_PPG)

    # --- PLOT 1: SO SÁNH ECG ---
    print("[INFO] Đang hiển thị đồ thị ECG (Bandpass + Notch 50Hz)...")
    fig1, axs1 = plt.subplots(2, 2, figsize=(15, 9), num="So Sánh Tín Hiệu ECG (Có bộ lọc Notch 50Hz)")
    axs1[0, 0].plot(time_ecg, ecg_raw, color='#FF5722', linewidth=0.6)
    axs1[0, 0].set_title("ECG Thô (Raw) - Miền Thời Gian", fontsize=11, fontweight='bold')
    axs1[0, 0].set_ylabel("Biên độ gốc")
    axs1[0, 0].grid(True, alpha=0.4)
    axs1[0, 1].plot(time_ecg, ecg_filtered, color='#4CAF50', linewidth=0.6)
    axs1[0, 1].set_title("ECG Đã Lọc (Bandpass + Notch 50Hz) - Miền Thời Gian", fontsize=11, fontweight='bold')
    axs1[0, 1].grid(True, alpha=0.4)
    axs1[1, 0].plot(freq_ecg_raw, amp_ecg_raw, color='#FF9800', linewidth=0.8)
    axs1[1, 0].set_xlim(0, 100)
    axs1[1, 0].grid(True, alpha=0.4)
    axs1[1, 1].plot(freq_ecg_filt, amp_ecg_filt, color='#2E7D32', linewidth=0.8)
    axs1[1, 1].set_xlim(0, 100)
    axs1[1, 1].grid(True, alpha=0.4)
    plt.tight_layout()
    plt.show()

    # --- PLOT 2: SO SÁNH PPG IR ---
    print("[INFO] Đóng cửa sổ ECG, đang hiển thị đồ thị PPG IR...")
    fig2, axs2 = plt.subplots(2, 2, figsize=(15, 9), num="So Sánh Tín Hiệu PPG IR")
    axs2[0, 0].plot(time_ppg, ppg_ir_raw, color='#9C27B0', linewidth=1)
    axs2[0, 0].grid(True, alpha=0.4)
    axs2[0, 1].plot(time_ppg, ppg_ir_filtered, color='#00BCD4', linewidth=1.2)
    axs2[0, 1].grid(True, alpha=0.4)
    axs2[1, 0].plot(freq_ppg_ir_raw, amp_ppg_ir_raw, color='#E040FB', linewidth=1)
    axs2[1, 0].set_xlim(0, 15)
    axs2[1, 0].grid(True, alpha=0.4)
    axs2[1, 1].plot(freq_ppg_ir_filt, amp_ppg_ir_filt, color='#006064', linewidth=1.2)
    axs2[1, 1].set_xlim(0, 15)
    axs2[1, 1].grid(True, alpha=0.4)
    plt.tight_layout()
    plt.show()

    # --- PLOT 3: SO SÁNH PPG RED ---
    print("[INFO] Đóng cửa sổ PPG IR, đang hiển thị đồ thị PPG RED...")
    fig3, axs3 = plt.subplots(2, 2, figsize=(15, 9), num="So Sánh Tín Hiệu PPG RED")
    axs3[0, 0].plot(time_ppg, ppg_red_raw, color='#EF5350', linewidth=1)
    axs3[0, 0].grid(True, alpha=0.4)
    axs3[0, 1].plot(time_ppg, ppg_red_filtered, color='#D32F2F', linewidth=1.2)
    axs3[0, 1].grid(True, alpha=0.4)
    axs3[1, 0].plot(freq_ppg_red_raw, amp_ppg_red_raw, color='#FF7043', linewidth=1)
    axs3[1, 0].set_xlim(0, 15)
    axs3[1, 0].grid(True, alpha=0.4)
    axs3[1, 1].plot(freq_ppg_red_filt, amp_ppg_red_filt, color='#B71C1C', linewidth=1.2)
    axs3[1, 1].set_xlim(0, 15)
    axs3[1, 1].grid(True, alpha=0.4)
    plt.tight_layout()
    plt.show()

    # ==================================================================
    # BỔ SUNG: MỤC PHÂN TÍCH VÀ TÌM ĐỈNH TRONG KHOẢNG GIÂY 35 ĐẾN 45 (CHO BÁO CÁO)
    # ==================================================================
    print("[INFO] Đang xử lý phân tích tìm đỉnh từ giây 35 đến 45...")
    
    # 1. Cắt dữ liệu ECG (Giây 35 - 45)
    ecg_mask = (time_ecg >= 35.0) & (time_ecg <= 45.0)
    t_ecg_window = time_ecg[ecg_mask]
    v_ecg_window = ecg_filtered[ecg_mask]
    
    # Thuật toán tìm đỉnh R trên ECG (Bám theo logic toán từ real_time_ver3)
    ecg_min, ecg_max, ecg_mean = np.min(v_ecg_window), np.max(v_ecg_window), np.mean(v_ecg_window)
    ecg_threshold = ecg_mean + 0.5 * (ecg_max - ecg_min)
    min_ecg_dist_samples = int(FS_ECG * 0.35) # Khoảng cách tối thiểu giữa 2 đỉnh R (~0.35s)
    
    ecg_peaks_idx, _ = find_peaks(v_ecg_window, height=ecg_threshold, distance=min_ecg_dist_samples)
    bpm_window_ecg = (60.0 * FS_ECG) / np.mean(np.diff(ecg_peaks_idx)) if len(ecg_peaks_idx) >= 2 else 0.0

    # 2. Cắt dữ liệu PPG (Giây 35 - 45)
    ppg_mask = (time_ppg >= 35.0) & (time_ppg <= 45.0)
    t_ppg_window = time_ppg[ppg_mask]
    v_ir_window = ppg_ir_filtered[ppg_mask]
    v_red_window = ppg_red_filtered[ppg_mask]
    
    min_ppg_dist_samples = int(FS_PPG * 0.4) # Khoảng cách tối thiểu giữa các đỉnh mạch mạch (~0.4s)
    
    # Tìm đỉnh mạch PPG IR
    ir_min, ir_max, ir_mean = np.min(v_ir_window), np.max(v_ir_window), np.mean(v_ir_window)
    ir_threshold = ir_mean + 0.2 * (ir_max - ir_min)
    ir_peaks_idx, _ = find_peaks(v_ir_window, height=ir_threshold, distance=min_ppg_dist_samples)
    bpm_window_ppg_ir = (60.0 * FS_PPG) / np.mean(np.diff(ir_peaks_idx)) if len(ir_peaks_idx) >= 2 else 0.0

    # Tìm đỉnh mạch PPG RED
    red_min, red_max, red_mean = np.min(v_red_window), np.max(v_red_window), np.mean(v_red_window)
    red_threshold = red_mean + 0.2 * (red_max - red_min)
    red_peaks_idx, _ = find_peaks(v_red_window, height=red_threshold, distance=min_ppg_dist_samples)
    bpm_window_ppg_red = (60.0 * FS_PPG) / np.mean(np.diff(red_peaks_idx)) if len(red_peaks_idx) >= 2 else 0.0

    # --- VẼ ĐỒ THỊ TĨNH PHÂN TÍCH ĐỈNH PHỤC VỤ BÁO CÁO ---
    fig4, axs4 = plt.subplots(3, 1, figsize=(14, 10), num="Báo Cáo Phân Tích Tìm Đỉnh Tín Hiệu (Giây 35 - 45)")
    
    # Subplot 1: ECG Peaks
    axs4[0].plot(t_ecg_window, v_ecg_window, color='#2E7D32', label='ECG Filtered', linewidth=1.2)
    axs4[0].scatter(t_ecg_window[ecg_peaks_idx], v_ecg_window[ecg_peaks_idx], color='red', marker='v', s=45, label=f'R-Peaks (HR: {bpm_window_ecg:.1f} BPM)')
    axs4[0].axhline(y=ecg_threshold, color='orange', linestyle='--', alpha=0.7, label='Dynamic Threshold (50%)')
    axs4[0].set_title("Phân Tích Đỉnh R-Wave Trên Tín Hiệu Khuếch Đại ECG (Window: 35s - 45s)", fontsize=11, fontweight='bold')
    axs4[0].set_ylabel("Biên độ")
    axs4[0].legend(loc="upper right")
    axs4[0].grid(True, alpha=0.3)

    # Subplot 2: PPG IR Peaks
    axs4[1].plot(t_ppg_window, v_ir_window, color='#006064', label='PPG IR Filtered', linewidth=1.5)
    axs4[1].scatter(t_ppg_window[ir_peaks_idx], v_ir_window[ir_peaks_idx], color='red', marker='o', s=40, label=f'Systolic Peaks (PR: {bpm_window_ppg_ir:.1f} BPM)')
    axs4[1].axhline(y=ir_threshold, color='magenta', linestyle='--', alpha=0.7, label='Dynamic Threshold (30%)')
    axs4[1].set_title("Phân Tích Đỉnh Tâm Thu Trên Tín Hiệu Quang Học PPG IR (Window: 35s - 45s)", fontsize=11, fontweight='bold')
    axs4[1].set_ylabel("Biên độ")
    axs4[1].legend(loc="upper right")
    axs4[1].grid(True, alpha=0.3)

    # Subplot 3: PPG RED Peaks
    axs4[2].plot(t_ppg_window, v_red_window, color='#B71C1C', label='PPG RED Filtered', linewidth=1.5)
    axs4[2].scatter(t_ppg_window[red_peaks_idx], v_red_window[red_peaks_idx], color='blue', marker='X', s=40, label=f'Systolic Peaks (PR: {bpm_window_ppg_red:.1f} BPM)')
    axs4[2].axhline(y=red_threshold, color='purple', linestyle='--', alpha=0.7, label='Dynamic Threshold (30%)')
    axs4[2].set_title("Phân Tích Đỉnh Tâm Thu Trên Tín Hiệu Quang Học PPG RED (Window: 35s - 45s)", fontsize=11, fontweight='bold')
    axs4[2].set_xlabel("Thời gian (Giây)")
    axs4[2].set_ylabel("Biên độ")
    axs4[2].legend(loc="upper right")
    axs4[2].grid(True, alpha=0.3)

    plt.tight_layout()
    plt.show()

    # --- IN KẾT QUẢ THỐNG KÊ THỰC NGHIỆM ---
    print("================== KẾT QUẢ THỐNG KÊ THỰC NGHIỆM ==================")
    print(f" * Tổng số dòng dữ liệu thu được : {total_rows} dòng")
    print(f" * Tổng số mẫu ECG phân tích     : {total_ecg_samples} mẫu")
    print(f" * Tổng số mẫu PPG phân tích     : {total_ppg_samples} mẫu")
    print(f" * Tổng thời gian đo đạc thực tế : {hours} giờ {minutes} phút {seconds:.2f} giây")
    print("------------------------------------------------------------------")
    print(f" * Tần số lấy mẫu trung bình thực tế (Sampling Rate):")
    print(f"   - Kênh ECG : {avg_fs_ecg:.2f} Hz (Cấu hình gốc: {FS_ECG} Hz)")
    print(f"   - Kênh PPG : {avg_fs_ppg:.2f} Hz (Cấu hình gốc: {FS_PPG} Hz)")
    print("------------------------------------------------------------------")
    print(f" * Phân tích trích đoạn báo cáo khoa học (Giây 35 - 45):")
    print(f"   - Số lượng đỉnh R phát hiện trên ECG : {len(ecg_peaks_idx)} đỉnh -> Nhịp tim vùng: {bpm_window_ecg:.1f} BPM")
    print(f"   - Số lượng đỉnh phát hiện trên PPG IR: {len(ir_peaks_idx)} đỉnh -> Nhịp mạch vùng: {bpm_window_ppg_ir:.1f} BPM")
    print(f"   - Số lượng đỉnh phát hiện trên PPG RED: {len(red_peaks_idx)} đỉnh -> Nhịp mạch vùng: {bpm_window_ppg_red:.1f} BPM")
    print("------------------------------------------------------------------")
    print(f" * Thống kê bộ lọc tần số lấy mẫu phần cứng (FS_Status):")
    print(f"   - Số gói đang đồng bộ (SYNCING) : {sync_count}")
    print(f"   - Số gói ĐẠT tiêu chuẩn (PASS) : {pass_count}")
    print(f"   - Số gói LỖI tần số    (FAIL) : {fail_count}")
    print(f"   => TỶ LỆ ĐẠT ỔN ĐỊNH TẦN SỐ    : {pass_rate:.2f}% PASS")
    print("------------------------------------------------------------------")
    print(f" * Thống kê truyền nhận dữ liệu qua Wifi/Serial:")
    print(f"   - Số gói tin bị mất (Lost)     : {lost_packets} gói")
    print(f"   => TỶ LỆ MẤT GÓI (Loss Rate)   : {loss_rate:.2f}%")
    print("==================================================================\n")

    # ==================================================================
    # BỔ SUNG: PLOT 5 - SO SÁNH CHỈ SỐ BPM & SPO2 TRƯỚC VÀ SAU KHI LỌC (GIÂY 35 - 45)
    # ==================================================================
    print("[INFO] Đang xử lý Plot 5: So sánh chỉ số BPM và SpO2 trước và sau khi lọc...")

    # Trích xuất phân đoạn dữ liệu Thô (Raw) trong khoảng 35s - 45s
    v_ir_raw_win = ppg_ir_raw[ppg_mask]
    v_red_raw_win = ppg_red_raw[ppg_mask]

    # --- 1. TÌM ĐỈNH VÀ TÍNH BPM TRÊN TÍN HIỆU THÔ (RAW) ---
    ir_raw_min, ir_raw_max, ir_raw_mean = np.min(v_ir_raw_win), np.max(v_ir_raw_win), np.mean(v_ir_raw_win)
    ir_raw_threshold = ir_raw_mean + 0.2 * (ir_raw_max - ir_raw_min)  # Ngưỡng cập nhật 20%
    ir_raw_peaks_idx, _ = find_peaks(v_ir_raw_win, height=ir_raw_threshold, distance=min_ppg_dist_samples)
    bpm_raw_ppg_ir = (60.0 * FS_PPG) / np.mean(np.diff(ir_raw_peaks_idx)) if len(ir_raw_peaks_idx) >= 2 else 0.0

    red_raw_min, red_raw_max, red_raw_mean = np.min(v_red_raw_win), np.max(v_red_raw_win), np.mean(v_red_raw_win)
    red_raw_threshold = red_raw_mean + 0.2 * (red_raw_max - red_raw_min)  # Ngưỡng cập nhật 20%
    red_raw_peaks_idx, _ = find_peaks(v_red_raw_win, height=red_raw_threshold, distance=min_ppg_dist_samples)
    bpm_raw_ppg_red = (60.0 * FS_PPG) / np.mean(np.diff(red_raw_peaks_idx)) if len(red_raw_peaks_idx) >= 2 else 0.0

    # --- 2. TÍNH SPO2 TRÊN TÍN HIỆU THÔ (RAW) ---
    ac_ir_raw = ir_raw_max - ir_raw_min
    dc_ir_raw = ir_raw_mean
    ac_red_raw = red_raw_max - red_raw_min
    dc_red_raw = red_raw_mean

    if dc_ir_raw > 0 and dc_red_raw > 0 and ac_ir_raw > 0:
        R_raw = (ac_red_raw / dc_red_raw) / (ac_ir_raw / dc_ir_raw)
        spo2_raw = 110 - 25 * R_raw  # Công thức thực nghiệm từ real_time_ver3
        spo2_raw = max(0.0, min(100.0, spo2_raw))
    else:
        spo2_raw = 0.0

    # --- 3. TÍNH SPO2 TRÊN TÍN HIỆU ĐÃ LỌC (FILTERED) ---
    # Lấy AC từ biên độ đỉnh-đỉnh sau lọc, kết hợp DC nền của tín hiệu thô gốc
    ac_ir_filt = ir_max - ir_min
    dc_ir_filt = ir_raw_mean
    ac_red_filt = red_max - red_min
    dc_red_filt = red_raw_mean

    if dc_ir_filt > 0 and dc_red_filt > 0 and ac_ir_filt > 0:
        R_filt = (ac_red_filt / dc_ir_filt) / (ac_ir_filt / dc_ir_filt)
        spo2_filt = 110 - 25 * R_filt
        spo2_filt = max(0.0, min(100.0, spo2_filt))
    else:
        spo2_filt = 0.0

    # --- 4. VẼ ĐỒ THỊ SO SÁNH TRỰC QUAN DASHBOARD ---
    fig5, axs5 = plt.subplots(2, 2, figsize=(15, 9), num="So Sánh Đánh Giá Chỉ Số Trước Và Sau Khi Lọc")

    # Kênh PPG IR Thô
    axs5[0, 0].plot(t_ppg_window, v_ir_raw_win, color='gray', alpha=0.7, label='PPG IR Raw')
    axs5[0, 0].scatter(t_ppg_window[ir_raw_peaks_idx], v_ir_raw_win[ir_raw_peaks_idx], color='orange', marker='o', s=35, label='Raw Peaks')
    axs5[0, 0].set_title(f"PPG IR THÔ (Chưa Lọc) - BPM: {bpm_raw_ppg_ir:.1f}", fontsize=11, fontweight='bold')
    axs5[0, 0].grid(True, alpha=0.2)
    axs5[0, 0].legend(loc="upper right")

    # Kênh PPG IR Đã Lọc
    axs5[0, 1].plot(t_ppg_window, v_ir_window, color='#006064', label='PPG IR Filtered')
    axs5[0, 1].scatter(t_ppg_window[ir_peaks_idx], v_ir_window[ir_peaks_idx], color='red', marker='v', s=45, label='Filtered Peaks')
    axs5[0, 1].set_title(f"PPG IR ĐÃ LỌC (Bandpass) - BPM: {bpm_window_ppg_ir:.1f}", fontsize=11, fontweight='bold')
    axs5[0, 1].grid(True, alpha=0.2)
    axs5[0, 1].legend(loc="upper right")

    # Kênh PPG RED Thô
    axs5[1, 0].plot(t_ppg_window, v_red_raw_win, color='gray', alpha=0.7, label='PPG RED Raw')
    axs5[1, 0].scatter(t_ppg_window[red_raw_peaks_idx], v_red_raw_win[red_raw_peaks_idx], color='orange', marker='o', s=35, label='Raw Peaks')
    axs5[1, 0].set_title(f"PPG RED THÔ (Chưa Lọc) - BPM: {bpm_raw_ppg_red:.1f}", fontsize=11, fontweight='bold')
    axs5[1, 0].grid(True, alpha=0.2)
    axs5[1, 0].legend(loc="upper right")

    # Kênh PPG RED Đã Lọc
    axs5[1, 1].plot(t_ppg_window, v_red_window, color='#B71C1C', label='PPG RED Filtered')
    axs5[1, 1].scatter(t_ppg_window[red_peaks_idx], v_red_window[red_peaks_idx], color='blue', marker='X', s=45, label='Filtered Peaks')
    axs5[1, 1].set_title(f"PPG RED ĐÃ LỌC (Bandpass) - BPM: {bpm_window_ppg_red:.1f}", fontsize=11, fontweight='bold')
    axs5[1, 1].grid(True, alpha=0.2)
    axs5[1, 1].legend(loc="upper right")

    # Nhúng bảng ma trận so sánh định lượng trực tiếp lên chân đồ thị để chụp ảnh báo cáo
    summary_box = (
        f"======================= MA TRẬN ĐÁNH GIÁ CHẤT LƯỢNG BỘ LỌC TÍN HIỆU =======================\n"
        f" CHỈ SỐ SINH HIỆU          | TRƯỚC KHI LỌC (RAW SIGNAL)      | SAU KHI LỌC (FILTERED SIGNAL)\n"
        f"--------------------------------------------------------------------------------------------\n"
        f" Nhịp mạch PPG IR          | {bpm_raw_ppg_ir:.1f} BPM                       | {bpm_window_ppg_ir:.1f} BPM\n"
        f" Nhịp mạch PPG RED         | {bpm_raw_ppg_red:.1f} BPM                       | {bpm_window_ppg_red:.1f} BPM\n"
        f" Nồng độ Oxy máu SpO2      | {spo2_raw:.1f}%                          | {spo2_filt:.1f}%\n"
        f"============================================================================================"
    )
    fig5.text(0.5, 0.02, summary_box, ha='center', fontsize=10.5, fontweight='bold', 
              bbox=dict(facecolor='#FFFDE7', alpha=0.9, edgecolor='orange', boxstyle='round,pad=0.6'), family='monospace')

    plt.tight_layout(rect=[0, 0.15, 1, 1])
    plt.show()

if __name__ == "__main__":
    analyze_biometric_csv(FILE_PATH)