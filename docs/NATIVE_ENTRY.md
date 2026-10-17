# Entrada ARM64 propia

`OppoReno5LitePkg/Sec/AArch64/Entry.S` es el comienzo de la entrada SEC propia.
Recibe directamente el control y conserva x0..x3 antes de usar registros
temporales. No incorpora BootShim, no copia una imagen a otra dirección ni
transfiere el control a un payload añadido.

La entrada admite EL1 o EL2, exige MMU y caché de datos desactivadas y aparca
otros núcleos. Enmascara interrupciones, instala una tabla de vectores alineada
a 2048 bytes, prepara una pila propia de 16 KiB y entrega a `OppoSecMain` un
registro de los argumentos originales, EL, SCTLR, frecuencia del contador y
MPIDR. No desactiva a ciegas un MMU que haya encontrado activo.

El contrato de entrada se basa en la
[documentación ARM64 de Linux](https://docs.kernel.org/arch/arm64/booting.html).
Esto describe el contrato que se espera de LK. La prueba corta alcanzó la
franja verde de SEC; todavía faltan medidas de los registros de entrada.

`OppoFdtLib` aporta un lector original para la estructura que entrega LK. El
formato se consulta en la
[especificación de Devicetree, capítulo 5](https://devicetree-specification.readthedocs.io/en/latest/chapter5-flattened-format.html).
No se copió un parser externo. La implementación limita el blob a 2 MiB,
32 niveles, 4096 nodos, nombres de 127 bytes y rutas de 1023 bytes. Examina
bloques y tokens, consulta propiedades por ruta absoluta y lee reservas de
64 bits big-endian; no interpreta todavía sus valores como un mapa asignable.

`OppoHandoffLib` interpreta los valores de plataforma usando ese lector propio.
El análisis de la captura detecta 17 reservas explícitas, seis dinámicas y un
nodo reservado que se comprobó vacío mediante recuento de propiedades. Informa esos casos por separado y conserva los tags de
framebuffer contradictorios. La presencia de una reserva que contiene el
framebuffer no valida por sí sola una escritura al panel. Tampoco leer
`method = "smc"` demuestra una llamada PSCI.

La política inicial propia ya se comprueba en PC: protege imagen y datos fijos,
redondea exclusiones a páginas y difiere las reservas dinámicas al OS como exige
DTSpec 3.5.4. Solo propone un pool inicial de 64 MiB. El prototipo SEC ya
contiene su materialización en HOBs y mapa MMU, pero falta verificar su ejecución
física; no se debe usar como mapa final de un sistema operativo.

## Qué se comprobó

`scripts/test_sec_entry.sh` enlaza esa misma entrada con una cabecera y una
rutina C de prueba originales. Arranca ambos casos en QEMU `virt`, uno en EL1
y otro en EL2. La rutina comprueba la identidad del FDT recibido, argumentos
marcadores, alineación de pila, DAIF y VBAR; después termina mediante
semihosting. Esa llamada pertenece solo a la prueba y no al código de entrada.

```bash
EDK2_HEADERS=/ruta/edk2 bash scripts/test_sec_entry.sh
```

Las imágenes se guardan en `.build/sec-entry/`. Son artefactos de prueba para
QEMU. No son imágenes de recovery, no se preparan para flasheo y no contienen
un núcleo DXE ni la Shell.

## Qué falta

`Sec/NativeMain.c` implementa ahora la rutina real del prototipo, distinta de la
rutina del harness QEMU. Construye HOBs y mapa MMU, habilita cachés y carga DXE;
se acompaña de GOP, servicios iniciales y BDS propios. El volumen incluye la
Shell del núcleo EDK2. Véase [NATIVE_FIRMWARE.md](NATIVE_FIRMWARE.md).

La prueba corta con transporte gzip ya mostró la franja verde después de las
validaciones de handoff, memoria, PSCI_VERSION y watchdog, y volvió a Android.
No ejecutó HOBs, MMU ni DXE. Todavía requieren validación física esas etapas,
GIC y temporizadores, además de medidas del estado de entrada y del reset PSCI.
Los errores tempranos pueden aparcarse; el retorno de una prueba corta no
garantiza una salida en todos los fallos. Véase la evidencia filtrada y las
limitaciones en [NATIVE_FIRMWARE.md](NATIVE_FIRMWARE.md).
