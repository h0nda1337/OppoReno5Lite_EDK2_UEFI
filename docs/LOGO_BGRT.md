# Logo original de OPPO y BGRT

El objetivo solicitado es conservar el logotipo original del firmware durante
el inicio de Windows. La implementación actual cubre extracción y construcción
de metadatos en PC. Aún no hay un renderizado UEFI ni un inicio de Windows
verificado en este OPPO.

## Resultado local

`extract_stock_logo.py` admite el contenedor MediaTek inspeccionado, valida la
tabla de imágenes y limita la descompresión. El fotograma cero del archivo de
referencia contiene un raster BGRA de 1080 × 2400. Tras revisar su vista previa,
se seleccionó la banda de la marca OPPO y se convirtió a BMP de 24 bits, BI_RGB,
con filas de abajo hacia arriba. La imagen resultante mide 620 × 150 y procede
de la posición (230, 827) del raster original. No se generó un logotipo nuevo.

El BMP y el firmware no se incluyen en el repositorio. Cada propietario debe
aportar su propio archivo local. Un ejemplo de conversión, sin acceso al teléfono:

```console
python scripts/extract_stock_logo.py /privado/logo.bin --width 1080 --height 2400 --frame 0 --band 0 --private-output-dir /privado/logo-extraido
```

La banda debe elegirse después de revisar el fotograma correspondiente; su
posición no se presume válida para otros firmwares.

## Biblioteca inicial

`OppoBgrtLib` valida el BMP que produce el extractor: cabeceras, dimensiones,
compresión, tamaño de píxeles, alineación de filas, dirección física y colocación
dentro de la pantalla. Prepara una BGRT de 56 bytes con checksum ACPI correcto y
estado inicial «no mostrado». Una segunda función actualiza estado y checksum.
Los fallos de validación dejan intacta la tabla de salida.

La biblioteca no asigna memoria, dibuja píxeles ni instala ACPI. El integrador
deberá reservar una copia persistente del BMP en `EfiBootServicesData`, obtener
la geometría real de GOP, dibujar y comprobar el resultado. Solo entonces podrá
marcar la imagen como mostrada e instalar la tabla. Si una Shell u otra pantalla
reemplaza el logo, deberá actualizar también la copia de BGRT publicada en ACPI.

Las pruebas de PC comprueban BMP malformados, límites, desbordamientos, checksum,
transiciones de visibilidad y el BMP original privado. La biblioteca también se
compiló como objeto AArch64. Ninguna de esas pruebas accede al teléfono.

```bash
EDK2_HEADERS=/ruta/a/edk2 bash scripts/build_bgrt_lib.sh
```

## Integración pendiente

Faltan una plataforma EDK2 arrancable, GOP, asignación de memoria, instalación
ACPI y un cargador de Windows funcional. También debe decidirse el tamaño final
del logo en el modo de pantalla real, sin tratar las coordenadas del firmware
como una validación de la presentación en Windows.

La [especificación ACPI 6.6](https://uefi.org/specs/ACPI/6.6/05_ACPI_Software_Programming_Model.html)
define la tabla y su indicador de visibilidad. La documentación de
[pantallas de arranque de Microsoft](https://learn.microsoft.com/es-es/windows-hardware/drivers/bringup/boot-screen-components)
explica cómo el firmware proporciona el gráfico y Windows puede conservarlo.
Publicar BGRT por sí solo no demuestra compatibilidad con Windows.
