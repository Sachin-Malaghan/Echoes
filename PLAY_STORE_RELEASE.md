# Putting ECHOES on Google Play — step by step

Follow the steps in order. Things marked **(you)** only you can do (passwords, accounts, publishing clicks).
Everything to paste into forms is in `STORE_LISTING.md`; every image is in `Build\Android\PlayStore\`.

What you will end up with: **Echoes: Time Loop Puzzle** (`com.brainrotinteractive.echoes`), version 1.0.0,
free, no ads, no data collected.

---

## Step 1 — Create the upload key (once, 3 minutes) (you)

The upload key proves future updates come from you. You type the password; it is never shown to anyone.

1. Open **PowerShell** (Start menu → type `PowerShell` → Enter).
2. Paste these two lines and press Enter:
   ```
   cd C:\SACHIN\Echoes
   powershell -ExecutionPolicy Bypass -File Tools\Build\create_upload_key.ps1
   ```
3. Type a password (8+ characters, no double quotes) twice. Write it down somewhere safe.
4. Name: `Brainrot Interactive Studios`. Country code: `IN`.
5. **Back up these two files** to a USB stick *and* a private cloud folder:
   - `C:\SACHIN\Echoes\Build\Android\echoes-upload.keystore`
   - `C:\SACHIN\Echoes\Config\Android\AndroidEngine.ini`

   They are git-ignored on purpose: they never go to GitHub.

## Step 2 — Build the release bundle (Claude can do this for you)

Tell Claude "build the release", or run:
```
cd C:\SACHIN\Echoes
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Release
```
It takes 10-20 minutes. The file to upload is `Packaged\Android\Echoes-Android-Shipping.aab`
(a `..._universal.apk` next to it is only for installing on your own phone).

## Step 3 — Put the privacy policy online (5 minutes) (you)

Google requires a privacy-policy web page. The repo already contains one (`Website\`) and a GitHub
Action that publishes it.

1. Go to https://github.com/Sachin-Malaghan/Echoes → **Settings** → **Pages** (left menu).
2. Under **Build and deployment → Source**, choose **GitHub Actions**.
3. Go to the **Actions** tab → **Website** → **Run workflow** → **Run workflow**. Wait for the green tick (about 1 minute).
4. Open https://sachin-malaghan.github.io/Echoes/privacy.html — you should see the ECHOES privacy policy.

> If the repository is **private**, GitHub Pages needs a paid plan. Either make the repo public
> (Settings → General → Danger Zone → Change visibility) or tell Claude and we will host the page elsewhere.

## Step 4 — Create the app in Play Console (you)

1. Go to https://play.google.com/console and sign in with your developer account (Brainrot Interactive Studios).
2. Click **Create app** (top right).
3. Fill in:
   - App name: `Echoes: Time Loop Puzzle`
   - Default language: `English (United States) – en-US`
   - App or game: **Game**
   - Free or paid: **Free** (this cannot be changed to paid later)
4. Tick the two declarations (Developer Program Policies, US export laws) → **Create app**.

## Step 5 — Finish "Set up your app" (the Dashboard checklist) (you)

On the app's **Dashboard**, open **View tasks** under "Set up your app" and do each one. Answers are in
`STORE_LISTING.md` → "Answers for the Play Console App content forms".

| Task | What to enter |
|---|---|
| Set privacy policy | `https://sachin-malaghan.github.io/Echoes/privacy.html` |
| App access | **All functionality is available without special access** |
| Ads | **No, my app does not contain ads** |
| Content rating | Email: yours. Category: **Game**. Answer the questionnaire from STORE_LISTING.md (no violence to people, no blood, no chat, no purchases) → Save → Submit. |
| Target audience | Ages **13-15, 16-17, 18+**. "Could unintentionally appeal to children?" **No** |
| News apps | **No** |
| Data safety | "Does your app collect or share any of the required user data types?" **No** → Next → Submit |
| Government apps | **No** |
| Financial features | **My app doesn't provide any financial features** |
| Health | **My app does not have any health features** |
| Select an app category and provide contact details | Category **Game → Puzzle**; tags Puzzle, Platformer, Brain games, Single player, Offline; your contact email; website `https://sachin-malaghan.github.io/Echoes/` |
| Set up your store listing | See Step 6 |

## Step 6 — Store listing (you)

**Grow users → Store presence → Main store listing**:

1. App name, short description, full description: copy from `STORE_LISTING.md`.
2. App icon: `Build\Android\PlayStore\icon-512.png`
3. Feature graphic: `Build\Android\PlayStore\feature-graphic-1024x500.png`
4. Phone screenshots: all eight `screenshot-*.jpg` from the same folder (drag them in, in number order).
   Optionally use the same files for 7-inch and 10-inch tablets.
5. **Save**.

## Step 7 — Internal test: install it from Play on your own phone (you)

1. **Test and release → Testing → Internal testing** → **Testers** tab → **Create email list**
   (name it `Me`, add your Gmail) → Save → tick the list → **Save**.
2. **Releases** tab → **Create new release**.
3. "App integrity": leave **Google Play App Signing** on (Google keeps the app signing key; your upload
   key from Step 1 signs uploads). Accept if asked.
4. **Upload** `Packaged\Android\Echoes-Android-Shipping.aab`.
5. Release name: `1.0.0`. Release notes: from `STORE_LISTING.md`. → **Next** → **Save and publish**.
6. Back on the **Testers** tab, copy the **join link**, open it on your phone, accept, then install
   ECHOES from the Play Store. Check touch, full screen, and that all 20 levels play.

## Step 8 — Closed test with 12 testers for 14 days (only for new personal accounts) (you)

Google requires this for **personal** developer accounts created after 13 November 2023 before an app can
go to production. (An **organization** account skips this step.) Dashboard will say if it applies.

1. **Testing → Closed testing** → **Create track** (or use the default "Closed testing - Alpha").
2. **Countries/regions**: add all countries.
3. **Testers**: create an email list with **at least 12** Google accounts (friends, family) — or use a
   Google Group. Save.
4. **Create new release** → **Add from library** → pick the 1.0.0 bundle from Step 7 → Next →
   **Send for review** (the first review can take a few days).
5. Send testers the join link. They must **opt in and keep the app installed for 14 days in a row**.
   Ask them to play a few levels (Google checks that testing really happened).
6. After 14 days the Dashboard shows **Apply for production**. Answer the short questions (how you
   tested, what you changed) and submit.

## Step 9 — Production release (you)

1. **Test and release → Production** → **Countries/regions** → **Add countries** → select all → Save.
2. **Create new release** → **Add from library** → 1.0.0 → release notes → **Next**.
3. Fix anything the page flags (it lists missing items), then **Save** → **Go to overview** →
   **Send changes for review**.
4. Review usually takes 1-7 days. You get an email when ECHOES is live; the store page is
   `https://play.google.com/store/apps/details?id=com.brainrotinteractive.echoes`.

## Later: every update

1. Raise `StoreVersion` in `Config\DefaultEngine.ini` (1 → 2 → 3 ...) and `VersionDisplayName` (1.0.1 ...).
2. Build again (Step 2), then **Production → Create new release → Upload** the new `.aab`.
3. Keep the upload key files from Step 1: every update must be signed with the same key.

## Things that can never change after the first upload

- The package name `com.brainrotinteractive.echoes`.
- Free → paid.
