# Prototipo de zombi básico

## Abrir y jugar

Usar este checkout: `C:\Users\valen\Documents\Codex\inventory-work\prototype3.uproject` (es el acceso corto al proyecto de este chat).

1. Abrir `/Game/Zombies/Lvl_ZombieLab` en Unreal y pulsar Play.
2. El zombi usa el maniquí Manny del proyecto, con reposo, caminata, golpes y caída. La tercera animación es un ataque provisional para la mordida; no es una animación de mordida dedicada. Desactivar **Use Mannequin** en Details vuelve a la geometría con indicador frontal.
3. Acercarse agachado por detrás, caminar/correr cerca y cruzar su campo visual para comparar detección.
4. Usar la pared y la esquina para romper contacto. El zombi buscará desde su última pista y podrá escuchar nuevos pasos.
5. Acercarse para probar golpe, golpe y mordida. Apartarse durante la preparación evita el impacto si se sale del alcance.
6. Atacar con el control de ataque primario del jugador, con las manos vacías. Se mantienen los controles configurados por el usuario.

El mapa inicial habitual del proyecto se conserva. El laboratorio contiene un zombi colocado, suelo, obstáculos, iluminación y navegación. Se pueden duplicar zombis en el editor para probar grupos pequeños.

## Ajustar las pruebas

Perfil inicial ajustado por Codex el 2026-10-10 a petición del usuario: visión y pasos de alcance moderado, ataques algo más lentos para aprender a esquivarlos, e interrupción accesible con los puños en este laboratorio. Los valores de la tabla ya están configurados; los pasos siguientes sirven para cambiarlos después.

Seleccionar `Zombie_Basic_Edit_Tuning_Here` en el Outliner. En Details aparecen los parámetros de cuerpo, movimiento, visión, oído, búsqueda y combate; se puede buscar un nombre concreto, por ejemplo **Sight Radius**. Cambiar los valores antes de Play para conservarlos al guardar el mapa; cambios en instancias durante Play se pierden al terminar.

| Ajuste | Valor inicial | Efecto |
|---|---:|---|
| Max Health | 60 | Salud inicial; requiere reiniciar Play |
| Wander Speed / Pursuit Speed | 100 / 260 cm/s | Deambulación / acercamiento caminando |
| Wander Radius | 400 cm | Zona de deambulación alrededor del punto inicial |
| Idle Seconds | 8 en el laboratorio | Pausa inicial/entre recorridos; valor de clase 3 |
| Sight Radius / Sight Half Angle | 1200 cm / 65° | Alcance y semiancho del campo visual |
| Close Sight Radius | 250 cm | Límite de detección rápida |
| Close / Far Detection Seconds | 0,25 / 2 s | Extremos del tiempo de exposición; transición gradual según distancia |
| Hearing Radius | 700 cm | Tope auditivo del zombi; también limita el radio de cada ruido |
| Search Seconds / Radius | 12 s / 500 cm | Persistencia y área alrededor de la última pista |
| Investigate Seconds | 6 s | Tiempo para investigar una pista sin objetivo confirmado |
| Attack Range | 135 cm | Distancia entre centros; también se comprueba al impactar |
| Strike / Bite Damage | 8 / 12 | Daño a la salud |
| Strike / Bite Windup | 0,8 / 1,1 s | Preparación antes del impacto |
| Recovery Seconds | 1 s | Pausa después de cada ataque |
| Interrupt Threshold / Stagger Seconds | 10 / 0,7 s en el laboratorio | Fuerza mínima y duración de interrupción; umbral de clase 25 |

En el personaje del jugador, el componente **FootstepNoise** controla los radios de caminar (400 cm), correr/sprint (600 cm) y distancia entre pasos (120 cm). Se emiten pistas solo al desplazarse por el suelo; agachado no genera estas pistas. Son eventos para IA, independientes del audio de pasos.

El componente **Melee** del jugador tiene **Impact Strength**, inicialmente 10. El zombi colocado ya tiene umbral 10, de modo que un puñetazo puede interrumpirlo. Para comparar con un zombi que resista esos puños, subir su **Interrupt Threshold** a 25. Esta configuración facilita observar la mecánica; el balance definitivo se ajustará tras jugar. La fuerza y el umbral no cambian el daño a la salud.

## Reglas del prototipo

- Barra amarilla: sospecha. Barra roja: objetivo detectado y visible. BUSCANDO: sigue alerta, pero perdió contacto.
- El indicador solo se dibuja frente a la cámara, a distancia limitada y sin una pared que lo oculte.
- Ver de cerca confirma más rápido, con una transición gradual hacia la detección lejana. Se comprueban torso y cabeza: una cobertura baja no oculta una cabeza expuesta. Agacharse reduce la altura de esas comprobaciones y no vuelve invisible al jugador.
- Un ruido registra un lugar; no proporciona seguimiento de su emisor.
- La búsqueda termina si no aparecen más pistas. Una pista nueva puede prolongarla.
- Los ataques siguen golpe, golpe, mordida. Los intentos fallidos/interrumpidos avanzan el orden; abandonar el encuentro lo reinicia.
- El zombi se detiene y mantiene la dirección del ataque. Cada intento puede dañar una sola vez; alcance y obstáculos se comprueban al impactar.
- Un impacto fuerte cancela el daño pendiente. La muerte del zombi o del objetivo también cancela el ataque.
- La mordida contiene identidad de ataque y un evento `OnAttackHit`; la radiación todavía no se acumula ni tiene consecuencias.

## Implementación

