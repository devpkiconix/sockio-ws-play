A `Socket.io` server and a `Websocket` client to understand the protocol implementation of a `socket.io` server.   This is useful, when, say, you need to implement your own `socket.io` client in C, Lua etc. where no `socket.io` clients are available.  Based on: https://github.com/LearnBoost/socket.io-spec

## Server 

A small `socket.io` server that echoes back the incoming messages.

## Client

A client written using `Websocket` node module (not `socket.io`) + basic http to simulate the workings of a `socket.io` client.  This client first makes an HTTP connection to fetch the `session ID`, and then makes a websocket connection using this `session ID` and periodically sends messages and prints ACKs and any other messages coming back from the server.

## Running JS code

To install dependencies:

	npm install

To run the server:

	node server.js

To run the client: 	

	node client.js 

OR

	coffee client.coffee

## Running C code with JS socket.io server

   - Checkout libwebsockets if you haven't already
   - Build libwebsockets:
```
       $ git submodule init && git submodule update && (cd libwesockets && mkdir build && cd build && cmake .. && make )
```
   - Build c code:

        $ make

   - Run server:
```   
     node server.js &
```     

   - Run client 
```   
     LD_LIBRARY_PATH=./libbwebsockets/build/lb ./sockio-test
```

You will see that the sockio C client connects and receives heartbeat
messages from the remote server.