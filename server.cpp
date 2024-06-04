#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <windows.h>

#include "LogError.h"
#include "ClientToSrvrMsgBuf.h"

namespace
{
     SOCKET srvrRunningSkt{};
}

void SrvrRunning()
{
     int dummy;
     SOCKADDR_IN clientAddr;
     int clientAddrSize = sizeof(clientAddr);
     while (true)
     {
          int len = recvfrom(srvrRunningSkt, (char *)&dummy, sizeof(int), 0,
                             (SOCKADDR *)&clientAddr, &clientAddrSize);

          if (len > 0)
          {
               sendto(srvrRunningSkt, (char *)&dummy, sizeof(int), 0,
                      (SOCKADDR *)&clientAddr, sizeof(clientAddr));
          }
     }
}

int main()
{
     WSADATA wsaData;
     if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
     {
          printf("Server: WSAStartup failed with error: %ld\n", WSAGetLastError());
          WSACleanup();

          return -1;
     }

     SOCKET srvrSkt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
     srvrRunningSkt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

     if (srvrSkt == INVALID_SOCKET || srvrRunningSkt == INVALID_SOCKET)
     {
          printf("Server: Error at socket(): %ld\n", WSAGetLastError());
          WSACleanup();

          return -1;
     }

     SOCKADDR_IN srvrAddr{AF_INET, htons(ErrorLogger::srvrPort), htonl(INADDR_ANY)};
     SOCKADDR_IN srvrRunningAddr{AF_INET, htons(ErrorLogger::srvrRunningPort), htonl(INADDR_ANY)};

     if (bind(srvrSkt, (SOCKADDR *)&srvrAddr, sizeof(srvrAddr)) == SOCKET_ERROR)
     {

          printf("srvrSkt: Error! bind() failed!\n");
          closesocket(srvrSkt);
          WSACleanup();

          return -1;
     }

     if (bind(srvrRunningSkt, (SOCKADDR *)&srvrRunningAddr, sizeof(srvrRunningAddr)) == SOCKET_ERROR)
     {

          printf("srvrRunningSkt: Error! bind() failed!\n");
          closesocket(srvrSkt);
          WSACleanup();

          return -1;
     }

     std::thread srvrRunning(SrvrRunning);

     printf("Server: Receiving IP(s) used: %s\n", inet_ntoa(srvrAddr.sin_addr));
     printf("Server:Waiting on port: %d...\n", srvrAddr.sin_port);

     while (true)
     {
          ErrorLogger::ClientToSrvrMsgBuf clientMsgBuf;
          SOCKADDR_IN clientAddr;
          int clientAddrSize = sizeof(clientAddr);
          int len = recvfrom(srvrSkt, (char *)&clientMsgBuf, sizeof(clientMsgBuf), 0,
                              (SOCKADDR *)&clientAddr, &clientAddrSize);

          if (len > 0)
          {
               auto msg = std::string(clientMsgBuf.errMsg.data()) + std::string(clientMsgBuf.instructions.data());
               auto result = MessageBoxA(NULL, msg.data(), clientMsgBuf.caption.data(), MB_ICONERROR | MB_ABORTRETRYIGNORE | MB_DEFBUTTON3);

               ErrorLogger::SrvrResponse srvrResp;

               switch (result)
               {
               case IDABORT:
                    srvrResp = ErrorLogger::SrvrResponse::TerminateProcess;
               case IDRETRY:
                    srvrResp = ErrorLogger::SrvrResponse::DisablePopups;
                    break;
               case IDIGNORE:
               default:
                    srvrResp = ErrorLogger::SrvrResponse::Ignore;
                    break;
               }

               sendto(srvrSkt, (char *)&srvrResp, sizeof(int), 0,
                      (SOCKADDR *)&clientAddr, sizeof(clientAddr));

               printf("Server: Sending IP used: %s\n", inet_ntoa(clientAddr.sin_addr));
               printf("Server: Sending port used: %d\n", htons(clientAddr.sin_port));
          }
     }

     // When your application is finished receiving datagrams close the socket.
     printf("Server: Finished receiving. Closing the listening socket...\n");
     if (closesocket(srvrSkt) != 0)
     {
          printf("Server: closesocket() failed! Error code: %ld\n", WSAGetLastError());
     }
     else
     {
          printf("Server: closesocket() is OK\n");
     }

     // When your application is finished call WSACleanup.
     printf("Server: Cleaning up...\n");

     if (WSACleanup() != 0)
     {
          printf("Server: WSACleanup() failed! Error code: %ld\n", WSAGetLastError());
     }
     else
     {
          printf("Server: WSACleanup() is OK\n");
     }

     // Back to the system
     return 0;
}
