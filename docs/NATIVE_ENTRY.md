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
Esto describe el contrato que se espera de LK; su cumplimiento en la entrada
nativa del OPPO todavía debe medirse.

`OppoFdtLib` aporta un lector original para la estructura que entrega LK. El
formato se consulta en la
[especificación de Devicetree, capítulo 5](https://devicetree-specification.readthedocs.io/en/latest/chapter5-flattened-format.html).
No se copió un parser externo. La implementación limita el blob a 2 MiB,
32 niveles, 4096 nodos, nombres de 127 bytes y rutas de 1023 bytes. Examina
bloques y tokens, consulta propiedades por ruta absoluta y lee reservas de
64 bits big-endian; no interpreta todavía sus valores como un mapa asignable.

`OppoHandoffLib` interpreta los valores de plataforma usando ese lector propio.
El análisis de la captura detecta 17 reservas explícitas, seis dinámicas y un
nodo reservado vacío. Informa esos casos por separado y conserva los tags de
framebuffer contradictorios. La presencia de una reserva que contiene el
framebuffer no valida por sí sola una escritura al panel. Tampoco leer
`method = "smc"` demuestra una llamada PSCI: siguen pendientes la política del
mapa UEFI y la inicialización física.

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

La rutina `OppoSecMain` solo existe en la prueba. Faltan su implementación real,
inicialización del estado C, HOBs, mapa de memoria con todas las reservas,
cachés/MMU, temporizador, reinicio, acceso nativo a la pantalla, empaquetado de
volúmenes EDK2 y transferencia al núcleo DXE. Los errores se aparcan; todavía
no existe una salida de recuperación nativa probada en el OPPO. Por ello esta
entrada no se instala en el teléfono.
