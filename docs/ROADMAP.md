# Etapas del port

## Recuperación de Android — completada; precauciones vigentes

- [x] Restaurar recovery original y comprobar sus 64 MiB mediante lectura.
- [x] Confirmar boot y recovery originales en una nueva sesión MediaTek.
- [x] Documentar las actualizaciones de claves hechas por el recovery durante
  el diagnóstico y suspender instalaciones que retengan el ramdisk OEM.
- [x] Verificar de nuevo Android tras el restablecimiento autorizado: arranque
  completo, bootloader desbloqueado y batería cargando por encima del mínimo.
- [x] Revisar servicios, montajes y propiedades AVB. El nuevo PID 1 no retiene
  servicios OEM ni monta sistemas de archivos persistentes.
- [x] Ejecutar el inventario mínimo, recibirlo por USB, volver a Android y
  restaurar de nuevo el recovery original; batería final 46 %.

## 1. Inventario y preparación — en curso

- [x] Registrar modelo, SoC, proyecto y firmware de referencia.
- [x] Extraer del respaldo la configuración real del kernel del recovery.
- [x] Confirmar que el recovery stock no tiene `CONFIG_KEXEC` habilitado.
- [x] Separar el repositorio público de los respaldos privados.
- [x] Fijar 20 % como batería mínima y comprobar sus límites en PC.
- [x] Extraer del DTB base CPU, GIC, UART, watchdog y reservas estáticas.
- [x] Compilar una biblioteca inicial de plataforma para ARM64 y comprobar
  propuestas de región de carga en PC.
- [x] Capturar el device tree activo, reservas declaradas, iomem y datos de
  pantalla mediante el diagnóstico Linux propio.
- [ ] Completar la interpretación de reservas dinámicas y el mapa propio de
  UEFI; validar estado de entrada y mecanismos de reinicio antes de Linux.
- [ ] Fijar revisiones y revisar las licencias de los componentes que se reutilicen.

## 2. Primera ejecución nativa — SEC, MMU y núcleo DXE observados

- [x] Escribir una entrada ARM64 propia y comprobar argumentos, pila y vectores
  mediante pruebas de sistema QEMU en EL1 y EL2.
- [x] Escribir operaciones BLT propias, comprobar límites con ASan/UBSan y
  construir las tres bibliotecas con EDK2 para AArch64.
- [x] Escribir el lector FDT propio en C, contrastarlo con la captura privada y
  compilar la cuarta biblioteca con EDK2 para AArch64.
- [x] Interpretar RAM, reservas, initrd, tags y PSCI con C propio y contrastar
  la quinta biblioteca contra esa captura sin operar el teléfono.
- [x] Definir y comprobar la política inicial: reservas fijas protegidas,
  dinámicas diferidas al OS y nodo vacío comprobado sin propiedades.
- [x] Contrastar en PC una propuesta de pool inicial e imagen con la captura,
  sin asignar memoria física ni considerarla un mapa completo de RAM.
- [x] Implementar HOBs y mapa MMU acotado en el prototipo SEC; código compilado,
  con validación física acotada mediante la variante corta MMU.
- [x] Componer el primer FD con entrada directa a SEC, núcleo DXE y Shell;
  comprobar cabeceras, módulos y entrada en PC.
- [x] Construir una variante corta de SEC separada de DXE y Shell y detectar,
  mediante expdb, el rechazo de su primer contenedor por falta de gzip.
- [x] Corregir y comprobar en PC el transporte gzip acotado, conservando el FD
  y la entrada propios. Restaurar y verificar el recovery tras el primer intento.
- [x] Observar la franja verde de SEC con gzip y confirmar el regreso automático
  a Android con arranque completo y motivo `reboot`; no se midió el reset PSCI.
- [x] Observar las tres franjas de la variante MMU, que comprueba HOBs, cambio
  de pila y bits M/C/I de SCTLR, y confirmar arranque completo de Android.
- [x] Contrastar el orden nativo rojo=16/azul=0 mediante las fotos de los dos
  candidatos: el azul dejó de verse rojo.
- [ ] Medir y validar en el OPPO esa materialización y una vía de restauración
  independiente de los temporizadores del prototipo.
- [x] Comprobar el reinicio manual con botones desde los diagnósticos detenidos,
  volver a Android y restaurar recovery original. Ese resultado no garantiza
  recuperación frente a cualquier fallo de hardware.
- [x] Observar el núcleo DXE mediante el registro de texto propio y localizar
  sus protocolos ausentes en fotografías revisadas.
- Revisar el protocolo de entrada de LK, estado ARM64, memoria y reservas.
- Evaluar la disponibilidad real de un arranque de prueba temporal.
- Validar la entrada ARM64 propia, sin BootShim, conservando el FDT que entrega
  LK y con diagnóstico observable.
- Validar cachés, MMU, temporizador, interrupciones y watchdog en el teléfono.

Las dos primeras pruebas de transferencia de imágenes originales mediante
`fastboot boot` no demostraron ejecución desde RAM. Tras la prueba de recovery
se observó `lk_crash` y retorno automático a Android. Se debe investigar el
manejo del comando en LK o preparar una vía alternativa de diagnóstico.

El éxito de esta etapa requiere evidencia física. Una compilación correcta por
sí sola no demuestra que el payload se haya ejecutado.

## 3. UEFI Shell — arranque visible; entrada pendiente

- [x] Integrar núcleo EDK2 y Shell en un prototipo de compilación, con GOP y BDS
  propios y variables solo en RAM.
