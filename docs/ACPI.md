# Preparación ACPI: generador propio y observación GICv3

El intento físico 26 sigue mostrando Recovery de Windows `0xc0000225`.
Los contadores no registraron error de lectura ni reset de Block I/O durante
`StartImage`. No identifican el objeto que falta. La ausencia de ACPI en la
composición actual es un requisito de plataforma pendiente, **no una causa
demostrada de ese código**. Microsoft describe el uso de RSDP, MADT, GTDT y
DSDT por Windows en [sus requisitos de tablas ACPI](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/acpi-system-description-tables).

`OppoAcpiLib` es un generador original de estructuras ACPI, comprobado en PC y
compilado como objeto ARM64. No se incluye en `OppoNative.dsc`/`.fdf`, no instala
tablas UEFI y no se ejecutó en el teléfono. No incorpora tablas ni código de
otro port. Los formatos C genéricos son los de la dependencia EDK2 identificada.

## Datos y decisiones de formato

La [inspección del FDT capturado](../evidence/acpi-platform-prerequisites-26.json)
aporta ocho afinidades `0x000, 0x100, …, 0x700`, GICv3, temporizadores y PSCI
mediante SMC. Las afinidades proceden de `reg`: los nombres `cpu@100` y siguientes
no representan esos valores. La inspección adicional de `/pmu` obtuvo
`interrupts = <1 7 8>`: PPI 7, nivel bajo, INTID/GSIV 23. No se modificó el FDT.

El prototipo construye este conjunto en un búfer de 1528 bytes:

| Estructura | Contenido del prototipo |
| --- | --- |
| RSDP y XSDT | Punteros físicos de 64 bits y ambos checksums de RSDP. |
| FADT, revisión 6 | Hardware reducido, PSCI SMC, perfil tablet y puntero a DSDT; sin FACS ni registros PM inventados. |
| DSDT, revisión 2 | Ocho dispositivos CPU `ACPI0007` bajo `\_SB`, `_UID` entero y `_STA`; sin métodos ni OperationRegions. |
| MADT, revisión 4 | Ocho GICC, CPU de arranque primero, un GICD v3 y mantenimiento GSIV 25. |
| GTDT, revisión 2 | GSIV 29/30/27/26, nivel bajo, sin garantía de always-on ni contadores MMIO publicados. |

