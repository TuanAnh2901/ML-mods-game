# Ghidra Method Extraction Workflow Design

## 1. Mục tiêu

Cải tiến `query_methods.py` và `gen_method_fallback.py` để:

- tìm method đáng tin cậy từ `method-pointer-map.json`;
- chọn đúng overload theo type, tên method, số tham số và gợi ý signature;
- lấy pseudocode C và disassembly của một danh sách method từ `GameAssembly`
  bằng Ghidra headless;
- không kích hoạt full analysis đồng bộ trong lệnh truy vấn thông thường;
- tạo report dễ đọc và dữ liệu JSON cho các công cụ khác;
- dùng cùng một file target cho truy vấn, trích xuất code và sinh C header;
- giữ tương thích với cách gọi CLI và danh sách `DESIRED` hiện tại;
- cung cấp hướng dẫn thủ công chi tiết khi targeted extraction không đủ tốt.

## 2. Giới hạn filesystem

Mọi thay đổi source, test, cache mặc định, report mặc định và tài liệu đều nằm
trong:

```text
D:\VSCode\EL_Native\tools
```

Không dò tìm, đọc, sửa, chạy test hoặc ghi dữ liệu vào các thư mục backup có tên
như `Before`, `Tools Before` hoặc biến thể tương tự. `GameAssembly` và metadata
JSON bên ngoài workspace chỉ là input read-only do người dùng truyền rõ ràng.

Các đường dẫn output/cache do người dùng truyền được chuẩn hóa thành đường dẫn
tuyệt đối và phải nằm bên trong workspace `D:\VSCode\EL_Native\tools`. Công cụ
từ chối output/cache ra ngoài workspace hoặc nằm trong thư mục có thành phần tên
biểu thị backup như `before` hay `tools before`; input read-only không chịu giới
hạn này.

## 3. Phương án kiến trúc

`query_methods.py` trở thành CLI kiêm thư viện dùng chung cho:

- model và validation của metadata/target;
- parsing target CLI và `targets.json`;
- tìm kiếm, phân giải overload và phát hiện shared RVA;
- quản lý Ghidra project/cache/job;
- sinh report.

`gen_method_fallback.py` import các API ổn định từ `query_methods.py` để phân
giải target, sau đó chỉ đảm nhiệm chuyển kết quả thành C header. Cách này tập
trung thay đổi vào hai file được yêu cầu và tránh nhân đôi thuật toán chọn
method.

Không thêm module production thứ ba. Ghidra post-script được sinh vào cache lúc
chạy từ template nằm trong `query_methods.py`. Test và tài liệu được thêm thành
file riêng.

## 4. Mô hình dữ liệu

### 4.1 Metadata entry chuẩn hóa

Mỗi entry sau khi đọc JSON có các trường:

- `type: str`
- `method: str`
- `signature: str`
- `rva: int | None`
- `rva_text: str`
- `source_index: int`
- `raw: dict`

Entry thiếu `type` hoặc `method` bị bỏ qua có cảnh báo. RVA thiếu, bằng không
hoặc không parse được vẫn cho phép tìm metadata nhưng không được gửi sang
decompiler.

Các dạng RVA hợp lệ gồm số nguyên, chuỗi hex có `0x`, và chuỗi số thập phân.

### 4.2 Target

Target tối thiểu:

```json
{
  "type": "Namespace.Type",
  "method": "Method"
}
```

Target đầy đủ:

```json
{
  "type": "Namespace.Type",
  "method": "Method",
  "argc": 1,
  "signature_contains": "System.Single",
  "assembly": "Assembly-CSharp",
  "namespace_override": null
}
```

File target:

```json
{
  "targets": [
    {
      "type": "Namespace.Type",
      "method": "Method",
      "argc": 1
    }
  ]
}
```

CLI `--target` nhận:

- `Namespace.Type::Method`
- `Namespace.Type::Method(System.Int32,System.String)`

Signature inline được dùng làm gợi ý overload và suy ra `argc`.

## 5. Phân giải method

Thứ tự lọc:

1. exact case-insensitive match cho `type` và `method`;
2. nếu không có exact type, cho phép substring type để tương thích hành vi cũ;
3. lọc `signature_contains` nếu được cung cấp;
4. lọc theo `argc` nếu được cung cấp;
5. giữ toàn bộ candidate còn lại để đánh giá.

Kết quả có một trong các trạng thái:

- `resolved`: đúng một candidate;
- `not_found`: không có candidate;
- `ambiguous`: còn nhiều candidate khác signature hoặc RVA;
- `non_decompilable`: tìm thấy metadata nhưng RVA thiếu hoặc bằng không.

