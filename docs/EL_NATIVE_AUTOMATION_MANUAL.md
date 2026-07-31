# EL Native Automation — hướng dẫn sử dụng

## 1. Thành phần và phiên bản

- Dùng `D:\Temp\opencode\el_build\injector.exe` để khởi chạy game và nạp `el_native.dll`.
- Đảm bảo thư mục game có `steam_appid.txt` đúng AppID Steam (bản hiện tại đã xử lý trường hợp chạy ngoài Steam).
- Bật overlay bằng phím tắt đã cấu hình, mở tab **Experimental → Automation**.
- Log nằm trong thư mục game; tìm các dòng có tiền tố `[AUTOMATION]`.

Bản native hiện bám theo flow trong `battle_cheat.exe_extracted/auto_battle.js` và `derank.js`: Play → battle → result → reward → đóng LeagueBar → vòng tiếp theo. Khác với các bài scan giá trị cũ trên FearLess, đây là hook observer trên IL2CPP; bài thảo luận cũng ghi nhận game dùng IL2CPP/anti-cheat và các phương pháp sửa giá trị cũ thường đã bị vá hoặc gây crash. [FearLess request thread](https://fearlessrevolution.com/viewtopic.php?start=30&t=34961)

## 2. Auto Rank / AutoBattle

1. Chọn `Automation mode = AutoBattle`.
2. Đặt `Delay (ms)` từ 1500–5000 khi thử lần đầu. Đây là thời gian chờ giữa các UI phase, không phải tốc độ trận.
3. Đặt `Max loops`:
   - `1–100`: tự dừng sau đúng số trận hoàn tất.
   - `0`: chạy liên tục cho đến khi bấm **Stop** (giống `MAX_LOOPS=0` của script tham khảo).
4. Bấm **Start automation**.
5. Chờ các trạng thái: `started; play is scheduled` → `Play invoked; battle running` → `result window observed` → `result advanced` → `LeagueBar closed; next loop armed`.
6. Bấm **Stop** trước khi đổi mode hoặc rời trận. Nút Stop đưa coordinator về `Idle` và không để lại click đang chờ.

Automation không tự chọn đội hình, không sửa damage và không thay kết quả trận. Nó chỉ gọi Play/next trên main thread và tự xử lý cửa sổ kết quả.

## 3. Derank

1. Chọn `Automation mode = Derank`.
2. Đặt delay ban đầu 3000–5000 ms; dùng `Max loops = 1` để kiểm tra một vòng.
3. Kiểm tra UI báo `Derank readiness: ready`, các field `settings`, `surrender`, `confirm` là `ok`.
4. Bấm **Start automation**. Flow đúng là:
   `Battlefield.Start` bắt nút Settings → mở Settings → bắt nút Surrender → bấm Surrender → bắt `TwoButtonWindow.leftButton` → xác nhận → chờ MatchCompleted → đóng LeagueBar → vòng kế tiếp.
5. Khi log đã sạch và kết quả đầu tiên đúng, đổi `Max loops = 0` nếu muốn chạy liên tục.
6. Bấm **Stop** ngay khi muốn kết thúc. Không đóng game trong lúc đang ở phase `AwaitConfirm`.

Nếu `Derank readiness` không sẵn sàng thì chỉ observer result/trace được cài; native không phát click mù vào UI.

## 4. Kiểm tra hook trước khi chạy

Trong log cần thấy dạng:

```text
[AUTOMATION] result observers ... hooked=1/1/1 play=1 advance=1
[AUTOMATION] derank methods=... resultDelegate=... ready=1
[AUTOMATION] league methods ... hooked=1/1/1/1 ready=1
```

`league ... ready=0` vẫn cho phép chạy vòng đơn, nhưng LeagueBar không được tự đóng; hãy Stop và kiểm tra lại DLL/metadata trước khi bật loop dài.

## 5. So sánh với battle_cheat

| Flow | battle_cheat | EL Native |
|---|---|---|
| Play + result delegate | Có | Có (`_onButtonAction`, fallback HandlePlayNext/Hide) |
| Derank Settings/Surrender/Confirm | Có | Có, gated theo field offsets |
| LeagueBar OnShown/Unlock/Update/OnClose | Có | Có, presenter được xoá trước khi gọi OnClose để tránh gọi kép |
| `MAX_LOOPS=0` | Có | Có (`0 = unlimited`) |
| Multichest open-all | Có trong script tham khảo | Có: tự gọi `OnOpenAll`, chờ animation rồi `OnCloseAction` |
| BundleForceShowWindow | Có trong script tham khảo | Có: `OnFocus` đặt lịch và gọi `OnClose` |

Multichest/Bundle hiện đã bám flow script: mở toàn bộ card thưởng, đóng sau animation và tự dismiss popup bundle. Nếu popup đang chứa một thao tác mua có giá thì Stop trước khi xác nhận.

## 6. Xử lý lỗi

- **`observer unresolved`**: đang nạp DLL cũ hoặc game build không khớp map. Tắt game, build/nạp lại DLL mới rồi xem lại ba dòng `[AUTOMATION]`.
- **Derank field layout unresolved**: kiểm tra `settings/surrender/confirm` trong log; không chạy Derank cho đến khi cả ba field là `ok`.
- **Result window đứng**: cần `_onButtonAction` hoặc `HandlePlayNext/Hide` có địa chỉ. Nếu chỉ `Show/Update` là `1/1`, hãy dùng trace để ghi log và dừng.
- **LeagueBar không đóng**: log `league ready=1` phải xuất hiện; thử delay 5000 ms. Nếu vẫn đứng, Stop, đóng thanh League thủ công và gửi đoạn log từ `result window observed` đến `LeagueBar`.
- **Mắc kẹt loading/disconnect**: thoát game sạch, kiểm tra `steam_appid.txt`, không chạy đồng thời proxy/server mode. Các thread web gần đây chủ yếu mô tả proxy/server và lỗi kết nối, không phải flow Automation native. [Everlusting Life web thread](https://fearlessrevolution.com/viewtopic.php?start=45&t=35808)

## 7. Quy trình A/B một lần

1. Tắt toàn bộ feature combat, chạy `AutoBattle`, `Max loops=1`, ghi log.
2. Lặp lại một vòng với `Derank`, `Max loops=1`.
3. Chỉ sau khi hai vòng sạch mới đặt `Max loops=0`.
4. Nếu có lỗi, giữ nguyên log từ lúc Start đến lúc Stop; không thử lại nhiều lần trong cùng một event có giới hạn lượt.

## 8. Rollback

Tắt Automation và nạp lại DLL baseline. Artifact rollback đã tạo tại `D:\Temp\opencode\el_native_rollback.ps1`; patch và verification record ở cùng thư mục `D:\Temp\opencode`.

