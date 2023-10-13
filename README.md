"server" is a server running on a TCP connection that receives and forwards custom-made application-level PDUs from multiple clients that connect to its inputted port.

"cclient" takes a username and the server's address and port as arguments to connect to the currently running server process.

  1. When first connecting, the client sends a "connect" packet to update the server's room state. If the proposed username is not unique to the server (another client is using it), the connection will be refused.

  2. The client can choose between Unicast, Multicast, or Broadcast messages. They can also choose to have the server send a list of all currently connected users.

  3. Upon ending the process, the client with send an exit packet to the server so that it can update the room state by removing the client from its active list.
