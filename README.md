# 🍡 DangoPlayer Switch

**Reproductor de video e IPTV para Nintendo Switch (Horizon OS)**

Port del [DangoPlayer](https://github.com/brunochanrio/DangoPlayer) de Android a Nintendo Switch, reescrito desde cero en C++17 como aplicación homebrew nativa para el sistema operativo Horizon OS sobre el ecosistema devkitPro / libnx.

---

## ✨ Características Principales

- 🎬 **Reproducción de Video Local**:
  - Soporte para formatos contenedores estándar: `.mp4`, `.mkv`, `.avi`, `.mov`, `.wmv`, `.flv`, `.webm`, `.ts`, `.m2ts`.
  - Decodificación acelerada por hardware **NVDEC** mediante el procesador NVIDIA Tegra X1 (H.264, HEVC/H.265, VP8, VP9).
  - Exploración directa en tarjeta microSD (`sdmc:/`) y almacenamiento USB externo (`ums0:/` a `ums3:/`).

- 📺 **Cliente IPTV & Streaming en Vivo**:
  - Parser robusto de listas **M3U**, **M3U8** y **PLS**.
  - Descarga remota de listas mediante **HTTP / HTTPS** con `libcurl` y TLS.
  - Agrupación automática por categorías (`group-title`) con filtrado dinámico.
  - Reproducción fluida de flujos de streaming en vivo **HLS** (`.m3u8`) y **RTSP**.
  - Integración con el **teclado oficial en pantalla de Nintendo Switch (`swkbd`)** para ingresar URLs y realizar búsquedas de canales.

- 📝 **Subtítulos y Pistas de Audio**:
  - Carga automática de subtítulos externos `.srt` y `.ass` por coincidencia de nombre (*fuzzy matching*).
  - Selector modal interactivo para cambiar pistas de audio multilingüe y subtítulos sobre la marcha.

- 🔄 **Reanudación Automática & Historial**:
  - Registro de los últimos 50 contenidos reproducidos con punto de tiempo exacto.
  - Sistema de favoritos (marca canales con una estrella mediante el botón **ZL**).

- 🎮 **Controles Nativos y Pantalla Táctil**:
  - Soporte completo para Joy-Cons (acoplados o inalámbricos), Nintendo Switch Pro Controller y pantalla táctil capacitiva.
  - Detección dinámica de modo portátil (*Handheld*) y modo TV (*Docked*).

---

## 📊 Estado de las Fases del Proyecto

| Fase | Descripción | Estado |
|---|---|---|
| **Fase 1** | Esqueleto base, ciclo SDL2 60 FPS, iconos RomFS y CI | ✅ Completada |
| **Fase 2** | UI Core, explorador de archivos virtualizado, widgets y configuración | ✅ Completada |
| **Fase 3** | Motor de video MPV con decodificación NVDEC, HUD y subtítulos | ✅ Completada |
| **Fase 4** | Parser M3U/PLS, cliente HTTP curl, navegador IPTV y teclado Horizon | ✅ Completada |
| **Fase 5** | Historial, favoritos, almacenamiento USB (`libusbhsfs`) y docked/handheld | ✅ Completada |
| **Fase 6** | Splash screen, documentación integral, optimización y release final | ✅ Completada |

---

## 🛠️ Stack Tecnológico

| Componente | Switch (Port) | Android (Original) |
|---|---|---|
| **Lenguaje** | C++17 | Kotlin / Java |
| **Motor de video** | MPV + FFmpeg (averne/nvtegra) | Media3 / ExoPlayer |
| **Aceleración HW** | Tegra X1 NVDEC (`hwdec=auto`) | Android MediaCodec |
| **UI & Renderizado** | SDL2 + SDL2_ttf + SDL2_gfx | Material Design 3 + Leanback |
| **Red & TLS** | libcurl + mbedtls + BSD Sockets | OkHttp / HttpURLConnection |
| **Entrada de texto** | libnx `swkbd` (teclado oficial Switch) | Teclado virtual Android |
| **Filesystem** | `sdmc:/` (SD) + `ums0:/` (USB) | Scoped Storage / SAF |
| **Build System** | Makefile (devkitPro / MSYS2) | Gradle |
| **Salida binaria** | `DangoPlayerSwitch.nro` | `.apk` |

---

## 📋 Requisitos de Ejecución

- **Nintendo Switch V1** (o cualquier modelo con custom firmware Atmosphère).
- **Atmosphère** (probado en versión 23.0.0 y superiores).
- **Modo Title Override**: Ejecutar siempre manteniendo presionado el botón **R** al iniciar cualquier juego instalado para contar con los **3.2 GB de memoria RAM** disponibles.

---

## 🚀 Compilación y Despliegue

Consulta las guías detalladas en el directorio `docs/`:

- [**docs/BUILD.md**](docs/BUILD.md): Instrucciones completas para compilar localmente con devkitPro, con Docker en 1 comando (`build_docker.bat`) o automáticamente en la nube vía GitHub Actions.
- [**docs/USAGE.md**](docs/USAGE.md): Manual paso a paso de uso, configuración de listas IPTV y almacenamiento.
- [**docs/CONTROLS.md**](docs/CONTROLS.md): Guía completa de botones físicos y gestos táctiles.

---

## 🎮 Controles Rápidos

| Acción | Botón / Gesto |
|---|---|
| **Navegar en listas** | D-Pad / Stick Izquierdo o Deslizar el dedo |
| **Seleccionar / Aceptar** | A o Toque simple |
| **Atrás / Subir carpeta** | B o Toque en botón Atrás |
| **Play / Pause** | A (en reproductor) o Toque en pantalla |
| **Seek ±10s** | ← / → |
| **Seek ±60s** | ZL / ZR |
| **Volumen** | ↑ / ↓ |
| **Pistas de Audio** | X (en reproductor) |
| **Subtítulos** | Y (en reproductor) |
| **Alternar Unidad (SD/USB)** | Y (en explorador de archivos) |
| **Marcar Favorito ⭐** | ZL (en navegador IPTV) |
| **Buscar por nombre** | Y (en navegador IPTV) |
| **Salir al Homebrew Menu** | + (Plus) |

---

## 📄 Licencia y Créditos

- **brunochanrio**: Creador del reproductor original [DangoPlayer](https://github.com/brunochanrio/DangoPlayer) para Android.
- **devkitPro & switchbrew**: Herramientas de desarrollo `devkitA64`, `libnx` y `switch-portlibs`.
- **proconsule & averne**: Desarrolladores de NXMP y SwitchWave (referencia técnica para decodificación NVDEC en Horizon OS).
- **Google Fonts**: Fuente Noto Sans (SIL Open Font License).
