# Percusiones Interactivas

> Sistema IoT para visuales audioreactivos en tiempo real durante la interpretación de **Rebonds B** de Iannis Xenakis.

Sistema interdisciplinario que captura las vibraciones de 10 instrumentos de percusión mediante sensores piezoeléctricos, procesa los datos en un ESP32 y genera parámetros visuales en tiempo real para TouchDesigner.

---

## Archivos del repositorio

| Archivo | Descripción |
|---|---|
| `main.c` | Firmware principal del ESP32 : detección de golpes y transmisión serial |
| `diagnostico_piezos.c` | Firmware de diagnóstico : lectura ADC cruda sin clasificaciones |
| `touchdesigner_piezos.py` | Script Python para el Serial DAT de TouchDesigner |

---

## Instrumentación

**Rebonds B** utiliza 10 instrumentos organizados en 3 capas rítmicas:

```
Capa lenta   →  Bombo                           (canal 0)
Capa media   →  Tom · Tumba · Bongo 1 · Bongo 2 (canales 1–4)
Capa rápida  →  Woodblock 1–5                   (canales 5–9)
```

---

## Hardware requerido

- ESP32 DevKit (240MHz, ADC 12-bit)
- Multiplexor analógico CD4067
- Discos piezoeléctricos 15mm × 9 y 35mm × 1 (tumba)
- Resistencias 1MΩ × 10
- Diodos Zener 3.3V / 0.5W × 10
- Capacitores 1nF × 10
- Cable de micrófono blindado ~1m por canal

### Circuito de protección por canal

```
Piezo ──┬── R 1MΩ ──── GND
        ├── Zener 3.3V (cátodo al nodo, ánodo a GND)
        ├── Cap 1nF (a GND)
        └── Pin MUX (canal correspondiente)
```

### Pines del MUX CD4067

| Pin MUX | GPIO ESP32 |
|---|---|
| S0 | GPIO 32 |
| S1 | GPIO 33 |
| S2 | GPIO 25 |
| S3 | GPIO 26 |
| SIG (salida) | GPIO 34 (ADC1_CH6) |

---

## Arquitectura del sistema

```
┌─────────────────────────────────┐
│  Capa 1 — Física / Sensorial    │
│  Piezos + circuito protección   │
│  + Multiplexor CD4067           │
└────────────────┬────────────────┘
                 │ voltaje analógico 0–3.3V
┌────────────────▼───────────────────┐
│  Capa 2 — Adquisición (ESP32)      │
│  ADC 12-bit · ESP-IDF v5.x         │
│  Salida: canal:pico_adc\n          │
└────────────────┬───────────────────┘
                 │ UART/USB Serial 115200 baud
┌────────────────▼───────────────────┐
│  Capa 3 — Procesamiento y visual   │
│  Python + TouchDesigner            │
│  Lógica musical · Acumuladores     │
│  Visuales narrativos en tiempo real│
└────────────────────────────────────┘
```

**Protocolo serial:** `canal:pico_adc\n`
```
3:1450    →  Bongo 1, pico ADC 1450
0:2100    →  Bombo, pico ADC 2100
```

---

## `main.c` — Firmware principal

Detecta golpes en los 10 canales y transmite el valor ADC máximo del pico por serial. Sin clasificación de intensidad — toda la lógica musical vive en Python/TouchDesigner.

### Parámetros de calibración

| Canal | Instrumento | umbral | ventana_peak | cooldown |
|---|---|---|---|---|
| 0 | Bombo | 200 | 35ms | 100ms |
| 1 | Tom | 200 | 12ms | 30ms |
| 2 | Tumba | 150 | 12ms | 22ms |
| 3 | Bongo 1 | 80 | 5ms | 10ms |
| 4 | Bongo 2 | 80 | 5ms | 10ms |
| 5–9 | WB 1–5 | 120 | 6ms | 12ms |

> **Nota:** El cooldown de los bongos (10ms) permite capturar apoyaduras dobles (drags) que caracterizan el patrón rítmico de Rebonds B.

### Flash