Công cụ không tự chọn candidate đầu tiên khi kết quả còn mơ hồ. Với
`ambiguous`, terminal và report liệt kê signature/RVA của mọi candidate liên
quan.

`Counter` theo RVA được dùng để gắn cảnh báo `shared_rva_count`. Shared RVA
không tự động bị loại vì có thể là thunk hợp lệ, nhưng report đánh dấu rõ để
người dùng xác minh.

## 6. Các mức thực thi Ghidra

### 6.1 Metadata mode

Không khởi động Ghidra. Chỉ đọc JSON, phân giải target và in/xuất metadata. Đây
là đường chạy mặc định của truy vấn tìm kiếm.

### 6.2 Targeted extraction

Đây là mode mặc định khi người dùng yêu cầu lấy code:

1. tính fingerprint của `GameAssembly`;
2. reuse project cache tương ứng nếu có;
3. nếu chưa có, import bằng `analyzeHeadless` với `-noanalysis`;
4. truyền request JSON chứa các RVA đã phân giải cho post-script;
5. tại mỗi `imageBase + RVA`, thử tìm function hiện có;
6. nếu chưa có, disassemble tại target và tạo function;
7. decompile riêng function mục tiêu với timeout;
8. xuất pseudocode và listing/disassembly;
9. ghi response JSON để Python tổng hợp report.

Targeted mode không tự chuyển sang full analysis khi thất bại. Nó giữ metadata,
log lỗi và địa chỉ cần mở thủ công.

Timeout được áp dụng theo subprocess và theo method. Hết timeout chỉ làm target
tương ứng thất bại; kết quả đã hoàn tất vẫn được giữ.

### 6.3 Full analysis background job

Full analysis chỉ chạy qua lệnh rõ ràng:

```powershell
python query_methods.py prepare-full ...
```

Lệnh tạo job nền, ghi PID/manifest/log rồi trả terminal ngay. Không giữ pipe
stdout/stderr trong bộ nhớ; cả hai được redirect vào log file để tránh deadlock
do buffer đầy.

Các lệnh quản lý:

```powershell
python query_methods.py status
python query_methods.py status --follow
python query_methods.py cancel
```

`status --follow` chỉ theo dõi file log/trạng thái. `Ctrl+C` dừng theo dõi nhưng
không kết thúc Ghidra. `cancel` mới kết thúc process của job được quản lý.

Job state:

- `queued`
- `running`
- `ready`
- `failed`
- `cancelled`
- `stale`

Manifest lưu PID, command, fingerprint, thời điểm bắt đầu/kết thúc, exit code,
log path và thời điểm log cập nhật gần nhất. Lock file ngăn hai full analysis
chạy đồng thời trên cùng project. Không có retry vô hạn.

## 7. Cache và an toàn dữ liệu

Cache mặc định:

```text
tools/.cache/ghidra-method-tools/
├── projects/<gameassembly-fingerprint>/
├── jobs/
├── requests/
├── responses/
└── scripts/
```

Fingerprint dùng SHA-256 của binary để đảm bảo binary thay đổi thì project được
tách riêng. Manifest cache lưu thêm kích thước, mtime và phiên bản Ghidra để hỗ
trợ chẩn đoán.

Không tự xóa project cũ. `--force-rebuild` chỉ tác động project cache hiện tại
sau khi đường dẫn được xác minh nằm trong cache an toàn của workspace.

## 8. CLI

### 8.1 `query_methods.py`

Cách dùng cũ tiếp tục hợp lệ:

```powershell
python query_methods.py keyword --type Type --method Method --sig
```

Các subcommand mới:

```powershell
python query_methods.py search [filters]
python query_methods.py extract --target TARGET [--target TARGET ...]
python query_methods.py extract --targets targets.json
python query_methods.py prepare-full
python query_methods.py status [--follow]
python query_methods.py cancel
```

Các option chung quan trọng:

- `--json PATH`
- `--game-assembly PATH`
- `--ghidra-home PATH`
- `--cache-dir PATH`
- `--report-dir PATH`
- `--extract metadata|targeted|full`
- `--timeout SECONDS`
- `--decompile-timeout SECONDS`
- `--force-rebuild`

`extract full` chỉ dùng project full-analysis đã ở trạng thái `ready`; nếu chưa
sẵn sàng, CLI hướng dẫn chạy `prepare-full`, không tự khởi chạy tác vụ dài.

### 8.2 `gen_method_fallback.py`

Các option mới:

- `--target TARGET` lặp lại nhiều lần;
- `--targets PATH`;
- `--extract-code none|targeted|full`;
- các option Ghidra/cache/report dùng chung.

