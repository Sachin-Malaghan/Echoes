# Setting up ECHOES on a new PC

This PC already has everything (same setup as HOLLOWLIGHT / EMBERHOME). On a new machine:

1. **Unreal Engine 5.8** from the Epic Games Launcher. In the Launcher: Library -> UE 5.8 -> Options -> tick **Android**.
2. **Visual Studio 2022** with the "Game development with C++" workload (the repo's `.vsconfig` lists the rest).
3. **Android Studio**, then SDK Manager -> SDK Tools -> tick **Android SDK Command-line Tools**. Then run
   `C:\Program Files\Epic Games\UE_5.8\Engine\Extras\Android\SetupAndroid.bat` once.
4. **JDK 21** (Gradle does not run on Android Studio's Java 25):
   `winget install Microsoft.OpenJDK.21`, then create `%USERPROFILE%\.gradle\gradle.properties` with
   `org.gradle.java.home=C:/Program Files/Microsoft/jdk-21.0.12.101-hotspot` (your version's folder).
5. Antivirus with HTTPS scanning (Avast, AVG...) is handled by `Tools\Build\package.ps1` automatically.

Build and check:

```
powershell -ExecutionPolicy Bypass -File Tools\SimHarness\run.ps1
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="C:\SACHIN\Echoes\Echoes.uproject" -game -rocket
powershell -ExecutionPolicy Bypass -File Tools\Validation\run_tests.ps1
powershell -ExecutionPolicy Bypass -File Tools\Build\package.ps1 -Platform Android -Config Development
```

The phone build lands in `Packaged\Android\`. Copy the `.apk` to the phone and tap it (allow "install unknown
apps" for your file manager), or `adb install -r <file>.apk` with USB debugging on.

Google Play release (M4): run `Tools\Build\create_upload_key.ps1` yourself once (you type the password;
back up the key file and `Config\Android\AndroidEngine.ini`), then `package.ps1 -Platform Android -Release`.
