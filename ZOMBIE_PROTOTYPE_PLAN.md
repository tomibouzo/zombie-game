# Primer prototipo de zombis

Fecha: 2026-10-09. Plan aprobado e implementado en `codex/zombie-prototype`, basado en `main` (`2e03817`). La guía vigente de uso y verificación es [ZOMBIE_GUIDE.md](ZOMBIE_GUIDE.md). Los apartados siguientes conservan el diseño y la revisión previos a la implementación.

## Base técnica verificada antes de implementar

- Checkout limpio al iniciar la revisión, rama `inventory/item-use`, commit `8fd4f18`; coincide con su rama remota.
- `main` remoto está en `2e03817`. La comparación local, cuyo `origin/main` coincide con el remoto consultado, contiene 11 commits exclusivos de main y uno de esta rama. Main incluye cambios recientes de movimiento, controles e inventario.
- Se creó `codex/zombie-prototype` desde el main remoto verificado en este mismo checkout, que estaba limpio. La rama `inventory/item-use` conserva su historial. El acceso corto `inventory-work` apunta a este checkout; no se creó otro worktree.
- `HANDOFF.md` conserva un estado del 2026-10-01 y no describe la publicación actual; no usar sus referencias Git como base vigente.
- `ShooterAIController` ya usa AI Perception y StateTree. Sus componentes se configuran parcialmente en Blueprint; los assets no se inspeccionaron en el editor.
- `ShooterNPC` depende de armas, reglas del shooter y del personaje del jugador, que incorpora cámara, inventario y uso de objetos. Crear un zombi propio basado en `ACharacter` evita arrastrar esos sistemas.
- El jugador ya expone marcha, postura, salud y resistencia. `PlayerMeleeComponent` aplica daño puntual y bloquea golpes a través de paredes. No transmite una fuerza de interrupción independiente del daño.
- No se encontró emisión de ruido de pasos en el código del personaje del jugador. Sí existe ruido de armas/proyectiles en la variante shooter.
- UE 5.8 está instalado y hay infraestructura de pruebas automatizadas/PIE. Esta revisión no ejecutó compilación ni pruebas, ni verificó navegación del mapa en el editor.

## Comportamiento acordado

- Zombi común aislado manejable; camina en este primer prototipo.
- Visión y oído normales. Visión progresiva, confirmación rápida cerca, campo visual limitado y paredes que ocultan al jugador.
- Indicador sobre el zombi muestra sospecha y se vuelve rojo al detectar.
- Correr genera ruido a distancia moderada; caminar a distancia igual o algo menor. Radios configurables.
- Pasar agachado por detrás no alerta al básico. Agachado sigue siendo visible por delante y no elimina una búsqueda ya iniciada.
- Al perder contacto: ir a la última posición conocida, buscar alrededor y reaccionar a nuevas pistas. No consultar la posición actual oculta para dirigir la búsqueda.
- Secuencia repetida golpe, golpe, mordida. Breve detención al atacar.
- Impactos suficientemente fuertes interrumpen el ataque; umbral ajustable.
- Tiempos, velocidades, distancias, daño y umbrales provisionales, expuestos para ajustar en pruebas.
- Habilidades futuras combinables; la zona y el progreso podrán influir en su selección. Tamaños, arte final y generación se diseñarán después.
- Distinguir mordida para conectar radiación en el futuro. La barra y sus consecuencias no forman parte de esta entrega.

## Orden de implementación completado

1. Preparar la base desde main y revisar sus puntos de integración; crear personaje zombi, controlador y configuración editable. Representación geométrica provisional con orientación visible.
2. Preparar una zona de pruebas con suelo, esquinas, obstáculos y navegación. Validar que el zombi se desplaza hacia puntos accesibles y no atraviesa paredes.
3. Conectar visión/oído, ruido del movimiento real del jugador y acumulación de sospecha. Añadir indicador visible cuando no esté oculto por obstáculos, como valor inicial revisable.
4. Crear un StateTree específico para reposo/deambulación, investigación, persecución, búsqueda, ataque, interrupción y muerte. Mantener datos de contacto actual separados de la última pista registrada.
5. Añadir combate con preparación, impacto y recuperación. Comprobar objetivo vivo, distancia y obstrucciones al impactar; aplicar daño una vez por ataque y cancelar impactos pendientes al interrumpirse o morir.
6. Conectar los golpes existentes del jugador a la salud del zombi y a una fuerza de interrupción configurable separada del daño. Conectar daño del zombi a la salud existente del jugador.
7. Probar los escenarios de aceptación y entregar instrucciones cortas para Play y para cambiar parámetros. Ejecutar regresiones pertinentes de movimiento, daño e inventario.

Archivos: `Gameplay/AI/Zombies` para personaje/controlador/comportamiento/configuración, `PlayerNoiseComponent` para pasos y `HorrorHUD` para el indicador de sospecha. StateTree y navegación utilizan los sistemas de Unreal presentes en el proyecto.

## Reglas menores propuestas para las primeras pruebas

- Un ataque fallido avanza la secuencia; fuera de alcance se acerca antes de intentar el siguiente.
- Una interrupción durante la preparación cancela el impacto pendiente y termina ese intento. Su duración es configurable.
- Reiniciar la secuencia al abandonar por completo el encuentro; conservarla mientras persigue o busca al mismo objetivo.
- Una pista sonora orienta una investigación sin revelar automáticamente la posición actual de su emisor.
- Si no hay camino, detener/replantear la tarea; no trasladarse artificialmente ni atravesar obstáculos.
- Definir salud inicial y muerte mínima del zombi para comprobar combate. Detener su IA y ataques al morir, con presentación provisional.

Estas reglas se usan como valores iniciales revisables en el prototipo autorizado; no constituyen decisiones definitivas de diseño.

## Pruebas de aceptación

- Frente/lejos: sospecha progresiva; cerca: detección rápida; pared: visión bloqueada.
- Por detrás/agachado: no alerta; por delante/agachado: puede detectar.
- Caminar/correr: ruido dentro del radio correspondiente; estar quieto o correr contra una pared sin desplazarse no genera pasos.
- Tras doblar una esquina: acude a la posición observada, no sigue la posición secreta; un ruido nuevo puede desviar la búsqueda.
- Tras agotar la búsqueda: vuelve a la calma; al reencontrar al jugador, retoma persecución.
- Golpe, golpe, mordida: orden correcto, una aplicación de daño por ataque, fallos al apartarse o quedar tras una pared.
- Impacto bajo el umbral: no interrumpe; impacto suficiente: cancela el ataque antes de causar daño.
- Muerte del objetivo o del zombi: sin ataques ni daño diferido indebido.
- Cambios de configuración alteran el comportamiento sin editar código.

## Entrega esperada

Un zombi básico colocado en un escenario de pruebas, jugable con el personaje actual, indicador de sospecha y parámetros editables. La primera entrega debe validar este ciclo completo antes de ampliar el catálogo de habilidades.
