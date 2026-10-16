# OPPO Reno5 Lite — EDK2 UEFI

Port experimental de EDK2 para **OPPO Reno5 Lite CPH2205 / OP4F43L1**, proyecto
20295, MediaTek MT6779 (Helio P95). El objetivo es conseguir una UEFI Shell nativa
y después ofrecer una opción **UEFI Shell** en el recovery que muestra Fastbootd.

**Estado, 16 de octubre de 2026: bibliotecas de plataforma y BGRT compiladas para
ARM64 y comprobadas en PC; diagnóstico Linux mínimo ejecutado en el OPPO con
captura USB de memoria y pantalla. Todavía no hay una imagen UEFI completa ni un
arranque UEFI observado en este OPPO.**

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
| EDK2 específico para MT6779 | Pendiente de desarrollo y validación |
| GIC, UART y watchdog | Direcciones extraídas del DTB original; biblioteca inicial ARM64 compilada |
| RAM y framebuffer bajo Linux | 6 GiB; 1080 × 2400, stride 4352; captura física verificada. Mapa UEFI y pantalla antes de Linux pendientes |
| Pantalla y entrada para la Shell | Pendientes |
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
