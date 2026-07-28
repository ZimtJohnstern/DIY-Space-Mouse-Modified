#include <TinyUSB_Mouse_and_Keyboard.h>
#include <OneButton.h>
#include <TLx493D_inc.hpp>
#include <SimpleKalmanFilter.h>
#include <Adafruit_SleepyDog.h>   // Library Manager: "Adafruit SleepyDog Library"
                                  // -> forces a board reset if loop() ever stalls
#include <string.h>               // memset/strncpy fuer das Boot-Log unten
#if defined(ESP32)
#include <esp_system.h>           // esp_reset_reason() fuer das Boot-Log unten
#endif

using namespace ifx::tlx493d;

TLx493D_A1B6 mag(Wire1, TLx493D_IIC_ADDR_A0_e);
SimpleKalmanFilter xFilter(1.0, 1.0, 0.5), yFilter(1.0, 1.0, 0.5), zFilter(1.0, 1.0, 0.5);

// Setup buttons
OneButton button1(27, true);
OneButton button2(24, true);

double xOffset = 0, yOffset = 0, zOffset = 0;
double xCurrent = 0, yCurrent = 0, zCurrent = 0;

int calSamples = 300;
float sensitivity = 0.5f; // higher = more mouse travel per mm of real stick deflection
                           // lowered from 3 - felt too fast/twitchy on rotations and pan,
                           // start here and nudge up in 0.1 steps if it now feels sluggish
int magRange = 3;        // currently unused - reserved if you wire up sensor range config later
int outRange = 127;      // theoretical max allowed in a mouse HID report
int moveLimit = 25;       // practical per-frame movement cap (was hardcoded as 25 before)

// --- Dead-zone / hysteresis tuning ---------------------------------------
// Two thresholds instead of one: ENGAGE must be crossed to *start* moving,
// DISENGAGE must be crossed to *stop*. The gap between them is the new dead
// zone. This is what fixes "2-3mm already moves the mouse": small nudges
// near the old single threshold no longer cause rapid press/release
// flicker. Raise xyEngageThreshold further if you want the dead zone to
// extend further out toward 7mm of real stick travel - tune by watching
// xCurrent/yCurrent with debugSerial = true below.
float xyEngageThreshold    = 0.14f;
float xyDisengageThreshold = 0.06f;

// Same idea for Orbit <-> Pan. zEngageThreshold is how far you have to push
// Z down to commit to Pan. Raised significantly from 0.18: the RawZ log
// showed plain XY tilting (no press at all) already produces Z up to ~0.22
// on its own (e.g. "vorn" alone averaged Z=0.22, "rechts" Z=0.15) - that's
// why normal Orbit movement kept tripping into Pan. A real full press reads
// Z≈3.0, so there's a huge margin to raise the threshold into without
// making an intentional press feel any harder. zDisengageThreshold raised
// to match, keeping the same relative gap for hysteresis (no flapping at
// the boundary).
float zEngageThreshold     = 0.80f;
float zDisengageThreshold  = 0.30f;

bool isOrbit = false;
bool isEngaged = false;

// --- Axis mirror correction ------------------------------------------------
// The RawAngle log shows this was never a rotation offset. A least-squares
// fit of the logged Raw vectors against the actually-wanted push directions
// (vorn/hinten/links/rechts) comes out with determinant -1 - that's a
// mirrored axis, not a rotated one. Concretely: pushing the stick LEFT gave
// a POSITIVE RawX, pushing RIGHT gave a NEGATIVE RawX - exactly backwards.
// RawY already had the correct sign for vorn/hinten in the log. No rotation
// angle (45°, -45°, anything) can ever fix a mirrored axis, since rotating
// preserves handedness and a mirror doesn't - that's why flipping the sign
// of axisRotationDeg never actually fixed the feel. Most likely cause: the
// TLx493D's Bx reading is inverted relative to how the stick is mounted.
bool mirrorX = true;   // X was backwards in the log - flip it
bool mirrorY = false;  // Y already matched vorn=unten / hinten=oben

const bool debugSerial = false; // Normalbetrieb. Auf true setzen, um RawAngle/CorrX/CorrY
                                 // im Serial Monitor zu beobachten oder Sensor-Fail-Meldungen
                                 // zu sehen (siehe consecutiveBadReads unten).

