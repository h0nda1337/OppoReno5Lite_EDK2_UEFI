# Diagnósticos Linux y recuperación de Android

Estado del 16 de octubre de 2026. Android volvió a arrancar después de un
restablecimiento desde el recovery original, autorizado por el propietario.
`recovery_diagnostic_session.py` rechaza instalaciones que retengan el ramdisk
OEM. Permite el PID 1 mínimo revisado solo con selección explícita y entrada
directa; conserva el modo de restauración original. La captura Linux posterior
funcionó. Ninguna de estas pruebas demuestra el arranque de EDK2 o un método
para recuperar datos cifrados.

## Lo comprobado

Se construyeron candidatos Android boot v2 conservando kernel, segundo
componente, recovery DTBO y DTB. El repaquetado actualiza la posición del recovery
DTBO al cambiar el tamaño del ramdisk. La reconstrucción con el ramdisk original
reprodujo el contenido original anterior al pie AVB, incluido el identificador
SHA-1 de la cabecera. Las pruebas sintéticas comprueban límites, archivos newc,
propietarios, permisos, enlaces y movimiento del DTBO.

Los dos primeros candidatos cambiaban propiedades y añadían un servicio de
captura en el ramdisk del fabricante. Android arrancó tras dos escrituras
seguidas de un reinicio normal. Al entrar directamente al segundo candidato,
se observó el menú OPPO, pero no apareció ADB ni se obtuvo una captura válida.
La presencia del menú no demuestra que el servicio añadido se ejecutara.

Después, los reinicios normales volvieron al recovery. El propietario observó
`init_user0_failed` y posteriormente `phenix.data_to_recovery`. En los respaldos
de registros aparecen fallos de `cryptfs mountFstab`, `cryptfs init_user0` y
apertura de la clave de cifrado de metadatos.

El registro del recovery documenta actualizaciones de archivos de claves en
`/metadata` y `/data` durante la prueba. Esto invalida cualquier supuesto de que
conservar el binario original o añadir solo un script que escribe en tmpfs
evitaría escrituras persistentes: el recovery original inicia otros servicios,
monta particiones y puede modificar datos por su cuenta.

Se leyó el recovery instalado y se confirmó que era el segundo candidato.
Después se restauró el respaldo original y se verificaron sus 64 MiB mediante
lectura completa y SHA-256. Android siguió fallando al arrancar. Boot y DTBO
coincidían con los respaldos, al igual que GPT, TEE, vbmeta y el estado de seccfg
de la sesión anterior. La nueva lectura del 16 de octubre volvió a confirmar
boot y recovery originales. No se enviaron comandos de bloqueo o desbloqueo.

Estos resultados identifican una modificación de claves y un fallo posterior
de apertura. No demuestran todavía qué parámetro de Keymaster provocó la
incompatibilidad. Los nuevos pies AVB usaban `NONE` y no conservaban las
propiedades AVB del fabricante; no deben considerarse equivalentes al original.
El generador corregido conserva ahora esas propiedades y valida su igualdad en
PC. Esto no recupera la firma OEM ni demuestra que el comportamiento físico sea
seguro para los servicios OEM. Las pruebas posteriores usan un ramdisk mínimo
que no conserva esos servicios.

Tras el restablecimiento, ADB confirmó el modelo esperado, F.72 México libre,
`sys.boot_completed=1`, bootloader desbloqueado y 28 % de batería cargando. Ese
resultado recupera Android; no valida el diagnóstico ni el arranque de EDK2.

## Diagnóstico mínimo propio: ejecución física

El nuevo ramdisk contiene únicamente directorios virtuales y un PID 1 original,
estático y AArch64. Usa el kernel original, monta devtmpfs/proc/sys/tmpfs/configfs,
no abre dispositivos de bloque y no inicia recovery ni Keymaster del fabricante.
Conserva las cinco propiedades AVB originales; el pie sigue siendo `NONE`, sin
firma OEM. La sesión escribe solo recovery y exige batería mínima del 20 %.

En el primer arranque apareció el marcador blanco y el gadget CDC-ACM propio.
El receptor no abrió el puerto porque pyserial para Windows no conservó la
identidad con guiones. El diagnóstico volvió a Android por su plazo de espera;
se verificó el arranque y se restauró el recovery original, con batería al 44 %.

