http = require('http');

Websocket = require("Websocket")

# Socket.io message codes
[DISCONNECT,CONNECT,HEARTBEAT,MESSAGE,JSON_MESSAGE,EVENT,ACK,ERROR,NOOP] = [0..8]

numConnAttempts = 0

# Real websocket connection (uses ws:// protocol)
websocketConnect = (sessionId)->
    WebSocketClient = require('websocket').client;
    client = new WebSocketClient();
    client.on "connectFailed", ()->
        console.log "client - connectFailed "
        if numConnAttempts < 3
          numConnAttempts = numConnAttempts+1
          websocketConnect()
    client.on "connect", (connection)->
      msgId = 1
      console.log "client - connected "
      connection.on "error", (error)->
          console.log "client - error: ", error
      connection.on "close", (error)->
          console.log "client - connection closed by server"
      connection.on "message", (message)->
        console.log "raw msg:\n", message
        if message.type is 'utf8'
          encodeRe = /^(\d+):(\d*):(\d*):(.*)$/
          match = encodeRe.exec(message.utf8Data)
          if match 
            [msgType, msgId, msgEndpoint, msgData] = match[1..4]
            console.log "[msgType, msgId, msgEndpoint, msgData]", [msgType, msgId, msgEndpoint, msgData]
            switch match[1]
              when String(DISCONNECT)
                console.log "DISCONNECT", match[4]
              when String(CONNECT)
                console.log "CONNECT", match[4]
              when String(HEARTBEAT)
                console.log "HEARTBEAT", match[4]
              when String(MESSAGE)
                console.log "MESSAGE", match[4]
              when String(JSON_MESSAGE)
                console.log "JSON_MESSAGE", match[4]
              when String(EVENT)
                console.log "EVENT", match[4]
              when String(ACK)
                console.log "ACK", msgId
              when String(ERROR)
                console.log "ERROR", match[4]
              when String(NOOP)
                console.log "NOOP", match[4]
          else
            console.log "client - unrecognized msg: \n", message

      # Send periodic requests
      timer = setInterval(->
          unless (connection.connected) 
            clearInterval(timer)
            return
          number = Math.round(Math.random() * 0xFFFFFF);
          console.log "sending", number
          connection.sendUTF("3:1:#{msgId}:my-message" + number);
          msgId = msgId + 1
      , 1000);

    client.connect('ws://localhost:3000/socket.io/1/websocket/'+sessionId);

# Initiate a handshake process by opening an http connection with a socket.io
# server
handshakeStart = ()->
    options = {
      host: 'localhost',
      port: 3000
      path: '/socket.io/1/websocket'
    };

    handshakeCb = (response)->
        str = '';

        response.on 'data', (chunk) ->
            str += chunk

        response.on 'end', ()->
            #sampleMsg = "cKe7qpdAzOuh80YzNoU-:60:60:websocket,flashsocket,htmlfile,xhr-polling,jsonp-polling"
            re = /([a-zA-Z0-9-]+)?:(\d*):(\d*)(.*)$/
            match = re.exec(str)
            unless match
                console.log "handshake failed, error from server:", str
                return
            [sessionId, hbTimeout, connTimeout, transports] = match[1..3]
            hbTimeout = +hbTimeout
            connTimeout = +connTimeout
            console.log "[sessionId, hbTimeout, connTimeout, transports]", [sessionId, hbTimeout, connTimeout, transports]
            websocketConnect(sessionId)
      

    console.log "handshake start.."
    req = http.request(options, handshakeCb)
    req.end();

handshakeStart()
