# microSD nativa: lectura antes de Windows

Se está implementando un lector propio para MSDC1 del MT6779. La meta es abrir
el cargador ARM64 de una tarjeta preparada por el propietario y después ejecutar
Windows Boot Manager. La presencia de un archivo EFI no demuestra que Windows PE
pueda arrancar: aún faltan validaciones de memoria, ACPI y otros dispositivos.

## Evidencia y límites actuales

- Android montó la tarjeta como vfat y permitió leer la cabecera de
  `EFI/BOOT/BOOTAA64.EFI`: PE ARM64 (`0xaa64`), aplicación UEFI (`10`). Esa lectura
  se hizo desde Android; no es una validación del lector EDK2.
- `OppoMsdcLib` enumera SDHC/SDSC y lee sectores con CMD17 mediante PIO, en un bit,
  sin DMA. Con fuente de 26 MHz utiliza aproximadamente 382 kHz para enumerar y
  6,5 MHz para leer. No implementa comandos de escritura ni borrado.
- `OppoSdDxe` comprueba el HOB de perfil y conserva el pinmux existente. Selecciona
  únicamente el reloj de MSDC1 y publica Block I/O de solo lectura si la tarjeta
  responde. Los fallos se exponen mediante un protocolo de diagnóstico propio.
- La variante ARM64 `sd-read-lab` compiló y llegó físicamente al diagnóstico BDS.
  Su primer intento se detuvo en un error de CRC de respuesta a CMD8; no terminó
  la enumeración ni publicó Block I/O. No se leyeron sectores ni se ejecutó Windows.

## Primer intento físico (08)

La fotografía del propietario permite leer:

```text
SD PROFILE=SUCCESS BOARD=SUCCESS INIT=CRC ERROR PUBLISH=NOT STARTED
SD PADS=1 MISC=00000000 MODE16=11111111 MODE17=11111111 MODE1=00040000
SD SECTORS=0 LAST CMD=8 INT=00000400
```

El bit de interrupción `0x400` corresponde a error de CRC de respuesta. Es un
resultado del controlador, no prueba de una respuesta válida de la tarjeta ni
un diagnóstico concluyente de su causa. La siguiente revisión investigará el
muestreo y los retardos heredados. No se omitirá la comprobación CRC.

![Diagnóstico microSD: error CRC en CMD8](images/native-sd-read-lab-08.jpg)

![Contexto de la pantalla del mismo intento](images/native-sd-read-lab-08-context.jpg)

Ambas fotografías se conservaron sin recomprimir sus datos de imagen. Se
retiraron los metadatos de cámara, conservando orientación y perfil de color.
La segunda aporta contexto; su texto superior no permite otra conclusión SD.
Los resultados filtrados están en
[la evidencia física](../evidence/native-sd-read-lab-08-validation.json).

ADB volvió a detectar Android completo; después se restauró el recovery
original y se comprobó otra vez el arranque, con batería al 97 %. El observador
registró unos 196 segundos entre el informe de entrada y la detección de Android;
ese intervalo incluye LK, Android y sondeo USB. No mide los 90 segundos nativos
ni demuestra por sí solo el mecanismo exacto del reinicio.

La SEC valida las regiones de MSDC1, topckgen, infracfg y GPIO contra el FDT de
ese arranque antes de mapearlas como MMIO de dispositivo. No se inicializa MSDC0
ni se exploran otros discos desde el diagnóstico BDS. Los módulos genéricos
DiskIo, Partition y FAT de EDK2 suministran las capas de partición y filesystem;
el controlador específico MSDC y su enlace de plataforma son originales.

## Alimentación y pines

El FDT capturado en este CPH2205 declara MSDC1 en `0x11240000`, con suministro
VMCH desde `ldo5` y VMC desde `ldo3`, ambos bajo el regulador externo
`subpmic_ldo@64` del controlador I²C5. No son los reguladores principales MT6359.
La variante inicial conserva el estado de alimentación dejado por el cargador:
no escribe registros del regulador, no cambia voltaje y no adivina su chip.

