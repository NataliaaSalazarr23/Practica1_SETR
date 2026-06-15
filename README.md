# Practica1_SETR
Práctica 1 de Sistemas embebidos en tiempo real

Emilio Hernández Santana 10095  

Natalia Marian Salazar Domínguez 10073


## Preguntas

1. ¿Por qué la variable g_estado debe declararse como volatile? ¿Qué ocurre si se omite esa palabra clave?

g_estado es utilizada por varias tareas, por lo que su valor puede cambiar en cualquier momento. La palabra clave volatile obliga al compilador a leer siempre el valor actualizado desde memoria. Si se omite, alguna tarea podría seguir usando un valor antiguo debido a optimizaciones del compilador.

2. ¿En qué momento exacto aparece el mensaje [IDLE] en la terminal? Describe el estado de las cuatro tareas en ese instante.
   
El mensaje aparece cuando todas las tareas están bloqueadas por un vTaskDelay() o suspendidas y no hay ninguna lista para ejecutarse. En ese momento FreeRTOS ejecuta la tarea Idle, que llama a vApplicationIdleHook().

3. ¿Qué diferencia existe entre vTaskDelay() y vTaskDelayUntil()? ¿En cuál de las tareas de esta práctica sería más apropiado usar vTaskDelayUntil()?
   
vTaskDelay() retrasa la tarea un tiempo relativo a partir de la llamada. vTaskDelayUntil() mantiene una ejecución periódica más precisa usando una referencia fija de tiempo. En esta práctica sería más adecuado para vTaskLedRapido() y vTaskLedLento(), ya que ambas generan parpadeos periódicos.

4. ¿Por qué vTaskLedRapido tiene prioridad menor que vTaskMonitor? Describe qué ocurriría si se invirtieran esas prioridades.

vTaskMonitor tiene mayor prioridad porque se encarga de detectar el botón y controlar los cambios de estado del sistema. Si se invirtieran las prioridades, la respuesta al botón podría ser más lenta, aunque en este programa el efecto sería pequeño porque las tareas usan vTaskDelay() frecuentemente.

5. ¿Qué riesgo existe al leer una variable volatile desde dos tareas distintas sin protección? Investiga el concepto de sección crítica.

El uso de volatile no evita accesos simultáneos. Si varias tareas modifican una variable al mismo tiempo pueden ocurrir condiciones de carrera y obtener resultados incorrectos. Una sección crítica protege el acceso a recursos compartidos para que solo una tarea pueda utilizarlos temporalmente.


## Conclusion  

En esta práctica se implementó un sistema multitarea en ESP32 utilizando FreeRTOS, donde se controlaron dos modos de operación para un LED: rápido y lento. A través de un botón se realizó el cambio de estado, mientras que en el modo lento se activó la lectura de un sensor analógico mediante el ADC. Además, se aplicó un temporizador para regresar automáticamente al modo rápido después de 5 segundos. Con esta práctica se reforzó el uso de tareas, prioridades, retardos, suspensión y reanudación de procesos, así como el monitoreo de recursos del sistema mediante heap, stack e Idle Hook.

