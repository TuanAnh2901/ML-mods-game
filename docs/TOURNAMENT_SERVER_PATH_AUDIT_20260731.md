# Tournament server-path audit (2026-07-31)

## Evidence from `el_native.log`

The captured Tournament run resolves and hooks:

- `ServerParticipant.UpdateStreaks` (`RVA 0xE18B00`)
- `ServerParticipant.get_IsLocal` (`RVA 0xC05D40`)
- `ServerParticipant.get_Uid` (`RVA 0x5CF280`)
- `ChessSocketsController.BattleResult` (`RVA 0xD774F0`)
- `ChessSocketsController.BattleCalculateResult` (`RVA 0xD77110`)

The run then logs:

```text
[FEATURE] BattleResult: ChessSocketsController winner overridden round=1
[NET] POST .../v2/battles/ac/ac_battle payload=PlayerStatisticBattleRequest
[NET] POST .../v2/matches/ac/ac_match payload=MatchStatisticData
```

There is no `Tournament UID match` and no `UpdateStreaks ... -> ...` line. This means the local `ServerParticipant.UpdateStreaks` path was not the authoritative result path in this run. The server response arrives through the ChessSockets callback layer.

## Consequence

Changing the client-side `winnerId` in `BattleResult` changes the local request, but the server still evaluates the battle and sends the authoritative match result. The `/v2/battles/ac/ac_battle` and `/v2/matches/ac/ac_match` HTTP calls are statistics/telemetry requests, not the authoritative Tournament decision. The existing mitmproxy addon only handles HTTP request logging and response rewriting; it does not decode or rewrite the ChessSockets websocket event.

## Current instrumentation

The DLL now traces, without changing data:

- `ChessSocketsController.ServerPlayerMatchResult`
- `ChessSocketsController.CalculateServerBattle`

The next Tournament capture should contain:

```text
[FEATURE] BattleResult: ServerPlayerMatchResult response=...
[FEATURE] BattleResult: CalculateServerBattle response=...
```

Those callbacks are the correct place to map the response object fields (participant list, winner/result, warlord health) before considering a narrowly scoped response observer. Do not treat the analytics HTTP endpoints as the result source.

## Request/response conclusion

Bunny Royale works at the model/response layer because its card list is delivered to the client in a normal HTTP response. Tournament uses a server-authoritative websocket result path. A reliable request rewrite would require decoding the websocket protocol and its signed/validated result contract; changing only the HTTP telemetry or local `UpdateStreaks` state does not change elimination.