// --- Boot-Zaehler / Reset-Grund-Log (Standby/Wake-Diagnose) --------------
// Zweck: Der vorherige Retry-Patch fuer mag.begin() war eine unbestaetigte
// Hypothese und hat das "nach Mac-Standby reagiert die Maus nicht mehr"
// Problem nicht geloest. Bevor wir weiter am Firmware-Code raten, brauchen
// wir handfeste Daten: Fand beim Einschlafen/Aufwachen ueberhaupt ein
// Watchdog-Reset statt? Falls ja, ist das Problem auf der Platine und wir
// koennen gezielt weiter debuggen. Falls bootCount nach einem Freeze
// unveraendert bleibt, hat die Platine durchgehend weitergelaufen und das
// Problem liegt auf USB-Host-Seite (Suspend/Resume-Reenumeration), nicht
// in dieser Firmware.
//
// SRAM-Inhalt ueberlebt einen Watchdog-/Software-Reset (nicht aber einen
// echten Stromverlust durch Abstecken) - der Trick mit einer .noinit
// Sektion + Magic-Value nutzt genau das: bleibt der Magic-Value nach einem
// Reset erhalten, war es ein Warmstart, kein Kaltstart. Auf ESP32 ist
// RTC_DATA_ATTR die robustere Variante fuer denselben Zweck.
#if defined(ESP32)
  #define BOOT_PERSIST_ATTR RTC_DATA_ATTR
#else
  #define BOOT_PERSIST_ATTR __attribute__((section(".noinit")))
#endif

#define BOOT_MAGIC     0xB007C0DEUL
#define BOOT_LOG_SIZE  10

BOOT_PERSIST_ATTR uint32_t bootMagic;
BOOT_PERSIST_ATTR uint32_t bootCount;      // Gesamtzahl Boots seit letztem echten Stromverlust
BOOT_PERSIST_ATTR uint8_t  bootLogIndex;   // naechster Schreib-Index (wrapt)
BOOT_PERSIST_ATTR char     bootLog[BOOT_LOG_SIZE][4]; // je Boot ein 3-Zeichen-Reset-Grund-Code

// Liest den Hardware-Reset-Grund aus - Register/API ist chip-spezifisch.
// Deckt die ueblichen Adafruit_SleepyDog-Zielplattformen ab. Bei anderen
// Chips faellt es auf "???" zurueck statt nicht zu kompilieren.
const char* readResetReasonCode()
{
#if defined(ARDUINO_ARCH_RP2040)
  // Adafruit QT Py RP2040 laeuft auf dem earlephilhower/arduino-pico Core.
  // Echte Enum-Werte (RP2040::resetReason_t, verifiziert - meine erste
  // Version hier hatte falsch geratene Namen, die gar nicht kompiliert
  // haetten): UNKNOWN_RESET, PWRON_RESET, RUN_PIN_RESET, SOFT_RESET,
  // WDT_RESET, DEBUG_RESET, GLITCH_RESET, BROWNOUT_RESET.
  switch (rp2040.getResetReason())
  {
    case RP2040::WDT_RESET:      return "WDT";
    case RP2040::PWRON_RESET:    return "PWR";
    case RP2040::RUN_PIN_RESET:  return "EXT";
    case RP2040::SOFT_RESET:     return "SW ";
    case RP2040::DEBUG_RESET:    return "DBG";
    case RP2040::BROWNOUT_RESET: return "BOD";
    case RP2040::GLITCH_RESET:   return "GLI";
    default:                     return "???";
  }
#elif defined(__SAMD51__)
  uint8_t rcause = RSTC->RCAUSE.reg;
  if (rcause & RSTC_RCAUSE_WDT)   return "WDT";
  if (rcause & RSTC_RCAUSE_POR)   return "PWR";
  if (rcause & RSTC_RCAUSE_EXT)   return "EXT";
  if (rcause & RSTC_RCAUSE_SYST)  return "SW ";
  if (rcause & (RSTC_RCAUSE_BOD12 | RSTC_RCAUSE_BOD33)) return "BOD";
  return "???";
#elif defined(__SAMD21__)
  uint8_t rcause = PM->RCAUSE.reg;
  if (rcause & PM_RCAUSE_WDT)   return "WDT";
  if (rcause & PM_RCAUSE_POR)   return "PWR";
  if (rcause & PM_RCAUSE_EXT)   return "EXT";
  if (rcause & PM_RCAUSE_SYST)  return "SW ";
  if (rcause & (PM_RCAUSE_BOD33 | PM_RCAUSE_BOD12)) return "BOD";
  return "???";
#elif defined(NRF52840_XXAA) || defined(NRF52_SERIES)
  uint32_t reas = NRF_POWER->RESETREAS;
  NRF_POWER->RESETREAS = 0xFFFFFFFF; // Flags loeschen, sonst kumulieren sie ueber Boots hinweg
  if (reas & POWER_RESETREAS_DOG_Msk)      return "WDT";
  if (reas & POWER_RESETREAS_RESETPIN_Msk) return "EXT";
  if (reas & POWER_RESETREAS_SREQ_Msk)     return "SW ";
  if (reas == 0)                            return "PWR";
  return "???";
#elif defined(ESP32)
  switch (esp_reset_reason())
  {
    case ESP_RST_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT: return "WDT";
    case ESP_RST_POWERON: return "PWR";
    case ESP_RST_SW:      return "SW ";
    case ESP_RST_PANIC:   return "PNC";
    default:               return "???";
  }
#else
  return "???"; // Chip nicht erkannt - hier ggf. die passende Reset-Cause-API ergaenzen
#endif
}

