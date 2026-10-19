# Procedencia del código

El propietario pidió código propio para la entrada ARM64 y los controladores
específicos del OPPO, sin BootShim. También autorizó reutilizar implementaciones
suyas de sus repositorios, comprobando autoría y conservando las atribuciones
que correspondan. No se incorporarán implementaciones ajenas adicionales.

EDK2 sigue siendo el núcleo base del proyecto. Sus fuentes y herramientas de
compilación son dependencias externas identificadas, con sus propias licencias.
El kernel original solo se empleó como firmware privado para el inventario
Linux; no se publica ni se presenta como código propio.

Las bibliotecas de plataforma, BGRT, framebuffer, FDT, handoff y plan de memoria,
la entrada SEC ARM64, su inicialización C, GOP, servicios iniciales y BDS, sus
pruebas, el PID 1 mínimo y las herramientas Python de este repositorio son
implementaciones originales del proyecto. Las fuentes
de OPPO, AOSP, Linux y edk2-mtk consultadas sirven para investigar formatos e
interfaces. Consultar una referencia no autoriza copiar su implementación.

La herramienta `prepare_winpe_crash_hive.py` es código propio para preparar y
comprobar una copia privada del registro de WinPE. Utiliza libhivex 1.3.24 como
escritor y python-registry 1.3.1 como lector independiente. wimlib 1.14.4 y
7-Zip 26.02 se emplearon para actualizar e inspeccionar el contenedor WIM.
Son dependencias genéricas de host con sus propias licencias; no se incorporan
sus implementaciones al firmware. El WIM de Microsoft y los hives se conservan
fuera del repositorio público.

QEMU 10.0.13, el firmware AAVMF de Debian 2025.02-8+deb13u1 y mtools 4.0.48
se emplean únicamente como herramientas de comparación en PC. No emulan el
MT6779 ni aportan código específico al firmware del OPPO. Los símbolos públicos
de Microsoft se consultaron para inspeccionar el kernel privado de WinPE;
no se publican el PDB, el ejecutable ni su desensamblado.

El medio de comparación 35 se extrajo de los MSI/CAB oficiales del complemento
WinPE de Microsoft, sin ejecutar sus instaladores. El WIM y los cargadores se
usan como carga de prueba privada; no se redistribuyen ni forman parte de los
controladores propios. Su comando `ver` funcionó en QEMU; el OPPO terminó en
una parada de Windows. Esos resultados se documentan por separado.

El renderer de `OppoDebugLib`, sus glifos 5×7, la página de cursor compartido y
la prueba de memoria protegida también son originales. El formateador utilizado
en el firmware sigue siendo el PrintLib genérico de EDK2; el del test de PC es
un stub propio acotado a sus casos. No se importó una fuente tipográfica externa.
`OppoBootLogLib` reutiliza ese código propio para el informe previo al reset;
su límite de fase y sus modelos de servicios y memoria también son originales.

`OppoMsdcLib`, el enlace MSDC1 `OppoSdDxe`, el diagnóstico SD del BDS y sus
modelos de prueba son originales. Los registros se investigaron en fuentes
públicas de OPPO; no se copiaron sus funciones. Las capas genéricas DiskIo,
Partition y FAT pertenecen a la dependencia base EDK2, con sus licencias.

La consulta PIO de I²C5 y CRC-8 de `OppoSdPowerLib`, su perfil FDT y sus pruebas
también son originales. La referencia del fabricante solo se consultó para
los contratos del bus y los registros LDO. No se importó su controlador Linux.
La operación explícita de encendido a 3,0 V, su restauración ante fallos y su
modelo de PC también son implementaciones originales del proyecto.

La composición de `OppoNative.dsc` y `OppoNative.fdf` es original. Identifica
explícitamente los módulos genéricos de la dependencia base EDK2: núcleos,
primitivas ARM, GIC, temporizador, consola, HII, variables y Shell. Su revisión
de referencia es `b03a21a63e3bd001f52c527e5a57feddb53a690b`; no se presentan
esos componentes como implementaciones propias.

La opción del menú Fastbootd se integrará después de validar la Shell y una
salida fiable a Android. El menú no sustituye esa validación.