La siguiente compilación usó una identidad alfanumérica. Transfirió FDT, iomem,
framebuffer y batería al PC. La recepción binaria se completó; el analizador
original rechazó dos declaraciones de framebuffer contradictorias. Se guardó
ese fallo y se analizaron de nuevo los archivos en PC, sin otra instalación.
El análisis corregido conserva la contradicción y no selecciona una dirección
para un payload nativo.

Resultados: RAM desde `0x40000000`, tamaño `0x180000000` (6 GiB); framebuffer
Linux `0x76520000`, 1080 × 2400, stride 4352, RGB en bits 0/8/16 y alfa en 24.
El framebuffer activo coincide con el tag nativo y con una reserva del FDT.
Las propiedades divididas indican otra dirección y tamaño. El
[kernel de referencia OPPO](https://github.com/oppo-source/android_kernel_oppo_mt6779/blob/0e1e01b9fb281b02d1f50f12ea475d881f3fafd5/drivers/misc/mediatek/video/mt6779/videox/mtkfb.c)
prioriza el tag nativo cuando existe; la coincidencia observada también se
comprobó con ioctl. No se ha demostrado todavía que la pantalla acepte acceso
nativo antes de la inicialización de Linux.

Se solicitó volver a Android por el protocolo del diagnóstico. ADB verificó
arranque completo y estado desbloqueado. Fastboot confirmó la restauración del
recovery original y Android volvió a verificarse con batería al 46 %. Esta
restauración tiene acuse de Fastboot y comprobación de Android; no se hizo una
nueva lectura completa de recovery en esta fase. No se necesitó otro borrado.

La captura filtrada está en
[runtime-linux-observations.json](../evidence/runtime-linux-observations.json).
Los archivos crudos, fotografías y registros privados no se publican.

## Herramientas y alcance de comprobación

- `recovery_image.py`: lectura acotada y repaquetado Android v2/newc.
- `inspect_runtime.py`: interpretación de un FDT de ejecución suministrado por
  el propietario. Rechaza mapas sin RAM y omite campos privados.
- `framebuffer_probe.c`: consulta del framebuffer mediante ioctl de Linux.
- `capture_init.c`: PID 1 mínimo de Linux con captura en tmpfs, marcador gráfico
  y transporte CDC-ACM. Su código no abre dispositivos de bloque; no retiene el
  proceso recovery del fabricante. Compilado para AArch64 y ejecutado en el OPPO.
- `receive_diagnostic.py`: protocolo binario acotado, plazo total de recepción
  y selección del gadget de diagnóstico por una identidad de desarrollo fija.
- `analyze_diagnostic.py`: geometría y rangos acotados, coincidencia del
  framebuffer con tag y reserva, filtrado explícito de iomem y campos JSON.

La ejecución con `qemu-aarch64` solo comprobó que el programa PID 1 rechaza una
invocación accidental como proceso ordinario. No fue un arranque de sistema
QEMU, una prueba de pantalla o una validación de USB en el teléfono.

Las imágenes y logs completos permanecen privados en G:. La recepción de un
archivo, un retorno cero de una herramienta o la transferencia de una imagen
no prueban por sí solos que una fase se completó. Las sesiones privadas ahora
comprueban todos los tamaños esperados antes de declarar una lectura terminada.

## Límites de las próximas pruebas

Los diagnósticos OEM permanecen suspendidos. Las siguientes fases requieren una
entrada ARM64 propia y una interpretación completa de las reservas, cachés,
MMU y reinicio. El inventario bajo Linux no demuestra ejecución nativa de EDK2.

Referencias de implementación: [AOSP init](https://android.googlesource.com/platform/system/core/+/4ee27039e0ee4d7c1b5314642d51f01096bfa79e/init/builtins.cpp)
relaciona ese motivo de recuperación con el fallo de `init_user0`;
[AOSP vold, KeyStorage.cpp](https://android.googlesource.com/platform/system/vold/+/android13-release/KeyStorage.cpp)
describe el reemplazo de blobs durante una actualización de claves. Son
referencias generales; no sustituyen el análisis de las implementaciones OPPO
y Trustonic de este firmware.
