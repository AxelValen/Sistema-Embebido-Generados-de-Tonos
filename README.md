# 🎹 Tone-Generator: Sistema embebido generador de señales de audio

![Status: Completed](https://img.shields.io/badge/Status-Completed-success)
![Hardware: RPi Pico 2W](https://img.shields.io/badge/Hardware-RPi_Pico_2W-red)
![Language: C](https://img.shields.io/badge/Language-C-blue)
![Build: CMake](https://img.shields.io/badge/Build-CMake-lightgrey)

## 📖 Descripción del proyecto
Generador de señales audibles desarrollado utilizando la arquitectura ARM a través de una Raspberry Pi Pico 2W. El sistema permite al usuario introducir una frecuencia objetivo mediante un teclado matricial, visualizar la información en tiempo real a través de una pantalla LCD vía I2C, y ajustar la amplitud de la señal (volumen/duty cycle) mediante conversión analógica-digital (ADC). La señal resultante es generada utilizando modulación por ancho de pulsos (PWM) hacia un buzzer piezoeléctrico.

## 🏗️ Arquitectura de hardware

<img width="993" height="659" alt="image" src="https://github.com/user-attachments/assets/d0da9cf4-1431-4042-a43e-e2d99601c101" />


La integración de periféricos hace uso directo de los pines GPIO del microcontrolador:
- **Salida de audio (PWM):** Buzzer conectado al GPIO16.
- **Control de amplitud (ADC):** Potenciómetro conectado al GPIO26 para lectura analógica.
- **Interfaz de usuario (I2C):** Pantalla LCD 16x2 en los GPIO4 (SDA) y GPIO5 (SCL).
- **Entrada de Datos:** Teclado matricial 4x4 mapeado a los GPIO 6-9 (filas) y GPIO 10-13 (columnas).

## 📊 Resultados y análisis de señales
Se realizaron mediciones físicas para validar la precisión de las señales generadas frente a los cálculos del microcontrolador:
- Precisión de frecuencia: Generación de 444Hz para un objetivo de 440Hz (0.9% de error).
- Precisión de duty cycle: Medición de 35% frente a un 36% programado mediante el ADC (2.7% de error).

## Mejoras futuras
Actualmente, el escaneo de la matriz del teclado carece de un filtro de debounce eficiente, lo que en ocasiones produce registros múltiples de una misma tecla al presionarla físicamente. Como futura iteración, se planea implementar un filtro de rebote por software, o la adición de un filtro paso bajo por hardware en las líneas del teclado.