// Traegt den aktuellen Boot in den Ringpuffer ein und gibt ueber Serial
// die komplette bisherige Historie aus (aelteste zuerst). Muss aufgerufen
// werden, NACHDEM Serial.begin() (falls debugSerial) bereits lief.
void logBootAndPrintHistory()
{
  const char* reasonCode = readResetReasonCode();
  bool coldBoot = (bootMagic != BOOT_MAGIC);

  if (coldBoot)
  {
    bootMagic = BOOT_MAGIC;
    bootCount = 0;
    bootLogIndex = 0;
    memset(bootLog, 0, sizeof(bootLog));
  }

  bootCount++;
  strncpy(bootLog[bootLogIndex % BOOT_LOG_SIZE], reasonCode, 3);
  bootLog[bootLogIndex % BOOT_LOG_SIZE][3] = '\0';
  bootLogIndex++;

  if (!debugSerial) return;

  Serial.println("=== Boot-Log ===");
  Serial.print(coldBoot ? "Kaltstart (Stromverlust erkannt)" : "Warmstart (Reset, kein Stromverlust)");
  Serial.println();
  Serial.print("Boots seit letztem Stromverlust: "); Serial.println(bootCount);
  Serial.print("Aktueller Reset-Grund: "); Serial.println(reasonCode);
  Serial.println("Historie (aelteste zuerst, WDT = Watchdog-Reset = das, wonach wir suchen):");

  uint8_t entries = (bootCount < BOOT_LOG_SIZE) ? bootCount : BOOT_LOG_SIZE;
  uint8_t startOffset = (bootCount < BOOT_LOG_SIZE) ? 0 : bootLogIndex;
  for (uint8_t i = 0; i < entries; i++)
  {
    uint8_t idx = (startOffset + i) % BOOT_LOG_SIZE;
    Serial.print("  #"); Serial.print(i + 1); Serial.print(": "); Serial.println(bootLog[idx]);
  }
  Serial.println("================");
}

