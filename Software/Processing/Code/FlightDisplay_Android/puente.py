import socket

# Escucha en la red Wi-Fi del ESP32
UDP_IP_IN = "0.0.0.0" 
UDP_PORT_IN = 12345

# Lo reenvía al túnel local del emulador
UDP_IP_OUT = "127.0.0.1" 
UDP_PORT_OUT = 12345

sock_in = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_in.bind((UDP_IP_IN, UDP_PORT_IN))

sock_out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print("Escuchando Wi-Fi y reenviando al Emulador...")

while True:
    data, addr = sock_in.recvfrom(1024)
    # Reenvía todo lo que recibe directo al túnel
    sock_out.sendto(data, (UDP_IP_OUT, UDP_PORT_OUT))
    print("Reenviado:", data.decode('utf-8').strip())