# HTTPS Proxy Mutation — Methodology & Playbook

> Hoc tu battle_cheat (Everlusting Life) + ban doc lap `el_proxy.py`.
> Muc dich: tai tao proxy doi request/response cho game khac, kem cach tac gia
> goc tim ra huong va cach thu thap du lieu tot hon.

## 1. Vi sao proxy doi request/response hoat dong

Client (game) --HTTPS--> Server (CDN) --HTTPS--> Game API. 3 diem yeu:

| Diem yeu | Khai thac | Ket qua |
|---|---|---|
| DNS client-side | Redirect host trong hosts file -> traffic vao may minh | Kiem soat toan bo duong di |
| TLS termination | Cai cert tu ky (SAN dung domain) vao trusted root | Doc/sua plaintext |
| Server tin request/response la "su that" | Doi gia tri request truoc khi forward / response truoc khi tra client | Server chap nhan du lieu da sua |

Nguyen tac vang: neu client khong bi danh dau (no signature, no encrypted
payload, no network anti-cheat) thi server khong the phat hien sua doi.
Game Unity IL2CPP thuong sign it - chu yeu dua vao TLS.

## 2. Kien truc proxy

```
hosts:  127.0.0.1 ga.adult-chess.com

Game --> 127.0.0.1:443 (proxy, TLS terminate bang cert cua minh)
            | 1. Doc request JSON (gzip decode neu can)
            | 2. MUTATE request (doi reward, sua action...)
            | 3. Forward toi IP THAT (Host header giu nguyen, CERT_NONE)
            |        https://<real_ip><path>
            | 4. Nhan response
            | 5. MUTATE response (doi resource, chan error code...)
            +--- 6. Tra client (gzip re-encode neu can)
```

### 3 manh ghep chinh (el_proxy.py)

1. **Hosts redirect** - `update_hosts()`: them/xoa `127.0.0.1 <host>` trong
   `C:\Windows\System32\drivers\etc\hosts`. Quirk Windows:
   - File hosts bi **ReadOnly + System + Hidden** (attrib `A SH`) -> dung
     `attrib -s -h` (elevated), fallback atomic replace `hosts.tmp` + `os.replace`
   - Khi hosts redirect, DNS resolve ra 127.x -> hard-code `FALLBACK_IPS`
     de forward den IP that

2. **TLS :443 terminate** - cert tu ky `adultchess.crt/.key`:
   - SAN phai gom: `adult-chess.com`, `ga.adult-chess.com`, `ru.adult-chess.com`, `*.adult-chess.com`
   - Cai vao trusted root: `certutil -addstore Root cert.crt` (admin)
   - Client chi tin neu cert dung host + da cai vao root store

3. **Forward + mutate** - `urllib` + `HTTPSHandler(context=CERT_NONE)`:
   - **Bat buoc** truyen context vao opener (bug kinh dien: tao `ctx` nhung
     quen gan -> upstream cert tu ky bi verify fail)
   - `ProxyHandler({})` de bypass system proxy
   - Giu `Host` header = host that (server ao hosting khong tu choi)

## 3. Cach tac gia battle_cheat tim ra huong (recon)

Bang chung tu `battle_cheat.exe_extracted/`:

### 3.1. Frida + IL2CPP exports
- `il2cpp_resolve.js`: enumerate classes/methods/fields qua
  `il2cpp_domain_get`, `il2cpp_image_get_class`, `il2cpp_class_get_methods`,
  `il2cpp_field_get_offset`
- Assembly muc tieu: `Assembly-CSharp` + **`ACTk.Runtime`** (= Anti-Cheat
  Toolkit, biet chong gi de ne)
- `cheats_menu.js`: mo **CheatsWindow built-in** (dev menu 30+ module: Account,
  Battle, Payments, Rewards, Statistics, StartMatch...) - game Unity thuong
  con dev tools
- `battle_cheat.js`: resolve method **runtime tu metadata** (update-resilient),
  field offset co fallback (`ArmySide` -> `side` -> `<ArmySide>k__BackingField`
  -> `_armySide` -> hardcode 144)

