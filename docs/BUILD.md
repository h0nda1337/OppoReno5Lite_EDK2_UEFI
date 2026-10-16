# Compilación inicial de la biblioteca

Esta etapa compila **OppoPlatformLib como objeto ARM64**. No genera una imagen
UEFI completa ni un archivo para flashear. La biblioteca no accede a MMIO ni
escribe memoria física; describe el SoC y comprueba regiones propuestas de carga.

Se necesitan Linux/WSL, GCC para el PC, `aarch64-linux-gnu-gcc`, binutils ARM64,
Python 3 y una copia revisada de EDK2. La prueba inicial utilizó las cabeceras
MdePkg de EDK2 `b03a21a63e3bd001f52c527e5a57feddb53a690b`, que coincidían con el
commit upstream aunque la copia del laboratorio G20 tuviera otros cambios.

```bash
EDK2_HEADERS=/ruta/a/edk2 bash scripts/build_platform_lib.sh
```

La salida privada de compilación permanece en `.build/platform/`.

El script ejecuta comprobaciones C de límites de región, reservas, extremos,
desbordamientos y mapas no verificados. Después produce un objeto ELF64 AArch64
y ejecuta las pruebas Python de la selección del dispositivo, umbral de batería,
lectura CRLF, límites del DTB y filtrado de campos privados.

Para reproducir el inventario desde un respaldo propio, fuera del repositorio:

```console
python scripts/inspect_platform.py /ruta/al/boot.bin --output evidence/stock-platform.json
```

El extractor admite el contenedor Android DT table usado dentro del boot v2 de
referencia. No publica argumentos de arranque, identificadores ni el DTB completo.
El DTB estático no reemplaza el mapa que LK modifica y entrega al kernel.

`collect_device.py` solo realiza lecturas ADB sobre un número de serie explícito.
Exige un directorio privado fuera del repositorio y al menos 20 % de batería.
En el Android stock probado, SELinux bloquea las propiedades del árbol activo;
una captura vacía o con errores no se acepta como evidencia de hardware.

`probe_stock_ram_boot.py` es una herramienta de sesión activa para imágenes
originales con SHA-256 fijado. No debe ejecutarse en automatizaciones. El fallo
de LK observado deja esa vía pendiente de investigación; no se debe considerar
un mecanismo de arranque temporal ya disponible para el port.

## BGRT y diagnósticos de Linux

La biblioteca BGRT tiene una compilación independiente, con las mismas
cabeceras revisadas de EDK2:

```bash
EDK2_HEADERS=/ruta/a/edk2 bash scripts/build_bgrt_lib.sh
```

Compila un ejecutable de comprobación para el PC y un objeto AArch64 en
`.build/bgrt/`. Se puede pasar al script la ruta de un BMP privado para validar
también la salida del extractor local. No instala ACPI ni utiliza el teléfono.
Los detalles de integración pendiente están en [LOGO_BGRT.md](LOGO_BGRT.md).

El PID 1 de diagnóstico se construye como ejecutable estático de Linux:

```bash
bash scripts/build_capture_init.sh
```

Requiere además `qemu-aarch64`. La prueba de QEMU comprueba exclusivamente el
rechazo de una invocación que no sea PID 1; no arranca un sistema emulado. La
salida se guarda en `.build/diagnostic/`. Los preparadores de recovery generan
archivos privados en PC y no producen una imagen EDK2. Su instalación está
suspendida por el incidente descrito en
[RECOVERY_DIAGNOSTICS.md](RECOVERY_DIAGNOSTICS.md).
