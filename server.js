// Require HTTP module (to start server) and Socket.IO
var http = require('http'), io = require('socket.io');

// Start the server at port 3000
var server = http.createServer(function(req, res){ 

	// Send HTML headers and message
	res.writeHead(200,{ 'Content-Type': 'text/html' }); 
	res.end('<h1>Hello Socket Lover!</h1>');
});
server.listen(3000);

// Create a Socket.IO instance, passing it our server
var socket = io.listen(server);

// Add a connect listener
socket.on('connection', function(client){ 
	
	// Success!  Now listen to messages to be received
	//client.on('message',function(event){ 
	//	client.emit("got it!");
	//});
	client.on('disconnect',function(){
		//clearInterval(interval);
		console.log('Server has disconnected');
	});
});
