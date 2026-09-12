# Online Tarot: OpenAI-compatible endpoints

Online Tarot uses the **OpenAI Responses API** over HTTPS. LiteLLM is the tested gateway. A server advertised as “OpenAI-compatible” may implement only Chat Completions; that is not sufficient for this firmware.

The three offline themes need no account, API key or Internet connection.

## Configure the SD card

Copy `ORACLE/config.example.json` to `ORACLE/config.json` on the microSD card. Edit the copy with your own settings:

```json
{
  "schema": 1,
  "wifi": [
    {"ssid": "YOUR_WIFI", "password": "YOUR_WIFI_PASSWORD"}
  ],
  "tarot": {
    "host": "ai.example.com",
    "model": "YOUR_MODEL_ALIAS",
    "token": "YOUR_API_KEY_HERE"
  }
}
```

| Setting | Accepted value |
| --- | --- |
| `wifi` | One to four saved 2.4 GHz networks, in preference order |
| `tarot.host` | HTTPS hostname only, without `https://`, port, path or trailing slash |
| `tarot.model` | Exact model alias exposed by your gateway, at most 96 bytes |
| `tarot.token` | Bearer token, 16–256 characters; ASCII letters, digits, `_` and `-` only |

The current URL is fixed to `https://HOST/v1/responses` on the default HTTPS port. There is no configurable `base_url`, custom path, custom port or HTTP mode. A token containing other characters is rejected before a request is sent.

Keep the real config private and preserve it when updating resources. It is excluded from Git and public release packages, but remains readable on the physical SD card. A gateway virtual key lets you retain provider credentials on your server and apply a project-specific usage limit.

## Gateway and model requirements

The device sends one non-streaming POST with `Authorization: Bearer TOKEN` and JSON containing:

- `model`: your configured alias;
- `input`: system instructions and the question plus three cards;
- `store: false`, `stream: false`, `max_output_tokens: 512`;
- `reasoning.effort: "none"`;
- `text.format`: a strict JSON schema named `tarot_reading`, requiring exactly one string field, `reading`, with no extra properties.

The gateway/model combination must accept that complete request. Changing the model name alone does not make an incompatible model work. Configure any provider-specific translation on the gateway.

The response must use the Responses API envelope: `status: "completed"`, an assistant message with `status: "completed"`, and exactly one `output_text` containing a JSON object such as:

```json
{"reading":"Your carefully made plan has room for one pleasant surprise. Leave a little space in the calendar."}
```

This object is the text inside the API response, not the complete HTTP response body. Bare JSON and Chat Completions `choices` responses are not accepted. Refusals, incomplete output, unsupported characters and oversized responses are rejected. The HTTP body limit is 16 KiB; the firmware also validates the reading for the selected language.

| Service setup | Current support |
| --- | --- |
| LiteLLM exposing `/v1/responses` with a compatible model | Tested from the host using the firmware parser |
| Another gateway implementing the same request and response contract | Potentially compatible; verify it before relying on it |
| Direct OpenAI Responses API | Same API family, but direct device access and its certificate chain have not been validated for this release |
| A server exposing only `/v1/chat/completions` | Not supported |
| HTTP, custom ports, custom path prefixes or self-signed certificates without a trusted root | Not supported by the default configuration |

The recorded LiteLLM check used `openai/gpt-5.6-luna`; treat this as a tested gateway alias, not a name guaranteed to exist on your server. See [Tarot implementation and validation notes](TAROT.md).

## TLS and network access

Keep `/ORACLE/tls/roots.pem` on the SD card. The app obtains time through NTP and verifies the server certificate and hostname. The supplied bundle contains ISRG Root X1 and X2. For another certificate chain, add the appropriate verified root certificate; do not disable verification.

Your Cardputer must be able to reach the gateway itself. A phone hotspot provides network access; do not assume that it shares the phone's VPN tunnel. A reachable gateway can make the upstream provider request from the server.

## Use and troubleshoot

Select Tarot, type a question or leave it blank for a general reading, then press Enter. The device chooses three different cards and asks for an interpretation in the build's language. The reading stays fixed on screen and paper; pressing P again does not call the model again. Online Tarot currently has no generated narration.

| Symptom | Check |
| --- | --- |
| Network not found | SSID spelling, hotspot availability and 2.4 GHz; try the iPhone's Maximize Compatibility setting |
| Configuration/token rejected | Host syntax, token length/characters, model alias and valid JSON |
| TLS connection failure | Reachability, NTP time, certificate hostname and trusted roots |
| HTTP 401/403 | Token validity, model permissions and gateway access rules |
| HTTP 404 | Gateway exposes `/v1/responses` at the root hostname |
| HTTP 400 | Gateway accepts Responses input, strict schema and `reasoning.effort: none` for this model |
| HTTP 429 | Gateway/provider quota or rate limits |
| Invalid/incomplete reading | Responses envelope, structured output, response size and supported language characters |

The firmware does not automatically retry a timed-out request. Check gateway logs without publishing credentials or users' questions, then deliberately try again. Requests already sent may still incur provider usage after cancellation. Offline themes remain available.

The exact contract lives in [request and parser code](../firmware/src/tarot_api.cpp) and [network client](../firmware/src/tarot_client.cpp). Hardware Wi-Fi, BLE transitions and Tarot printing still need verification for the current multilingual release.
