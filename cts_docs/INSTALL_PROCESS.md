# Luckfox Pico - Quy Trình Install Ứng Dụng

## 📋 Tóm Tắt Quy Trình

### 1. Build Ứng Dụng
**Vị trí:** `project/app/Makefile`
```makefile
app_src := $(wildcard ./*/Makefile ./component/fastboot_server/Makefile)
```

Các app được build bằng:
```bash
make -C <folder> || exit -1
```

### 2. Output của Build Phase
Các file compile được copy vào `project/app/out/` qua macro:
```bash
MAROC_COPY_PKG_TO_APP_OUTPUT($(RK_APP_OUTPUT), $(compiled_apps))
```

**Cấu trúc output:**
```
RK_PROJECT_PATH_APP = output/out/app_out/
├── bin/          # Executable files
├── lib/          # Libraries
├── share/        # Shared resources
├── usr/          # User files
├── etc/          # Config files
└── root/         # Files copy to rootfs root
```

### 3. 🔴 **INSTALL VÀO HỆ THỐNG** (Chính!)
**Vị trí:** `build.sh` - Hàm `__COPY_FILES()` tại dòng 1357

Khi chạy `./build.sh all`, quá trình sau diễn ra:

#### 3.1 Extract base rootfs
```bash
tar -xf $rootfs_tarball -C $RK_PROJECT_PACKAGE_ROOTFS_DIR
```

#### 3.2 Copy App Files vào Rootfs
```bash
__COPY_FILES $RK_PROJECT_PATH_APP/bin $_install_dir/bin/
__COPY_FILES $RK_PROJECT_PATH_APP/lib $_install_dir/lib/
__COPY_FILES $RK_PROJECT_PATH_APP/share $_install_dir/share/
__COPY_FILES $RK_PROJECT_PATH_APP/usr $_install_dir/
__COPY_FILES $RK_PROJECT_PATH_APP/etc $_install_dir/etc/
```

#### 3.3 Copy Media Library vào Rootfs
```bash
__COPY_FILES $RK_PROJECT_PATH_MEDIA/bin $_install_dir/bin/
__COPY_FILES $RK_PROJECT_PATH_MEDIA/lib $_install_dir/lib/
__COPY_FILES $RK_PROJECT_PATH_MEDIA/share $_install_dir/share/
__COPY_FILES $RK_PROJECT_PATH_MEDIA/usr $_install_dir/
```

#### 3.4 Copy Kernel Drivers
```bash
__COPY_FILES $RK_PROJECT_PATH_SYSDRV/kernel_drv_ko/ $_install_dir/ko
```

#### 3.5 Copy External Files
```bash
__COPY_FILES $SDK_ROOT_DIR/external $RK_PROJECT_PACKAGE_ROOTFS_DIR
__COPY_FILES $RK_PROJECT_PATH_APP/root $RK_PROJECT_PACKAGE_ROOTFS_DIR
__COPY_FILES $RK_PROJECT_PATH_MEDIA/root $RK_PROJECT_PACKAGE_ROOTFS_DIR
```

#### 3.6 Thêm Init Script
```bash
cp -f $RK_PROJECT_FILE_OEM_SCRIPT $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d
# Đây là: S21appinit (định nghĩa ở dòng 60 của build.sh)
```

### 4. Userdata Installation (Optional)
```bash
# Nếu có folder install_to_userdata
$RK_PROJECT_PATH_APP/install_to_userdata/* → $RK_PROJECT_PACKAGE_USERDATA_DIR
$RK_PROJECT_PATH_MEDIA/install_to_userdata/* → $RK_PROJECT_PACKAGE_USERDATA_DIR
```

### 5. OEM Partition Installation (Optional)
Một số app có `PKG_INSTALL_TO_ROOTFS = NO` (ví dụ: ipcweb)
- Thay vì copy vào rootfs, nó copy vào **OEM partition**
- OEM partition được init bởi `S21appinit` script tại boot time

```makefile
# ipcweb/Makefile dòng 64
ifeq ($(PKG_INSTALL_TO_ROOTFS),YES)
    @cp -rfa $(PKG_TARBALL)/etc $(PKG_TARPATH)/etc
else
    @cp -rfa $(PKG_TARBALL)/etc4oem $(PKG_TARPATH)/etc
endif
```

---

## 📊 Sơ Đồ Quy Trình

```
[App Source Code]
        ↓
   [Compile each app folder]  ← Makefile của từng app
        ↓
[Output bin/lib/etc → app_out/]  ← project/app/Makefile
        ↓
Build System (build.sh)
        ↓
[__COPY_FILES Function]
        ├─→ Copy app_out/bin → rootfs/usr/bin/
        ├─→ Copy app_out/lib → rootfs/usr/lib/
        ├─→ Copy app_out/etc → rootfs/etc/
        ├─→ Copy media_out/* → rootfs/usr/
        └─→ Copy kernel_drv_ko/ → rootfs/usr/ko/
        ↓
[Package into rootfs.tar / rootfs.bin / rootfs.img]
        ↓
[Flash vào thiết bị Luckfox Pico]
```

---

## 🔍 Các File Quan Trọng

| File | Vai Trò |
|------|--------|
| `build.sh` | Script build chính, thực hiện install |
| `project/app/Makefile` | Build tất cả app |
| `project/app/Makefile.param` | Cấu hình compile flags |
| `project/app/*/Makefile` | Build app cụ thể |
| `output/out/S21appinit` | Init script chạy lúc boot |
| `.BoardConfig.mk` | Cấu hình board, định nghĩa RK_CHIP, partition, v.v. |

---

## 📝 Kết Luận

✅ **CÓ install**: Qua hàm `__COPY_FILES()` trong `build.sh`

❌ **KHÔNG chạy chương trình**: Chỉ copy binaries vào rootfs. Chương trình được start bởi:
- Init scripts trong boot (`/etc/init.d/`)
- S21appinit script (OEM partition)
- User manual execution