void setup()
{
  // If loop() ever stalls (sensor I2C lockup, USB CDC backpressure from
  // Serial prints with no monitor attached, etc.) the board force-resets
  // itself instead of needing a manual unplug/replug.
  Watchdog.enable(2000); // 2s timeout

  button1.attachClick(goHome);
  button1.attachLongPressStop(goHome);

  button2.attachClick(fitToScreen);
  button2.attachLongPressStop(fitToScreen);

  Mouse.begin();
  Keyboard.begin();

  if (debugSerial) {
    Serial.begin(115200);
  }

  logBootAndPrintHistory();

  Wire1.begin();
  delay(100);
  if (!mag.begin())
  {
    // Vorher stand hier eine Endlosschleife, die den Watchdog fuer immer
    // gefuettert hat ("keep feeding the watchdog instead of resetting mid
    // error-loop") und NIE erneut mag.begin() versucht hat. USB-HID
    // (Mouse.begin()/Keyboard.begin()) laeuft aber bereits davor - d.h.
    // schlaegt mag.begin() hier einmal fehl, blieb die Platine fuer immer
    // haengen, ohne dass loop() je erreicht wurde: keine Bewegung mehr,
    // kein Selbstheilungsmechanismus, nur manuelles Aus-/Einstecken half.
    // Das passt zum "nach Standby/Wakeup reagiert die Maus nicht mehr"
    // Symptom: waehrend des Mac-Wake-Uebergangs ist VBUS/I2C oft kurz
    // instabil - trifft das genau den Moment eines Reboots, schlug
    // mag.begin() hier fehl und die Platine haengte sich fuer immer auf,
    // obwohl der Sensor Sekundenbruchteile spaeter wieder ansprechbar
    // gewesen waere.
    //
    // Jetzt: bis zu initRetryLimit Versuche mit kurzer Pause (deckt eine
    // kurze Power-/Bus-Instabilitaet beim Aufwachen ab). Bleibt der Sensor
    // danach wirklich weg, wird der Watchdog bewusst NICHT mehr gefuettert
    // - der bestehende 2s-Hardware-Watchdog resettet die Platine dann hart
    // (inkl. neuer USB-Reenumeration) statt fuer immer zu haengen, und der
    // ganze Ablauf (inkl. Retry-Fenster) beginnt beim naechsten Boot von
    // vorne.
    if (debugSerial) Serial.println("Sensor Init Fehler - versuche erneut...");

    const int initRetryLimit = 20; // 20 x 100ms ~= 2s Kulanz fuer eine kurze VBUS/I2C-Instabilitaet
    int initRetries = 0;
    bool initOk = false;

    while (initRetries < initRetryLimit)
    {
      Watchdog.reset();
      delay(100);
      initRetries++;
      if (mag.begin()) { initOk = true; break; }
    }

    if (!initOk)
    {
      if (debugSerial) Serial.println("Sensor Init dauerhaft fehlgeschlagen - erzwinge Watchdog-Reset");
      while (1) { delay(10); } // Watchdog absichtlich NICHT fuettern -> harter Reset in <=2s
    }

    if (debugSerial) Serial.println("Sensor Init nach Retry erfolgreich");
  }

  // crude offset calibration on first boot
  for (int i = 1; i <= calSamples; i++)
  {
    delay(10);
    Watchdog.reset();

    double x, y, z, t;
    if (!mag.getMagneticFieldAndTemperature(&x, &y, &z, &t)) continue; // skip a bad sample instead of baking a glitch into the offset

    xOffset += x;
    yOffset += y;
    zOffset += z;

    if (debugSerial) Serial.print(".");
  }

  xOffset = xOffset / calSamples;
  yOffset = yOffset / calSamples;
  zOffset = zOffset / calSamples;

  if (debugSerial)
  {
    Serial.println();
    Serial.println(xOffset);
    Serial.println(yOffset);
    Serial.println(zOffset);
    Serial.println("Kalibrierung fertig");
  }
}

// If the sensor read keeps failing for this many consecutive loops (~1s at
// the 10ms loop delay below), we stop feeding the watchdog on purpose and
// let the existing 2s hardware watchdog reboot the board cleanly. A single
// transient I2C glitch still passes through fine - the counter resets on
// every good read, see loop() below.
int consecutiveBadReads = 0;
const int maxConsecutiveBadReads = 100;

// --- Stale/stuck sensor detection ----------------------------------------
// Root cause confirmed from mein_log.txt (2026-07-02 capture): at 11:40:30
// RawX/RawY/Z jumped to 0.21/-0.02/-0.12 and then stayed BIT-IDENTICAL for
// the remaining ~77.700 log lines (~14 minutes) until the capture ended.
// In that whole window mag.getMagneticFieldAndTemperature() kept returning
// true - zero "Sensor read fail" lines were logged - so consecutiveBadReads
// above never incremented and the watchdog recovery path was never armed.
// During 28 minutes of genuine normal operation before that, the value
// changed on every single sample (sensor noise, even through the Kalman
// filter) - it never repeated bit-for-bit for more than a couple of frames.
// So an exact-match run this long is not something normal operation
// produces; it is the sensor signature of an I2C latch-up that returns the
// last successfully latched register value without ever raising a
// communication error. This is what caused the reported drift-to-the-left
// and the total mouse lockup: isEngaged latched true at RawX=0.21 (above
// xyEngageThreshold) and stayed true forever, so the same negative xMove
// (mirrorX=true) got resent every 10ms loop.
double lastRawX = 0, lastRawY = 0, lastRawZ = 0;
bool haveLastRaw = false;
int staleReadCount = 0;
// ~0.5-1s of bit-identical readings at the ~10-15ms actual loop rate.
// Chosen well above anything normal sensor noise produces (see note above)
// but short enough that recovery still kicks in fast if it happens again.
const int maxStaleReads = 50;

