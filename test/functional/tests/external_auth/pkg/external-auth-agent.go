package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"net"
	"os"
)

func main() {
	// Remove existing socket file if it exists
	socketPath := "/tmp/test_external_auth.sock"
	if err := os.Remove(socketPath); err != nil && !os.IsNotExist(err) {
		log.Printf("Warning: failed to remove existing socket: %v", err)
	}

	// Create Unix socket listener
	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		log.Fatalf("Failed to create Unix socket listener: %v", err)
	}
	defer listener.Close()
	defer os.Remove(socketPath)

	fmt.Printf("External auth agent listening on %s\n", socketPath)

	authCount := 0

	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("Failed to accept connection: %v", err)
			continue
		}

		authCount++
		go handleConnection(conn, authCount)
	}
}

func handleConnection(conn net.Conn, authNum int) {
	defer conn.Close()

	fmt.Printf("Authentication #%d: Handling connection\n", authNum)

	// Read two messages
	message1, err := readMessage(conn)
	if err != nil {
		log.Printf("Auth #%d: Failed to read first message: %v", authNum, err)
		return
	}
	if string(message1[:len(message1)-1]) != "postgres" {
		log.Printf("Auth #%d: Received first message: %s, expected 'postgres'", authNum, message1)
		sendFailure(conn)
		return
	}
	fmt.Printf("Auth #%d: Received first message: %s\n", authNum, message1)

	message2, err := readMessage(conn)
	if err != nil {
		log.Printf("Auth #%d: Failed to read second message: %v", authNum, err)
		return
	}
	if string(message2[:len(message2)-1]) != "some-token" {
		log.Printf("Auth #%d: Received second message: %s, expected 'some-token'", authNum, message2)
		sendFailure(conn)
		return
	}
	fmt.Printf("Auth #%d: Received second message: %s\n", authNum, message2)

	sendSuccess(conn)
	fmt.Printf("Auth #%d: Connection handled successfully\n", authNum)
}

func sendSuccess(conn net.Conn) {
	sendMessage(conn, []byte{1})
	sendMessage(conn, []byte("test-id"))
}

func sendFailure(conn net.Conn) {
	sendMessage(conn, []byte{0})
	sendMessage(conn, []byte("failed-id"))
}

func readMessage(conn net.Conn) ([]byte, error) {
	// Read 8 bytes for size (uint64)
	sizeBuf := make([]byte, 8)
	if _, err := io.ReadFull(conn, sizeBuf); err != nil {
		return nil, fmt.Errorf("failed to read size: %v", err)
	}

	// Convert to uint64 (little-endian)
	size := binary.LittleEndian.Uint64(sizeBuf)
	if size > 1024*1024 { // 1MB limit for safety
		return nil, fmt.Errorf("message size too large: %d", size)
	}

	// Read the actual message
	msgBuf := make([]byte, size)
	if _, err := io.ReadFull(conn, msgBuf); err != nil {
		return nil, fmt.Errorf("failed to read message: %v", err)
	}

	return msgBuf, nil
}

func sendMessage(conn net.Conn, data []byte) error {
	// Send size first (8 bytes, uint64, little-endian)
	sizeBuf := make([]byte, 8)
	binary.LittleEndian.PutUint64(sizeBuf, uint64(len(data)))
	if _, err := conn.Write(sizeBuf); err != nil {
		return fmt.Errorf("failed to write size: %v", err)
	}

	// Send the actual data
	if _, err := conn.Write(data); err != nil {
		return fmt.Errorf("failed to write data: %v", err)
	}

	return nil
}
