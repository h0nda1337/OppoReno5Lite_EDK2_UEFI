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

## 2. Primera ejecución nativa — pendiente

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
- [ ] Materializar la política en HOBs y en el mapa MMU de la plataforma.
- Revisar el protocolo de entrada de LK, estado ARM64, memoria y reservas.
- Evaluar la disponibilidad real de un arranque de prueba temporal.
- Escribir la entrada ARM64 de la propia plataforma EDK2, sin BootShim,
  conservando el FDT que entrega LK y con diagnóstico observable.
- Validar cachés, MMU, temporizador, interrupciones y watchdog en el teléfono.

Las dos primeras pruebas de transferencia de imágenes originales mediante
`fastboot boot` no demostraron ejecución desde RAM. Tras la prueba de recovery
se observó `lk_crash` y retorno automático a Android. Se debe investigar el
manejo del comando en LK o preparar una vía alternativa de diagnóstico.

El éxito de esta etapa requiere evidencia física. Una compilación correcta por
sí sola no demuestra que el payload se haya ejecutado.

## 3. UEFI Shell — pendiente

- Conseguir salida de consola visible y un mecanismo de entrada.
- Integrar el núcleo EDK2 y Shell con una descripción de plataforma coherente.
- Validar arranque repetible y salida mediante reinicio controlado.

El código específico se escribe aquí o se reutiliza de repositorios del
propietario tras comprobar autoría. Véase [ORIGIN.md](ORIGIN.md).

La primera prueba puede limitarse a RAM y consola. UFS, microSD y USB tendrán
validación independiente; su funcionamiento no se presumirá por compartir SoC.

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
