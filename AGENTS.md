# Instrucciones del proyecto

- Mantener las fuentes, dependencias y compilaciones voluminosas en G:.
- El propietario fijó 20 % como batería mínima para operar sobre el teléfono.
  Comprobar el nivel antes de cada fase y detener las operaciones del teléfono
  si está por debajo; se puede continuar el trabajo de PC mientras carga.
- Distinguir resultados de inspección, compilación, simulación y ejecución física.
  No declarar que UEFI arranca sin evidencia del OPPO.
- Reutilizar direcciones de hardware solo con evidencia específica de MT6779/CPH2205.
- Conservar licencias y atribuciones de código importado.
- El propietario exige código propio para la entrada ARM64 y los controladores
  específicos del port. Se permite reutilizar código suyo de sus repositorios
  comprobando autoría; no incorporar implementaciones ajenas ni usar BootShim.
  EDK2 y las herramientas siguen siendo dependencias base identificadas;
  las referencias externas sirven para investigación.
- Avisar al propietario antes de empezar la integración en el menú Fastbootd:
  quiere cambiar a Astra para esa fase. Mientras tanto continuar con el
  arranque nativo y la Shell, sin iniciar esa integración.
- Mantener los respaldos y datos de dispositivo fuera de este repositorio público.
- Antes de publicar: revisar el diff, ejecutar `python scripts/check_publication.py`
  sobre el índice preparado y `git diff --cached --check`.
- Publicar cambios verificados sin commits vacíos ni force-push. Resolver un
  conflicto conservando el trabajo de todas las partes.
- La revisión diaria de publicación no debe operar sobre teléfonos. Las pruebas
  físicas se coordinan en el chat activo con el propietario y seleccionan siempre
  explícitamente el dispositivo objetivo; no reutilizar herramientas de flasheo
  anteriores como pasos de preparación.