### 3.2. Tu class/method -> network endpoint
```
Class co method nghi ngo (ApplyCardRouletteSpinRewards)
  -> Frida hook ghi doi so (JSON string)
  -> thay action name trong body gui di
  -> grep action name trong code
  -> xac dinh endpoint /gs_api/profile/update (POST, action-based batch)
  -> luong: profile/update (pick) -> card_roulette/cached_spin (deal)
     -> get_progress (state)
```

### 3.3. Phat hien host/port/IP
- Quan sat traffic (Wireshark/Fiddler/mitmproxy) -> `ga.adult-chess.com:443`
- hosts redirect + FALLBACK_IPS hard-code -> server IP that
- Cert cua chinh game (co trong extracted) -> SAN chua moi domain that

### 3.4. Tim reward presets (chinh xac tu pyc)
Presets nam trong GUI config dang tuple
`(rtype, rid, default_qty, template, desc)`:
```python
('item', 'minievents_card_roulette_resource', 500, None, 'entry currency')
('resource', 'gems', 500, None, 'premium (untested)')
('resource', 'gold', 48, 'preset_gold_portion:{n}', 'server scales 12/24/36/48')
('monster', 'from:unlocked_by_config;rarity:common', 4,
 'preset_souls_portion:{n}', ...)
```
Cach extract: scan bytecode `re.finditer(rb'[\x20-\x7e]{4,}')` + marshal load
code objects -> tim tuple consts trong `_build_ui`.

### 3.5. Chien thuat "an toan"
- **Bat dau voi currency entry** (tokens) - server it validate, khong phai
  premium that
- **Black_mark pass-through** - khong doi request chon black_mark (tranh phat)
- **Response chi log, khong sua reward claim** - de server credit that
- Kiem tra `get_progress` (`accumulated_rewards`, `has_black_mark`) de biet state

## 4. Co che da giai ma (Everlusting Life)

### 4.1. Luong Card Roulette
```
1. Client vao roulette -> POST /gs_api/card_roulette/cached_spin
   Response: { result: { rewards: [4 cards], is_black_mark: bool } }
2. Client pick card -> POST /gs_api/profile/update
   Body update[]: { action: "ApplyCardRouletteSpinRewards",
                    data: { roulette_id, step_id, reward_id, reward_type,
                            reward_qty, rewards:[{type,quantity,id}] } }
3. Client xem state -> POST /gs_api/card_roulette/get_progress
   Response: { result: { step_id, accumulated_rewards, has_black_mark } }
```

### 4.2. Deck slide (bay hieu nham)
- `cached_spin` response = bo cards **moi deal cho vong sau** (khong phai bo
  UI dang hien thi)
- UI hien thi bo **cu** toi khi flip -> card #1 lan sau = card #2 lan truoc
  (deck dich)
- **Index that** = so khop `reward_type/reward_id` cua pick voi deck
  cached_spin gan nhat -> `picked card #[N]` (da implement trong el_proxy)
- `is_black_mark` + vi tri `<<BM>>` trong deck = biet ne card nao

### 4.3. accumulate khong reset
- `accumulated_rewards` cong don **ca session roulette** (Continue giu state)
- Pick luc proxy tat/chua bat = reward goc nam lai trong acc -> khong the
  "sua lai"
- Session moi = sach se 100% reward doi

### 4.4. Error code blocking (mitm_addon cu)
- Response chua `error_code`/`code`/`err_code` in {700,701,702,703,801,802}
  -> doi thanh 0 + `success:true` + `message:"OK"`
- Resource override: gold/gems/energy/contribution/elixir/spin -> 999999
  (nested paths: `userData/profile/data/result/resources`)

## 5. Playbook — tai tao cho game khac

### Buoc 1: Recon (1-3 ngay)
- Chay game + Wireshark/Fiddler -> ghi host/IP/port
- Dump cert game (tim trong extracted/install) -> lay SAN
- Neu Unity IL2CPP: Frida enumerate classes -> tim class lien quan
  (keyword: Roulette, Reward, Spin, Battle, Payments...)
- Hook method gui network (WWW, UnityWebRequest, HttpClient, socket)
  -> ghi body plaintext
