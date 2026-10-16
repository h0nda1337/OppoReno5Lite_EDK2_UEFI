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
archivos privados en PC y no producen una imagen EDK2. Solo se permite el PID 1
mínimo revisado, con selección explícita y entrada directa al recovery; los
candidatos que conservan el ramdisk OEM siguen suspendidos por el incidente en
[RECOVERY_DIAGNOSTICS.md](RECOVERY_DIAGNOSTICS.md).

El receptor selecciona VID/PID e identidad de desarrollo alfanumérica. La captura
se guarda fuera de Git. Para analizar una captura propia sin operar el teléfono:

```console
python scripts/analyze_diagnostic.py --private-capture /ruta/privada/captura --output /ruta/resumen.json
```

El analizador publica únicamente campos de hardware permitidos. Contrasta el
framebuffer activo con el FDT y sus reservas; conserva las declaraciones
contradictorias sin utilizarlas como prueba de un GOP nativo.

## Framebuffer propio y bibliotecas EDK2

`OppoFrameBufferLib` implementa las cuatro operaciones BLT sobre un búfer ya
mapeado: relleno, búfer a vídeo, vídeo a búfer y copia de vídeo solapada. Valida
límites y tamaños, convierte BGR de BLT a RGBX/BGRX y conserva el padding del
stride. Permite escribir alfa opaca para el canal que declara el ioctl de Linux.
El llamador deberá validar y mapear el framebuffer y tratar las cachés. La
biblioteca no programa la pantalla ni instala por sí misma un protocolo GOP.

```bash
EDK2_HEADERS=/ruta/edk2 bash scripts/build_framebuffer_lib.sh
```

Las pruebas usan ASan/UBSan, guardas, colores conocidos, la geometría medida
del OPPO y todas las copias de rectángulos válidas de una superficie pequeña.
Después se compila el objeto AArch64. Ninguna prueba usa memoria del teléfono.

Las seis bibliotecas también se construyen con el sistema de EDK2:

```bash
EDK2_SOURCE=/ruta/edk2 bash scripts/build_edk2_libraries.sh
```

Se requiere BaseTools compilado y los submódulos que exige el núcleo EDK2. La
revisión utilizada emplea el perfil `GCC`, no el antiguo `GCC5`. El DSC de
validación solo genera archivos de biblioteca, sin FD/FV ni imagen arrancable.
La salida del build se comprueba además por su marca de finalización y por los
seis archivos producidos.

La entrada ARM64 original tiene pruebas independientes de sistema QEMU en EL1
y EL2, descritas en [NATIVE_ENTRY.md](NATIVE_ENTRY.md).

## Lector FDT propio

`OppoFdtLib` interpreta cabecera, tokens, rutas de propiedades y tabla de
reservas con lecturas byte a byte. No incorpora libfdt ni implementaciones
externas al proyecto. Valida límites, bloques separados, profundidad, orden de
propiedades y terminación; rechaza propiedades o nodos seleccionados ambiguos.

```bash
EDK2_HEADERS=/ruta/edk2 bash scripts/build_fdt_lib.sh
EDK2_HEADERS=/ruta/edk2 bash scripts/build_fdt_lib.sh /ruta/privada/runtime.dtb
```

El segundo comando opcional contrasta el archivo privado de esta captura con
los hechos públicos esperados: MT6779, banco de 6 GiB y método PSCI `smc`. Solo
lee el archivo; no ejecuta SMC ni usa el teléfono. Las pruebas sintéticas cubren
todos los truncamientos, propiedades duplicadas, reservas superpuestas y 4096
alteraciones deterministas bajo ASan/UBSan. El objeto AArch64 no tiene símbolos
externos pendientes. Esto prepara la inspección de la entrega de LK; no construye
un mapa UEFI ni valida acceso físico.

## Interpretación propia de la entrega de LK

`OppoHandoffLib` utiliza únicamente las bibliotecas propias de FDT y plataforma.
Interpreta bancos RAM, reservas fijas, initrd, tags de framebuffer y la interfaz
PSCI declarada. Distingue reservas dinámicas y nodos sin dirección resuelta; no
los presenta como memoria disponible. Una dirección física de FDT suministrada
por el llamador se comprueba contra RAM y se añade a las exclusiones.

```bash
EDK2_HEADERS=/ruta/edk2 bash scripts/build_handoff_lib.sh
EDK2_HEADERS=/ruta/edk2 bash scripts/build_handoff_lib.sh /ruta/privada/runtime.dtb
```

La prueba opcional usa la captura privada existente: un banco de 6 GiB,
17 reservas explícitas —incluido initrd—, seis dinámicas y un nodo comprobado
vacío, sin propiedades. Distingue ese marcador vacío de una reserva incompleta
con propiedades. Conserva la discrepancia entre los dos tags de pantalla. Las pruebas
con ASan/UBSan comprueban exclusiones, estados de recursos, argumentos inválidos,
512 alteraciones y que un fallo deje intacta la salida. La biblioteca no ejecuta
SMC, no crea HOBs y no programa cachés, memoria física ni pantalla.

## Primer plan de memoria propio

`OppoMemoryPlanLib` propone únicamente la imagen y un pool inicial, con exclusiones
fijas redondeadas a páginas. Los nodos `no-map` y la tabla de reservas se tipan
como `EfiReservedMemoryType`; los otros datos fijos usan `EfiBootServicesData`.
Las solicitudes dinámicas se difieren al sistema operativo según
[DTSpec 3.5.4](https://devicetree-specification.readthedocs.io/en/latest/chapter3-devicenodes.html#reserved-memory-and-uefi).
No se asignan como reservas UEFI. Los nodos no vacíos sin dirección o tamaño y
las exclusiones redondeadas superpuestas hacen fallar el plan.

```bash
EDK2_HEADERS=/ruta/edk2 bash scripts/build_memory_plan_lib.sh /ruta/privada/runtime.dtb
```

La comprobación de PC contrasta una propuesta de imagen de 8 MiB y pool de
64 MiB con la captura: resultan 19 regiones protegidas, incluidas imagen, FDT e
initrd. La dirección FDT de esta prueba es suministrada, no un x0 nativo medido.
No se reserva memoria física, no se crean HOBs ni tablas MMU y no se describe
todavía toda la RAM disponible para un sistema operativo.
