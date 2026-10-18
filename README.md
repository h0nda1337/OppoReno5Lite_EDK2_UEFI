# OPPO Reno5 Lite — EDK2 UEFI

Port experimental de EDK2 para **OPPO Reno5 Lite CPH2205 / OP4F43L1**, proyecto
20295, MediaTek MT6779 (Helio P95). El objetivo es conseguir una UEFI Shell nativa
y después ofrecer una opción **UEFI Shell** en el recovery que muestra Fastbootd.

**Estado, 18 de octubre de 2026: UEFI Interactive Shell v2.2 llegó al prompt
`Shell>` en el OPPO, con entrada ARM64, SEC, GOP y BDS propios y núcleo/Shell
de la dependencia base EDK2, sin BootShim ni kernel Linux en el candidato.
El séptimo intento quedó documentado mediante fotografía del propietario.
Volvió automáticamente a Android; ADB verificó arranque completo y después
se restauró recovery original, con batería al 100 %. La entrada de usuario,
ejecución de comandos y la opción del menú Fastbootd siguen pendientes.**

El lector microSD propio ya abrió `BOOTAA64.EFI` desde la tarjeta en EDK2 y leyó
un prefijo ARM64 UEFI válido de 4096 bytes, tras habilitar VMC/VMCH a 3,0 V.
La foto del intento 16 y el regreso a Android están documentados en
[docs/MICROSD.md](docs/MICROSD.md#encendido-vmcvmch-16-lectura-física-de-la-microsd-conseguida).
El intento 21 hizo legible la pantalla de recuperación de Windows Boot Manager
al ajustar la opacidad antes del apagado. Muestra `0xc0000017`: memoria insuficiente
para crear el ramdisk. EDK2 aún entrega un pool inicial de solo 64 MiB, con
52 MiB convencionales libres antes de entrar al cargador. El plan opcional de
RAM adicional respeta las reservas del handoff; el intento 25 confirmó
2100 MiB convencionales libres antes de entrar al cargador y mostró un nuevo
error de Windows, `0xc0000225`. WinPE y el kernel siguen sin arranque verificado.
Las dos fotos, el diagnóstico y la restauración
del recovery original están documentados en [el intento 21](docs/MICROSD.md#diagnóstico-de-opacidad-21).

El intento 24 verificó lectura SD de cuatro bits a 13 MHz: 16 MiB del WIM a
1141 KiB/s, con huella coincidente con Android. Esa variante no lanzó Windows.
La foto, los límites de la medición y la restauración del recovery están en
[el benchmark físico](docs/MICROSD.md#lectura-sd-de-cuatro-bits-24-verificada-en-el-teléfono).
El [intento combinado 25](docs/MICROSD.md#combinación-de-lectura-rápida-y-ram-adicional-25-nuevo-error-de-windows)
documenta el nuevo error y el regreso a Android con recovery original restaurado.
El [intento 26 con contadores](docs/MICROSD.md#reset-de-sd-y-contadores-del-cargador-26-resultado-físico)
mantiene `0xc0000225`: contabiliza 540 MiB de lecturas correctas, sin errores
de Block I/O ni resets registrados durante el cargador. Ese total incluye
posibles relecturas y no prueba la integridad del WIM completo.

El [observador ACPI/GICv3 del intento 28](docs/ACPI.md#resultado-físico-del-intento-28)
identificó físicamente las ocho afinidades CPU y sus redistributors, sin escribir
registros GIC ni instalar tablas. Android volvió a arrancar y el recovery original
quedó restaurado, con batería al 100 %. El generador ACPI propio sigue comprobado
en PC; su instalación y el arranque de WinPE están pendientes.

Una prueba anterior de recovery terminó en un retorno repetido al recovery, con
fallos al abrir las claves de cifrado de datos. El recovery original se restauró
y su lectura completa se verificó. Android volvió a arrancar tras un
restablecimiento autorizado por el propietario; se verificaron arranque completo,
28 % de batería cargando y bootloader desbloqueado. Los diagnósticos que retienen
el ramdisk del fabricante siguen suspendidos. El diagnóstico mínimo posterior
usa un PID 1 propio y no inicia esos servicios: capturó el FDT, iomem y framebuffer,
volvió a Android y se restauró de nuevo el recovery original, con 46 % de batería.
Véase
[docs/RECOVERY_DIAGNOSTICS.md](docs/RECOVERY_DIAGNOSTICS.md).

| Componente | Estado comprobado |
| --- | --- |
| Modelo y plataforma | CPH2205 / OP4F43L1 / proyecto 20295 / MT6779 |
| Android de referencia | Android 13, ColorOS F.72, México libre NV7B |
| Configuración del recovery original | ARM64; `CONFIG_KEXEC` desactivado |
| Firmware integrado MT6779 | Shell v2.2 inicializada hasta el prompt; regreso automático a Android y restauración de recovery original comprobados |
| Entrada ARM64 propia | QEMU EL1/EL2; franja verde de SEC observada en el OPPO y regreso a Android; registros de entrada aún sin medir |
| GIC, UART y watchdog | Direcciones extraídas del DTB original; GICv3 y ocho asociaciones CPU/GICR observados físicamente en el intento 28; entrega de interrupciones pendiente |
| ACPI | Generador original comprobado en PC y perfil GICv3 observado; tablas aún sin instalar |
| RAM y framebuffer bajo Linux | 6 GiB; 1080 × 2400, stride 4352; captura física verificada. SEC y primer mapa MMU observados; orden nativo rojo=16/azul=0 contrastado con fotos |
| Pantalla y entrada para la Shell | Encabezado, mapa FS0 de firmware y prompt visibles; entrada física y comandos sin validar |
| GOP y framebuffer propios | BLT comprobados con sanitizadores; consola gráfica UEFI mostrando la Shell en el OPPO |
| Lector FDT propio en C | Comprobado con sanitizadores y captura; la prueba nativa alcanzó su validación de handoff y plan |
| Interpretación de la entrega de LK | C propio; reservas fijas, dinámicas, vacías e incompletas diferenciadas; primera política comprobada en PC |
| Primer plan de memoria propio | HOBs, cambio de pila y MMU/cachés alcanzados por la prueba corta; mapa de RAM completo pendiente |
| Arranque temporal con `fastboot boot` | Transferencia completada; fallo de LK y retorno a Android; sin ejecución de RAM demostrada |
| Opción adicional en el menú de Fastbootd | Diseño propuesto; sin implementar |
| microSD y cargador ARM64 | FAT, carga EFI y pantalla Recovery de Windows Boot Manager verificados; cuatro bits a 13 MHz, 16 MiB a 1141 KiB/s y 2100 MiB libres antes del cargador. Nuevo error `0xc0000225`; WinPE pendiente |
| Logo original y BGRT | Extractor y biblioteca comprobados en PC; presentación del logo en UEFI y Windows pendiente |

Los respaldos de Android y la recuperación MediaTek ya se obtuvieron durante el
trabajo previo. Se mantienen fuera de este repositorio. El propietario fijó
**20 % como batería mínima** para operar sobre el teléfono; las herramientas
comprueban el nivel y detienen la operación si es inferior o no puede leerse.

## Primer objetivo

Identificar una entrada de arranque y un mapa de memoria válidos, producir una
imagen de diagnóstico específica para este hardware y observar su ejecución.
Después se incorporarán consola, Shell e integración con el recovery.

La entrada ARM64 y los controladores específicos serán código original del
proyecto o código del propietario con autoría comprobada. No se usará BootShim
ni se incorporarán implementaciones de terceros adicionales al núcleo EDK2.
Véase [docs/ORIGIN.md](docs/ORIGIN.md).

El trabajo [Moto G20 EDK2 UEFI](https://github.com/h0nda1337/Moto-G20_EDK2_UEFI)
aporta experiencia y componentes genéricos. Su plataforma Unisoc requiere otras
direcciones y otros controladores; sus imágenes no son imágenes para este OPPO.

## Herramientas disponibles

La herramienta inicial inspecciona una imagen Android de arranque o recovery ya
respaldada. Solo lee el archivo y escribe un resumen con campos públicos de
configuración; no utiliza ADB ni Fastboot.

```console
python scripts/inspect_boot.py /ruta/al/recovery.bin --output evidence/recovery-kernel.json
```

El resumen publicado está en [evidence/recovery-kernel.json](evidence/recovery-kernel.json).
El [inventario del DTB](evidence/stock-platform.json) identifica ocho CPU,
GICv3, UART y watchdog. Es el DTB base del firmware: no contiene el mapa final
de RAM ni permite deducir el framebuffer del arranque.

`OppoReno5LitePkg/` contiene la primera biblioteca de plataforma y metadatos EDK2.
Describe registros del SoC y valida propuestas de región de carga, rechazando
mapas no verificados, desbordamientos y solapamientos con regiones reservadas.
Compila como objeto ARM64; no constituye una imagen arrancable ni inicializa
todavía el hardware. Véase [docs/BUILD.md](docs/BUILD.md).

El primer firmware integrado enlaza la SEC, GOP, servicios iniciales y BDS
propios con el núcleo DXE y la Shell de EDK2. Las pruebas QEMU de la entrada
continúan usando una rutina C de prueba; no ejecutan ese firmware completo.
Las imágenes permanecen privadas; siete candidatos integrados ya se probaron
mediante la partición recovery y después se restauró su original. Aún no hay
controladores de teclado o USB nativo validados. El lector microSD propio está
en su primera prueba física: véase [docs/MICROSD.md](docs/MICROSD.md).
El séptimo llegó al prompt y volvió solo a
Android. El retorno automático está observado; su plazo nativo y la ruta exacta
de reset todavía no están identificados.
Véanse [docs/NATIVE_ENTRY.md](docs/NATIVE_ENTRY.md) y
[docs/NATIVE_FIRMWARE.md](docs/NATIVE_FIRMWARE.md).

La planificación y fuentes están en [docs/ROADMAP.md](docs/ROADMAP.md) y
[docs/RESEARCH.md](docs/RESEARCH.md).

## Resultado de las primeras pruebas físicas

Android bloqueó la lectura del árbol activo, `/proc/iomem` y los atributos de
framebuffer. Se continuó con los respaldos, sin considerar válidas esas lecturas.

Las pruebas de `fastboot boot` con boot y recovery originales completaron la
transferencia, pero USB se desconectó antes del acuse de arranque. Tras la prueba
del recovery, el propietario observó un reinicio automático a Android, y las
propiedades de arranque registraron `lk_crash`. No se demostró el arranque del
recovery desde RAM. Android volvió a arrancar y conservó el estado desbloqueado.
Estas pruebas no enviaron comandos de flasheo, borrado, bloqueo ni desbloqueo.
El detalle filtrado está en [evidence/stock-ram-recovery-probe.json](evidence/stock-ram-recovery-probe.json).

Después se probaron dos ramdisks de diagnóstico escribiendo únicamente recovery.
La entrada directa al segundo candidato fue seguida por el fallo de Android.
Los registros muestran que el proceso de recovery actualizó archivos de claves;
que el script añadido solo escriba en `/tmp` no convierte al recovery completo
en una herramienta de solo lectura. No se obtuvo un inventario válido de RAM o
framebuffer en esas pruebas. Más tarde, el PID 1 mínimo propio sí se ejecutó y
transfirió las cuatro observaciones por USB. El framebuffer activo coincide con
el tag nativo de LK y su reserva, aunque contradice las propiedades divididas del
árbol. Se conservan ambas descripciones; el análisis no convierte ninguna en una
configuración GOP ya validada. Los resultados filtrados están en
[evidence/runtime-linux-observations.json](evidence/runtime-linux-observations.json).

La primera prueba de SEC nativa falló en LK porque el FD se había empaquetado
sin gzip. Tras corregir ese transporte, el propietario vio la franja verde de
la SEC propia y el retorno automático a Android. ADB confirmó arranque completo
y motivo `reboot`; no se midió el intervalo ni se confirmó de forma concluyente
la ruta PSCI de reset. Esta variante termina antes de HOBs, MMU, DXE y Shell.
Véase [evidence/native-entry-probe-validation.json](evidence/native-entry-probe-validation.json).

La prueba corta siguiente mostró tres franjas después de crear HOBs, cambiar
de pila y comprobar MMU/cachés activas, y regresó a Android. El azul esperado
se observó rojo: falta confirmar los canales del framebuffer antes de Linux.
Véase [evidence/native-mmu-probe-validation.json](evidence/native-mmu-probe-validation.json).

El primer intento integrado se detuvo en las tres franjas de SEC, ahora con
azul correcto, sin evidencia de BDS ni Shell. No reinició automáticamente en
142 segundos; volvió a Android mediante reinicio físico y se restauró recovery.
Las fotografías solicitadas por el propietario se encuentran en
[docs/NATIVE_FIRMWARE.md](docs/NATIVE_FIRMWARE.md#laboratorio-dxeshell).

El siguiente diagnóstico añade un registro de texto propio a SEC y al núcleo
DXE, con glifos originales y una página reservada para conservar el cursor.
Compiló para ARM64 y pasó las pruebas de PC con sanitizadores; esos resultados
se registran en [la evidencia de PC](evidence/native-display-log-host-validation.json).
El registro de diagnóstico por sí solo no constituye una consola UEFI ni Shell.
En el segundo intento integrado sí se observó el núcleo DXE y su assert por
protocolos ausentes. La foto y el ciclo de dependencia PCD identificado en el
FD están en [el diagnóstico físico](docs/NATIVE_FIRMWARE.md#ejecución-física-del-diagnóstico-de-texto).
Tras corregir PCD se cargaron controladores y quedó únicamente `Capsule` ausente.
La tercera foto y el siguiente ajuste runtime, verificado todavía en PC, están
en [el tercer intento](docs/NATIVE_FIRMWARE.md#tercer-intento-pcd-corregido-y-cápsulas-pendientes).
El cuarto alcanzó la carga y entrada de `Shell.efi`, que devolvió
`EFI_UNSUPPORTED`. Su foto, el requisito HII corregido y el diagnóstico adicional
del BDS están en [el cuarto intento](docs/NATIVE_FIRMWARE.md#cuarto-intento-carga-de-shell-y-rechazo-unsupported).

## Logo de OPPO durante el arranque de Windows

Se extrajo localmente el logotipo del firmware propio, separando la marca OPPO
del texto inferior de Android. `OppoBgrtLib` valida ese BMP y prepara una tabla
BGRT con checksum correcto y estado inicial «no mostrado». Las pruebas y la
compilación ARM64 no equivalen a mostrar el logo desde UEFI. Faltan GOP, la
instalación de la tabla y la validación con Windows; los gráficos del fabricante
se conservan fuera de Git. Véase [docs/LOGO_BGRT.md](docs/LOGO_BGRT.md).

## Publicación de avances

Se solicitó revisar y publicar diariamente los cambios verificados. El estado
de las pruebas debe distinguir siempre inspección, compilación y ejecución física.
Se publicarán fuentes, herramientas y documentación revisadas. Los días sin
cambios publicables no requieren commits vacíos. Véase [docs/PUBLISHING.md](docs/PUBLISHING.md).

## Licencias

Los archivos originales de este proyecto usan BSD-2-Clause. La compilación de
la biblioteca utiliza cabeceras EDK2 externas; las referencias inspeccionadas y
sus revisiones están en [evidence/dependencies.json](evidence/dependencies.json).
Las fuentes descargadas, los binarios y los respaldos no están incluidos en Git.
Las incorporaciones futuras deben conservar sus licencias y atribuciones originales.
