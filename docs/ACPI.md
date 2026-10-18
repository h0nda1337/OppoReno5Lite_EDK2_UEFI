# Preparación ACPI: generador propio en PC

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
**todavía no se han capturado para este perfil**. Comprueba región, alineación,
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

Falta recoger y validar la correspondencia GICR/afinidad, enlazar los hechos del
FDT recibido en cada arranque y revisar CPU/PSCI antes de habilitarlas para un OS.
La posterior instalación deberá reservar memoria `EfiACPIReclaimMemory`, mantener
sus direcciones físicas y publicar RSDP únicamente al completar todas las tablas.
También faltan SMBIOS, tiempo RTC y la descripción y control de otros dispositivos.
Estas tablas mínimas no certifican compatibilidad con Windows ni arranque de WinPE.
No hubo una nueva operación sobre el teléfono durante esta preparación.
