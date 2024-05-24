#!/bin/bash

# Número de sucursales
NUM_SUCURSALES=$1

# Verificar si se proporcionó el número de sucursales como argumento
if [ -z "$NUM_SUCURSALES" ]; then
    echo "Debe proporcionar el número de sucursales como argumento."
    exit 1
fi

# Crear el grupo ufvauditor
sudo groupadd ufvauditor

# Crear usuarios para cada sucursal y añadirlos al grupo ufvauditor
for ((i = 1; i <= NUM_SUCURSALES; i++)); do
    username="userSU$(printf '%03d' $i)" # Formatear el número de sucursal con ceros a la izquierda si es necesario
    sudo useradd -m -G ufvauditor -s /bin/bash $username
done

# Crear el grupo ufvauditores y añadir usuarios
sudo groupadd ufvauditores
sudo useradd -m -G ufvauditores -s /bin/bash userfp
sudo useradd -m -G ufvauditores -s /bin/bash usermonitor

# Asegurarse de que el archivo fp.conf existe en el directorio conf
# Si el archivo no existe, crea uno vacío
touch conf/fp.conf

# Establecer permisos para fp.conf
sudo chown userfp:ufvauditores conf/fp.conf
sudo chmod 640 conf/fp.conf

echo "Usuarios creados exitosamente."
