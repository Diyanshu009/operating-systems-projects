import socket
import threading
import json
import os
import base64

HOST = '127.0.0.1'
PORT = 9735

def handle_client(conn, addr):
    print(f"Connected: {addr}")

    try:
        data = conn.recv(4096).decode()
        request = json.loads(data)

        action = request["action"]
        filename = request["filename"]

        if action == "UPLOAD":
            filedata = base64.b64decode(request["data"])

            with open("cloud_" + filename, "wb") as f:
                f.write(filedata)

            response = {"status": "OK"}

        elif action == "DOWNLOAD":
            filepath = "cloud_" + filename

            if os.path.exists(filepath):
                with open(filepath, "rb") as f:
                    filedata = base64.b64encode(f.read()).decode()

                response = {"status": "OK", "data": filedata}
            else:
                response = {"status": "NOT_FOUND"}

        conn.send(json.dumps(response).encode())

    except Exception as e:
        print("Error:", e)

    finally:
        conn.close()


server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(5)

print("Alternate Server Running...")

while True:
    conn, addr = server.accept()
    thread = threading.Thread(target=handle_client, args=(conn, addr))
    thread.start()