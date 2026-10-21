# PWM Keyboard

Generátor PWM signálu na Raspberry Pi Pico. Frekvenci a střídu nastavuješ číselníkem z maticové klávesnice 4×4, aktuální hodnoty vidíš na OLED displeji SSD1306 128×32.

---

## Co to dělá

PWM výstup je na **GPIO 15**. Za chodu měníš:

- frekvenci – 1 Hz až 62,5 MHz
- střídu – 0 až 100 %

Displej ukazuje obě hodnoty, grafický bar střidy, stav ON/OFF a číslo co zrovna píšeš. Přes USB chodí debug výpisy do terminálu.

---

## Schéma zapojení



Pull-up rezistory na SDA/SCL jsou zapnuté softwarově. Na delších vodičích nebo problematických modulech pomůžou externí 4,7 kΩ na 3,3 V.

OLED napájej z 3V3 pinu Pica. Pokud máš modul označený „3V3/5V", funguje na obou napětích, ale I²C piny Pica jsou 3,3 V – bez level shifteru nepřipojuj na 5V logiku.

Na GPIO 15 připoj osciloskop nebo analyzér. Pro ovládání motoru, reproduktoru nebo silnější LED budeš potřebovat tranzistor nebo jiný budič – přímý výstup Pica dá max. 12 mA.

---

## Ovládání

| Klávesa | Co dělá |
|---------|---------|
| `A` | přepne na zadávání frekvence |
| `B` | přepne na zadávání střidy |
| `0`–`9` | píše číslo (max 6 číslic) |
| `#` s číslem | potvrdí a nastaví hodnotu |
| `#` prázdný | spustí / zastaví PWM |
| `*` | smaže rozepsané číslo |
| `C` | předvolba 1 kHz / 50 %, spustí PWM |
| `D` | zastaví PWM |

Příklad – nastavit 1 kHz, 75 %:

1. `A` → aktivní FREQ
2. `1000` + `#` → frekvence nastavena
3. `B` → aktivní DUTY
4. `75` + `#` → střída nastavena
5. `#` (prázdný) → PWM běží

---

## Omezení

**Přesnost frekvence** – RP2040 počítá z 125 MHz hodin přes děličku (1–255) a wrap (max 65535). Výsledek je vždy nejbližší dosažitelná hodnota, ne přesně to co zadáš. U nízkých frekvencí je to zanedbatelné, u stovek kHz a výš už to může být znát.

**Proměnná střída nad 1–2 MHz** – při wrap = 1 (62,5 MHz) máš jen 0 % nebo 100 %, nic mezi tím. Praktická horní hranice pro smysluplnou střídu je někde kolem 1–2 MHz.

**Stop neresetuje pin** – `D` vypne PWM periferii, pin zůstane v posledním stavu (HIGH nebo LOW). Většinou nevadí, ale pozor u citlivých obvodů.

**Buffer** – max 6 číslic. Hodnota mimo rozsah (frekvence > 62 500 000 nebo střída > 100) se tiše zahodí.

---

## Sestavení

Potřebuješ Pico SDK, CMake ≥ 3.13 a arm-none-eabi-gcc.

```bash
export PICO_SDK_PATH=/cesta/k/pico-sdk
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Nahrání: poddrž BOOTSEL, připoj USB, zkopíruj `build/pwm_keyboard.uf2` na disk co se objeví.

---

## Časté problémy

**OLED se nerozsvítí** – displej potřebuje chvíli po připojení napájení, v kódu jsou pauzy (250 + 100 + 50 ms). Když to stejně nefunguje: zkontroluj zapojení SDA/SCL, napájení (3,3 V) a I²C adresu – výchozí je `0x3C`, některé moduly mají `0x3D` (změna v `ssd1306.h`).

**Klávesnice nereaguje** – zkontroluj pořadí vodičů. Záměna řádku a sloupce je nejčastější chyba.

**PWM nejde změřit** – displej ukazuje ON? Pokud ano, zkontroluj GPIO 15.
