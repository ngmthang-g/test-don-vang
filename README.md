# Test Đơn Vàng — Auto Target ID (phát triển từ bản 10.6)

- Vẫn dùng trình quản lý nhiều ACC Windows, tự nhận client đang nạp `GameAssembly.dll`, Bridge theo từng PID, cấu hình riêng từng nhân vật, hai lệnh **BẮT ĐẦU/DỪNG ACC TICK** và tab **LOG**.
- Trong **AUTO TARGET ID**, chọn ACC rồi **QUÉT NGƯỜI XUNG QUANH**. Danh sách checkbox nhận `RoleID` và tên từ `ObjectManager.sprites` (chuyển từ Auto Buff v1.3.1, bỏ lọc Peace). Mỗi ACC tick **đúng một** RoleID; nhiều ACC có thể tick chung một RoleID. Chọn RoleID được lưu theo RoleID của chính ACC vào `%LOCALAPPDATA%\ThanLongCleanRoute\target_id_accounts.ini`, kể cả khi nhân vật được tick rời tầm quét.
- Click 1: người dùng tự đặt điểm click trong game bằng **F7** theo từng ACC. Bridge click tọa độ đã lưu, không tìm portrait hay tự suy ra tọa độ.
- Click 2: chọn ACC, đưa chuột vào vị trí mong muốn ở cửa sổ game rồi nhấn **F8** để lưu tọa độ cho ACC đó.
- Trình tự worker của mỗi ACC: `SelectTargetByRoleID → ClickInternalPoint(Click1) → ClickTravelSemantic(Trade) → ClickInternalPoint(Click2) → lặp`. Lỗi/timeout của từng bước không chặn vòng kế, và không gửi chồng yêu cầu Bridge cho cùng PID.
- Giữ nguyên license launcher/API gate từ Test Đơn Vàng gốc. Các workflow train, lọc vũ khí, dồn đồ, MAIN/CON, Telegram, developer và tab giới thiệu đã được bỏ khỏi GUI và build.

## Build

Yêu cầu Windows x64 + Visual Studio 2022 + CMake 3.24+.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Đặt `TestDonVang_TargetID.exe` và `ThanLongCleanRouteBridge.dll` **cạnh nhau**. GitHub Actions job `build-target-id.yml` xuất cả hai file.

## Thận trọng

Chưa có kiểm thử thực tế trên game Windows. Scan RVA dựa trên chữ ký phiên bản đã kiểm tra trong source Auto Buff 1.3.1; game khác bản sẽ từ chối scan thay vì đọc sai. Tọa độ F7/F8 được gán thủ công; cần kiểm tra trực tiếp để xác nhận click đúng UI. **Không coi một build pass là đã được nghiệm thu in-game.**

## Đưa source lên nhánh GitHub riêng để lấy EXE

Do môi trường tạo bản nguồn không thể kết nối trực tiếp GitHub để đẩy ZIP lên nhánh,
source đã được đóng gói riêng. Giải nén ZIP; trên máy có Git và quyền ghi repository,
mở PowerShell tại thư mục `TestDonVang_TargetID` rồi chạy:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\publish-to-branch.ps1
```

Script **chỉ** cập nhật `exe/target-id-loop`, không động vào `main`.
Sau đó mở [Actions](https://github.com/ngmthang-g/test-don-vang/actions) để lấy artifact
chứa `TestDonVang_TargetID.exe` và `ThanLongCleanRouteBridge.dll` nếu build pass.
Chưa chạy được Windows CI hoặc kiểm tra trên game thật trong môi trường phát triển này.

## Điều khiển Click 1 / Click 2, số chuỗi và delay riêng từng ACC

- Mở ACC, bấm **QUÉT NGƯỜI XUNG QUANH**, tick một RoleID. Nhiều ACC được tick cùng RoleID.
- **Click 1**: đặt con trỏ chuột vào tọa độ *mặt nhân vật* mong muốn trong cửa sổ game của ACC đang chọn, rồi nhấn **F7**. Không tự dò vị trí mặt.
- **Click 2**: đặt chuột tại điểm thứ hai trong game, nhấn **F8**. Cả hai điểm là tọa độ tương đối theo client window (0..9999), lưu riêng theo RoleID ACC.
- **Số chuỗi**: 0 = vô hạn; N = chạy đủ N chuỗi rồi tự dừng.
- Delay sau Target, Click 1, Callback Giao dịch, Click 2 và **delay giữa chuỗi** có thể đặt riêng, đơn vị ms (0..60000). Delay của mỗi bước áp dụng cả khi bước đó báo lỗi; sau Click 2 cộng thêm delay giữa chuỗi.
- Bấm **LƯU REPEAT / DELAY CHO ACC** để lưu. Các thay đổi không ảnh hưởng ACC khác. Lưu ở `target_id_accounts.ini` theo RoleID của ACC.
- Nếu chưa gán tọa độ click, bước click báo lỗi trong tab LOG rồi chuyển sang bước kế.


### Giao diện thu gọn và ON/OFF LOG

- Giữ nguyên chiều ngang **1060 px**, đổi chiều cao ban đầu từ **935 px** xuống **475 px**; thu gọn chiều cao nút, các bảng danh sách, tab và khoảng cách dọc. Giữ chữ ở mức 13px thay vì thu còn 8px để tránh khó đọc.
- Nút **LOG: ON / LOG: OFF** nằm trên hàng điều khiển chung, hiển thị ở cả tab AUTO TARGET ID và LOG. **OFF** chỉ ngừng thu thập log mới; các dòng cũ vẫn hiện, vẫn xóa/xuất được. **ON** tiếp tục thu thập log.
- Trạng thái LOG lưu trong `target_id_accounts.ini`, mục `[Interface] LogEnabled=1/0`. Tắt log không thay đổi trạng thái chạy ACC, RoleID, tọa độ F7/F8 hoặc repeat/delay.
