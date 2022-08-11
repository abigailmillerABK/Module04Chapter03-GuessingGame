#include <enet/enet.h>
#include <future>
#include <iostream>
#include "Message.h"
#include "User.h"
#include <thread>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

using namespace std;

ENetAddress address;
ENetHost* server = nullptr;
ENetHost* client = nullptr;
bool isDone = false;

void ClientInput(ENetHost* client, User& thisUser) {
    while (isDone == false) {
        Message message;
        std::string text;
           
        std::getline(std::cin, text);
        if (text != "") {
            if (thisUser.name == "") {
                thisUser.name = text;
                text = "*SETUSERID*" + text;
            }
            else {
                text = "\n*" + thisUser.name + "*" + "Guessed :" + text;
            }
            if (text == "quit") {
                isDone = true;
            };
            message.message = text;
            Buffer* msgBuffer = message.Serialize();
            ENetPacket* packet = enet_packet_create(msgBuffer->data,
                msgBuffer->dataSize,
                ENET_PACKET_FLAG_RELIABLE);
            enet_host_broadcast(client, 0, packet);
            enet_host_flush(client);
        }
    }
}
void ServerOuput(User& thisUser) {
    while (isDone == false) {
        ENetEvent event;
        /* Wait up to 1000 milliseconds for an event. */
        while (enet_host_service(client, &event, 1000) > 0)
        {
            system("CLS");
            switch (event.type)
            {
            case ENET_EVENT_TYPE_RECEIVE:
                cout << (char*)event.packet->data
                    << endl;
                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);
            };
        }
    }
}

bool CreateServer()
{
    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */
    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = 1234;
    server = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    return server != nullptr;
}

bool isNumber(string str)
{
    for (char c : str) {
        if (std::isdigit(c) == false) {
            return false;
        }
    }
    return true;
}

bool CreateClient()
{
    client = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return client != nullptr;
}

std::string RunGame(int& randomNumber, std::string name = "", int guess = -1) {
    std::string text = "";
    if (randomNumber == -1) {  //Generate random number
        randomNumber = rand() % 100 + 1; //Between 1 and 100
        text = "Generated random number between 1 and 100\n";
    }
    else {
        text += name + " guessed " + std::to_string(guess);
        if (guess > randomNumber) {
            text += " which was higher than the magic number\n";
        }
        else if (guess < randomNumber) {
            text += " which was lower than the magic number\n";
        }
        else {
            text += ". You guessed correctly!";
            return text;
        }
    }
    text += "Type in your guess below\n";
    return text;
}

