import socket
import json
import base64
from cryptography.fernet import Fernet
import os

HOST = '127.0.0.1'
PORT = 9735

# Fixed key (same requirement)
key = b'REPLACE_WITH_KEY'
cipher = Fernet(key)

print("1. Upload")
print("2. Download")

choice = input("Choice: ")

client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client.connect((HOST, PORT))

try:
    if choice == "1":
        filename = input("Enter file: ")

        if not os.path.exists(filename):
            print("File not found!")
        else:
            with open(filename, "rb") as f:
                data = f.read()

            encrypted = cipher.encrypt(data)
            encoded = base64.b64encode(encrypted).decode()

            request = {
                "action": "UPLOAD",
                "filename": filename,
                "data": encoded
            }

            client.send(json.dumps(request).encode())
            response = json.loads(client.recv(4096).decode())

            print("Upload:", response["status"])

    elif choice == "2":
        filename = input("Enter file: ")

        request = {
            "action": "DOWNLOAD",
            "filename": filename
        }

        client.send(json.dumps(request).encode())
        response = json.loads(client.recv(4096).decode())

        if response["status"] == "NOT_FOUND":
            print("File not found!")
        else:
            encrypted = base64.b64decode(response["data"])
            decrypted = cipher.decrypt(encrypted)

            with open("alt_" + filename, "wb") as f:
                f.write(decrypted)

            print("Download successful!")

    else:
        print("Invalid choice")

finally:
    client.close()