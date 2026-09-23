# Inventario: núcleo por casillas

Base: merge `fea929b` (PR #5). Esta implementación usa formas por casillas,
contenedores con compartimentos y carga baja/media/alta según ocupación.
Todavía no es una pantalla de inventario jugable.

## Mapa del código

| Archivo | Responsabilidad |
| --- | --- |
| `Gameplay/Items/Data/ItemDefinition.h/.cpp` | Definición compartida: nombre, categoría, casillas, rotación y límite por pila. |
| `Gameplay/Player/Inventory/InventoryTypes.h` | Datos de contenedores, compartimentos y objetos guardados. |
| `Gameplay/Player/Inventory/InventoryComponent.h/.cpp` | Validar y ejecutar operaciones; calcular ocupación y nivel de carga. |
| `Core/Characters/prototype3Character.h/.cpp` | Crear el componente y aplicar sus multiplicadores a movimiento y gasto de resistencia. |
| `Gameplay/Player/Inventory/Tests/InventoryTests.cpp` | Seis pruebas automatizadas del núcleo. |

Las rutas de la tabla parten de `Source/prototype3/`. Para empezar a leer,
seguir `AddItem`, luego `CheckPlacement`, y finalmente `TransferItem`.

## 1. La forma de un objeto

Una definición es un Data Asset `ItemDefinition`; no es un actor del mundo.
Cada objeto guardado referencia esa definición y tiene un ID propio (`FGuid`).
Dos vendas comparten su definición, pero tienen IDs y posiciones diferentes.
Las definiciones deben permanecer sin cambios durante una partida.

Este rifle ocupa seis casillas dentro de un rectángulo de 4 × 2:

```text
A A A A
A . A .
```

`OccupiedCells`: `(0,0), (1,0), (2,0), (3,0), (0,1), (2,1)`.
Otra pieza puede ocupar `(1,1)` o `(3,1)`. El rectángulo exterior no bloquea
esos huecos. X crece hacia la derecha y Y hacia abajo.

`QuarterTurns` admite 0, 1, 2 o 3 giros de 90 grados. Tras girar se normaliza
la forma para conservar una posición de referencia en su esquina superior
izquierda. `bAllowRotation` permite deshabilitar esta operación por definición.

Los objetos no se apilan por defecto (`MaxStackSize = 1`). Para munición,
crear una definición por tipo compatible y elegir un límite, por ejemplo 30.
Sólo se combinan pilas que referencian exactamente el mismo Data Asset.
El inventario no decide compatibilidad a partir del nombre o la categoría.

## 2. Contenedores y compartimentos

El componente empieza vacío: no impone ropa, rarezas ni equipo inicial.
`InitialContainers` permite configurar los espacios de un actor desde Unreal.
También se pueden añadir en ejecución con `AddContainer`.

Cada contenedor necesita un `Id` único dentro de su componente. Cada uno de
sus compartimentos necesita un `Id` único dentro del contenedor y un tamaño.
Por ejemplo: contenedor `Bag`, compartimentos `Main` de 4 × 4 y `Front` de 2 × 2.
Un objeto debe caber entero en un compartimento; no puede repartirse entre capas.

- `Quick`: se accede directamente. Adecuado para cinturón de habilidad,
  riñonera y bolsillos, principalmente de pantalones.
- `RequiresOpening`: comienza cerrado. Se abre/cierra con `SetContainerOpen`.
  Adecuado para mochilas y contenedores externos. Añadir, retirar, mover y
  apilar objetos exige que ambos contenedores implicados sean accesibles.

Abrir es actualmente un cambio de estado instantáneo. No hay aún tiempo de
apertura, restricción por distancia, interfaz ni animación. La presentación
3D futura está fuera de las tareas actuales.

Los contenedores externos deben vivir en otro componente, en su actor del
mundo. Así no cuentan como carga del personaje. `TransferContainer` mueve un
contenedor entero entre componentes, conserva sus objetos y posiciones y lo
cierra al llegar. Permite guardar/dejar una mochila sin vaciarla. No implementa
el actor físico ni el equipamiento. No hay mochilas dentro de otras mochilas.

## 3. Operaciones y validación

`AddItem` crea una entrada con una cantidad que cabe en una sola pila. Para
tres vendas se llama tres veces, en posiciones distintas. No busca huecos ni
divide excedentes automáticamente.

`TransferItem` sirve para mover, rotar o trasladar a otro inventario. Si se
traslada una pila completa, conserva su ID. Si se traslada una parte, el resto
permanece en origen y la parte nueva recibe otro ID.

`StackItems` combina una cantidad explícita con una pila existente.
`RemoveItem` consume o retira unidades; para trasladar objetos se utiliza
`TransferItem`, evitando una secuencia manual de quitar y añadir.

El flujo es:

1. Comprobar que existen el objeto, contenedor y compartimento.
2. Comprobar cantidad y acceso.
3. Comprobar bordes y colisiones, usando sólo las casillas ocupadas.
4. Si todo es válido, actualizar origen y destino.
5. Avisar a los observadores mediante `OnInventoryChanged`.

Si algo falla, no se modifica ninguno de los dos lados. `EInventoryResult`
explica el motivo: cerrado, fuera de límites, ocupado, pila llena, etc.
`CheckPlacement` permite anticipar el resultado en una futura interfaz.
`GetContainers` devuelve copias para que la interfaz no edite el estado interno.

## 4. Carga simplificada: baja, media y alta

La carga usa casillas ocupadas / casillas disponibles. Los huecos de una forma
son libres; una pila ocupa su forma una sola vez, tenga una o treinta balas.
No hay pesos individuales, kilos ni un segundo límite de peso.

| Nivel | Ocupación predeterminada | Velocidad | Gasto de resistencia al correr |
| --- | --- | --- | --- |
| Baja | 0 a 1/3, inclusive | ×1 | ×1 |
| Media | Más de 1/3 hasta 2/3, inclusive | ×0,95 | ×1,15 |
| Alta | Más de 2/3 | ×0,85 | ×1,35 |

Son valores provisionales y ajustables en el componente. Se aplica a caminar,
correr, sprint y agacharse; sólo correr/sprintar añade gasto continuo de
resistencia. La recuperación y los ataques conservan sus reglas actuales.
El componente vacío no penaliza al personaje.

`GetContainerLoad` calcula el nivel de una mochila o contenedor concreto.
`GetLoad` calcula la ocupación conjunta de todos los contenedores del componente
del personaje y determina su penalización. Una mochila vacía adicional puede
reducir esa proporción: es una consecuencia intencional de este modelo
provisional por capacidad, no una simulación física del peso.

## 5. Probarlo desde Blueprints

1. Crear un Data Asset de clase `ItemDefinition` para venda, munición y rifle.
   Configurar sus formas y los límites 1, 30 y 1 respectivamente.
2. En un Blueprint de personaje derivado de `prototype3Character`, seleccionar
   el componente `Inventory` y añadir un contenedor a `InitialContainers`.
   Para una prueba neutral usar `Storage`, `Quick`, compartimento `Main` de 4 × 2.
3. En `BeginPlay`, después de la llamada al padre, obtener `GetInventoryComponent`
   y llamar `AddItem` con el rifle, cantidad 1 y posición `(0,0)`. Guardar su ID.
4. Añadir una venda en `(1,1)`: debe funcionar. Otra en `(2,1)` debe devolver
   `Occupied`. Mostrar el resultado y `GetFillRatio` con `Print String`.
5. Usar `TransferItem` con el ID guardado. Si no cabe en destino, comprobar que
   `GetItem` sigue encontrándolo en su posición original.
6. Repetir con `RequiresOpening`: antes de `SetContainerOpen(true)` las
   operaciones deben devolver `Closed`.
7. En una cuadrícula 3 × 1 vacía, añadir tres objetos de una casilla de uno en
   uno: los niveles serán baja, media y alta. Retirarlos debe restaurar la
   velocidad y el gasto de resistencia iniciales.

Estas son instrucciones de comprobación, no pruebas jugables ya realizadas.
No se han añadido assets ni modificado los mapas existentes.

## Validación y límites actuales

Se añadieron seis pruebas de Unreal bajo `Prototype.Inventory`:

- Formas irregulares, huecos, bordes y rotación.
- Traslados sin pérdida de objetos cuando fallan.
- Apilamiento, incompatibilidad y división de munición.
- Contenedores cerrados, capas y traslado del contenedor con su contenido.
- Niveles de ocupación y multiplicadores.
- Definiciones, cantidades e identificadores inválidos.

Estado de validación del 17/09/2026: revisión de código realizada; compilación
y ejecución de las pruebas pendientes. La primera compilación se detuvo por
rutas de más de 260 caracteres en el plugin VisualStudioTools. Con una ruta
corta, Windows bloqueó la carga de `prototype3ModuleRules.dll` mediante su
política de integridad de código (evento 3077, error 0x800711C7), antes de
compilar el C++ nuevo. No se modificó esa política.

Cuando la compilación esté habilitada, ejecutar `Scripts/VerifyInventory.ps1`
o compilar el objetivo `prototype3Editor` y ejecutar `Prototype.Inventory` desde
Session Frontend > Automation. Después comprobar jugabilidad: desplazamiento,
crouch, resistencia y ataques, tanto con inventario vacío como con carga.

En esta máquina, la copia de trabajo está en la rama `feature/grid-inventory`
y tiene un acceso corto en `C:\Users\valen\Documents\Codex\inventory-work`.
Es la misma carpeta mediante una unión de directorio, no una segunda copia.
Abrir el proyecto o ejecutar el script desde ese acceso evita las rutas largas.
El proyecto original del escritorio conserva su rama anterior. No se ha hecho
commit, push ni merge de estos cambios.

Esta fase no incluye interfaz, pickups, guardado/carga de partidas, red,
equipamiento, generación de rarezas ni animaciones 3D. Los bolsillos en prendas
superiores serán excepcionales; la asignación a ropa queda para una fase posterior.
