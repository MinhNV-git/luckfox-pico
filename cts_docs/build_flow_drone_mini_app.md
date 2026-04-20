# PHÂN TÍCH LUỒNG BUILD DRONE_MINI TRONG BUILD.SH

## 1. KHỞI TẠO BIẾN (Lines 1-100)
- **Line 56**: `export RK_PROJECT_PATH_APP=$SDK_ROOT_DIR/output/out/app_out`
  → Định nghĩa thư mục output chính cho app, tương đương: `/home/minhnv/workdir/luckfox-pico/output/out/app_out`
  
## 2. HÀM BUILD_APP() - BUILD TẤT CẢ APP (Lines 619-660)
```bash
function build_app() {
    # Line 646: Đây là lệnh BUILD DRONE_MINI
    test -d ${SDK_APP_DIR} && make -C ${SDK_APP_DIR}
    finish_build
}
```
- **Line 646**: `make -C ${SDK_APP_DIR}`
  → Gọi Makefile chính tại: `/home/minhnv/workdir/luckfox-pico/project/app/Makefile`
  → Makefile chính duyệt tất cả thư mục con có Makefile (bao gồm drone_mini/)
  → Mỗi app gọi: `make -C ./drone_mini/` (và các app khác)

## 3. DRONE_MINI MAKEFILE BUILD FLOW
```bash
# File: /home/minhnv/workdir/luckfox-pico/project/app/drone_mini/Makefile
SRC := $(wildcard *.c) $(wildcard imu/*.c)
CFLAGS := -I include

all:
    mkdir -p out/bin;
    arm-rockchip830-linux-uclibcgnueabihf-gcc $(CFLAGS) $(SRC) -o out/bin/drone_mini
    $(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))
```
- **Kết quả**: Binary được tạo tại: `/home/minhnv/workdir/luckfox-pico/project/app/drone_mini/out/bin/drone_mini`
- **Sau đó**: Copy vào: `/home/minhnv/workdir/luckfox-pico/project/app/out/bin/drone_mini` (RK_APP_OUTPUT)

## 4. HÀM MAIN BUILD_ALL() - LUỒNG BUILD CHÍNH (Lines 1235-1250)
```bash
function build_all() {
    build_sysdrv      # Line 1242
    build_media       # Line 1243
    build_app         # Line 1244 ← GỌI BUILD_APP
    build_firmware    # Line 1245
}
```

## 5. COPY APP VÀO OEM PARTITION - HÀNG __PACKAGE_RESOURCES (Lines 1365-1405)
```bash
function __PACKAGE_RESOURCES() {
    _install_dir=$_target_dir/usr
    
    # Copy APP components:
    __COPY_FILES $RK_PROJECT_PATH_APP/bin $_install_dir/bin/         # Line 1379
    __COPY_FILES $RK_PROJECT_PATH_APP/lib $_install_dir/lib/         # Line 1380
    __COPY_FILES $RK_PROJECT_PATH_APP/share $_install_dir/share/     # Line 1381
    __COPY_FILES $RK_PROJECT_PATH_APP/usr $_install_dir/             # Line 1382
    __COPY_FILES $RK_PROJECT_PATH_APP/etc $_install_dir/etc/         # Line 1383
}
```
- **Kết quả**: Binary drone_mini được copy từ:
  - Source: `/home/minhnv/workdir/luckfox-pico/output/out/app_out/bin/drone_mini`
  - Destination: `/home/minhnv/workdir/luckfox-pico/output/out/oem/usr/bin/drone_mini` (OEM Partition)

## 6. COPY APP ROOT VÀO ROOTFS - HÀM __PACKAGE_ROOTFS (Lines 1490-1510)
```bash
function __PACKAGE_ROOTFS() {
    __COPY_FILES $RK_PROJECT_PATH_APP/root $RK_PROJECT_PACKAGE_ROOTFS_DIR  # Line 1507
}
```
- Nếu có thư mục `drone_mini/root/`, nó sẽ được copy vào rootfs

## 7. HÀM __PACKAGE_OEM - TẠO OEM PARTITION (Lines 1442-1467)
```bash
function __PACKAGE_OEM() {
    mkdir -p $RK_PROJECT_PACKAGE_OEM_DIR
    __PACKAGE_RESOURCES $RK_PROJECT_PACKAGE_OEM_DIR  # Line 1443
}
```
- Gọi __PACKAGE_RESOURCES để copy tất cả app (bao gồm drone_mini) vào OEM

## 8. HÀM BUILD_FIRMWARE - TẠO FIRMWARE IMAGE (Lines 2524-2600)
```bash
function build_firmware() {
    __PACKAGE_ROOTFS    # Line 2546
    __PACKAGE_OEM       # Line 2547
    __BUILD_ENABLE_COREDUMP_SCRIPT
    build_mkimg $GLOBAL_ROOT_FILESYSTEM_NAME $RK_PROJECT_PACKAGE_ROOTFS_DIR  # Line 2569
    build_mkimg $GLOBAL_OEM_NAME ...            # Tạo OEM image chứa drone_mini
}
```

## TÓICÓM TẮT LUỒNG HOÀN CHỈNH:
```
build.sh (line 1244)
  ↓
build_app() (line 619)
  ↓
make -C project/app (line 646)
  ↓
Makefile: make -C drone_mini/
  ↓
drone_mini/Makefile: gcc compile → out/bin/drone_mini
  ↓
Copy → project/app/out/bin/drone_mini
  ↓
Copy → output/out/app_out/bin/drone_mini
  ↓
build_firmware() (line 2524)
  ↓
__PACKAGE_OEM() (line 2547)
  ↓
__PACKAGE_RESOURCES() (line 1443)
  ↓
Copy → output/out/oem/usr/bin/drone_mini
  ↓
build_mkimg oem (tạo OEM image)
  ↓
output/image/oem.img (chứa drone_mini)
```
