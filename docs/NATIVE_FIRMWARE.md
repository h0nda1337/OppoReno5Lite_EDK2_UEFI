# Prototipo de firmware nativo

`OppoNative.dsc` y `OppoNative.fdf` componen un prototipo para compilar e
inspeccionar en PC. No son instrucciones de instalación. Todavía no se ha
observado su ejecución en el OPPO ni se ha probado su salida de recuperación.

La imagen tiene 8 MiB y comienza en `0x40080000`. Su cabecera ARM64 salta
directamente al volumen FV situado en `0x40081000`; el vector de ese volumen
entra en la SEC propia. No hay BootShim, un cargador de otro port, una copia
del firmware a otra dirección ni un payload concatenado.

## Implementación propia

- **SEC:** conserva el FDT, comprueba la plataforma y el estado de entrada,
  contrasta direcciones de GIC y watchdog con el árbol recibido, interpreta las
  reservas y propone el pool inicial. Construye HOBs y un mapa de identidad
  acotado, habilita MMU/cachés y llama al cargador del núcleo DXE de EDK2.
- **GOP:** consume exclusivamente el HOB propio con geometría validada y ofrece
  un modo, consultas y las cuatro operaciones BLT mediante la biblioteca
  original de framebuffer. No inicializa el panel ni cambia sus relojes.
- **Servicios iniciales:** proporciona solicitudes de reinicio y apagado PSCI.
  Las operaciones RTC devuelven expresamente `EFI_UNSUPPORTED`.
- **BDS de laboratorio:** conecta controladores, selecciona la consola gráfica
  y busca la Shell dentro del FV. Solicita un reinicio mediante un evento de
  90 segundos para las primeras pruebas.

Los núcleos DXE y runtime, las primitivas ARM, GIC, temporizador, HII, consola,
variables en RAM y la Shell proceden de la dependencia base EDK2. No se importó
código de otro port ni de controladores específicos ajenos. Véase [ORIGIN.md](ORIGIN.md).

## Límites del perfil

El pool inicial de 64 MiB empieza en `0x43000000`. Es una propuesta contrastada
con la captura Linux existente; no representa toda la RAM disponible para un
OS. Las reservas estáticas se protegen con los tipos de memoria correspondientes;
las solicitudes dinámicas quedan diferidas al OS. La dirección real del FDT se
valida y protege al recibir el control.

El perfil de pantalla usa la base del tag nativo de LK y la geometría observada
por el diagnóstico Linux: 1080 × 2400 y stride 4352. Su continuidad antes de
Linux sigue siendo una hipótesis que requiere prueba física. Las barras de
etapa nativas se sitúan desde la fila 160; la barra de la fotografía anterior
pertenece al diagnóstico Linux y no demuestra ejecución de este firmware.

El modo del watchdog se trata con la clave `0x22000000` y el bit de habilitación
documentados en el
[kernel OPPO inspeccionado](https://github.com/oppo-source/android_kernel_oppo_mt6779/blob/0e1e01b9fb281b02d1f50f12ea475d881f3fafd5/drivers/watchdog/mtk_wdt.c).
La implementación es propia. Este acceso, las llamadas PSCI y el funcionamiento
de GIC y temporizadores todavía no se probaron desde la entrada nativa.

El temporizador BDS depende de que funcionen esas piezas. No garantiza que una
excepción temprana o un bloqueo vuelva a Android; el reset tampoco demuestra
que LK abandone el modo recovery. Antes de una instalación física se necesita
una estrategia de prueba y restauración independiente de ese temporizador.

No hay controladores de almacenamiento, teclado, botones ni USB nativo, ni
persistencia de variables, ACPI o arranque de Windows. La consola de entrada
genérica puede existir sin un dispositivo de entrada conectado. Este prototipo
no integra una opción en el menú Fastbootd; se avisará al propietario antes de
empezar esa fase para que pueda cambiar a Astra.

## Comprobaciones

La compilación y la inspección del contenedor son pasos distintos de ejecutar
el firmware. El registro público de resultados debe indicar cada uno por
separado. El inspector comprueba la cabecera ARM64, límites y checksums FV/FFS,
módulos AArch64 requeridos y la correspondencia entre el salto del FV y la
entrada TE de la SEC.

La prueba del GOP enlaza el controlador real con servicios UEFI simulados y
RAM sintética. Comprueba el rechazo de HOBs incorrectos, geometría, padding,
consultas de modo y las cuatro operaciones BLT con ASan/UBSan. No accede a la
pantalla física. Las pruebas QEMU de la entrada SEC usan una rutina C de prueba;
no ejecutan este núcleo DXE ni su Shell.

Los comandos se encuentran en [BUILD.md](BUILD.md). El script de compilación
no empaqueta un recovery ni utiliza ADB, Fastboot o herramientas MediaTek.
