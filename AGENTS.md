# Instrucciones del proyecto

- Mantener las fuentes, dependencias y compilaciones voluminosas en G:.
- Distinguir resultados de inspección, compilación, simulación y ejecución física.
  No declarar que UEFI arranca sin evidencia del OPPO.
- Reutilizar direcciones de hardware solo con evidencia específica de MT6779/CPH2205.
- Conservar licencias y atribuciones de código importado.
- Mantener los respaldos y datos de dispositivo fuera de este repositorio público.
- Antes de publicar: revisar el diff, ejecutar `python scripts/check_publication.py`
  sobre el índice preparado y `git diff --cached --check`.
- Publicar cambios verificados sin commits vacíos ni force-push. Resolver un
  conflicto conservando el trabajo de todas las partes.
- La revisión diaria de publicación no debe operar sobre teléfonos. Las pruebas
  físicas se coordinan en el chat activo con el propietario y seleccionan siempre
  explícitamente el dispositivo objetivo; no reutilizar herramientas de flasheo
  anteriores como pasos de preparación.