- [x] Probar el controlador GOP real con servicios simulados y ASan/UBSan.
- [x] Observar texto de consola UEFI en el OPPO, mediante el mensaje de excepción.
- [ ] Implementar y verificar un mecanismo de entrada en el OPPO.
- [x] Verificar ejecución del núcleo DXE y de su diagnóstico sobre la plataforma física.
- [x] Observar el encabezado UEFI Interactive Shell v2.2 y el prompt Shell> en el OPPO.
- [ ] Ejecutar y comprobar comandos mediante la entrada física.
- [x] Probar el primer candidato integrado y registrar el bloqueo en las tres
  franjas de SEC; no se observó BDS/Shell ni reset automático en 142 segundos.
- [x] Añadir diagnóstico propio para localizar el bloqueo de carga o ejecución de DXE.
- [x] Corregir el bootstrap PCD y observar físicamente la carga de controladores,
  dejando Capsule como único protocolo arquitectónico ausente.
- [x] Compilar servicios Capsule propios que declaran operaciones no compatibles,
  comprobar el controlador con sanitizadores y verificar su PE generado.
- [x] Superar la comprobación arquitectónica del núcleo y llegar desde BDS a la
  carga y entrada de Shell.efi; el retorno Unsupported impidió inicializarla.
- [x] Verificar consola UEFI visible e inicialización de la Shell hasta el prompt.
- [x] Corregir el requisito HII y verificar sus valores efectivos en el build.
- [x] Probar en PC el temporizador temprano del BDS, la consulta del contador al
  regresar una aplicación y el ciclo de vida de su handle bajo sanitizadores.
- [x] Observar retorno automático a Android tras el rechazo de Shell con HII
  habilitado; restaurar recovery original y comprobar arranque completo.
- [x] Probar en PC la selección y activación explícita del modo de consola.
- [x] Observar instalaciones de ShellParameters/Shell y localizar la excepción
  posterior mediante el binario exacto y la fotografía del sexto intento.
- [ ] Verificar el plazo y retorno de BDS en el teléfono, sin depender de ellos
  como única vía de recuperación.
- Validar arranque repetible y salida mediante reinicio controlado.

El código específico se escribe aquí o se reutiliza de repositorios del
propietario tras comprobar autoría. Véase [ORIGIN.md](ORIGIN.md).

La primera prueba puede limitarse a RAM y consola. UFS, microSD y USB tendrán
validación independiente; su funcionamiento no se presumirá por compartir SoC.

### microSD y cargador ARM64 — en curso

- [x] Inspeccionar desde Android una tarjeta vfat con aplicación EFI ARM64.
- [x] Implementar lector MSDC1 propio mediante PIO y enlace Block I/O de solo lectura.
- [x] Probar capacidad, direccionamiento, fallos y aislamiento con modelos de PC.
- [x] Compilar variante ARM64 con particiones y FAT de la dependencia EDK2.
- [x] Verificar enumeración y lectura de la tarjeta desde el firmware físico.
- [x] Abrir y cargar BOOTAA64.EFI desde esa tarjeta en el OPPO.
- [x] Observar Windows Boot Manager y documentar el punto alcanzado.
- [x] Verificar lectura de cuatro bits a 13 MHz y comparar un prefijo de 16 MiB con Android.
- [x] Confirmar RAM adicional visible en el mapa UEFI: 2100 MiB libres antes del cargador.
- [ ] Avanzar desde la pantalla Recovery de Windows al arranque de WinPE.

Véase [MICROSD.md](MICROSD.md). No se ha iniciado la integración de Fastbootd.

### ACPI — generación en PC y perfil GICv3 observado

- [x] Construir RSDP/XSDT/FADT/DSDT/MADT/GTDT con código propio y validar bytes,
  checksums y AML en PC, sin instalar tablas.
- [x] Validar el perfil del FDT antes de leer registros GICv3.
- [x] Observar físicamente las ocho asociaciones entre afinidad y redistributor,
  volver a Android y restaurar el recovery original en el intento 28.
- [ ] Vincular el perfil observado al generador y comprobar su instalación UEFI.
- [ ] Validar CPU/PSCI y los demás requisitos de plataforma para el OS.

Véase [ACPI.md](ACPI.md). El error `0xc0000225` no tiene aún una causa demostrada.

## 4. Opción en Fastbootd — pendiente

- Avisar al propietario antes de iniciar esta fase: quiere cambiar a Astra.
- Estudiar cómo el recovery OPPO construye el menú y despacha sus acciones.
- Evaluar adaptación del recovery o una interfaz personalizada con Fastbootd.
- Implementar un mecanismo real de transferencia a EDK2. Kexec requeriría un
  kernel compatible, un cargador adecuado y validación del estado de hardware.
- Conectar la opción **UEFI Shell** únicamente cuando exista un payload probado.

El protocolo Fastbootd y la interfaz gráfica del recovery son componentes
distintos. Añadir una etiqueta al menú no proporciona por sí mismo el arranque.

## 5. Logo original y arranque de Windows — preparación en PC

- [x] Extraer de un firmware propio la marca OPPO y convertirla a BMP.
- [x] Construir y comprobar metadatos BGRT en PC y compilar la biblioteca ARM64.
- [ ] Dibujar el logo mediante GOP, reservar su BMP e instalar BGRT.
- [ ] Validar visibilidad y transición al cargador de Windows en el teléfono.

Esta etapa no implica que Windows ya pueda arrancar. Véase
[LOGO_BGRT.md](LOGO_BGRT.md).
