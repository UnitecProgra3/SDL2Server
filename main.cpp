// SDL_net Server | r3dux.org | 14/01/2011

#include <iostream>
#include <cstdlib>
#include <string>

#include <SDL2/SDL_net.h>

using namespace std;

const unsigned short PORT        = 87833;            // The port our server will listen for incoming connecions on
const unsigned short BUFFER_SIZE = 512;             // Size of our message buffer
const unsigned short MAX_SOCKETS = 4;               // Max number of sockets
const unsigned short MAX_CLIENTS = MAX_SOCKETS - 1; // Max number of clients in our socket set (-1 because server's listening socket takes the 1st socket in the set)

// Messages to send back to any connecting client to let them know if we can accept the connection or not
const string SERVER_NOT_FULL = "OK";
const string SERVER_FULL     = "FULL";

int main(int argc, char **argv)
{
    IPaddress serverIP;                  // The IP of the server (this will end up being 0.0.0.0 - which means roughly "any IP address")
    TCPsocket serverSocket;              // The server socket that clients will use to connect to us
    TCPsocket clientSocket[MAX_CLIENTS]; // An array of sockets for the clients, we don't include the server socket (it's specified separately in the line above)
    bool      socketIsFree[MAX_CLIENTS]; // An array of flags to keep track of which client sockets are free (so we know whether we can use the socket for a new client connection or not)

    char buffer[BUFFER_SIZE];            // Array of characters used to store the messages we receive
    int receivedByteCount = 0;           // A variable to keep track of how many bytes (i.e. characters) we need to read for any given incoming message i.e. the size of the incoming data

    int clientCount = 0;                 // Count of how many clients are currently connected to the server

    bool shutdownServer = false;         // Flag to control when to shut down the server

    // Initialise SDL_net (Note: We don't initialise or use normal SDL at all - only the SDL_net library!)
    if (SDLNet_Init() == -1)
    {
        cout << "Failed to intialise SDL_net: " << SDLNet_GetError() << endl;
        exit(-1); // Quit!
    }

    // Create the socket set with enough space to store our desired number of connections (i.e. sockets)
    SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(MAX_SOCKETS);
    if (socketSet == NULL)
    {
        cout << "Failed to allocate the socket set: " << SDLNet_GetError() << "\n";
        exit(-1); // Quit!
    }
    else
    {
        cout << "Allocated socket set with size:  " << MAX_SOCKETS << ", of which " << MAX_CLIENTS << " are availble for use by clients." <<  endl;
    }

    // Initialize all the client sockets (i.e. blank them ready for use!)
    for (int loop = 0; loop < MAX_CLIENTS; loop++)
    {
        clientSocket[loop] = NULL;
        socketIsFree[loop] = true; // Set all our sockets to be free (i.e. available for use for new client connections)
    }

    // Try to resolve the provided server hostname. If successful, this places the connection details in the serverIP object and creates a listening port on the provided port number
    // Note: Passing the second parameter as "NULL" means "make a listening port". SDLNet_ResolveHost returns one of two values: -1 if resolving failed, and 0 if resolving was successful
    int hostResolved = SDLNet_ResolveHost(&serverIP, NULL, PORT);

    if (hostResolved == -1)
    {
        cout << "Failed to resolve the server host: " << SDLNet_GetError() << endl;
    }
    else // If we resolved the host successfully, output the details
    {
        // Get our IP address in proper dot-quad format by breaking up the 32-bit unsigned host address and splitting it into an array of four 8-bit unsigned numbers...
        Uint8 * dotQuad = (Uint8*)&serverIP.host;

        //... and then outputting them cast to integers. Then read the last 16 bits of the serverIP object to get the port number
        cout << "Successfully resolved server host to IP: " << (unsigned short)dotQuad[0] << "." << (unsigned short)dotQuad[1] << "." << (unsigned short)dotQuad[2] << "." << (unsigned short)dotQuad[3];
        cout << " port " << SDLNet_Read16(&serverIP.port) << endl << endl;
    }

    // Try to open the server socket
    serverSocket = SDLNet_TCP_Open(&serverIP);

    if (!serverSocket)
    {
        cout << "Failed to open the server socket: " << SDLNet_GetError() << "\n";
        exit(-1);
    }
    else
    {
        cout << "Sucessfully created server socket." << endl;
    }

    // Add our server socket to the socket set
    SDLNet_TCP_AddSocket(socketSet, serverSocket);

    cout << "Awaiting clients..." << endl;

    // Main loop...
    do
    {
        // Check for activity on the entire socket set. The second parameter is the number of milliseconds to wait for.
        // For the wait-time, 0 means do not wait (high CPU!), -1 means wait for up to 49 days (no, really), and any other number is a number of milliseconds, i.e. 5000 means wait for 5 seconds
        int numActiveSockets = SDLNet_CheckSockets(socketSet, 0);

        if (numActiveSockets != 0)
        {
            cout << "There are currently " << numActiveSockets << " socket(s) with data to be processed." << endl;
        }

        // Check if our server socket has received any data
        // Note: SocketReady can only be called on a socket which is part of a set and that has CheckSockets called on it (the set, that is)
        // SDLNet_SocketRead returns non-zero for activity, and zero is returned for no activity. Which is a bit bass-ackwards IMHO, but there you go.
        int serverSocketActivity = SDLNet_SocketReady(serverSocket);

        // If there is activity on our server socket (i.e. a client has trasmitted data to us) then...
        if (serverSocketActivity != 0)
        {
            // If we have room for more clients...
            if (clientCount < MAX_CLIENTS)
            {

                // Find the first free socket in our array of client sockets
                int freeSpot = -99;
                for (int loop = 0; loop < MAX_CLIENTS; loop++)
                {
                    if (socketIsFree[loop] == true)
                    {
                        //cout << "Found a free spot at element: " << loop << endl;
                        socketIsFree[loop] = false; // Set the socket to be taken
                        freeSpot = loop;            // Keep the location to add our connection at that index in the array of client sockets
                        break;                      // Break out of the loop straight away
                    }
                }

                // ...accept the client connection and then...
                clientSocket[freeSpot] = SDLNet_TCP_Accept(serverSocket);

                // ...add the new client socket to the socket set (i.e. the list of sockets we check for activity)
                SDLNet_TCP_AddSocket(socketSet, clientSocket[freeSpot]);

                // Increase our client count
                clientCount++;

                // Send a message to the client saying "OK" to indicate the incoming connection has been accepted
                strcpy( buffer, SERVER_NOT_FULL.c_str() );
                int msgLength = strlen(buffer) + 1;
                SDLNet_TCP_Send(clientSocket[freeSpot], (void *)buffer, msgLength);

                cout << "Client connected. There are now " << clientCount << " client(s) connected." << endl << endl;
            }
            else // If we don't have room for new clients...
            {
                cout << "*** Maximum client count reached - rejecting client connection ***" << endl;

                // Accept the client connection to clear it from the incoming connections list
                TCPsocket tempSock = SDLNet_TCP_Accept(serverSocket);

                // Send a message to the client saying "FULL" to tell the client to go away
                strcpy( buffer, SERVER_FULL.c_str() );
                int msgLength = strlen(buffer) + 1;
                SDLNet_TCP_Send(tempSock, (void *)buffer, msgLength);

                // Shutdown, disconnect, and close the socket to the client
                SDLNet_TCP_Close(tempSock);
            }

        } // End of if server socket is has activity check

        // Loop to check all possible client sockets for activity
        for (int clientNumber = 0; clientNumber < MAX_CLIENTS; clientNumber++)
        {
            // If the socket is ready (i.e. it has data we can read)... (SDLNet_SocketReady returns non-zero if there is activity on the socket, and zero if there is no activity)
            int clientSocketActivity = SDLNet_SocketReady(clientSocket[clientNumber]);

            //cout << "Just checked client number " << clientNumber << " and received activity status is: " << clientSocketActivity << endl;

            // If there is any activity on the client socket...
            if (clientSocketActivity != 0)
            {
                // Check if the client socket has transmitted any data by reading from the socket and placing it in the buffer character array
                receivedByteCount = SDLNet_TCP_Recv(clientSocket[clientNumber], buffer, BUFFER_SIZE);

                // If there's activity, but we didn't read anything from the client socket, then the client has disconnected...
                if (receivedByteCount <= 0)
                {
                    //...so output a suitable message and then...
                    cout << "Client " << clientNumber << " disconnected." << endl << endl;

                    //... remove the socket from the socket set, then close and reset the socket ready for re-use and finally...
                    SDLNet_TCP_DelSocket(socketSet, clientSocket[clientNumber]);
                    SDLNet_TCP_Close(clientSocket[clientNumber]);
                    clientSocket[clientNumber] = NULL;

                    // ...free up their slot so it can be reused...
                    socketIsFree[clientNumber] = true;

                    // ...and decrement the count of connected clients.
                    clientCount--;

                    cout << "Server is now connected to: " << clientCount << " client(s)." << endl << endl;
                }
                else // If we read some data from the client socket...
                {
                    // Output the message the server received to the screen
                    cout << "Received: >>>> " << buffer << " from client number: " << clientNumber << endl;

                    // Send message to all other connected clients
                    int originatingClient = clientNumber;

                    for (int loop = 0; loop < MAX_CLIENTS; loop++)
                    {
                        // Send a message to the client saying "OK" to indicate the incoming connection has been accepted
                        //strcpy( buffer, SERVER_NOT_FULL.c_str() );
                        int msgLength = strlen(buffer) + 1;

                        // If the message length is more than 1 (i.e. client pressed enter without entering any other text), then
                        // send the message to all connected clients except the client who originated the message in the first place
                        if (msgLength > 1 && loop != originatingClient && socketIsFree[loop] == false)
                        {
                            cout << "Retransmitting message: " << buffer << " (" << msgLength << " bytes) to client number: " << loop << endl;
                            SDLNet_TCP_Send(clientSocket[loop], (void *)buffer, msgLength);
                        }

                    }

                    // If the client told us to shut down the server, then set the flag to get us out of the main loop and shut down
                    if ( strcmp(buffer, "shutdown") == 0 )
                    {
                        shutdownServer = true;

                        cout << "Disconnecting all clients and shutting down the server..." << endl << endl;
                    }

                }

            } // End of if client socket is active check

        } // End of server socket check sockets loop

    }
    while (shutdownServer == false); // End of main loop

    // Free our socket set (i.e. all the clients in our socket set)
    SDLNet_FreeSocketSet(socketSet);

    // Close our server socket, cleanup SDL_net and finish!
    SDLNet_TCP_Close(serverSocket);

    SDLNet_Quit();

    return 0;
}
