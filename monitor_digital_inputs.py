#!/usr/bin/env python3
"""
Monitor de Entradas Digitales DI1/DI2
Monitorea el puerto serial y muestra solo los eventos relacionados con las entradas digitales
"""

import serial
import sys
import time
from datetime import datetime

# Configuración
PORT = '/dev/cu.usbserial-3110'
BAUD = 115200

# Colores ANSI
RESET = '\033[0m'
RED = '\033[91m'
GREEN = '\033[92m'
YELLOW = '\033[93m'
BLUE = '\033[94m'
MAGENTA = '\033[95m'
CYAN = '\033[96m'
BOLD = '\033[1m'

def print_header():
    """Imprime el encabezado del monitor"""
    print(f"\n{BOLD}{CYAN}{'='*80}{RESET}")
    print(f"{BOLD}{CYAN}   Monitor de Entradas Digitales DI1 (GPIO36) y DI2 (GPIO39){RESET}")
    print(f"{BOLD}{CYAN}{'='*80}{RESET}")
    print(f"\n{YELLOW}Puerto:{RESET} {PORT}")
    print(f"{YELLOW}Baudrate:{RESET} {BAUD}")
    print(f"{YELLOW}Hora de inicio:{RESET} {datetime.now().strftime('%H:%M:%S')}")
    print(f"\n{BOLD}Esperando eventos de entradas digitales...{RESET}")
    print(f"{CYAN}{'─'*80}{RESET}\n")

def print_event(timestamp, event_type, message, color=GREEN):
    """Imprime un evento formateado"""
    print(f"{BOLD}[{timestamp}]{RESET} {color}{event_type}{RESET} {message}")

def monitor_serial():
    """Monitorea el puerto serial y muestra eventos de entradas digitales"""
    try:
        # Abrir puerto serial
        ser = serial.Serial(PORT, BAUD, timeout=1)
        print_header()
        
        # Contadores
        di1_triggers = 0
        di2_triggers = 0
        
        while True:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]
                    
                    # Detectar eventos de entradas digitales
                    if '[DI1]' in line or '[DI2]' in line:
                        # Determinar tipo de evento
                        if 'Flanco de subida' in line:
                            if '[DI1]' in line:
                                di1_triggers += 1
                                print_event(timestamp, '🔼 DI1 SUBIDA', 
                                          f"Pulso detectado (#{di1_triggers})", GREEN)
                            else:
                                di2_triggers += 1
                                print_event(timestamp, '🔼 DI2 SUBIDA', 
                                          f"Pulso detectado (#{di2_triggers})", GREEN)
                            
                            # Mostrar detalles del relé
                            if 'Activando Relé' in line:
                                import re
                                match = re.search(r'Relé (\d+) por ([\d.]+)s', line)
                                if match:
                                    relay = match.group(1)
                                    duration = match.group(2)
                                    print(f"  {CYAN}└─▶{RESET} Relé {BOLD}{relay}{RESET} activado por {BOLD}{duration}s{RESET}")
                        
                        elif 'Flanco de bajada' in line:
                            input_num = 'DI1' if '[DI1]' in line else 'DI2'
                            print_event(timestamp, f'🔽 {input_num} BAJADA', 
                                      'Sistema listo para nuevo pulso', YELLOW)
                        
                        elif 'Configuración actualizada' in line:
                            input_num = 'DI1' if '[DI1]' in line else 'DI2'
                            print_event(timestamp, f'⚙️  {input_num} CONFIG', 
                                      line.split(':]')[1].strip() if ':]' in line else 'Actualizada', 
                                      MAGENTA)
                    
                    # Detectar carga de configuración inicial
                    elif 'Configuración de entradas digitales' in line:
                        print_event(timestamp, '📥 CONFIGURACIÓN', line, BLUE)
                    
                    # Detectar inicialización de pines
                    elif 'Entradas digitales configuradas' in line:
                        print_event(timestamp, '🔌 INICIALIZACIÓN', line, BLUE)
                    
                    # Detectar estado inicial
                    elif 'Estado inicial: DI1=' in line:
                        print_event(timestamp, '📊 ESTADO INICIAL', line, BLUE)
                    
                    # Detectar eventos MQTT de entradas digitales
                    elif 'digital_input' in line.lower() and 'mqtt' in line.lower():
                        if 'publicado' in line.lower():
                            print_event(timestamp, '📡 MQTT ENVIADO', 
                                      'Evento publicado al servidor', CYAN)
                        elif 'error' in line.lower():
                            print_event(timestamp, '❌ MQTT ERROR', 
                                      'Error al publicar evento', RED)
                    
                    # Mostrar estadísticas cada 50 eventos
                    total = di1_triggers + di2_triggers
                    if total > 0 and total % 50 == 0:
                        print(f"\n{CYAN}{'─'*80}{RESET}")
                        print(f"{BOLD}📊 Estadísticas:{RESET}")
                        print(f"  DI1 (GPIO36): {GREEN}{di1_triggers}{RESET} pulsos")
                        print(f"  DI2 (GPIO39): {GREEN}{di2_triggers}{RESET} pulsos")
                        print(f"  Total: {GREEN}{total}{RESET} pulsos")
                        print(f"{CYAN}{'─'*80}{RESET}\n")
                
                except UnicodeDecodeError:
                    pass  # Ignorar errores de decodificación
            
            time.sleep(0.01)  # Pequeña pausa para no saturar la CPU
    
    except serial.SerialException as e:
        print(f"\n{RED}❌ Error al abrir el puerto serial:{RESET} {e}")
        print(f"\n{YELLOW}Sugerencias:{RESET}")
        print(f"  1. Verifica que la placa esté conectada")
        print(f"  2. Verifica que el puerto {PORT} sea correcto")
        print(f"  3. Cierra otros programas que puedan estar usando el puerto")
        sys.exit(1)
    
    except KeyboardInterrupt:
        print(f"\n\n{CYAN}{'─'*80}{RESET}")
        print(f"{BOLD}📊 Resumen de la sesión:{RESET}")
        print(f"  DI1 (GPIO36): {GREEN}{di1_triggers}{RESET} pulsos detectados")
        print(f"  DI2 (GPIO39): {GREEN}{di2_triggers}{RESET} pulsos detectados")
        print(f"  Total: {GREEN}{di1_triggers + di2_triggers}{RESET} pulsos")
        print(f"{CYAN}{'─'*80}{RESET}")
        print(f"\n{YELLOW}Monitor detenido por el usuario.{RESET}\n")
        sys.exit(0)
    
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()

if __name__ == "__main__":
    print(f"{BOLD}Iniciando monitor...{RESET}")
    monitor_serial()