Los dispositivos CPU y sus UID siguen el [modelo de declaración de procesadores](https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/08_Processor_Configuration_and_Control/declaring-processors.html).
La [especificación ACPI, capítulo 5](https://uefi.org/specs/ACPI/6.4_A/05_ACPI_Software_Programming_Model.html)
define los punteros, checksums, FADT, MADT y GTDT. Se usan `UINT64_MAX` para
los dos contadores MMIO opcionales no suministrados y flags de temporizador
`2` para nivel bajo. El bit always-on queda a cero.

La biblioteca exige las direcciones individuales de redistributor como entrada;
**se capturaron físicamente en el intento 28**, descrito más abajo. Comprueba región, alineación,
solapamientos y afinidades únicas, pero no demuestra que una dirección pertenezca
a esa CPU. Las pruebas usan una asociación sintética declarada como tal, incluso
desordenada, para detectar una asignación accidental por índice.

El prototipo no publica una estructura GICR de descubrimiento porque no se ha
demostrado que todos los redistributors estén en un dominio siempre encendido.
Describe GICC individuales. Los campos de compatibilidad GICv2 son cero; antes
de instalar tablas hay que confirmar ese contrato y clasificar el tercer rango
del nodo GIC. No se interpreta `0x0c53a650` como una interfaz GICC.

## Comprobación y trabajo pendiente

```console
EDK2_HEADERS=/ruta/a/edk2 bash scripts/test_acpi.sh
```

La comprobación ejecuta el generador con ASan/UBSan, cada tamaño de búfer
insuficiente, perfiles inválidos o superpuestos, memoria de salida con canarios,
CPU de arranque distinta y direcciones de tablas por encima de 4 GiB.
Un lector Python independiente valida bytes, revisiones, punteros, longitudes,
checksums, IRQ y campos reservados. ACPICA interpreta y recompila la DSDT con
cero errores y advertencias; su intérprete evalúa los UID extremos y `_STA`.
El objeto ARM64 solo depende de `CopyMem` y `ZeroMem`.
Véase [la evidencia de PC](../evidence/acpi-serialization-host-validation-27.json).

La correspondencia GICR/afinidad ya se observó en el intento 28. Falta enlazarla
con el generador usando los hechos del FDT recibido en cada arranque y revisar
CPU/PSCI antes de habilitarlas para un OS.
La posterior instalación deberá reservar memoria `EfiACPIReclaimMemory`, mantener
sus direcciones físicas y publicar RSDP únicamente al completar todas las tablas.
También faltan SMBIOS, tiempo RTC y la descripción y control de otros dispositivos.
Estas tablas mínimas no certifican compatibilidad con Windows ni arranque de WinPE.
La preparación del generador (27) no operó sobre el teléfono; el observador
separado (28) sí se ejecutó físicamente.

## Observador GICv3 separado (28)

`OppoAcpiProbeLib` valida primero CPU, GIC, temporizador, PMU y PSCI del FDT
recibido. También comprueba celdas, traducción de direcciones y padres de las
interrupciones seleccionadas. Una incompatibilidad impide cualquier lectura
de registros. Los nombres de los nodos CPU no determinan su afinidad.

Después lee exclusivamente PIDR2/TYPER/CTLR del distribuidor y PIDR2/TYPER de
los redistributors. Exige arquitectura GICv3, tipos estables en dos lecturas,
ocho afinidades únicas que coincidan con el FDT y el indicador `Last`. Rechaza
VLPIs antes de avanzar con un tamaño de frame incompatible. El orden de salida
sitúa primero la CPU ejecutora, sin suponer que el orden físico coincide con
`MPIDR_EL1`. Los campos se contrastaron con la documentación de
[Arm GICv3](https://documentation-service.arm.com/static/601412d54ccc190e5e681269)
y sus [reglas de afinidad](https://documentation-service.arm.com/static/5f1068720daa596235e7f6ef).

La referencia de [MT6779 en Linux](https://github.com/torvalds/linux/blob/6f771ce6c3bb2c4dc9d348b578871c7adb8b285b/arch/arm64/boot/dts/mediatek/mt6779.dtsi#L133)
describe `0x0c53a650/0x50` como controlador de polaridad, separado de GICD/GICR.
Esto aporta una clasificación documental del tercer rango del FDT del OPPO;
no prueba compatibilidad GICv2 ni un controlador Windows para esa polaridad.
El observador no accede a ese rango ni a los registros de habilitación,
prioridad, enrutamiento o energía de GIC.

El modo `NATIVE_ACPI_PROFILE_LAB=1` es optativo y no se combina con los
laboratorios SD o las sondas SEC. SEC entrega una referencia al FDT ya protegido
en un HOB separado; BDS valida ese HOB, muestra el resultado y solicita retorno
a los 90 segundos. No inicia Shell, microSD o Windows ni instala tablas ACPI.
El inspector y el empaquetador comprueban el marcador, el plazo y la ausencia
del controlador SD en el FD. El manifiesto conserva ese modo explícitamente.

Las pruebas de PC cubren 32 FDT negativos, el FDT real capturado con registros
simulados, afinidades desordenadas, 43 puntos de fallo de lectura, identidades
inestables, límites del HOB y la lista exacta de direcciones permitidas. BDS
pasa 42 escenarios de consola/plazo/despacho y el empaquetador rechaza modos
incompatibles. La imagen ARM64 compiló en 33,576 segundos con las opciones
emitidas por EDK2 comprobadas.

### Resultado físico del intento 28

La fotografía muestra la CPU ejecutora con afinidad `0x000`, GICD PIDR2 `0x3b`,
TYPER `0x0079040f` y CTLR `0x12`, seguidos de estas ocho asociaciones:

| UID | Afinidad | Base GICR | GICR_TYPER |
| --- | --- | --- | --- |
| 0 | `0x000` | `0x0c040000` | `0x0000000000000020` |
| 1 | `0x100` | `0x0c060000` | `0x0000010000000120` |
| 2 | `0x200` | `0x0c080000` | `0x0000020000000220` |
| 3 | `0x300` | `0x0c0a0000` | `0x0000030000000320` |
| 4 | `0x400` | `0x0c0c0000` | `0x0000040000000420` |
| 5 | `0x500` | `0x0c0e0000` | `0x0000050000000520` |
| 6 | `0x600` | `0x0c100000` | `0x0000060000000620` |
| 7 | `0x700` | `0x0c120000` | `0x0000070000000730` |

Todos los PIDR2 de redistributor muestran `0x3b`; el último TYPER tiene `Last`.
La línea inicial `ACPI PROFILE` quedó fuera del registro visible. El código
revisado solo imprime las ocho entradas cuando termina correctamente toda
la validación; la fotografía también muestra la finalización del laboratorio.
Esto verifica la asociación de registros y afinidades, sin demostrar ejecución
de las CPU secundarias, entrega de interrupciones ni dominios de energía.

El propietario observó la pantalla fija y después un reinicio. ADB detectó
Android con arranque completo; después se restauró el recovery original y se
verificó Android de nuevo, con batería al 100 % antes y después. El intervalo
de 201,281 segundos hasta la detección incluye LK y Android y no mide el plazo
nativo de 90 segundos. Esta variante no instaló tablas ni lanzó Windows, por
lo que no cambia el resultado pendiente de WinPE.

![Ocho afinidades GICv3 observadas en el OPPO](images/native-acpi-gic-profile-lab-28.jpg)

Véase [la transcripción y el retorno físico](../evidence/acpi-gic-profile-lab-28-validation.json).
Véase [la composición y las comprobaciones 28](../evidence/acpi-gic-profile-lab-28-host-validation.json).
