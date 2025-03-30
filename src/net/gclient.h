#ifndef __GCLIENT_HPP_ // Include guard to prevent multiple inclusion
#define __GCLIENT_HPP_

// Include POSIX operating system API if available
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

// Include networking socket functionality
#include "sock.h"
// Include base game networking handler interface
#include "ghandler.h"

/*
 * game_client - Handles client-side network communication in multiplayer games
 * Inherits from game_handler to provide client-specific implementations
 * Responsible for:
 * - Maintaining connection with game server
 * - Sending local player input to server
 * - Processing game state updates from server
 * - Managing synchronization during level loads
 */
class game_client : public game_handler
{
  private:
    net_socket *client_sock; // Socket for reliable TCP communication with server
    int wait_local_input; // Flag indicating if waiting for local player input
    int process_server_command(); // Processes control commands from server
    net_address *server_data_port; // Server's address/port for game state data

  public:
    // Constructor - initializes client connection to server
    game_client(net_socket *client_sock, net_address *server_addr);

    // Main update function - processes all network events
    int process_net();

    // Called when expected input from server hasn't arrived
    int input_missing();

    // Adds local player's input to be sent to server
    void add_engine_input();

    // Initiates level reload process with server
    virtual int start_reload();

    // Notifies server that level reload is complete
    // disconnect: whether to disconnect if reload fails
    virtual int end_reload(int disconnect = 0);

    // Handles disconnection of inactive clients
    virtual int kill_slackers();

    // Clean disconnection from server
    virtual int quit();

    // Destructor - cleans up network resources
    virtual ~game_client();
};

#endif