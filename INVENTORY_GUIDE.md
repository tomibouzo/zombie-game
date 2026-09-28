# Inventario: primer prototipo sobre el contrato de ítems

2026-09-27. Rama `feature/grid-inventory`, basada en PR #8 (`626491e`).

## Probar dentro de Unreal

1. Abrir `C:\Users\valen\Documents\Codex\inventory-work\prototype3.uproject`.
   Es el acceso corto al worktree del inventario; la copia del Escritorio tiene otra rama.
2. Después de compilar, abrir `Content/FirstPerson/Lvl_FirstPerson` y pulsar **Play**.
3. Hacer clic en la vista del juego y pulsar **I**. Aparece el laboratorio del inventario.
4. Arrastrar una pieza entre los dos bolsillos. Verde indica una colocación válida;
   rojo indica colisión o falta de espacio. Una operación rechazada conserva el original.
5. Pulsar **R** mientras se arrastra para girar. También se puede seleccionar con un clic
   y después pulsar R o el botón Girar para girar en la posición actual.
6. Probar una venda dentro del hueco de la pieza L. Solo las casillas ocupadas bloquean.
7. **Añadir venda** prepara una instancia; un clic en el bolsillo intenta colocarla.
   **Botón derecho** cancela. **Supr** o **Retirar** elimina la selección de esta demostración.
8. **I** o **Cerrar I** devuelve el control al juego. Volver a abrir conserva los objetos
   durante esa sesión. Detener Play descarta todos los datos de prueba.

La pantalla usa un inventario de demostración propio, no modifica el asset de venda ni
el equipo real del personaje. Tiene dos vendas reales y dos definiciones transitorias
de piezas de prueba. Las masas/categorías de esas piezas son datos de prueba.
La venda usa un perfil **provisional 1 x 1**; no fija su tamaño final ni deriva tamaño del icono.
La pantalla actual está preparada para estos dos bolsillos y teclado/ratón local.

## Contrato y responsabilidades

- El `UItemDefinition` y el `FItemInstance` del compañero permanecen sin cambios.
- `FInventoryItemProfile`: referencia a la definición, casillas normalizadas, permiso de
  rotación e indicador provisional. Se registra una vez por ID en el componente.
- `FInventoryEntry`: instancia completa, ID de perfil, bolsillo, posición y cuartos de vuelta.
- `FInventoryPocket`: ID y dimensiones. El acceso y la compatibilidad específica quedan pendientes.
- `UInventoryComponent`: registro, validación, añadir, mover, retirar, consultas y evento de cambio.

Los perfiles y bolsillos se copian al registrar y no pueden reemplazarse mientras contienen
objetos. Las consultas devuelven copias. Las definiciones compartidas se consideran inmutables
durante la partida, tal como indica el contrato de ítems.

La posición usa casillas enteras, origen arriba a la izquierda, X hacia la derecha y Y hacia abajo.
Una forma se define por casillas ocupadas; el mínimo X y el mínimo Y deben ser cero,
aunque `(0,0)` puede ser un hueco. Los giros admitidos son 0, 1, 2 y 3 cuartos de vuelta.
Las dimensiones de bolsillo y coordenadas locales de forma están limitadas a 256 por eje.
Cada objeto debe caber entero en un único bolsillo. No hay búsqueda automática de huecos.

`AddItem` recibe una instancia ya creada con `FItemInstance::Create`; la entrada conserva su ID.
Rechaza IDs duplicados dentro del mismo componente y perfiles de una definición diferente.
El llamador entrega la instancia al inventario solo si la operación tiene éxito. No hay todavía
un gestor global de propietarios: no se debe insertar la misma copia en dos componentes.
`RemoveItem` devuelve la instancia completa. Un futuro traslado entre componentes debe ser
una operación atómica propia; no debe implementarse con una eliminación seguida de un alta que pueda fallar.
`MoveItem` sí es atómico para movimientos dentro del componente, incluso entre sus bolsillos.

No se han recuperado la antigua definición de ítems, penalizaciones de carga ni configuración
del editor del commit revertido. Esta etapa no implementa pilas/divisiones, guardado, red,
curación, pickups, equipo, animación de mochila ni peso sobre movimiento.

## Archivos

- `Source/prototype3/Gameplay/Player/Inventory/`: núcleo y pruebas.
- `Source/prototype3/UI/Inventory/`: datos transitorios y pantalla de laboratorio.
- `Core/PlayerControllers/prototype3PlayerController`: tecla I y cambio entre control del juego y UI.
- `Scripts/VerifyInventory.ps1`: compilar y ejecutar `Prototype.Items` y `Prototype.Inventory`.

## Verificación

Resultado del 2026-09-27: compilación Development Editor correcta en Unreal 5.8.1.
Las **nueve pruebas** de ambos grupos pasaron, incluidas la captura gráfica y la integración
en Play (apertura, cierre mediante I y reapertura). El renderizador emitió un aviso de
`r.MotionVectorSimulation` durante Play; la prueba no tuvo errores.
Se revisaron las capturas inicial, colisión y colocación válida. No se ha hecho una sesión
manual prolongada de jugabilidad ni se ha publicado un commit.

Desde la ruta corta, ejecutar `Scripts/VerifyInventory.ps1` (PowerShell), o los grupos
`Prototype.Items` y `Prototype.Inventory` en Session Frontend > Automation.
`-Capture` habilita renderizado y guarda imágenes de la pantalla en
`Saved/InventoryVerification/`; `-SkipBuild` reutiliza una compilación actual.
`-PlayTest` abre el mapa en un editor de verificación independiente, inicia Play,
comprueba apertura/cierre/reapertura, foco del teclado y restauración de controles.
Se pueden combinar los tres parámetros. Esta prueba no debe lanzarse desde una sesión
de Play que contenga trabajo manual en curso.
Las pruebas normales usan NullRHI y omiten explícitamente la captura gráfica.

Las pruebas cubren límites, huecos, giro, colisión, identidad, errores sin mutación,
validación de perfiles/bolsillos, integración con el asset real y eventos de interacción
de la pantalla. La prueba de eventos llama los manejadores de Slate: no equivale a una
sesión manual completa con ratón y foco del sistema operativo. La comprobación opcional
de Play usa el controlador y la ventana de juego reales, y envía la tecla de cierre por Slate.
