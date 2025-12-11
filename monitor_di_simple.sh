#!/bin/bash
# Monitor Simple de Entradas Digitales DI1/DI2
# No requiere dependencias Python

PORT="/dev/cu.usbserial-3110"
BAUD="115200"

# Colores
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Contadores
DI1_COUNT=0
DI2_COUNT=0

# Función para imprimir header
print_header() {
    clear
    echo -e "${BOLD}${CYAN}================================================================================${NC}"
    echo -e "${BOLD}${CYAN}   Monitor de Entradas Digitales DI1 (GPIO36) y DI2 (GPIO39)${NC}"
    echo -e "${BOLD}${CYAN}================================================================================${NC}"
    echo ""
    echo -e "${YELLOW}Puerto:${NC} $PORT"
    echo -e "${YELLOW}Baudrate:${NC} $BAUD"
    echo -e "${YELLOW}Hora de inicio:${NC} $(date '+%H:%M:%S')"
    echo ""
    echo -e "${BOLD}Esperando eventos de entradas digitales...${NC}"
    echo -e "${BOLD}Presiona Ctrl+C para detener${NC}"
    echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
    echo ""
}

# Función para manejar Ctrl+C
cleanup() {
    echo ""
    echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
    echo -e "${BOLD}📊 Resumen de la sesión:${NC}"
    echo -e "  DI1 (GPIO36): ${GREEN}$DI1_COUNT${NC} pulsos detectados"
    echo -e "  DI2 (GPIO39): ${GREEN}$DI2_COUNT${NC} pulsos detectados"
    echo -e "  Total: ${GREEN}$((DI1_COUNT + DI2_COUNT))${NC} pulsos"
    echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
    echo -e "\n${YELLOW}Monitor detenido.${NC}\n"
    exit 0
}

# Capturar Ctrl+C
trap cleanup INT TERM

# Verificar que el puerto existe
if [ ! -e "$PORT" ]; then
    echo -e "${RED}❌ Error: Puerto $PORT no encontrado${NC}"
    echo -e "${YELLOW}Puertos disponibles:${NC}"
    ls -la /dev/cu.usbserial* 2>/dev/null || echo "  Ninguno"
    exit 1
fi

# Imprimir header
print_header()

# Configurar puerto serial y leer
stty -f "$PORT" "$BAUD" 2>/dev/null