void runServer(int& randomNumber, int& numUsers, int& numNamedUsers) {
    ENetEvent event;
    /* Wait up to 1000 milliseconds for an event. */
    while (enet_host_service(server, &event, 1000) > 0)
    {
        bool canGuess = false;
        Message message;
        std::string output;
        std::string name;
        switch (event.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            numUsers++;
            cout << "A new client connected from "
                << event.peer->address.host
                << ":" << event.peer->address.port
                << endl;
            /* Store any relevant client information here. */
            //event.peer->data = (void*)("Client information");

            output = "Please choose a name: \n";

            break;
        case ENET_EVENT_TYPE_RECEIVE:
        {
            char* data = (char*)event.packet->data;
            size_t dataLength = event.packet->dataLength;
            Buffer newBuffer = Buffer(dataLength, data);
            std::string input = message.DeSerialize(&newBuffer);

            int number;

            if (numNamedUsers != numUsers) {
                if (input.find("*SETUSERID*") != std::string::npos) { //If name convention
                    name = input.substr(11);
                    event.peer->data = (char*)(name.c_str());
                    numNamedUsers++;

                    


                    if (numNamedUsers == numUsers && numUsers >= 2) {
                        output = "All users registered.\nMagic number selected. Begin guessing";
                        message.message = output;
                        Buffer* newBuffer = message.Serialize();
                        ENetPacket* packet = enet_packet_create(newBuffer->data,
                            newBuffer->dataSize,
                            ENET_PACKET_FLAG_RELIABLE);
                        enet_host_broadcast(server, 0, packet);
                    }
                    else {
                        output = "User ID Set.\n";
                        message.message = output;
                        Buffer* newBuffer = message.Serialize();
                        ENetPacket* packet = enet_packet_create(newBuffer->data,
                            newBuffer->dataSize,
                            ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, packet);
                    }
                    enet_host_flush(server);

                    enet_packet_destroy(event.packet);
                    return;
                }

            }
            if (numUsers >= 2) {
                if (numNamedUsers == numUsers) {
                    //Get name
                    newBuffer.data = (char*)event.peer->data;
                    newBuffer.dataSize = (size_t)(sizeof(newBuffer.data) + 1);
                    name = message.DeSerialize(&newBuffer);
                    //Get number
                    std::string inputstr = (std::string)(input);
                    auto pos = inputstr.find("Guessed :");
                    std::string substring = inputstr.substr(pos + 9);                    
                    if (isNumber(substring) == false) { //If bad input
                        output = name + " submitted a faulty entry.\nTry entering a whole integer between 1 and 100\n";
                        message.message = output;
                        Buffer* newBuffer = message.Serialize();

                        //Send back info
                        ENetPacket* packet = enet_packet_create(newBuffer->data,
                            newBuffer->dataSize,
                            ENET_PACKET_FLAG_RELIABLE);
                        enet_host_broadcast(server, 0, packet);
                        enet_host_flush(server);
                        enet_packet_destroy(event.packet);
                        return;
                    }
                    number = stoi(substring);
                    if (numNamedUsers == numUsers) {
                        canGuess = true;
                        output = RunGame(randomNumber, name, number);
                    }
                }               
            }
            
            else {
                if (numNamedUsers == numUsers) {
                    output = "Waiting for another client to connect....\n";
                }
            }
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT:
            numUsers--;
            cout << (char*)event.peer->data << " disconnected." << endl;
            /* Reset the peer's client information. */
            event.peer->data = NULL;
        }

        message.message = output;
        Buffer* newBuffer = message.Serialize();

        //Send back info
        ENetPacket* packet = enet_packet_create(newBuffer->data,
            newBuffer->dataSize,
            ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(server, 0, packet);
        enet_host_flush(server);

        /* Clean up the packet now that we're done using it. */
        enet_packet_destroy(event.packet);
    }
}

int main(int argc, char** argv)
{
    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }
    atexit(enet_deinitialize);


    
    cout << "1) Create Server " << endl;
    cout << "2) Create Client " << endl;
    int UserInput;
    cin >> UserInput;
    if (UserInput == 1)
    {
        int randomNumber = -1;
        srand(time(NULL));
        int numUsers = 0;
        int numNamedUsers = 0;
        std::string textLog;
        if (!CreateServer())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet server host.\n");
            exit(EXIT_FAILURE);
        }

        while (1)
        {
            runServer(randomNumber, numUsers, numNamedUsers);
        }
    }
            
    else if (UserInput == 2)
    {
        if (!CreateClient())
        {
            fprintf(stderr,
                "An error occurred while trying to create an ENet client host.\n");
            exit(EXIT_FAILURE);
        }
        else {
            User thisUser;
            thisUser.name = "";
            ENetAddress address;
            ENetEvent event;
            ENetPeer* peer;
            /* Connect to some.server.net:1234. */
            enet_address_set_host(&address, "127.0.0.1");
            address.port = 1234;

              /* Initiate the connection, allocating the two channels 0 and 1. */
            peer = enet_host_connect(client, &address, 2, 0);
            if (peer == NULL)
            {
                fprintf(stderr,
                    "No available peers for initiating an ENet connection.\n");
                exit(EXIT_FAILURE);
            }
            /* Wait up to 5 seconds for the connection attempt to succeed. */
            if (enet_host_service(client, &event, 5000) > 0 &&
                event.type == ENET_EVENT_TYPE_CONNECT)
            {
                cout << "Connection to 127.0.0.1:1234 succeeded." << endl;
            }
            else
            {
                /* Either the 5 seconds are up or a disconnect event was */
                /* received. Reset the peer in the event the 5 seconds   */
                /* had run out without any significant event.            */
                enet_peer_reset(peer);
                cout << "Connection to 127.0.0.1:1234 failed." << endl;
            }

            std::thread outputThread(ServerOuput, std::ref(thisUser));
            std::thread inputThread(ClientInput, client, std::ref(thisUser));
            outputThread.join();
            inputThread.join();               
        }
      
        
      
    }
    else
    {
        cout << "Invalid Input" << endl;
    }
    
    if (server != nullptr)
    {
        enet_host_destroy(server);
    }

    if (client != nullptr)
    {
        enet_host_destroy(client);
    }
    

    return EXIT_SUCCESS;
}