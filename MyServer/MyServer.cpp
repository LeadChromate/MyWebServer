#include <winsock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

constexpr int MAX_BUFFER_SIZE = 4096;

const char* webPageRootPath = R"(E:\Desktop\CodeField\webcsh\example\)";

// Helper function to determine the content type based on the file extension
std::string GetContentType(const std::string&);

// Function for socket initialization
bool InitializeSocket(WSADATA& wsaData, SOCKET& srvSocket, int port);

// Function for handling client requests
void HandleClientRequest(SOCKET clientSocket, const sockaddr_in& socketAddress);

int main()
{
	WSADATA wsaData;
	SOCKET srvSocket;

	if (!InitializeSocket(wsaData, srvSocket, 80))
	{
		return -1;
	}

	std::cout << "Listening on port 80..." << std::endl;

	sockaddr_in socketAddress;
	int addressLen = sizeof(socketAddress);

	while (true)
	{
		const SOCKET clientSocket = accept(srvSocket, reinterpret_cast<SOCKADDR*>(&socketAddress), &addressLen);
		if (clientSocket == INVALID_SOCKET)
		{
			std::cerr << "Socket accept failed with error: " << WSAGetLastError() << std::endl;
			break;
		}

		HandleClientRequest(clientSocket, socketAddress);
		closesocket(clientSocket); // Close the client socket
	}
	closesocket(srvSocket);
	WSACleanup();
	return 0;
}

bool InitializeSocket(WSADATA& wsaData, SOCKET& srvSocket, const int port)
{
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "Winsock startup failed with error: " << WSAGetLastError() << std::endl;
		return false;
	}

	if (wsaData.wVersion != MAKEWORD(2, 2))
	{
		std::cerr << "Winsock version is not correct!" << std::endl;
		WSACleanup();
		return false;
	}

	srvSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (srvSocket == INVALID_SOCKET)
	{
		std::cerr << "Socket create failed with error: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return false;
	}

	sockaddr_in socketAddress;
	int addressLen = sizeof(socketAddress);
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons(port);
	socketAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(srvSocket, reinterpret_cast<SOCKADDR*>(&socketAddress), sizeof(socketAddress)) == SOCKET_ERROR)
	{
		std::cerr << "Socket bind failed with error: " << WSAGetLastError() << std::endl;
		closesocket(srvSocket);
		WSACleanup();
		return false;
	}

	if (listen(srvSocket, 5) == SOCKET_ERROR)
	{
		std::cerr << "Socket listen failed with error: " << WSAGetLastError() << std::endl;
		closesocket(srvSocket);
		WSACleanup();
		return false;
	}

	return true;
}

void HandleClientRequest(SOCKET clientSocket, const sockaddr_in& socketAddress)
{
	char receiveBuffer[MAX_BUFFER_SIZE];
	int bytesRead;

	// Read the request data
	while ((bytesRead = recv(clientSocket, receiveBuffer, sizeof(receiveBuffer), 0) > 0))
	{
		std::istringstream receiveStream(receiveBuffer);
		std::string requestPath, requestType;
		receiveStream >> requestType >> requestPath;

		char clientIP[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(socketAddress.sin_addr), clientIP, INET_ADDRSTRLEN);
		unsigned short clientPort = ntohs(socketAddress.sin_port);

		std::cout << "Received " << bytesRead << " bytes from ip " << clientIP << " port " << clientPort << ": \"" <<
			requestType << " " << requestPath << " ...\"" << std::endl;

		// Process the request
		if (requestType == "GET")
		{
			if (requestPath == "/") requestPath = "index.html";
			if (requestPath == "/ip")
			{
				std::ostringstream response;
				auto content = std::string(clientIP) + ":" + std::to_string(clientPort);
				// Respond with the client's IP address
				response << "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " <<
					content.length() << "\r\n\r\n" << content;
				std::string responseStr = response.str();
				send(clientSocket, responseStr.c_str(), static_cast<int>(responseStr.length()), 0);
				std::cout << "ip sent length " << content.length() << std::endl;
				continue;
			}

			std::string filePath = webPageRootPath + requestPath;
			std::ifstream file(filePath, std::ios::binary);
			if (file.is_open())
			{
				std::ostringstream response;
				std::string fileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

				response << "HTTP/1.1 200 OK\r\nContent-Type: " << GetContentType(filePath)
					<< "\r\nContent-Length: " << fileContents.length() << "\r\n\r\n" << fileContents;

				std::string responseStr = response.str();
				std::cout << fileContents.length() << " : " << strlen(responseStr.c_str())<< std::endl;
				if (requestPath == "index.html")
					std::cout << fileContents;
				send(clientSocket, responseStr.c_str(), static_cast<int>(responseStr.length()), 0);

				file.close();
			}
			else
			{
				auto notFoundResponse =
					"HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 49\r\n\r\n"
					"<html><body><h1>404 Not Found</h1></body></html>";
				std::cout << "Failed to open file!" << std::endl;
				send(clientSocket, notFoundResponse, static_cast<int>(strlen(notFoundResponse)), 0);
			}
		}
	}

	// Handle client disconnect or socket receive failure
	if (bytesRead == 0)
	{
		std::cout << "Client disconnected." << std::endl;
	}
	else
	{
		std::cerr << "Socket receive failed with error: " << WSAGetLastError() << std::endl;
	}
}

std::string GetContentType(const std::string& filePath)
{
	if (filePath.find(".html") != std::string::npos)
		return "text/html";
	if (filePath.find(".css") != std::string::npos)
		return "text/css";
	if (filePath.find(".js") != std::string::npos)
		return "application/javascript";
	return "text/plain"; // Default to plain text
}
