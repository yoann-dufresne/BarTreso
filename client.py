import socket


if __name__ == "__main__":
    host = "192.168.1.63"
    port = 4040

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))

        msg = [
            1, # 1 color declared
            255, 0, 0, # white color
            1, # Mode pixels
            3, # 1 pixel to follow
            0, 0, 0, # led_strip 0
            0, 1, 0, # led_strip 0
            0, 3, 0 # led_strip 0
        ]

        s.sendall(bytes(msg))
