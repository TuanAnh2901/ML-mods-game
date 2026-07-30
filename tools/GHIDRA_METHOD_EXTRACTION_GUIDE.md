# Hướng dẫn trích xuất phương thức bằng Ghidra

Tài liệu này hướng dẫn toán tử tra cứu metadata Cpp2IL, trích xuất có mục tiêu và phục hồi thủ công một phương thức theo RVA. Mọi lệnh dưới đây chạy từ thư mục `tools` trong PowerShell.

## Chuẩn bị

Đặt các biến đường dẫn trước khi chạy. `TARGET` dùng cú pháp `Namespace.Type::Method`; khi có overload, thêm danh sách tham số để công cụ suy ra arity.

```powershell
$MAP = 'D:\analysis\method-pointer-map.json'
$TARGET = 'Game.Unit::Apply(System.Int32,System.String)'
$GHIDRA_HOME = 'C:\ghidra_11.1_PUBLIC'
$GAME_ASSEMBLY = 'D:\game\GameAssembly.dll'
$WORKSPACE = (Get-Location).Path
$CACHE = Join-Path $WORKSPACE '.cache'
```

Kiểm tra các tùy chọn hiện có:

```powershell
python .\query_methods.py --help
python .\gen_method_fallback.py --help
```

## metadata-only

Tra cứu trước để xác nhận tên kiểu, tên phương thức, chữ ký và RVA; thao tác này không khởi động Ghidra.

```powershell
python .\query_methods.py search Apply --json $MAP --sig
python .\query_methods.py extract --target $TARGET --json $MAP --workspace $WORKSPACE --report-dir reports\metadata-only
Get-Content .\reports\metadata-only\summary.md
```

Đọc `metadata.json` của phương thức đã resolved. Nếu có nhiều kết quả, dùng chữ ký đầy đủ trong `TARGET` hoặc chuẩn bị tệp JSON `targets` có `argc` và `signature_contains` để chọn đúng overload.

## Targeted extraction nhanh (-noanalysis)

Với phương thức đã chọn, tạo header và yêu cầu targeted extraction. Chế độ này gọi Ghidra headless với `-noanalysis`, vì vậy phù hợp để lấy nhanh pseudocode và một dòng disassembly tại địa chỉ target mà không chờ full analysis.

```powershell
python .\gen_method_fallback.py `
  --json $MAP `
  --target $TARGET `
  --extract-code targeted `
  --ghidra-home $GHIDRA_HOME `
  --game-assembly $GAME_ASSEMBLY `
  --workspace $WORKSPACE `
  --cache-dir $CACHE `
  --report-dir reports\targeted
```

Sau khi lệnh hoàn tất, xem `reports\targeted\manifest.json`, `summary.md`, và thư mục `methods\<type>__<method>__<rva>`. Thành công tạo `code.c` và `disassembly.asm`; metadata vẫn được ghi khi timeout hoặc lỗi.

## Mở thủ công target RVA

Khi targeted extraction chưa nhận dạng đúng hàm, mở `GameAssembly.dll` trong Ghidra để phục hồi tại chỗ:

1. Mở project, import `GameAssembly.dll`, chọn ngôn ngữ/processor đúng với binary, rồi mở chương trình trong CodeBrowser.
2. Lấy RVA dạng hexadecimal từ report, ví dụ `0x123456`. Trong Ghidra, `image base` là địa chỉ cơ sở đang hiển thị ở Memory Map hoặc Program Properties.
3. Tính địa chỉ cần đến: **image base + RVA**. Ví dụ image base `0x180000000` cộng RVA `0x123456` cho địa chỉ `0x180123456`.
4. Nhấn `G`, nhập địa chỉ tuyệt đối vừa tính, rồi xác nhận để đi tới target RVA. Không nhập RVA trần khi image base khác zero.
5. Quan sát Listing, bytes và các tham chiếu quanh địa chỉ trước khi tạo hàm.

## Create Function và Decompiler

Tại địa chỉ đã đi tới, thực hiện lần lượt:

1. Nếu vùng đó chưa có instruction, chọn địa chỉ đầu, nhấp phải **Disassemble**; kiểm tra kiến trúc, boundary và flow được tạo ra.
2. Chọn instruction đầu hàm, nhấp phải **Create Function**. Đặt tên tạm theo type/method và xác nhận function body không ăn sang hàm kế tiếp.
3. Mở cửa sổ **Decompiler** qua `Window > Decompiler`; chọn function mới tạo để xem pseudocode.
4. Sửa function signature trong Function Editor: return type, calling convention, các tham số và tên tham số theo metadata. Với instance method, kiểm tra tham số `this`/register đầu tiên theo ABI.
5. Dùng `File > Export Program` hoặc copy từ Decompiler để xuất pseudocode; dùng Listing, `File > Export` hoặc copy selection để xuất disassembly. Lưu hai bản cạnh metadata và ghi rõ image base, RVA và địa chỉ tuyệt đối.

