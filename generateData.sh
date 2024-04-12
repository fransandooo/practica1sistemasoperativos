#!/bin/bash

# Definición de los estados posibles
estados=("Error" "Correcto" "Finalizado")

# Función para mostrar el mensaje de uso
usage() {
    echo "Uso: $0 -l <num_lineas> -s <sucursal> -t <tipo_operacion> -u <num_usuarios>"
    echo "  -l, --lineas            Número de líneas de datos a generar por archivo"
    echo "  -s, --sucursal          Especifica la sucursal (por ejemplo, SU004)"
    echo "  -t, --tipo              Especifica el tipo de operación (por ejemplo, COMPRA01)"
    echo "  -u, --usuarios          Número de usuarios distintos que participan en las operaciones"
    exit 1
}

# Inicializar variables con valores predeterminados
num_lineas=100
sucursal=""
tipo_operacion=""
num_usuarios=10
directorio_salida=$(dirname "$0")

# Analizar argumentos de línea de comandos
while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
        -l|--lineas)
        num_lineas="$2"
        shift
        shift
        ;;
        -s|--sucursal)
        sucursal="$2"
        shift
        shift
        ;;
        -t|--tipo)
        tipo_operacion="$2"
        shift
        shift
        ;;
        -u|--usuarios)
        num_usuarios="$2"
        shift
        shift
        ;;
        *)
        # Opción desconocida
        echo "Opción desconocida: $1"
        usage
        ;;
    esac
done

# Verificar si se proporcionaron todos los argumentos requeridos
if [[ -z $sucursal || -z $tipo_operacion ]]; then
    echo "Error: Debes proporcionar los argumentos -l, -s y -t"
    usage
fi

# Generar una fecha y hora aleatoria para la fecha de inicio entre los últimos 30 días
hora_inicio=$((RANDOM%24))
minuto_inicio=$((RANDOM%60))
fecha_inicio=$(date -d "-$((RANDOM%30+1)) days $hora_inicio:$minuto_inicio" +"%d/%m/%Y %H:%M")

# Obtener solo la fecha sin la hora
fecha_inicio_sin_hora=$(date -d "$fecha_inicio" +"%d/%m/%Y")

# Generar datos de prueba
# Ruta completa del archivo
nombre_archivo="${sucursal}_${tipo_operacion}_$(date +"%d%m%Y").csv"
ruta_archivo="$directorio_salida/$nombre_archivo"
# Cabecera del archivo
echo "IdOperacion;FECHA_INICIO;FECHA_FIN;IdUsuario;IdTipoOperacion;NoOperacion;Importe;Estado" > "$ruta_archivo"
# Generar datos de prueba
for ((i = 1; i <= num_lineas; i++)); do
    usuario=$((1 + RANDOM % num_usuarios)) # Genera un número aleatorio entre 1 y el número de usuarios
    
    # Generar una hora aleatoria para la fecha de finalización entre las 00:00 y las 23:59
    hora_fin=$((RANDOM%24))
    minuto_fin=$((RANDOM%60))
    
    # Combinar la fecha de inicio con la nueva hora de finalización
    fecha_fin=$(date -d "$fecha_inicio_sin_hora $hora_fin:$minuto_fin" +"%d/%m/%Y %H:%M")
    
    echo "OPE00$i;$fecha_inicio;$fecha_fin;USER00$usuario;$tipo_operacion;$i;$importe.00€;${estados[$((RANDOM % ${#estados[@]}))]}" >> "$ruta_archivo"
done

echo "Datos generados con éxito en $ruta_archivo"

