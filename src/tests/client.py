from scapy.all import IP, send

def send_ip_packet():
    server_ip = "127.0.0.1"

    # Create a raw IP packet with 1KB of data
    packet = IP(dst=server_ip) / b'V' * 1024

    print(f"Sending IP packet to {server_ip}")
    send(packet, verbose=0)

if __name__ == "__main__":
    send_ip_packet()
