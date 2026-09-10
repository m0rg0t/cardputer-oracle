# Oracle cat

Generated with the built-in imagegen tool on 2026-09-09. Original: `assets/oracle-cat.png`. Device derivative: `sdcard/ORACLE/images/oracle.jpg`, resized to 92×92 and JPEG-encoded for firmware delivery. No semantic changes to the illustration.

Prompt:

> Use case: illustration-story. Asset type: square illustration displayed at only 92x92 pixels on a Cardputer fortune-telling device. Primary request: a humorous yet mystical oracle cat solemnly consulting a glowing crystal ball, looking deadpan and slightly unimpressed, benevolent playful fortune teller. Style: beautifully composed retro pixel-art inspired flat illustration with large readable shapes, very limited deep midnight plum, warm cream, antique gold palette matching a tiny handheld screen; clean silhouette, bold facial features. Composition: central large bust of cat, crystal ball immediately below face, sparse crescent moon and tiny stars, plain dark #100e1b backdrop. No text, no letters, no border, no watermark, no fine detail. One single square image, not a sprite sheet.

## Additional artwork, 2026-09-10

Six new square illustrations generated separately with the built-in imagegen tool. Full, verbatim prompt set: `assets/additional-art-prompts.json`. They continue the existing midnight-plum, cream and antique-gold pixel-art style. No user photographs were used as references.

| Original | Device JPEG | Subject |
|---|---|---|
| `assets/terminal-cat.png` | `/ORACLE/images/terminal-cat.jpg` | Deadpan cat wizard at an enchanted terminal with a tiny bug spirit |
| `assets/tea-alchemy.png` | `/ORACLE/images/tea-alchemy.jpg` | Cat alchemist consulting an enchanted teacup |
| `assets/moon-nap.png` | `/ORACLE/images/moon-nap.jpg` | Sleeping cat wizard in a crescent moon |
| `assets/beer-oracle.png` | `/ORACLE/images/beer-oracle.jpg` | Cat fortune teller with beer and a foam moustache |
| `assets/hop-wizard.png` | `/ORACLE/images/hop-wizard.jpg` | Cat bartender with an enchanted hop cone and beer tap |
| `assets/cosmic-toast.png` | `/ORACLE/images/cosmic-toast.jpg` | Two tankards clinking under the stars |

The six full-resolution originals are kept unmodified. `tools/prepare_images.py` performs only delivery conversion: RGB, proportional downscale to 92×92, baseline JPEG encoding. It also builds a review sheet from the decoded device JPEGs at 1× and 3× scale. No semantic changes were made after generation. Per-card assignments are recorded in `assets/artwork.json`. Text and voice content are independent of the illustration assignments.

## Tea collection, 2026-09-10

Two further illustrations were generated with the built-in imagegen tool for the new tea theme. Full prompts: `assets/tea-art-prompts.json`. No user photographs were used as references. Both 1254×1254 originals are preserved unchanged:

| Original | Device JPEG | Subject |
|---|---|---|
| `assets/teapot-wizard.png` | `/ORACLE/images/teapot-wizard.jpg` | Solemn cat wizard pouring tea from an enchanted golden teapot |
| `assets/tea-leaf-oracle.png` | `/ORACLE/images/tea-leaf-oracle.jpg` | Cat fortune teller reading a constellation of floating tea leaves |

The two new pictures and the existing tea alchemist each accompany four tea predictions. Device exports use the same RGB 92×92 baseline JPEG conversion. Review sheet: `docs/tea-artwork.png`; all nine device images: `docs/artwork-overview.png`.
