# Publicación diaria

Repositorio de destino: `h0nda1337/OppoReno5Lite_EDK2_UEFI`.

La revisión diaria publica cambios reales y verificados de este proyecto. Antes
del commit se revisan los archivos, `git diff --check` y la comprobación de
publicación. Se conserva el trabajo local y remoto; no se usa force-push.

```console
git add README.md LICENSE .gitignore .gitattributes AGENTS.md docs scripts evidence
python scripts/check_publication.py
git diff --cached --check
```

Los commits describen la implementación y la validación que realmente se hizo.
Una prueba de PC se identifica como tal. Una imagen se considera ejecutada solo
cuando hay evidencia del dispositivo. No se publica una release o imagen como
funcional si no ha sido validada en el teléfono.

Los respaldos, firmware del fabricante, particiones, datos de calibración,
identificadores del equipo, credenciales y archivos personales se almacenan
fuera del repositorio. Los directorios de dependencias y compilación tampoco se
añaden automáticamente. La herramienta de comprobación reduce errores, pero
no reemplaza la revisión del contenido.

Las fotografías de la prueba MMU, de los cuatro primeros intentos integrados
y las del sexto y séptimo intento solicitadas
por el propietario, las dos del primer laboratorio microSD, las del segundo al séptimo
y la primera lectura física de la tarjeta en el intento 16, la carga rechazada del 17
y las solicitudes de apagado del 19 y 20, el error de ramdisk de Windows del 21
y el benchmark físico de lectura de cuatro bits del 24 y las dos capturas del
nuevo error de Windows del 25, del error con contadores SD del 26 y del perfil
GICv3 con ocho afinidades del 28, de la instalación ACPI del 30, de CPU/PSCI del 31
y las dos del intento combinado ACPI/SD con bugcheck de Windows del 32 y la
fotografía SD intermedia del diagnóstico de parámetros de WinPE del 33,
están revisadas y fijadas por ruta, tamaño y SHA-256 en
`check_publication.py`. Se retiraron los
metadatos personales de cámara; se conservaron orientación y perfil de color,
sin recomprimir los datos de imagen. Esa excepción corresponde exclusivamente
a `docs/images/native-mmu-probe.jpg`, `docs/images/native-dxe-shell-lab-01.jpg`,
`docs/images/native-dxe-shell-lab-02.jpg`, `docs/images/native-dxe-shell-lab-03.jpg`,
`docs/images/native-dxe-shell-lab-04.jpg`, `docs/images/native-dxe-shell-lab-06.jpg`,
`docs/images/native-dxe-shell-lab-06-detail.jpg`, `docs/images/native-dxe-shell-lab-07.jpg`,
`docs/images/native-sd-read-lab-08.jpg`, `docs/images/native-sd-read-lab-08-context.jpg`
`docs/images/native-sd-read-lab-09.jpg`, `docs/images/native-sd-read-lab-10.jpg`
`docs/images/native-sd-read-lab-11.jpg`, `docs/images/native-sd-read-lab-12.jpg`,
`docs/images/native-sd-read-lab-13.jpg`, `docs/images/native-sd-read-lab-14.jpg`
`docs/images/native-sd-power-lab-16.jpg`, `docs/images/native-sd-boot-lab-17.jpg`
`docs/images/native-sd-boot-lab-19.jpg`, `docs/images/native-sd-boot-lab-20-reset.jpg`
`docs/images/native-sd-boot-lab-20-context.jpg`, `docs/images/native-sd-boot-lab-21-reset.jpg`
`docs/images/native-sd-boot-lab-21-context.jpg`
`docs/images/native-sd-fast-read-lab-24.jpg`,
`docs/images/native-sd-fast-boot-lab-25-context.jpg`
`docs/images/native-sd-fast-boot-lab-25-error.jpg`,
`docs/images/native-sd-reset-trace-lab-26-reset.jpg`
`docs/images/native-sd-reset-trace-lab-26-error.jpg`
`docs/images/native-acpi-gic-profile-lab-28.jpg`
`docs/images/native-acpi-install-shape-lab-30.jpg`
`docs/images/native-cpu-psci-profile-lab-31.jpg`,
`docs/images/native-sd-acpi-boot-lab-32-read.jpg`
`docs/images/native-sd-acpi-boot-lab-32-bugcheck.jpg`
y `docs/images/native-winpe-crash-parameters-lab-33-read.jpg`;
las demás imágenes y binarios siguen
requiriendo revisión y no se aceptan automáticamente.

La tarea diaria revisa y publica avances existentes. No programa pruebas físicas,
reinicios, cambios de particiones ni operaciones de bloqueo o desbloqueo.
Si no hay cambios, no genera commits vacíos. Si hay un conflicto, problema de
validación o fallo de acceso, conserva los archivos y comunica el bloqueo.
