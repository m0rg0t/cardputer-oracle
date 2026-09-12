# Launch kit

Positioning: **A pocket fortune teller for Cardputer ADV—with spoken fortunes, AI Tarot and printable receipts.**

Project page: https://m0rg0t.github.io/cardputer-oracle/
Repository: https://github.com/m0rg0t/cardputer-oracle

## Next steps, in order

1. Verify the current EN/RU/ZH packages on a physical Cardputer ADV: installation, SD loading, fonts, narration, buttons and printing. Use [device checks](DEVICE_CHECK.md). Record which printer identifier was tested.
2. Validate the existing release archives and publish clearly labelled language packages with checksums and installation guides. There are no published GitHub releases yet; do not advertise a download button until its asset exists. Mark any remaining hardware limitations in release notes.
3. Record the short real-device demo below. Keep a separate full installation recording for a follow-up tutorial.
4. Share a concise project post with the demo in M5Stack Community Projects, then adapt it for r/CardPuter. Check each community's current posting rules. Use a longer build write-up for Hackster.
5. Invite reports of installation issues, tested printer identifiers, new themes and translation improvements. Track successful installations and useful feedback before expanding promotion.

## Real-device demo: 40 seconds

Record a steady landscape shot showing both Cardputer and printer. Keep the room quiet enough to hear the built-in speaker. Use a non-personal Tarot question and keep Wi-Fi passwords/API keys out of shot. Do not replace device audio with a voiceover that implies better speaker quality.

| Time | Real footage | Caption |
| --- | --- | --- |
| 0–4 s | Device and a finished receipt together | A pocket fortune teller |
| 4–9 s | Press a key; show the ritual animation | Press a key. Meet your future. |
| 9–18 s | Show and hear one complete short fortune | 120 spoken fortunes per language. Works offline. |
| 18–26 s | Show the actual receipt emerging and the readable artwork/text | Keep your fortune on paper |
| 26–33 s | Show a previously completed Tarot reading, or a question and actual response | Optional AI Tarot via your own compatible gateway |
| 33–37 s | Show clearly labelled EN/RU/ZH simulator screenshots | English · Russian · 简体中文 |
| 37–40 s | Device, receipt and project URL | Build it. Try it. Add your own fortunes. |

If a spoken fortune or print takes longer, extend the shot or label the cut. Do not imply a live API response is instantaneous. Label simulator screenshots as simulator screenshots. Include an actual Tarot request only after it works on the device; otherwise omit that shot and describe it in the post.

Suggested narration, if needed between device-audio segments:

> This is Cardputer Oracle: a tiny fortune teller with a voice and a paper trail. Choose tech, beer or tea for an offline fortune. Bring your own AI gateway for a three-card Tarot reading. English, Russian and Simplified Chinese builds are available from source. The code and setup guides are on GitHub.

Change “available from source” to “ready to download” only after release assets are published.

## Community post draft — not posted

Title: **I built a pocket fortune teller for Cardputer ADV—with voice and thermal receipts**

> I made Cardputer Oracle, a playful fortune teller for the M5Stack Cardputer ADV. Press a key and it picks a fortune, shows artwork and plays a prepared voice recording. With a supported BQ95/FunPrint device, it can also print a keepsake receipt.
>
> There are 120 offline fortunes in each of three language builds: English, Russian and Simplified Chinese. The themes are tech/everyday life, beer and tea. Offline mode needs no API key. Optional Tarot takes your question and sends a three-card spread to your own Responses-compatible AI gateway; LiteLLM is the tested gateway.
>
> Code, screenshots and installation guides: https://m0rg0t.github.io/cardputer-oracle/
>
> I'd especially welcome help testing installation, other printer identifiers, and the Chinese translation. This targets Cardputer ADV; compatibility with every toy printer is not claimed. The current multilingual builds have passed software checks; physical validation results will be listed with the release.

Replace the last sentence with specific hardware results once verified. Attach the real demo; do not present simulator footage as hardware footage.

## Contribution invitations

- Report a working printer model and BLE identifier, firmware version and paper width. Avoid sharing unrelated devices' scan results.
- Suggest a coherent theme with three sample fortunes: recognisable situations, kind humour, no event-specific references.
- Improve EN/RU/ZH phrasing and report missing glyphs with a screenshot.
- Test installation from a clean SD card using the published guide and report the exact step that was unclear.

## Longer build write-up outline

Start with the finished device and receipt, then cover hardware, language package installation, one-card-at-a-time SD loading, prepared speech versus generated sound effects, BQ95 protocol work, the Responses API contract, and the hardware limitations. Link to the compatibility guide rather than claiming support for every OpenAI-style endpoint.
