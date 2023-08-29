import socket
from time import sleep, time


if __name__ == "__main__":
    host = "192.168.1.63"
    port = 4040

    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
    # colors = [(255, 0, 0)]

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.connect((host, port))
        
        for c in colors:
            for l in range(50):
                msg = [
                    1, # 1 color declared
                    *c, # white color
                    1, # Mode pixels
                    1, # 1 pixel to follow
                    0, l, 0 # pixel coord
                ]

                print(time(), msg)
                s.sendall(bytes(msg))
                sleep(.1)
