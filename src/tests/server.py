import socket
from scapy.all import IP, Raw, sniff
client_ip = "128.105.145.148"

def handle_packet(packet):
    if IP in packet:
        ip_src = packet[IP].src
        ip_dst = packet[IP].dst
        data = packet[Raw].load if Raw in packet else b''

        print(f"Received IP packet from {ip_src} to {ip_dst}")
        print(f"Data: {data.decode('cp1252')}")

def start_server():
    print("Server listening for IP packets")

    # Use BPF filter to capture only IP packets
    sniff(prn=handle_packet, filter=f"ip src {client_ip}", store=0)

if __name__ == "__main__":
    start_server()
