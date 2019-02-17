import socket
import struct
import datetime

UDP_IP = "192.168.1.2"
UDP_PORT = 9999
sock = socket.socket(socket.AF_INET, # Internet
                     socket.SOCK_DGRAM) # UDP
sock.bind((UDP_IP, UDP_PORT))

f = open("logfile.txt", "a")
try:
    while True:
        data, addr = sock.recvfrom(28) # buffer size is 1024 bytes
        ts,ax,ay,az,gx,gy,gz,yaw,pitch,roll = struct.unpack('ihhhhhhfff',data)
        line = str(datetime.datetime.now())
        line = line +" "+str(ts)+" "+str(ax)+" "+str(ay)+" "+str(az)+" "+str(gx)+" "+str(gy)+" "+str(gz)+" "+str(yaw)+" "+str(pitch)+" "+str(roll)+"\n"
        f.write(line)
        print(line)

except (KeyboardInterrupt, SystemExit):
    print("Closing files....")
    f.close()
    exit()
