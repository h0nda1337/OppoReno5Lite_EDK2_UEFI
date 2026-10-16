# Etapas del port

## 1. Inventario y preparación — en curso

- [x] Registrar modelo, SoC, proyecto y firmware de referencia.
- [x] Extraer del respaldo la configuración real del kernel del recovery.
- [x] Confirmar que el recovery stock no tiene `CONFIG_KEXEC` habilitado.
- [x] Separar el repositorio público de los respaldos privados.
- [x] Fijar 20 % como batería mínima y comprobar sus límites en PC.
- [x] Extraer del DTB base CPU, GIC, UART, watchdog y reservas estáticas.
- [x] Compilar una biblioteca inicial de plataforma para ARM64 y comprobar
  propuestas de región de carga en PC.
- [ ] Capturar, mediante lectura, el device tree activo, reservas de memoria,
  datos de pantalla, parámetros de arranque y mecanismos de reinicio.
- [ ] Fijar revisiones y revisar las licencias de los componentes que se reutilicen.

## 2. Primera ejecución nativa — pendiente

- Revisar el protocolo de entrada de LK, estado ARM64, memoria y reservas.
- Evaluar la disponibilidad real de un arranque de prueba temporal.
- Adaptar un BootShim y una plataforma mínima con diagnóstico observable.
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

La primera prueba puede limitarse a RAM y consola. UFS, microSD y USB tendrán
validación independiente; su funcionamiento no se presumirá por compartir SoC.

## 4. Opción en Fastbootd — pendiente

- Estudiar cómo el recovery OPPO construye el menú y despacha sus acciones.
- Evaluar adaptación del recovery o una interfaz personalizada con Fastbootd.
- Implementar un mecanismo real de transferencia a EDK2. Kexec requeriría un
  kernel compatible, un cargador adecuado y validación del estado de hardware.
- Conectar la opción **UEFI Shell** únicamente cuando exista un payload probado.

El protocolo Fastbootd y la interfaz gráfica del recovery son componentes
distintos. Añadir una etiqueta al menú no proporciona por sí mismo el arranque.