# Leer línea por línea y filtrar eventos de DI
while IFS= read -r line; do
    TIMESTAMP=$(date '+%H:%M:%S.%3N')
    
    # Detectar eventos de DI1
    if echo "$line" | grep -q "\[DI1\]"; then
        if echo "$line" | grep -q "Flanco de subida"; then
            ((DI1_COUNT++))
            echo -e "[${TIMESTAMP}] ${BOLD}${GREEN}🔼 DI1 SUBIDA${NC} Pulso detectado (#$DI1_COUNT)"
            
            # Extraer info del relé si está disponible
            if echo "$line" | grep -q "Relé"; then
                RELAY=$(echo "$line" | grep -oP 'Relé \K\d+' || echo "$line" | sed -n 's/.*Relé \([0-9]\).*/\1/p')
                DURATION=$(echo "$line" | grep -oP '\d+\.?\d*s' || echo "$line" | sed -n 's/.*por \([0-9.]*\)s.*/\1s/p')
                if [ -n "$RELAY" ]; then
                    echo -e "  ${CYAN}└─▶${NC} Relé ${BOLD}$RELAY${NC} activado por ${BOLD}$DURATION${NC}"
                fi
            fi
            
        elif echo "$line" | grep -q "Flanco de bajada"; then
            echo -e "[${TIMESTAMP}] ${YELLOW}🔽 DI1 BAJADA${NC} Sistema listo para nuevo pulso"
            
        elif echo "$line" | grep -q "Configuración actualizada"; then
            CONFIG=$(echo "$line" | sed 's/.*]:\s*//')
            echo -e "[${TIMESTAMP}] ${MAGENTA}⚙️  DI1 CONFIG${NC} $CONFIG"
        fi
    fi
    
    # Detectar eventos de DI2
    if echo "$line" | grep -q "\[DI2\]"; then
        if echo "$line" | grep -q "Flanco de subida"; then
            ((DI2_COUNT++))
            echo -e "[${TIMESTAMP}] ${BOLD}${GREEN}🔼 DI2 SUBIDA${NC} Pulso detectado (#$DI2_COUNT)"
            
            # Extraer info del relé si está disponible
            if echo "$line" | grep -q "Relé"; then
                RELAY=$(echo "$line" | grep -oP 'Relé \K\d+' || echo "$line" | sed -n 's/.*Relé \([0-9]\).*/\1/p')
                DURATION=$(echo "$line" | grep -oP '\d+\.?\d*s' || echo "$line" | sed -n 's/.*por \([0-9.]*\)s.*/\1s/p')
                if [ -n "$RELAY" ]; then
                    echo -e "  ${CYAN}└─▶${NC} Relé ${BOLD}$RELAY${NC} activado por ${BOLD}$DURATION${NC}"
                fi
            fi
            
        elif echo "$line" | grep -q "Flanco de bajada"; then
            echo -e "[${TIMESTAMP}] ${YELLOW}🔽 DI2 BAJADA${NC} Sistema listo para nuevo pulso"
            
        elif echo "$line" | grep -q "Configuración actualizada"; then
            CONFIG=$(echo "$line" | sed 's/.*]:\s*//')
            echo -e "[${TIMESTAMP}] ${MAGENTA}⚙️  DI2 CONFIG${NC} $CONFIG"
        fi
    fi
    
    # Detectar inicialización
    if echo "$line" | grep -q "Entradas digitales configuradas"; then
        echo -e "[${TIMESTAMP}] ${BLUE}🔌 INICIALIZACIÓN${NC} $line"
    fi
    
    # Detectar estado inicial
    if echo "$line" | grep -q "Estado inicial: DI"; then
        echo -e "[${TIMESTAMP}] ${BLUE}📊 ESTADO INICIAL${NC} $line"
    fi
    
    # Detectar carga de configuración
    if echo "$line" | grep -qi "Configuración de entradas digitales"; then
        echo -e "[${TIMESTAMP}] ${BLUE}📥 CONFIGURACIÓN${NC} Cargada desde EEPROM"
    fi
    
    # Detectar eventos MQTT
    if echo "$line" | grep -qi "digital_input"; then
        if echo "$line" | grep -qi "publicado.*mqtt\|mqtt.*publicado"; then
            echo -e "[${TIMESTAMP}] ${CYAN}📡 MQTT${NC} Evento enviado al servidor"
        elif echo "$line" | grep -qi "error.*mqtt\|mqtt.*error"; then
            echo -e "[${TIMESTAMP}] ${RED}❌ MQTT ERROR${NC} Error al publicar"
        fi
    fi
    
    # Mostrar estadísticas cada 50 eventos
    TOTAL=$((DI1_COUNT + DI2_COUNT))
    if [ $TOTAL -gt 0 ] && [ $((TOTAL % 50)) -eq 0 ]; then
        echo ""
        echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
        echo -e "${BOLD}📊 Estadísticas:${NC}"
        echo -e "  DI1 (GPIO36): ${GREEN}$DI1_COUNT${NC} pulsos"
        echo -e "  DI2 (GPIO39): ${GREEN}$DI2_COUNT${NC} pulsos"
        echo -e "  Total: ${GREEN}$TOTAL${NC} pulsos"
        echo -e "${CYAN}────────────────────────────────────────────────────────────────────────────────${NC}"
        echo ""
    fi
    
done < "$PORT"

# Si llega aquí (puerto cerrado), ejecutar cleanup
cleanup