void loop()
{
  // Withhold the watchdog feed once the sensor has been wedged for a while
  // instead of resetting it unconditionally every loop. Buttons/USB keep
  // working through a wedged I2C bus (this is the "Maus reagiert nicht
  // mehr, Buttons gehen noch" symptom) because they don't depend on it -
  // only a real reboot reliably clears a stuck I2C peripheral.
  if (consecutiveBadReads < maxConsecutiveBadReads)
  {
    Watchdog.reset();
  }
  else if (debugSerial)
  {
    Serial.println("Sensor wedged - warte auf Watchdog-Reset...");
  }

  // keep watching the push buttons
  button1.tick();
  button2.tick();

  double x, y, z, t;
  if (!mag.getMagneticFieldAndTemperature(&x, &y, &z, &t))
  {
    consecutiveBadReads++;
    if (debugSerial) {
      Serial.print("Sensor read fail (#"); Serial.print(consecutiveBadReads); Serial.println(")");
    }

    // Bad read: don't act on stale/garbage data, and don't leave the
    // mouse/keyboard stuck down from a previous good frame.
    Mouse.release(MOUSE_MIDDLE);
    Keyboard.releaseAll();
    isOrbit = false;
    isEngaged = false;
    delay(10);
    return;
  }
  // WICHTIG: consecutiveBadReads wird hier NICHT mehr zurueckgesetzt.
  // Das war der eigentliche Bug in der vorigen Fassung dieses Patches:
  // getMagneticFieldAndTemperature() liefert bei eingefrorenen Werten
  // weiterhin true, also lief dieser Reset JEDEN Loop und hat das
  // consecutiveBadReads++ im Stale-Zweig unten sofort wieder auf 0
  // gesetzt, bevor es je die maxConsecutiveBadReads-Schwelle erreichen
  // konnte - der Watchdog wurde dadurch fuer immer weitergefuettert.
  // Bestaetigt durch 1783112688345_spacemouse.txt: 259.806 "Sensor
  // stale"-Zeilen ueber 34 Stunden, aber kein einziger Boot-Log-Eintrag,
  // d.h. kein einziger Reset in dieser ganzen Zeit. Der Reset gehoert
  // jetzt in den "frischer Wert"-Zweig unten, wo er hingehoert.

  // Catch the "success but frozen" failure mode that a false return value
  // can never catch (see note above the maxStaleReads declaration).
  if (haveLastRaw && x == lastRawX && y == lastRawY && z == lastRawZ)
  {
    staleReadCount++;
  }
  else
  {
    staleReadCount = 0;
    consecutiveBadReads = 0; // nur bei einem tatsaechlich frischen Wert zuruecksetzen
  }
  lastRawX = x;
  lastRawY = y;
  lastRawZ = z;
  haveLastRaw = true;

  if (staleReadCount >= maxStaleReads)
  {
    // Same recovery path as a hard read failure: don't act on the frozen
    // value, release anything that might be held down, and feed the exact
    // same counter the watchdog logic above already watches - so a stuck
    // sensor now reliably forces a reboot instead of holding the mouse
    // pinned against the screen edge indefinitely.
    consecutiveBadReads++;
    if (debugSerial) {
      Serial.print("Sensor stale (#"); Serial.print(staleReadCount);
      Serial.println(") - Wert eingefroren, Watchdog-Recovery aktiv");
    }

    Mouse.release(MOUSE_MIDDLE);
    Keyboard.releaseAll();
    isOrbit = false;
    isEngaged = false;
    delay(10);
    return;
  }

  xCurrent = xFilter.updateEstimate(x - xOffset);
  yCurrent = yFilter.updateEstimate(y - yOffset);
  zCurrent = zFilter.updateEstimate(z - zOffset);

  // Mirror the affected axis (see comment above) - a flip, not a rotation.
  // No separate "fusion" swap needed anymore: the fit showed mouse-X maps
  // directly from (mirrored) sensor-X and mouse-Y directly from sensor-Y.
  float xCorrected = mirrorX ? -xCurrent : xCurrent;
  float yCorrected = mirrorY ? -yCurrent : yCurrent;

  float xFusion = xCorrected;
  float yFusion = yCorrected;

  if (debugSerial)
  {
    // RawAngle is independent of mirrorX/mirrorY below - this is the number
    // to use if you ever need to re-derive the correction from scratch.
    float rawAngleDeg = atan2((float)yCurrent, (float)xCurrent) * 180.0f / PI;
    Serial.print("RawX="); Serial.print(xCurrent);
    Serial.print(" RawY="); Serial.print(yCurrent);
    Serial.print(" RawAngle="); Serial.print(rawAngleDeg);
    Serial.print(" | CorrX="); Serial.print(xCorrected);
    Serial.print(" CorrY="); Serial.print(yCorrected);
    Serial.print(" Z="); Serial.println(zCurrent);
  }

  float magnitude = max(abs(xCorrected), abs(yCorrected));

  // Hysteresis: must clear the higher threshold to start moving, must drop
  // below the lower one to stop. No more flicker right at the boundary.
  if (!isEngaged && magnitude > xyEngageThreshold) isEngaged = true;
  else if (isEngaged && magnitude < xyDisengageThreshold) isEngaged = false;

  if (isEngaged)
  {
    float moveScale = 20.0f * sensitivity; // default sensitivity=1.2 -> 24 (was 60 before)

    int xMove = constrain((int)(xFusion * moveScale), -moveLimit, moveLimit);
    int yMove = constrain((int)(yFusion * moveScale), -moveLimit, moveLimit);

    // Orbit <-> Pan with its own hysteresis (see thresholds above).
    if (isOrbit)
    {
      if (abs(zCurrent) > zEngageThreshold)
      {
        isOrbit = false;
        Keyboard.release(KEY_LEFT_SHIFT);
      }
    }
    else
    {
      if (abs(zCurrent) < zDisengageThreshold)
      {
        isOrbit = true;
        Keyboard.press(KEY_LEFT_SHIFT);
      }
    }

    Mouse.press(MOUSE_MIDDLE); // no-op if already pressed
    if (xMove != 0 || yMove != 0) Mouse.move(xMove, yMove, 0);
  }
  else
  {
    // release the mouse and keyboard if within the center threshold
    Mouse.release(MOUSE_MIDDLE);
    Keyboard.releaseAll();
    isOrbit = false;
  }

  delay(10);
}

