# Fuentes y evidencia inicial

## Evidencia del teléfono

El resumen de [recovery-kernel.json](../evidence/recovery-kernel.json) se obtiene
del respaldo original del recovery. Contiene tamaños, huella del archivo de
referencia y una selección explícita de opciones de configuración, sin número
de serie, IMEI, cuentas ni contenido de usuario.

El dispositivo se verificó previamente como CPH2205/OP4F43L1, MT6779, proyecto
20295. Android F.72 México libre NV7B arrancó correctamente tras instalarse.
Estos resultados de Android no son una validación de EDK2.

## Fuentes primarias

- [linux-mediatek/edk2-mtk](https://github.com/linux-mediatek/edk2-mtk): base EDK2
  para MediaTek. En la revisión inicial, su lista de dispositivos no declaraba
  CPH2205 ni MT6779; el repositorio estaba archivado. Debe revisarse de nuevo y
  fijarse un commit antes de reutilizar código.
- [OPPO: kernel MT6779](https://github.com/oppo-source/android_kernel_oppo_mt6779):
  la rama encontrada corresponde a F19 Pro/Android 12.1. La equivalencia con el
  kernel F.72 de este teléfono todavía debe comprobarse.
- [Linux: mt6779.dtsi](https://github.com/torvalds/linux/blob/master/arch/arm64/boot/dts/mediatek/mt6779.dtsi):
  descripción pública de parte del SoC; no sustituye el device tree de la placa.
- [AOSP: menú del recovery en Fastbootd](https://android.googlesource.com/platform/bootable/recovery/+/refs/heads/main/fastboot/fastboot.cpp):
  referencia de opciones y acciones. No es el código de la interfaz propietaria
  exacta que se observó en el OPPO.
- [Linux: requisitos de kexec](https://docs.kernel.org/admin-guide/kdump/kdump.html):
  la vía kexec necesita soporte en el kernel. Habilitarlo no demuestra que un
  payload EDK2 pueda recibir correctamente el control.
- [Moto G20 EDK2 UEFI](https://github.com/h0nda1337/Moto-G20_EDK2_UEFI): trabajo
  relacionado sobre Unisoc UMS512. Sus resultados y direcciones son específicos
  de esa plataforma.

## Preguntas abiertas

Entrada y nivel de excepción del payload; mapa y reservas de RAM; estado de
cachés y MMU; framebuffer entregado por LK; temporizador y watchdog; entradas de
usuario; mecanismo de arranque temporal; disponibilidad del recovery para cambios
de interfaz; compatibilidad de fuentes de kernel y módulos.
