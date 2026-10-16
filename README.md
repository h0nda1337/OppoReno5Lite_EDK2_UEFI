# OPPO Reno5 Lite — EDK2 UEFI

Port experimental de EDK2 para **OPPO Reno5 Lite CPH2205 / OP4F43L1**, proyecto
20295, MediaTek MT6779 (Helio P95). El objetivo es conseguir una UEFI Shell nativa
y después ofrecer una opción **UEFI Shell** en el recovery que muestra Fastbootd.

**Estado, 15 de octubre de 2026: investigación y preparación. Todavía no hay una
imagen UEFI compilada ni un arranque UEFI observado en este OPPO.**

| Componente | Estado comprobado |
| --- | --- |
| Modelo y plataforma | CPH2205 / OP4F43L1 / proyecto 20295 / MT6779 |
| Android de referencia | Android 13, ColorOS F.72, México libre NV7B |
| Configuración del recovery original | ARM64; `CONFIG_KEXEC` desactivado |
| EDK2 específico para MT6779 | Pendiente de desarrollo y validación |
| Entrada, memoria, temporizador y watchdog | Pendientes de análisis de plataforma |
| Pantalla y entrada para la Shell | Pendientes |
| Arranque temporal con `fastboot boot` | Sin probar en este modelo |
| Opción adicional en el menú de Fastbootd | Diseño propuesto; sin implementar |

Los respaldos de Android y la recuperación MediaTek ya se obtuvieron durante el
trabajo previo. Se mantienen fuera de este repositorio. El teléfono está cargando;
las pruebas físicas se reanudarán cuando su propietario indique que está listo.

## Primer objetivo

Identificar una entrada de arranque y un mapa de memoria válidos, producir una
imagen de diagnóstico específica para este hardware y observar su ejecución.
Después se incorporarán consola, Shell e integración con el recovery.

El trabajo [Moto G20 EDK2 UEFI](https://github.com/h0nda1337/Moto-G20_EDK2_UEFI)
aporta experiencia y componentes genéricos. Su plataforma Unisoc requiere otras
direcciones y otros controladores; sus imágenes no son imágenes para este OPPO.

## Herramientas disponibles

La herramienta inicial inspecciona una imagen Android de arranque o recovery ya
respaldada. Solo lee el archivo y escribe un resumen con campos públicos de
configuración; no utiliza ADB ni Fastboot.

```console
python scripts/inspect_boot.py /ruta/al/recovery.bin --output evidence/recovery-kernel.json
```

El resumen publicado está en [evidence/recovery-kernel.json](evidence/recovery-kernel.json).
La planificación y fuentes están en [docs/ROADMAP.md](docs/ROADMAP.md) y
[docs/RESEARCH.md](docs/RESEARCH.md).

## Publicación de avances

Se solicitó revisar y publicar diariamente los cambios verificados. El estado
de las pruebas debe distinguir siempre inspección, compilación y ejecución física.
Se publicarán fuentes, herramientas y documentación revisadas. Los días sin
cambios publicables no requieren commits vacíos. Véase [docs/PUBLISHING.md](docs/PUBLISHING.md).

## Licencias

Los archivos originales de este proyecto usan BSD-2-Clause. No se han importado
todavía EDK2, el kernel OPPO ni código del G20. Las incorporaciones futuras deben
conservar sus licencias y atribuciones originales.
