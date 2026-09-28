# Inventario de prueba: colocación libre

Actualizado: 2026-09-28. Sustituye la cuadrícula del primer prototipo.
Continúa siendo un laboratorio; la integración con el inventario del personaje se hará después.

## Probar en Unreal

1. Abrir `C:\Users\valen\Documents\Codex\inventory-work\prototype3.uproject`.
2. Abrir `Content/FirstPerson/Lvl_FirstPerson` y pulsar Play.
3. Pulsar **I** (o el control de apertura que hayas configurado).
4. Tomar una silueta y moverla libremente. Por defecto se mantiene pulsado el botón izquierdo.
5. Mantener **Q/E** para girar en ambos sentidos alrededor del centro.
6. Soltar: verde confirma; rojo cancela y conserva posición y ángulo originales.
7. Probar una venda en el hueco de la L o dentro del marco. Los bordes pueden tocarse.
8. Cambiar controles en el panel derecho haciendo clic en la acción y pulsando la nueva tecla o botón.

Hay un único espacio cuadrado de 560 × 560 unidades lógicas. La interfaz completa se adapta
al tamaño de pantalla; ni la posición ni el giro se ajustan a una cuadrícula.
Incluye dos instancias de la venda real y tres siluetas de prueba: L, barra y marco.

## Controles y preferencias

Todas las acciones del laboratorio se pueden reasignar a teclas o botones del ratón:
abrir/cerrar, agarrar/soltar, girar a izquierda/derecha, cancelar, retirar y añadir venda.
La rueda también se admite para girar (2 grados por paso). Las teclas o botones mantenidos
giran continuamente, con velocidad ajustable de 15 a 360 grados por segundo.

Valores iniciales:

| Acción | Control |
|---|---|
| Abrir/cerrar | I |
| Agarrar/soltar | Botón izquierdo |
| Girar izquierda/derecha | Q / E |
| Cancelar colocación | Botón derecho |
| Retirar selección | Supr |
| Añadir venda | B |

El modo de agarre se elige entre mantener pulsado o pulsar una vez para tomar y otra para
soltar. Para reasignar a un botón del ratón, hacer clic en la fila y después pulsar ese botón.
Las asignaciones duplicadas se rechazan con una explicación. Durante la asignación, el
enlace «Cancelar asignación» permite salir sin cambiarla. «Restaurar controles» recupera
los valores iniciales. Los botones del panel se activan con el clic habitual de la interfaz.

Las preferencias se guardan en la configuración local GameUserSettings, sección
`/Script/prototype3.InventoryInputSettings`, y sobreviven al reinicio.
Las pruebas usan preferencias aisladas y no sobrescriben las del jugador.
Esta versión admite teclado y ratón, con una entrada por acción; no incluye mando ni combinaciones de teclas.

## Colocación y siluetas

- Posición continua del centro y ángulo en grados, normalizado a [0, 360).
- Giro alrededor del centro del rectángulo que encierra la silueta completa.
- La silueta usada para dibujar, seleccionar y detectar colisiones es la misma.
- Las partes vacías, las concavidades y los agujeros permanecen utilizables.
- Contacto de bordes o vértices válido; cualquier superposición de superficie se rechaza.
- La figura completa debe quedar dentro del espacio, también después de rotarla.
- La previsualización no cambia el inventario. Al soltar en rojo, cancelar, cerrar o perder
  el foco/captura se mantiene la colocación original, incluido el ángulo.
- El centro no salta al agarrar desde un extremo. El desplazamiento entre cursor y centro
  se mantiene durante el movimiento y el giro.

Las formas y tamaños son provisionales. El icono del ítem no determina automáticamente su
colisión: cada perfil contiene la geometría explícita. Aquí se dibuja esa geometría directamente.
Para futuros gráficos habrá que definir siluetas que correspondan a su contorno.

## Contrato técnico

`UItemDefinition` y `FItemInstance` permanecen como contrato compartido.
La entrada conserva la instancia, el bolsillo, el perfil, `FVector2D Position` y
`double AngleDegrees`; ya no usa casillas enteras ni cuartos de vuelta.

`FInventoryItemProfile.ShapeParts` representa la unión de polígonos convexos rellenos,
con coordenadas locales alrededor del centro. La descomposición permite formas cóncavas
y agujeros sin llenar su rectángulo exterior. No hay una resolución de casillas escondida.
Los perfiles aceptan hasta 64 partes de 256 vértices, con coordenadas locales hasta ±4096.
La tolerancia numérica de contacto es 0,0000001 unidades lógicas.

Los perfiles se validan al registrar. Se rechazan geometrías vacías, no finitas, degeneradas,
no centradas y partes cóncavas o autointersectadas. Cada parte debe ser convexa y ordenada;
una figura cóncava se describe con varias partes. Los perfiles y bolsillos registrados no
pueden reemplazarse. Las consultas devuelven copias.

`AddItem`, `MoveItem`, `CheckPlacement` y `CheckMove` reciben centro y ángulo.
Los movimientos conservan identidad y cantidad y son atómicos, también entre bolsillos
del mismo componente. La UI de esta entrega solo muestra el primer y único bolsillo de
la demostración. Los fallos no modifican entradas ni retiran el original.

## Archivos y verificación

- `Gameplay/Player/Inventory/`: geometría, componente y pruebas.
- `UI/Inventory/`: pantalla, datos de prueba y preferencias.
- `Core/PlayerControllers/prototype3PlayerController`: apertura con la asignación vigente y restauración de controles.
- `Scripts/VerifyInventory.ps1`: compilación, pruebas y capturas.

Ejecutar desde la ruta corta `Scripts/VerifyInventory.ps1 -Capture -PlayTest`.
`-SkipBuild` reutiliza una compilación actual. Las pruebas se ejecutan en un editor de
verificación independiente. No se deben ejecutar dentro de una sesión Play con trabajo manual.
El informe y las imágenes quedan en `Saved/InventoryVerification`.

Verificación del 2026-09-28: compilación Development Editor correcta en Unreal 5.8.1; las 12 pruebas pasaron (11 sin avisos y una con el aviso ya conocido de r.MotionVectorSimulation). Se revisaron las cuatro capturas: inicial, colisión, colocación válida dentro del marco y resultado confirmado. La prueba en Play comprobó apertura/cierre/reapertura con una tecla reasignada, foco y restauración de controles.

Corregido el giro que cancelaba el agarre al volver a capturar el ratón. La nueva prueba
`RotationPlayIntegration` reproduce entradas a través de Slate y espera fotogramas reales
de Play: Q/E, botón de ratón reasignado, rueda, agarre mantenido o por clic, soltar el
control de giro sin soltar el objeto, cancelar, perder captura/foco y soltar fuera del espacio.
Fallaba antes de la corrección y ahora pasa. Las preferencias guardadas del jugador se
conservaron. El usuario ya aprobó el movimiento libre y las colisiones; falta su prueba
manual del giro corregido.

## Alcance pendiente

El laboratorio conserva objetos al cerrar y reabrir durante Play, y los descarta al terminarlo.
Todavía no implementa equipo, recogida del mundo, pilas/divisiones, transferencias entre
componentes, partidas guardadas, red, peso sobre movimiento ni acciones de curación.
Las preferencias de controles sí se guardan. Los assets de ítems y del mapa no se modifican.