- Liet ke endpoint + action names + cau truc JSON

### Buoc 2: Chung minh (1 ngay)
- Nhan ban 1 request bang tay (curl/Postman) toi server that
  -> xem co validate khong
- Tim 1 field "an toan" de test (currency entry > premium)

### Buoc 3: Dung proxy (0.5 ngay — copy el_proxy.py)
- Sinh cert:
  `openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365
  -nodes -subj "/CN=<host>" -addext "subjectAltName=DNS:<host>,DNS:*.<host>"`
- Sua `el_proxy.json`: `server_host`, `fallback_ips`, `port`
- `attrib -s -h hosts` (admin) truoc khi chay
- Install cert root store (admin): `certutil -addstore Root cert.pem`
- Test: `py el_proxy.py --start-bg` -> curl qua proxy -> 200 tu server that

### Buoc 4: Mutate (0.5 ngay)
- Xac dinh action/endpoint -> viet `mutate_request` (giu schema, chi doi gia tri)
- Xac dinh response field -> viet `mutate_response` (giu JSON valid, re-gzip dung)
- Capture all: bat proxy + FILE log de so sanh pick luon ban dau vs ban goc
- Chay 1 luot that -> verify reward vao tai khoan -> lap

### Buoc 5: Scale (khi can)
- Fuzz action values (qty 1 -> 999999) de tim gioi han server
- Test "dirty" flow: pick 2 cards, pick sau khi het spin, BM+reward cung luc
- Doc ky thread mac dinh: proxy KHONG sua reward claim trong response
  (de server credit that) — chi sua khi hieu ro vi sao

## 6. Thu thap du lieu tot hon (gap hien tai)

Proxy hien tai chi log request/response cua game. Muon day du:

1. **Log ca 2 phia**: user_agent + `x-*` headers tu game (de xem client
   co gui device fingerprint khong), va headers tu server
2. **Mitmdump**: `mitmdump -s el_addon.py --set flow_detail=0 -w cap.mitm`
   -> replay offline, diff chinh xac
3. **Frida ben canh proxy**: hook client method de doi chieu
   response goc vs response nhan duoc -> biet client xu ly field nao
   (gom: apply toi dau, ignore field nao)
4. **Diff request goc**: bat request gui di khi KHONG doi -> so sanh
   voi request DA doi -> chi xem server cham vao field nao (validate hay khong)
5. **Fuzz boundary**: thu qty am, qty 0, reward_id khong ton tai,
   action khong hop le -> map hanh vi server

## 7. Gotchas (da gap - dung lam lai)

- **Tao ctx nhung quen gan vao opener** -> SSL verify fail, mat ca tieng
- **Gzip double-encode** -> client khong doc duoc response
  (doc request neu `Content-Encoding: gzip`, tra response gzip lai cung co dau)
- **Hosts file bi ReadOnly+System+Hidden** -> `attrib -s -h` truoc,
  file moi phai UTF-8 ko BOM
- **Upstream cert verify** -> luon CERT_NONE, IP-based SSL de tranh SNI miss
- **client login payload khac ca 2 phia** -> doc ky, ghi `request_bytes_key`
  truoc khi mutate (bytes trung -> dung chung, khac -> dung field rieng)
- **TCP half-open khi forward fail** -> server timeout -> client cho lau;
  nen co `--passthrough` khi web khong muon proxy xen vao

## 8. Config reference (el_proxy.json)

```json
{
  "server_host": "ga.adult-chess.com",
  "fallback_ips": ["<ip>"],
  "port": 443,
  "action": ["ApplyCardRouletteSpinRewards"],
  "mutate_requests": ["spin"],   // chi doi request co chuoi nay trong path/body
  "capture_dir": "gui",          // null = khong luu, diem khac: ghi ra gui/
  "passthrough": false,          // true: doi qua proxy khi khong muon mutate
  "tier_logging_vrb": [0, 1, 2]  // do chi tiet log
}
```

Preset co the them vao GUI config cua game (nhin `_build_ui` de biet format)
va dem vao `mutate_request` theo weight chu khong random deu.