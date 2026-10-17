# OPPO Reno5 Lite — EDK2 UEFI

Port experimental de EDK2 para **OPPO Reno5 Lite CPH2205 / OP4F43L1**, proyecto
20295, MediaTek MT6779 (Helio P95). El objetivo es conseguir una UEFI Shell nativa
y después ofrecer una opción **UEFI Shell** en el recovery que muestra Fastbootd.

**Estado, 16 de octubre de 2026: bibliotecas propias de plataforma, BGRT,
framebuffer, FDT, handoff y plan de memoria compiladas con EDK2 para ARM64;
entrada ARM64 propia probada en QEMU EL1/EL2; primer prototipo integrado de
firmware compilado e inspeccionado en PC, con SEC, GOP, servicios iniciales y
BDS propios más núcleo DXE y Shell de EDK2. El diagnóstico Linux mínimo sí se
ejecutó en el OPPO y capturó memoria y pantalla por USB. La prueba corta de SEC
nativa ya mostró su franja verde y volvió a Android con transporte gzip.
La segunda prueba corta alcanzó HOBs, cambio de pila y MMU/cachés y volvió a
Android. DXE y Shell todavía no se han observado en el teléfono.**

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
| Firmware integrado MT6779 | Primer intento alcanzó tres franjas de SEC; detenido sin marca de BDS ni texto de Shell |
| Entrada ARM64 propia | QEMU EL1/EL2; franja verde de SEC observada en el OPPO y regreso a Android; registros de entrada aún sin medir |
| GIC, UART y watchdog | Direcciones extraídas del DTB original; biblioteca inicial ARM64 compilada |
| RAM y framebuffer bajo Linux | 6 GiB; 1080 × 2400, stride 4352; captura física verificada. SEC y primer mapa MMU observados; orden nativo rojo=16/azul=0 contrastado con fotos |
| Pantalla y entrada para la Shell | Pendientes |
| GOP y framebuffer propios | BLT comprobados con sanitizadores; franja nativa de SEC observada; controlador GOP en DXE pendiente |
| Lector FDT propio en C | Comprobado con sanitizadores y captura; la prueba nativa alcanzó su validación de handoff y plan |
| Interpretación de la entrega de LK | C propio; reservas fijas, dinámicas, vacías e incompletas diferenciadas; primera política comprobada en PC |
| Primer plan de memoria propio | HOBs, cambio de pila y MMU/cachés alcanzados por la prueba corta; mapa de RAM completo pendiente |
| Arranque temporal con `fastboot boot` | Transferencia completada; fallo de LK y retorno a Android; sin ejecución de RAM demostrada |
| Opción adicional en el menú de Fastbootd | Diseño propuesto; sin implementar |
| Logo original y BGRT | Extractor y biblioteca comprobados en PC; pantalla UEFI y Windows pendientes |

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
La imagen permanece privada y no se ha instalado. No hay entrada, USB nativo o
almacenamiento, y el reinicio de laboratorio no es todavía una vía de retorno
probada. Véanse [docs/NATIVE_ENTRY.md](docs/NATIVE_ENTRY.md) y
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