```bash
idf.py fullclean
idf.py build
idf.py -p COM5 flash monitor    # Windows
idf.py -p /dev/ttyUSB0 flash monitor  # Linux/Mac
```

---

## `diagnostico_piezos.c` — Firmware de diagnóstico

Captura la curva completa de voltaje de los 10 canales durante 150ms tras un trigger. Imprime los valores ADC crudos por serial en formato CSV — sin umbrales, sin detección de golpes, sin clasificaciones.

**Úsalo para:**
- Conocer el rango ADC real de cada instrumento
- Medir el crosstalk entre canales
- Calcular el capacitor correcto antes de comprarlo
- Verificar que el cableado del MUX es correcto

### Salida serial

```
=== INICIO_CAPTURA canal=0 nombre=Bombo muestras=300 ===
PICOS: Bombo=2287@2500µs Tom=89@3100µs Tumba=1592@2800µs
t_us,Bombo,Tom,Tumba,Bongo1,Bongo2,WB1,WB2,WB3,WB4,WB5
0,12,8,11,9,10,7,8,9,8,10
500,1450,45,820,38,42,12,11,13,10,12
...
=== FIN_CAPTURA ===
```

### Flash

Reemplaza `main.c` con `diagnostico_piezos.c` y flashea normalmente:

```bash
idf.py fullclean && idf.py build && idf.py -p COM5 flash monitor
```

> **Importante:** Después de la calibración, vuelve a flashear `main.c` para el uso en producción.

---

## `touchdesigner_piezos.py` — Script Python para TouchDesigner

Pega el contenido de este archivo en el **Callbacks** del Serial DAT de TouchDesigner.

### Configuración del Serial DAT

| Parámetro | Valor |
|---|---|
| Port | COM5 (o el tuyo) |
| Baud Rate | 115200 |
| Row/Callback Format | One Per Line |
| Active | On |

### Variables generadas en `tabla_piezos`

El script escribe automáticamente estas variables en un Table DAT llamado `tabla_piezos`:

**Bombo y Tom** — eventos con timeout de 400ms:
```
bombo_pico          ADC crudo del último golpe
bombo_evento_1      acumulador nivel mp activo (0/1)
bombo_evento_2      acumulador nivel mf activo (0/1)
bombo_evento_fuerte disparo nivel f/fff, pulso 80ms (0/1)
bombo_activo        pulso 80ms post-golpe (0/1)
```

**Tumba** — valor continuo:
```
tumba_pico          ADC crudo
tumba_continuo      normalizado 0.0–1.0
tumba_activo        pulso 80ms (0/1)
```

**Bongos** — acumuladores de 250ms:
```
bongo1_pico         ADC crudo
bongo1_energia      promedio normalizado en ventana (0.0–1.0)
bongo1_densidad     golpes/segundo en ventana
bongo1_acento       1 si algún golpe superó umbral de acento (0/1)
bongo1_activo       pulso 80ms (0/1)
```

**Woodblocks** — niveles discretos:
```
wb1_pico            ADC crudo
wb1_nivel           1–4 según intensidad
wb1_activo          pulso 80ms (0/1)
```

### Topografía de nodos en TouchDesigner

```
Serial DAT [serial1]
    └── Script Python (onReceiveLine)
            └── Table DAT [tabla_piezos]
                    ├── Select DAT → DAT to CHOP → parámetro visual
                    ├── Select DAT → DAT to CHOP → parámetro visual
                    └── ...
Execute DAT [Frame Start] → resetea _activo y detecta silencio
```

### Parámetros ajustables

```python
VENTANA_ENERGIA_MS  = 250   # duración ventana bongo (ms) — semicorchea a 60 BPM
UMBRAL_ACENTO_BONGO = 900   # ADC mínimo para acento de Xenakis
PICO_MAX_BONGO      = 2500  # techo para normalización de energía
BLOQUEO_B2_MS       = 30    # filtro crosstalk bongo1 → bongo2 (ms)
DURACION_ACTIVO_MS  = 80    # duración del pulso _activo (ms)
```

---



<div align="center">
  <sub>IoT · Música contemporánea · Percusión · TouchDesigner</sub>
</div>
