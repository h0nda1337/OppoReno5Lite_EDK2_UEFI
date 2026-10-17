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
por el propietario están revisadas y fijadas por ruta, tamaño y SHA-256 en
`check_publication.py`. Se retiraron los
metadatos personales de cámara; se conservaron orientación y perfil de color,
sin recomprimir los datos de imagen. Esa excepción corresponde exclusivamente
a `docs/images/native-mmu-probe.jpg`, `docs/images/native-dxe-shell-lab-01.jpg`,
`docs/images/native-dxe-shell-lab-02.jpg`, `docs/images/native-dxe-shell-lab-03.jpg`,
`docs/images/native-dxe-shell-lab-04.jpg`, `docs/images/native-dxe-shell-lab-06.jpg`,
`docs/images/native-dxe-shell-lab-06-detail.jpg` y `docs/images/native-dxe-shell-lab-07.jpg`;
las demás imágenes y binarios siguen
requiriendo revisión y no se aceptan automáticamente.

La tarea diaria revisa y publica avances existentes. No programa pruebas físicas,
reinicios, cambios de particiones ni operaciones de bloqueo o desbloqueo.
Si no hay cambios, no genera commits vacíos. Si hay un conflicto, problema de
validación o fallo de acceso, conserva los archivos y comunica el bloqueo.
