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

El renderer de `OppoDebugLib`, sus glifos 5×7, la página de cursor compartido y
la prueba de memoria protegida también son originales. El formateador utilizado
en el firmware sigue siendo el PrintLib genérico de EDK2; el del test de PC es
un stub propio acotado a sus casos. No se importó una fuente tipográfica externa.

`OppoMsdcLib`, el enlace MSDC1 `OppoSdDxe`, el diagnóstico SD del BDS y sus
modelos de prueba son originales. Los registros se investigaron en fuentes
públicas de OPPO; no se copiaron sus funciones. Las capas genéricas DiskIo,
Partition y FAT pertenecen a la dependencia base EDK2, con sus licencias.

La composición de `OppoNative.dsc` y `OppoNative.fdf` es original. Identifica
explícitamente los módulos genéricos de la dependencia base EDK2: núcleos,
primitivas ARM, GIC, temporizador, consola, HII, variables y Shell. Su revisión
de referencia es `b03a21a63e3bd001f52c527e5a57feddb53a690b`; no se presentan
esos componentes como implementaciones propias.

La opción del menú Fastbootd se integrará después de validar la Shell y una
salida fiable a Android. El menú no sustituye esa validación.
