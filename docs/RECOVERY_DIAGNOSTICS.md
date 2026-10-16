# Diagnóstico de recovery: resultados y suspensión

Estado del 16 de octubre de 2026. Android volvió a arrancar después de un
restablecimiento desde el recovery original, autorizado por el propietario.
`recovery_diagnostic_session.py` rechaza nuevas instalaciones de diagnóstico;
conserva la validación de archivos en PC y el modo de restauración original.
Este documento registra una investigación fallida, no un procedimiento probado
para instalar UEFI ni para recuperar datos cifrados.

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
seguro. El nuevo candidato mínimo no se ha instalado en el teléfono.

Tras el restablecimiento, ADB confirmó el modelo esperado, F.72 México libre,
`sys.boot_completed=1`, bootloader desbloqueado y 28 % de batería cargando. Ese
resultado recupera Android; no valida el diagnóstico ni el arranque de EDK2.

## Herramientas que solo se comprobaron en PC

- `recovery_image.py`: lectura acotada y repaquetado Android v2/newc.
- `inspect_runtime.py`: interpretación de un FDT de ejecución suministrado por
  el propietario. Rechaza mapas sin RAM y omite campos privados.
- `framebuffer_probe.c`: consulta del framebuffer mediante ioctl de Linux.
- `capture_init.c`: PID 1 mínimo de Linux con captura en tmpfs, marcador gráfico
  y transporte CDC-ACM. Su código no abre dispositivos de bloque; no retiene el
  proceso recovery del fabricante. Compilado para AArch64, sin ejecución física.
- `receive_diagnostic.py`: protocolo binario acotado, plazo total de recepción
  y selección del gadget de diagnóstico por una identidad de desarrollo fija.

La ejecución con `qemu-aarch64` solo comprobó que el programa PID 1 rechaza una
invocación accidental como proceso ordinario. No fue un arranque de sistema
QEMU, una prueba de pantalla o una validación de USB en el teléfono.

Las imágenes y logs completos permanecen privados en G:. La recepción de un
archivo, un retorno cero de una herramienta o la transferencia de una imagen
no prueban por sí solos que una fase se completó. Las sesiones privadas ahora
comprueban todos los tamaños esperados antes de declarar una lectura terminada.

## Condiciones para reanudar pruebas físicas

Se comprobó el arranque normal de Android y su batería. Sigue pendiente revisar
toda la secuencia de inicio, incluidos servicios originales,
montajes y metadatos AVB, antes de permitir otra instalación. Un inventario bajo
Linux tampoco demostrará ejecución nativa de EDK2.

Referencias de implementación: [AOSP init](https://android.googlesource.com/platform/system/core/+/4ee27039e0ee4d7c1b5314642d51f01096bfa79e/init/builtins.cpp)
relaciona ese motivo de recuperación con el fallo de `init_user0`;
[AOSP vold, KeyStorage.cpp](https://android.googlesource.com/platform/system/vold/+/android13-release/KeyStorage.cpp)
describe el reemplazo de blobs durante una actualización de claves. Son
referencias generales; no sustituyen el análisis de las implementaciones OPPO
y Trustonic de este firmware.
