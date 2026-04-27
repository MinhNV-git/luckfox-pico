# IMU Serial Monitor Guide

File chinh:

- `python_tool/IMU_calib.py`

## 1. Chuc nang

App nay dung de:

- Gui ky tu `A` dinh ky xuong cong serial moi `10 ms`
- Doc frame nhi phan tu cong serial
- Hien thi gia tri thong so IMU theo thoi gian thuc
- Ve 3 bieu do realtime, moi bieu do hien toi da 3 thong so
- Cho phep chon thong so hien thi tren tung bieu do
- Uu tien toi da dien tich cho vung bieu do

## 2. Yeu cau moi truong

Can co:

- Python 3.8 tro len
- Ho tro tao `venv`

App hien tai su dung Qt platform thong qua `PySide6`.
Phan ve bieu do realtime su dung `pyqtgraph`.

## 3. Tao va kich hoat venv

Tu thu muc repo:

```bash
cd /home/minhnv/workdir/luckfox-pico
python3 -m venv python_tool/.venv
```

Kich hoat tren Linux/macOS:

```bash
source python_tool/.venv/bin/activate
pip install -r python_tool/environment.txt
```

Kich hoat tren Windows PowerShell:

```powershell
python_tool\.venv\Scripts\Activate.ps1
```

Khi kich hoat thanh cong, terminal thuong se hien tien to `(.venv)`.

## 4. Cai dat thu vien trong venv

Sau khi da activate `venv`:

```bash
pip install pyserial pyqtgraph PySide6
```

Kiem tra lai:

```bash
python -m py_compile python_tool/IMU_calib.py
```

## 5. Cach chay app trong venv

Vi du co ban:

```bash
python python_tool/IMU_calib.py --port /dev/ttyUSB0 --baudrate 115200
```

Neu cong serial la `ttyACM0`:

```bash
python python_tool/IMU_calib.py --port /dev/ttyACM0 --baudrate 115200
```

Neu khong truyen `--port`, app van mo len va co the chon cong trong giao dien.

Quy trinh su dung:

1. Bam `Connect` de mo cong serial
2. Chon `Period` trong khoang `1..1000 ms`
3. Bam `Start` de bat dau gui ky tu `A` dinh ky va nhan frame IMU
4. Bam `Stop` neu muon tam dung polling ma van giu ket noi
5. Bam `Disconnect` de dong cong serial

## 6. Cac tham so ho tro

- `--port`: ten cong serial, vi du `/dev/ttyUSB0`
- mac dinh tren giao dien la `/dev/ttyUSB0`
- `--baudrate`: toc do serial, mac dinh `115200`
- `--timeout`: timeout khi doc serial, mac dinh `0.02`
- `--header`: byte header cua frame, mac dinh `0xAA`
- `--period`: chu ky gui ky tu `A`, mac dinh `10 ms`, gioi han `1..1000 ms`
- `--window`: so mau giu lai tren bieu do, mac dinh `300`

Vi du:

```bash
python python_tool/IMU_calib.py \
  --port /dev/ttyUSB0 \
  --baudrate 115200 \
  --timeout 0.1 \
  --header 0xAA \
  --period 10 \
  --window 500
```

## 7. Giao thuc serial

App khong doc text nua. App gui dinh ky:

```text
'A'
```

Sau do app cho thiet bi tra ve frame dang:

```text
header(1) + size(1) + data(sizeof(S_IMU_DATA)) + crc16_modbus(2)
```

Trong do:

- `header`: mac dinh `0xAA`
- `size`: kich thuoc payload data
- `data`: struct `S_IMU_DATA`
- `crc`: 2 byte cuoi, little-endian, tinh CRC16-Modbus tren `header + size + data`

Struct du lieu:

```c
#pragma pack(push, 1)
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
} S_IMU_DATA;
#pragma pack(pop)
```

## 8. Cac truong mac dinh

App se hien thi 6 truong:

```text
accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z
```

Ca 9 o chon field duoc dat tren cung 1 dong de danh nhieu dien tich hon cho bieu do.
Moi bieu do co 3 o chon field va co the dat `None` de an mot line.
Gia tri 6 truong duoc thu gon thanh 1 hang nho, va phan con lai cua cua so danh cho 3 bieu do.

## 9. Loi thuong gap

### Khong mo duoc cong serial

Kiem tra:

- Dung ten cong chua, vi du `/dev/ttyUSB0`
- Thiet bi da cam chua
- Baudrate co dung khong

Tren Linux, neu gap loi quyen truy cap serial:

```bash
sudo usermod -a -G dialout $USER
```

Sau do dang xuat va dang nhap lai.

### Loi thieu `PySide6`

Can kiem tra da activate `venv`, sau do cai:

```bash
pip install PySide6
```

### Loi thieu `pyserial`, `pyqtgraph` hoac `PySide6`

Can kiem tra da activate `venv`, sau do cai lai:

```bash
pip install pyserial pyqtgraph PySide6
```

## 10. Ghi chu

- App giu log noi bo cho xu ly serial, giao dien uu tien hien thi bieu do
- Bieu do tu dong scale theo gia tri du lieu nhan duoc
- App chi chap nhan payload dung bang `sizeof(S_IMU_DATA) = 12`
- Neu CRC sai, frame se bi bo qua va log loi se duoc hien thi
