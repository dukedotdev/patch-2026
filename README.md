# Rec Room 2026 patch

Use Rec Room client **20260323.14** with a compatible community server. Other
client versions are unsupported. The patch leaves the original game files intact.

## Installation

1. Extract all five files from `Patch26Dist.zip` into the folder containing
   `RecRoom.exe`: `2026patch.dll`, `Injector.exe`, `2026patch.ini`,
   `RecRoomScreen.bat` and `RecRoomVR.bat`.
2. Edit `2026patch.ini` using the connection details supplied by your server operator.
3. Run `RecRoomScreen.bat` for desktop mode or `RecRoomVR.bat` for VR.

Alternatively, run `Injector.exe` first, leave its window open, then start
`RecRoom.exe` from the same installation. The batch launchers handle this order
automatically and select desktop or VR mode.

When updating an existing installation, ensure the game is closed before replacing patch files
and keep your configured `2026patch.ini` unless explicitly mentioned.

## Server settings

The included INI has notes beside every option. It contains no server address or
RSA key. Missing or blank connection overrides keep the client's existing values;
they do not select a community server automatically.

Your operator should provide the discovery address, service domain and any
Tachyon endpoint/public key their server needs. For example, the discovery settings
for a server using `example.com` would be:

```ini
[Network]
ServerUrl=https://ns.example.com
ServerDomain=example.com
```

Replace these example values with your operator's details. Keep the remaining
sections of the supplied INI and configure Tachyon if your operator requires it.

| Setting | What to enter |
| --- | --- |
| `ServerUrl` | Discovery address starting with `http://` or `https://`. A port is allowed; paths, trailing slashes, credentials and query strings are not. |
| `ServerDomain` | Service domain without a scheme, port or path. It should cover the server's service addresses, including `ServerUrl` when both are set. |
| `CachedServerDomain` | Previous server domain when migrating cached addresses. Leave blank unless your operator asks you to set it. Needs `ServerDomain`. |
| `TachyonHost` | Room/voice endpoint as `hostname:port` or `IPv4:port`, with a port from 1 to 65535. IPv6 is unsupported. Blank keeps the supplied endpoint. |
| `TachyonServerId` | Transport ID supplied by your operator. Usually leave blank to keep the matchmaking ID. An override needs `TachyonHost`. |
| `VoiceKeyXml` | Your operator's RSA **public** key, on one line, containing only `Modulus` and `Exponent`. A remote Tachyon override needs it; loopback can keep the client's key. Never enter a private key. |
| `DisableCertificatePinning` | `1` enables certificate compatibility for your configured service domain and its subdomains. Use `0` to keep original checks. |
| `AllowUnsignedImages` | `1` accepts images without the original service's signature. This applies to all images checked by the patch. Use `0` to keep original checks. |
| `FramesPerSecond` | `0` keeps original screen-sharing presets. Set 1-60 for a target rate; the delivered rate can be lower. Higher rates use more bandwidth. |
| `Verbose` | `0` keeps routine diagnostics off. Set `1` temporarily when troubleshooting. |

## Troubleshooting

- **Missing files:** Place the missing patch files beside `RecRoom.exe`. Keep your
  existing INI if it already contains your server settings.
- **Rejected settings:** Check the option named in the error against the INI notes
  and your operator's details. To run a separate configuration check, open a terminal
  in the game folder and enter `.\Injector.exe --check-install`.
- **Unsupported client:** Use client `20260323.14`, Steam manifest `8867414017562658247`. Do not mix files from different
  game versions.
- **Injector stays on Waiting:** If you started the injector manually, start the
  game from the same installation. The batch launchers start it automatically.
  Check that the patch files and `RecRoom.exe` are in the same folder and that the
  game actually started. The injector skips other installations and processes that
  already have the patch loaded.
- **Connection problems:** Confirm addresses and the public key with your server
  operator. The developer menu's transport address can show `none` at the title screen until a room
  connection begins.
- **Need more information:** Check `2026patch.log` in the game folder. Set `Verbose=1`
  before your next launch, reproduce the issue, then set it back to `0`.
  The log resets each launch and is limited to 1 MiB, so save it before launching again.

## Other information

For Studio's **Test Subroom**, launch Rec Room and sign in first. The patch preserves
local Maker Room, Rooms 2 and Studio test presence when a server heartbeat still
names the previous online room. Returning to an online room resumes normal presence
handling. This does not resolve missing or incompatible room save data.

Developer and build guidance is in [AGENTS.md](AGENTS.md).

Credits:
[Carpetsoft/Rec-Room-2025-Patch](https://github.com/Carpetsoft/Rec-Room-2025-Patch) for the original 2025 patch and injector. Amazing work!
[recflare/patch-2025](https://github.com/recflare/patch-2025) for formatting, packaging, and other miscellaneous references. Stay awesome and open-source! <3