Nếu không có `--target` hoặc `--targets`, script dùng `DESIRED` hiện tại để giữ
tương thích. Header chỉ chứa target ở trạng thái `resolved`. Mọi target thiếu,
mơ hồ hoặc không có RVA được tổng hợp rõ ở cuối và ảnh hưởng exit code.

## 9. Report

Report mặc định:

```text
tools/method-report/
├── summary.md
├── manifest.json
└── methods/
    └── Namespace.Type__Method__RVA/
        ├── metadata.json
        ├── decompiled.c
        └── disassembly.txt
```

`summary.md` gồm:

- input và fingerprint;
- phiên bản Ghidra/mode;
- bảng trạng thái tất cả target;
- signature/RVA/shared RVA;
- link tới pseudocode/disassembly;
- lỗi, timeout và bước xử lý thủ công tương ứng.

Tên thư mục được slug hóa và thêm RVA để tránh collision giữa overload. Mỗi file
được ghi vào file tạm trong cùng thư mục rồi thay thế atomically.

## 10. Exit code và lỗi

- `0`: mọi target yêu cầu đã hoàn tất theo mode;
- `2`: lỗi CLI hoặc cấu hình;
- `3`: JSON/schema/input path lỗi;
- `4`: có target `not_found`;
- `5`: có target `ambiguous`;
- `6`: Ghidra/import/decompile lỗi;
- `7`: timeout;
- `8`: vi phạm giới hạn output/cache hoặc job conflict.

Nếu có nhiều lỗi, ưu tiên lỗi cấu hình/an toàn trước, sau đó Ghidra/timeout, rồi
trạng thái target. Dù exit code khác không, report của các target đã xử lý vẫn
được giữ.

## 11. Hướng dẫn thủ công

Thêm `GHIDRA_METHOD_EXTRACTION_GUIDE.md` với quy trình chi tiết:

1. xác định `GHIDRA_HOME`;
2. import `GameAssembly` và chọn language/compiler spec;
3. xác minh/set image base;
4. chuyển RVA thành `imageBase + RVA`;
5. đi tới địa chỉ bằng phím `G`;
6. disassemble vùng mục tiêu;
7. tạo function;
8. chạy analyzer trên selection/function thay vì toàn binary;
9. mở Decompiler;
10. chỉnh prototype, calling convention và parameter type;
11. nhận diện thunk, stub và shared RVA;
12. phân biệt overload bằng metadata signature;
13. xuất pseudocode và disassembly;
14. lưu project để CLI reuse;
15. xử lý project lock, analyzer lâu, invalid instruction và function boundary
    sai;
16. đọc log/job manifest của công cụ.

Hướng dẫn cung cấp cả quy trình targeted nhanh và full GUI analysis khi người
dùng chủ động chấp nhận thời gian chạy dài.

## 12. Kiểm thử

Triển khai theo TDD. Test được viết và quan sát thất bại trước mỗi thay đổi
production tương ứng.

Unit tests:

- parse target CLI và file JSON;
- validation schema;
- parse RVA;
- đếm tham số với generic/nested type;
- exact/substring resolution;
- chọn overload theo `argc`/signature;
- `not_found`, `ambiguous`, RVA bằng không;
- shared RVA;
- slug và report rendering;
- C header rendering;
- kiểm tra đường dẫn output/cache.

Integration tests dùng fake `analyzeHeadless`:

- targeted extraction thành công;
- subprocess ghi log lớn mà không deadlock;
- timeout;
- background start/status/follow/cancel;
- stale PID và job conflict;
- reuse cache;
- response thiếu hoặc hỏng.

CLI smoke tests chạy hoàn toàn trên fixture và thư mục tạm nằm bên trong
workspace. Test không mở `GameAssembly` thật, không chạy Ghidra thật và không
truy cập bất kỳ folder backup nào.

## 13. Tiêu chí hoàn tất

- Cách gọi cũ của hai script vẫn hoạt động.
- CLI và `targets.json` dùng chung resolver.
- Target mơ hồ không bị chọn âm thầm.
- Metadata mode không khởi động Ghidra.
- Targeted mode không chạy full analysis.
- Full analysis trả terminal ngay và quản lý được qua status/cancel.
- Report chứa metadata, pseudocode và disassembly khi extraction thành công.
- Cache được tách theo fingerprint.
- Tài liệu thủ công đủ để lấy code khi headless targeted thất bại.
- Toàn bộ test tự động vượt qua.
- Mọi thay đổi nằm trong workspace `D:\VSCode\EL_Native\tools`; không tác động
  folder backup.