- `Gameplay/AI/Zombies`: personaje, perfil editable, controlador/percepción y tareas/condiciones del StateTree.
- `/Game/Zombies/ST_ZombieBasic`: estados visibles y editables de reposo, deambulación, investigación, persecución, búsqueda, ataque, interrupción y muerte.
- `Gameplay/Player/PlayerNoiseComponent`: pasos basados en desplazamiento real y postura.
- `Gameplay/Combat/CombatImpact.h`: evento de daño con fuerza independiente. Compatible con receptores de daño puntual.
- `UI/HUD/HorrorHUD`: indicador proyectado sobre los zombis.

El perfil es por instancia y separa sentidos, movimiento y combate para admitir futuras habilidades combinables. Selección procedural de habilidades, olfato, mutaciones, tamaños y generación de poblaciones son trabajo posterior.

## Verificación

`Scripts/VerifyZombies.ps1 -Capture -Regression` compila, ejecuta pruebas reales en Play y regresiones de movimiento, ítems, inventario e interfaz. `-SkipBuild` reutiliza una compilación vigente. Resultados en `Saved/ZombieVerification`.

`-CreateFixtures` sirve únicamente para crear los dos assets desde cero en un checkout que aún no los tenga; rechaza sobrescribir assets existentes para proteger ediciones del mapa/StateTree.

Verificación final del 2026-10-10, tras ajustar el perfil: compilación Development Editor correcta y **3/3 pruebas de zombis aprobadas**, sin pruebas omitidas. Informe UTC `2026.10.10-16.27.58`, en `Saved/ZombieVerification/Tests/index.json`. Incluye StateTree, navegación alrededor de obstáculos, visión, pasos con entradas reales, búsqueda, secuencia de daño, interrupciones y muerte. También se guardó y recargó el mapa para comprobar los valores persistidos; registro en `Saved/ZombieVerification/Tuning/applied.json`. Se revisaron las capturas `suspicion.png` y `detected.png`. Hay avisos de Unreal sobre `r.MotionVectorSimulation` y creación del gestor de multitudes al cerrar Play; las comprobaciones de navegación en ejecución pasan. No se ha validado un build empaquetado ni una sesión manual prolongada.

Regresión general del 2026-10-09: 37 de 41 pruebas aprobadas. Fallan `DropPlayIntegration`, `ItemUsePlayIntegration`, `PlayIntegration` y `RotationPlayIntegration` del inventario. Las cuatro se reprodujeron con exactamente los mismos mensajes de error en una copia aislada de `main` (`2e03817`), compilada sin estos cambios. Informes comparables: `Saved/ZombieVerification/Regression/index.json` y `Saved/ZombieVerification/Baseline/index.json`. El comando con `-Regression` devuelve error mientras continúen esos fallos anteriores.

Limitaciones: maniquí y animaciones genéricas provisionales (incluida la tercera animación de la mordida), audición por radio sin simulación acústica de paredes, búsqueda local sencilla, objetivo actual limitado al jugador y sin cooperativo. No hay sistema de puertas, escalada, agarres ni optimización de hordas grandes.

## Pulido — 2026-11-02

El laboratorio se reconstruyó y guardó con navegación completa. Antes tenía suelo y límites de ±1975 cm, pero solo rutas en los cuatro tiles centrales (±988 cm); por eso la persecución podía no arrancar o no reanudarse fuera de esa zona. La construcción ahora espera el desbloqueo real de carga, verifica las cuatro esquinas y conserva actores, StateTree y preferencias de reconstrucción automática. No fue necesario alterar las pausas del combate ni reemplazar el StateTree.

La visión comprueba torso y cabeza según la postura; una cobertura baja no oculta una cabeza expuesta. La adquisición inicial ya no espera una notificación de AI Perception, y los tiempos de detección cambian gradualmente según distancia.

Manny utiliza animaciones existentes de reposo, caminata, dos golpes y caída. La tercera animación representa provisionalmente la mordida; no es una mordida dedicada. La presentación sigue los tiempos de preparación/recuperación y no aplica daño mediante notifies. El movimiento lo conserva la navegación, sin desplazar la cápsula mediante root motion. **Use Mannequin** permite volver a la geometría.

Para verificar:

```powershell
./Scripts/VerifyZombies.ps1 -Capture -Regression -DerivedDataCache InstalledNoZenLocalFallback
```

Si se modifica la distribución del laboratorio, añadir `-RebuildNavigation` para reconstruir y guardar las rutas. Este parámetro conserva el mapa; `-CreateFixtures` sigue rechazando sobrescribir assets existentes. La opción de caché es opcional, útil si Zen no responde.

Verificación fresca: compilación Development Editor correcta; reconstrucción de navegación 1/1; seis pruebas de zombis aprobadas en ejecución renderizada tras guardar/recargar el mapa. `ChaseRecovery` empieza sin calentamiento y repite cuatro ciclos de ataque/retirada, con recuperación de persecución en 2,2–2,5 s incluyendo las pausas y el desplazamiento comprobado. La captura `detected.png` muestra el maniquí con indicador. `Presentation` comprueba alternativa geométrica, animación de golpe, avance de preparación, posición de la raíz en una pose de golpe, cancelación por interrupción y animación de muerte. Se revisaron las capturas `mannequin-windup.png` y `mannequin-death.png`. El contacto visual usa una fase aproximada del clip; no es una calibración de animación final. Informe completo UTC 2026.11.02-23.30.25: 6/6, sin omitidas.

Regresión general fresca del 2026-11-02: **37/41**, con los mismos cuatro fallos del inventario enumerados arriba. Sus 16 mensajes de error coinciden exactamente con el informe histórico de main en `Baseline`; no se recompiló main de nuevo en esta sesión. No hay build empaquetado ni sesión manual prolongada. Un bloqueo transitorio de Smart App Control afectó una DLL intermedia; las compilaciones posteriores necesarias cargaron y ejecutaron las pruebas sin cambiar la configuración de seguridad.