El GPIO admite dos disposiciones documentadas para MSDC1. La variante solo
acepta un mux retenido que coincida con una de ellas. Si no coincide, se detiene
antes de programar el reloj. Un timeout de tarjeta por sí solo no identifica un
problema de alimentación.

Los contratos de registros y reloj se investigaron en las fuentes públicas de
[OPPO MT6779, revisión fijada](https://github.com/oppo-source/android_kernel_oppo_mt6779/tree/0e1e01b9fb281b02d1f50f12ea475d881f3fafd5/drivers/mmc/host/mediatek/ComboA)
y su [controlador de relojes](https://github.com/oppo-source/android_kernel_oppo_mt6779/blob/0e1e01b9fb281b02d1f50f12ea475d881f3fafd5/drivers/clk/mediatek/clk-mt6779.c).
Son referencias de interfaces; no se importó su implementación.

## Comprobaciones de PC

Con `EDK2_HEADERS` apuntando a la dependencia base revisada:

```sh
bash scripts/test_msdc.sh
bash scripts/test_sd_binding.sh
bash scripts/test_sd_read.sh
bash scripts/test_bds.sh
```

Las pruebas con ASan/UBSan cubren capacidad y direccionamiento SDHC/SDSC,
lecturas no alineadas, errores de CRC/timeout, esperas acotadas, exclusión de
escrituras y DMA, rechazo de perfiles no validados, readback de relojes y API
Block I/O. El diagnóstico de filesystem rechaza otras unidades y cabeceras PE
truncadas, de otra arquitectura o con offsets fuera del prefijo leído.
Son modelos de PC: no prueban señales eléctricas ni lectura física.

La segunda variante establece el muestreo común de lectura para SD legado y
limita el reintento de CMD8 a un segundo flanco de respuesta, únicamente después
de un CRC. Un segundo CRC o timeout detiene la enumeración; no se interpreta como
permiso para ignorar errores ni para reintentar lecturas de sectores fallidas.
También registra IOCON, CFG, PS y PAD_TUNE0 para comprobar el estado retenido.
La revisión compiló y pasó los modelos de PC; su prueba física mantuvo el error.

## Segundo intento físico (09)

![CMD8 mantiene CRC con ambos flancos](images/native-sd-read-lab-09.jpg)

La fotografía muestra el mismo resultado de perfil, placa e inicialización del
primer intento, y añade:

```text
SD IOCON BEFORE=00000000 ACTIVE=00000002 CFG=0200119B PS=00000002
SD CMD8 FIRST=CRC ERROR ATTEMPTS=2 PAD_TUNE0=00000000
```

El IOCON inicial ya estaba en cero; el alternativo tampoco resolvió el CRC.
PS no muestra altos en los bits de DAT/CMD. La foto no distingue entre entradas
inhabilitadas, alimentación ausente u otra condición eléctrica; no se concluye
la causa. La siguiente comprobación será la habilitación de entrada y el estado
de los pads específicos, antes de alterar reguladores. No se leyeron sectores.
La imagen conserva sus datos comprimidos; solo se retiraron metadatos personales.
Véase [la evidencia de este intento](../evidence/native-sd-read-lab-09-validation.json).

## Tercer intento físico (10): asociación de banco incorrecta

![Comprobación de pads detiene el diagnóstico](images/native-sd-read-lab-10.jpg)

```text
SD PROFILE=SUCCESS BOARD=DEVICE ERROR INIT=NOT STARTED PUBLISH=NOT STARTED
SD IES BEFORE=00003DEF AFTER=00003DFF SMT BEFORE=00000000 AFTER=00000000
SD PULLS PUPD=00000001 R0=00000000 R1=0000010F
```

La comprobación de lectura posterior a la escritura detuvo la preparación antes
de inicializar MSDC. `LAST CMD=4294967295` es el centinela de no iniciado, no un
comando enviado. El `CMD8 FIRST=SUCCESS` con cero intentos era un valor inicial
engañoso del diagnóstico; no hubo una respuesta CMD8 correcta.

La implementación de este intento tomó `0x11e70000` de un comentario del header
de referencia. Fue un error: el código del fabricante resuelve MSDC1 mediante
`mediatek,iocfg_rm`, y el árbol activo lo sitúa en **`0x11c20000`**. El banco
`0x11e70000` corresponde a `iocfg_lb`. Que una dirección exista en `reg` no prueba
su asociación con el periférico. La tabla de pinctrl de MT6779 ubica GPIO
134–139 en banco 1, consistente con `iocfg_rm` y sus máscaras IES/SMT/pulls.

Se conservará este resultado como evidencia histórica. La corrección debe
comprobar nombre compatible y rango en el FDT, usar el banco RM y restaurar los
valores originales si falla el readback. Este intento no valida alimentación,
enumeración SD, lectura de sectores ni Windows. Los modelos de PC no detectaban
la asociación equivocada porque simulaban los registros seleccionados.
Véase [la evidencia filtrada](../evidence/native-sd-read-lab-10-validation.json).

Las referencias primarias son el
[mapeo de recursos de MSDC](https://github.com/oppo-source/android_kernel_oppo_mt6779/blob/0e1e01b9fb281b02d1f50f12ea475d881f3fafd5/drivers/mmc/host/mediatek/ComboA/mt6779/msdc_cust.c)
y la [tabla pinctrl MT6779](https://github.com/oppo-source/android_kernel_oppo_mt6779/blob/0e1e01b9fb281b02d1f50f12ea475d881f3fafd5/drivers/pinctrl/mediatek/pinctrl-mt6779.c).

## Aplicación EFI de la tarjeta

Una copia privada leída mediante ADB `exec-out` mide 1 538 888 bytes. Su cabecera
indica ARM64 y aplicación UEFI; su recurso de versión la identifica como
**Boot Manager**, `bootmgr.exe`, versión `10.0.19041.662`, de Microsoft. El recurso
no equivale a verificar una firma digital. La imagen declara unos 1,73 MiB en
memoria y una reserva de pila de 1 MiB; esos números no son un requisito total
de RAM para Windows PE. Véase [la inspección](../evidence/sd-windows-loader-inspection.json).
No se publica la copia del ejecutable ni contenido de la tarjeta.

Una vez leída desde UEFI, se probarán por separado `LoadImage` y `StartImage`
con la ruta de esa microSD, conservando el resultado exacto de cada servicio.
La [especificación UEFI](https://uefi.org/specs/UEFI/2.11/07_Services_Boot_Services.html#image-services)
define esas operaciones y la carga desde un filesystem. Los
[requisitos de Windows sobre SoC](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/uefi-requirements-that-apply-to-all-windows-platforms)
incluyen servicios de firmware y tablas de plataforma que aún no se han
validado aquí. Ver una pantalla del cargador no demostraría que WinPE o su
kernel puedan completar el arranque.

Durante la inspección se detectó que `adb shell -T` alteraba los bytes LF del
archivo en este equipo. La cabecera inválida fue rechazada; `exec-out` conservó
el PE válido. Los métodos de captura binaria ahora utilizan esa vía, con pruebas
de bytes NUL/CR/LF y el mismo mínimo de batería. Los diagnósticos nativos MSDC
no utilizan ADB para leer la tarjeta.

Para compilar esta variante, se añade `NATIVE_SD_READ_LAB=1` al entorno de
`scripts/build_native_firmware.sh`. Su salida se separa en
`.build/native/sd-read-lab/`; no sustituye el FD anterior de Shell. El script
solo compila. Las imágenes y los informes con identificadores permanecen privados.

El BDS de esta variante muestra resultados y solicita reset a los 90 segundos,
sin lanzar Shell ni Windows. La ruta y el plazo exactos del retorno siguen sin
medir. La batería mínima sigue siendo 20 % antes de cada fase física.
