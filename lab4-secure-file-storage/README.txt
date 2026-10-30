CSE 3320 – Programming Assignment 4
Networking and Security

Name: Diyanshu Goli

Description:
This project implements a secure cloud-based file storage system similar to Dropbox or Google Drive using a client-server model. The system allows users to upload and download files securely over a network.

Features:

* Client-server communication using TCP sockets
* Multi-client handling using threading on the server
* File upload and download functionality
* Encryption and decryption using Fernet (AES-based)
* JSON-based communication protocol
* Base64 encoding for safe transmission of binary data

Requirements:

* Python 3
* cryptography library

Installation:
pip install cryptography

How to Run:

1. Start the server:
   python server_alt.py

2. Run the client:
   python client_alt.py

3. Choose an option:
   1 → Upload file
   2 → Download file

Working:

* During upload, the client encrypts the file and encodes it using Base64 before sending it to the server.
* The server decodes and stores the encrypted file.
* During download, the server sends the encoded file back to the client.
* The client decodes and decrypts the file to restore the original content.

Notes:

* Files stored on the server are prefixed with "cloud_"
* Downloaded files are saved with prefix "alt_"
* The system supports multiple clients simultaneously using threads
* A fixed encryption key is used for both upload and download
