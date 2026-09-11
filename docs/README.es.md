Plugin para el panel de Xfce que controla la reproducción de música mediante **MPRIS/D-Bus**. Funciona con cualquier reproductor que exponga la interfaz `org.mpris.MediaPlayer2` —como Spotify, VLC, Rhythmbox, mpv, Firefox, Chrome y otros— sin necesidad de integraciones específicas para cada aplicación.

> **⚠ Fase inicial de desarrollo.** Este complemento se encuentra en desarrollo activo, solo se ha probado en un único equipo o configuración (Ubuntu 26.04, Xfce 4.20) y aún no cuenta con una versión estable. Es de esperar que presente imperfecciones; por ello, considera el archivo `.deb` y las instrucciones que figuran a continuación como elementos sujetos a cambios constantes, más que como un producto terminado.

## Características

- Botones anterior, reproducción/pausa y siguiente, vinculados al reproductor MPRIS activo.
- Información de la pista con campos de **título**, **artista**, **álbum** y **carátula** que se pueden activar o desactivar de forma independiente.
- Reproducción/pausa mediante clic en el texto.
- Indicadores de progreso de la reproducción que se pueden activar o desactivar.
- Colores que se adaptan al tema Xfce/GTK actual.
- Ancho configurable.
- Selección del reproductor preferido (o automática).

## Instalación

- Existe un [paquete .deb](https://github.com/rod-farias/xfce4-mediaplayer-plugin/releases) para Xfce 4.20 en Ubuntu 26.04, no se garantiza que se instale correctamente en otras combinaciones.
- Para otras distribuciones: clonar el repositorio, instalar las dependencias y compilar con `make` según se detalla [acá](docs/DETAILS.md#Building and installing). 

## Capturas de pantalla


## Limitaciones conocidas

- La carátula del álbum solo se carga para URLs de tipo `file://` (que es lo que ofrecen la mayoría de los reproductores MPRIS de escritorio). Los reproductores que solo proporcionan una URL remota `http(s://)` para la carátula no mostrarán una miniatura.
- La posición de reproducción no forma parte de la señal de notificación de cambios de MPRIS, por lo que se consulta una vez por segundo mientras se reproduce una pista; se trata de una llamada D-Bus deliberada de baja frecuencia, no de un sondeo continuo.
- No se puede agregar a paneles verticales. En tal caso, se muestra un pequeño icono de advertencia en lugar de una interfaz deformada o rota.