# Procedencia del código

El propietario pidió código propio para la entrada ARM64 y los controladores
específicos del OPPO, sin BootShim. También autorizó reutilizar implementaciones
suyas de sus repositorios, comprobando autoría y conservando las atribuciones
que correspondan. No se incorporarán implementaciones ajenas adicionales.

EDK2 sigue siendo el núcleo base del proyecto. Sus fuentes y herramientas de
compilación son dependencias externas identificadas, con sus propias licencias.
El kernel original solo se empleó como firmware privado para el inventario
Linux; no se publica ni se presenta como código propio.

Las bibliotecas de plataforma, BGRT y framebuffer, la entrada SEC ARM64, sus
pruebas, el PID 1 mínimo y las herramientas Python de este repositorio son
implementaciones originales del proyecto. Las fuentes
de OPPO, AOSP, Linux y edk2-mtk consultadas sirven para investigar formatos e
interfaces. Consultar una referencia no autoriza copiar su implementación.

La opción del menú Fastbootd se integrará después de validar la Shell y una
salida fiable a Android. El menú no sustituye esa validación.
