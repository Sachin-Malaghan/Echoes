# ECHOES — store listing copy

Everything here is ready to paste into Google Play Console (and later App Store Connect). The graphics
are in `Build\Android\PlayStore\`, made by `Tools\Build\store_graphics.ps1` from the capture screenshots.

## Main store listing

**App name (30 max):** `Echoes: Time Loop Puzzle`
(Plain "Echoes" is used by many apps; the subtitle makes ours findable and says what it is.)

**Short description (80 max):**
`You have 10 seconds. Then time rewinds and your past self helps you. 20 levels.`

**Full description (4000 max):**

```
You have ten seconds.

When time runs out, the loop rewinds — and the run you just played comes back as an Echo, a glowing
ghost of yourself that does exactly what you did. Echoes stand on switches, hold doors open, become the
step you need and even take a laser for you. Plan a few loops, and on the last one every Echo moves
in perfect sync while you walk out through the portal.

You are your only teammate.

• 20 handmade puzzles in two worlds: the cold glass corridors of The Lab and the burning Factory
• Pressure plates, doors, spikes, towers of Echoes, and pulsing lasers that only an Echo can stop
• Every level can be solved in a few loops — can you match par?
• Three stars per level: solve it, solve it at par, find the hidden shard
• Up to six Echoes at once, each with its own colour and look
• A soundtrack locked to the loop: every Echo you record adds an instrument
• Rewind any time: end a loop early and your Echo holds its pose for good
• Big touch controls made for phones, plus keyboard and gamepad support
• Short levels, 30 to 90 seconds — perfect for a break
• No ads. No in-app purchases. No account. No data collected. Plays offline.

A lab assistant is trapped in a collapsing time-experiment facility. The loops are the only way out.
```

**App category:** Game → Puzzle
**Tags (pick up to 5 in Play Console):** Puzzle, Platformer, Brain games, Single player, Offline
**Contact email:** your developer email (shown publicly on the listing)
**Website:** `https://sachin-malaghan.github.io/Echoes/`
**Privacy policy:** `https://sachin-malaghan.github.io/Echoes/privacy.html`

## Graphics (upload from `Build\Android\PlayStore\`)

| Play Console field | File | Size |
|---|---|---|
| App icon | `icon-512.png` | 512 x 512 PNG |
| Feature graphic | `feature-graphic-1024x500.png` | 1024 x 500 |
| Phone screenshots (2-8) | `screenshot-01 ... 08.jpg` | 1600 x 900 / 1440 x 720, all within 2:1 |
| 7-inch and 10-inch tablet screenshots (optional) | the same files | |

## Release notes (1.0.0)

```
First release: 20 levels in two worlds, The Lab and The Factory.
```

## Answers for the Play Console "App content" forms

These describe the game exactly as built (checked on the release APK, 2026-09-30).

- **Privacy policy:** the URL above.
- **Ads:** No, the app does not contain ads.
- **App access:** All functionality is available without special access (no login).
- **Content rating (IARC questionnaire):** category *Game*.
  - Violence: the character is stopped by spikes and lasers; the screen flashes and the loop restarts.
    No blood, no injuries shown, no people or animals harmed. Answer the violence questions with this in
    mind (it is at most mild fantasy peril).
  - Sexuality, language, drugs, alcohol, tobacco, gambling, horror: none.
  - User interaction: users cannot talk to each other, share content or location; no purchases.
  - Expected result: PEGI 3 / ESRB Everyone (possibly PEGI 7 for "mild fantasy violence").
- **Target audience and content:** ages 13-15, 16-17 and 18+ (the design brief's 13+). Choosing only
  13+ keeps the app out of the Families programme's extra rules. "Appeal to children": No.
- **News app:** No. **Government app:** No. **Financial features:** None. **Health:** None.
- **Data safety:**
  - Does your app collect or share any of the required user data types? **No.**
  - Is all user data encrypted in transit? Not applicable (nothing is sent). Answer as the form requires
    after "No data collected".
  - Account creation: **No accounts.** Data deletion request: not applicable.
  - The only thing stored is a small progress/settings file on the device itself; it never leaves it.
- **Permissions** in the APK: INTERNET, ACCESS_NETWORK_STATE, ACCESS_WIFI_STATE, WAKE_LOCK, VIBRATE,
  MODIFY_AUDIO_SETTINGS, CHECK_LICENSE — all ordinary engine defaults, none "dangerous", none needs a
  declaration form. The game itself makes no network requests.
- **Advertising ID:** the app does not use the advertising ID (answer "No" if asked).

## Technical facts (for the "Release" pages)

- Package name (permanent): `com.brainrotinteractive.echoes`
- Version: `1.0.0` (versionCode 1). **Every new upload needs a higher `StoreVersion`** in
  `Config\DefaultEngine.ini` (2, 3, ...).
- Min Android 8.0 (API 26), target API 36, arm64, 16 KB page aligned, Vulkan + OpenGL ES 3.1.
- Upload format: Android App Bundle (`.aab`) signed with the upload key; Google Play App Signing holds
  the app signing key.