Nếu Decompiler hiển thị sai, quay lại Listing để sửa instruction boundary, function body hoặc data/code definition, sau đó refresh Decompiler.

## overload/thunk/stub/shared RVA

- **overload:** cùng type/method nhưng khác arity hoặc signature. Chỉ chọn một overload khi `argc` hay `signature_contains` làm kết quả resolved duy nhất.
- **thunk:** target có thể chỉ là thunk chuyển hướng. Theo `Thunked Function`, jump/call đích, rồi ghi cả RVA thunk lẫn RVA implementation.
- **stub:** body rất ngắn có thể là stub/throw hoặc wrapper. Kiểm tra caller và xref trước khi coi đó là implementation.
- **shared RVA:** report có `shared_rva_count` lớn hơn 1 hoặc search hiện `[SHARED xN]` nghĩa nhiều metadata entry cùng địa chỉ. Giữ các tên logical riêng, nhưng trích xuất một body theo shared RVA và ghi mapping rõ ràng.

## Full analysis nền

Full analysis là thao tác tường minh, không phải một phần của targeted mode, và có thể mất nhiều thời gian. Chỉ khởi chạy khi cần cross-reference, function recovery hoặc decompilation toàn diện:

```powershell
$manifest = python .\query_methods.py prepare-full `
  --ghidra-home $GHIDRA_HOME `
  --game-assembly $GAME_ASSEMBLY `
  --workspace $WORKSPACE `
  --cache-dir $CACHE
$job = Get-Content $manifest | ConvertFrom-Json
$FINGERPRINT = $job.fingerprint
```

Lệnh trả về ngay một manifest; Ghidra tiếp tục chạy nền và ghi log trong cache.

## status --follow và cancel

`status --follow` là quy ước theo dõi trong PowerShell: CLI hiện nhận một lần `status`, còn vòng lặp dưới đây thực hiện follow an toàn.

```powershell
while ($true) {
  Clear-Host
  python .\query_methods.py status $FINGERPRINT --cache-dir $CACHE
  Start-Sleep -Seconds 5
}
```

Dừng vòng lặp bằng `Ctrl+C`. Nếu cần dừng job Ghidra, chạy:

```powershell
python .\query_methods.py cancel $FINGERPRINT --cache-dir $CACHE
python .\query_methods.py status $FINGERPRINT --cache-dir $CACHE
```

## cache

Cache mặc định là `.cache`; công cụ tạo project, script targeted, request/response và job manifest dưới `.cache\ghidra-method-tools`. Giữ cache khi cần kiểm tra `analysis.log`, fingerprint hoặc kết quả cũ. Xóa cache chỉ khi không còn job chạy và cần import/analysis sạch; dùng một cache riêng cho mỗi workspace hoặc binary để tránh nhầm fingerprint.

## Chẩn đoán lỗi

| Triệu chứng | Kiểm tra và xử lý |
| --- | --- |
| `ambiguous` | Bổ sung chữ ký trong `TARGET`, `argc` hoặc `signature_contains`; không chọn ngẫu nhiên overload. |
| `non_decompilable` hoặc thiếu RVA | Metadata không có RVA hợp lệ; dùng symbol/xref trong Ghidra hoặc cập nhật method map. |
| Targeted timeout | Tăng `--timeout-seconds`, kiểm tra response trong cache, rồi làm theo luồng mở thủ công target RVA. |
| Không có `code.c` | Kiểm tra `manifest.json` và `metadata.json` để xem `extraction_status` và `extraction_error`. |
| Ghidra không khởi động | Xác nhận `$GHIDRA_HOME\support\analyzeHeadless.bat` và `$GAME_ASSEMBLY` tồn tại, đồng thời chạy lại lệnh với đường dẫn tuyệt đối. |
| Decompiler sai | Kiểm tra image base + RVA, Disassemble, Create Function, function body và signature trước khi xuất lại. |
| Job vẫn `running` | Đọc `analysis.log`; dùng vòng `status --follow` ở trên, hoặc `cancel` nếu cần kết thúc job. |