// go to home view in Fusion 360 (CMD + SHIFT + H, custom Add-in shortcut)
void goHome()
{
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.write('h');

  delay(10);
  Keyboard.releaseAll();
  if (debugSerial) Serial.println("pressed home");
}

// fit to view by double-clicking the middle mouse button
void fitToScreen()
{
  Mouse.press(MOUSE_MIDDLE);
  Mouse.release(MOUSE_MIDDLE);
  Mouse.press(MOUSE_MIDDLE);
  Mouse.release(MOUSE_MIDDLE);

  if (debugSerial) Serial.println("pressed fit");
}

// --- Tuning notes ---------------------------------------------------------
// - Set debugSerial = true and watch X/Y/Z while moving the stick by known
//   amounts (a ruler against the housing works) to map mm -> filtered value,
//   then set xyEngageThreshold/xyDisengageThreshold to match your target
//   3-7mm dead zone.
// - If Orbit/Pan still flickers after this, widen the gap between
//   zEngageThreshold and zDisengageThreshold further rather than moving
//   either one in isolation.
// - Leave debugSerial = false for normal use - continuous Serial.print()
//   with no monitor attached was the most likely cause of the
//   "needs a reboot after a while" symptom (USB CDC write backpressure
//   blocking the loop). The watchdog above is a backstop for any other
//   cause of a stalled loop.
// - staleReadCount / maxStaleReads (added after the 2026-07-02 log
//   analysis): catches a sensor that keeps returning true but with a
//   frozen, un-updating value (I2C latch-up without a comms error). This
//   is what caused the left-drift/total-lockup - the sensor jumped to
//   RawX=0.21 at 11:40:30 and never changed again for the rest of that
//   capture, which is above xyEngageThreshold and so latched isEngaged
//   permanently true. If this fires often in practice, check the I2C
//   wiring/pull-ups on Wire1 for marginal signal integrity rather than
//   raising maxStaleReads - raising it just delays the same recovery.
