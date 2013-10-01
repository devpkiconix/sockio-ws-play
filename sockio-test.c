#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>

#include "libwebsockets/lib/libwebsockets.h"


typedef struct  {
  char sessionId[32];
  int heartbeatInterval;
  int connTimeoutInterval;
} SockIoHandshakeData;
enum demo_protocols {

  CUSTOM_CHANNEL,

  /* always last */
  DEMO_PROTOCOL_COUNT
};


/* Forward declarations */
static int
callback_customChannel(struct libwebsocket_context *this,
      struct libwebsocket *wsi,
      enum libwebsocket_callback_reasons reason,
                 void *user, void *in, size_t len);

static SockIoHandshakeData* handshake(const char* host, int port, const char* path); 
static int create_tcp_socket();
static char *get_ip(const char *host);
static char *build_get_query(const char *host, const char *page);
static void usage();
static void connectWs(
    SockIoHandshakeData* pHandshakeData,
    const char* address, int port, 
    struct libwebsocket_context *context
  );

  /* list of supported protocols and callbacks */

  static struct libwebsocket_protocols protocols[] = {
    {
      "customChannel",
      callback_customChannel,
      0,
      20,
    },
    { NULL, NULL, 0, 0 } /* end */
  };


#define USERAGENT "HTMLGET 1.0"

static int force_exit=0, was_closed = 0;

void sighandler(int sig)
{
  force_exit = 1;
}

int main(int argc, char **argv)
{
  char *host;
  int  port;
  char *page;
  int debug_level = 519;
  SockIoHandshakeData* pHandshakeData = NULL;

  signal(SIGINT, sighandler);

  host = "127.0.0.1";
  port = 3000;
  page = "/socket.io/1/websocket"; 

  pHandshakeData = handshake(host, port, page);
  fprintf(stderr, "sessionId: %20s\nheartbeatInterval: %d\nconnTimeoutInterval: %d\n", pHandshakeData->sessionId,pHandshakeData->heartbeatInterval, pHandshakeData->connTimeoutInterval );

  lws_set_log_level(debug_level, lwsl_emit_syslog);
  struct lws_context_creation_info info;
  struct libwebsocket_context *context;
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.gid = -1;
  info.uid = -1;
  info.ssl_cert_filepath = NULL;
  info.ssl_private_key_filepath = NULL;
  info.ssl_cipher_list = NULL;
  info.extensions = libwebsocket_get_internal_extensions();
  context = libwebsocket_create_context(&info);

  if (context == NULL) {
    fprintf(stderr, "Creating libwebsocket context failed\n");
    return 1;
  }

  connectWs(pHandshakeData, host, port, context);
  return 0;
}

static void 
connectWs(
    SockIoHandshakeData* pHandshakeData,
    const char* address, int port, 
    struct libwebsocket_context *context
  )
  {
  int n = 0;
  int ret = 0;
  int use_ssl = 0;
  struct libwebsocket *websock;
  int ietf_version = -1; /* latest */

  char uri[1024];
  sprintf(uri, "/socket.io/1/websocket/%s", pHandshakeData->sessionId);
    lwsl_notice("Client connecting to %s:%u....\n", address, port);
  websock = libwebsocket_client_connect(context, address,
        port, use_ssl, uri, address, address,
         NULL, -1);

  if (websock == NULL) {
    fprintf(stderr, "libwebsocket connect failed\n");
    ret = 1;
    goto bail;
  }

  n = 0; 


  while (n >= 0 && !was_closed && !force_exit) {
      
    n = libwebsocket_service(context, 10);

  }
  fprintf(stderr, "Exited while loop\n");
  return;

  bail:
    fprintf(stderr, "Error connecting WS\n");
  return;
  }


static void 
connectWs1(
    SockIoHandshakeData* pHandshakeData,
    const char* address, int port, 
    struct libwebsocket_context *context
  )
  {
  int n = 0;
  int ret = 0;
  int use_ssl = 0;
  struct libwebsocket *websock;
  int ietf_version = -1; /* latest */


  websock = libwebsocket_client_connect(context, address, port, use_ssl,
      "/", address, "origin",
      NULL, ietf_version);

  if (websock == NULL) {
    fprintf(stderr, "libwebsocket connect failed\n");
    ret = 1;
    goto bail;
  }

  n = 0; 

  while (n >= 0 && !was_closed && !force_exit) {
    n = libwebsocket_service(context, 10);

  }
  fprintf(stderr, "Exited while loop\n");
  return;

  bail:
    fprintf(stderr, "Error connecting WS\n");
  return;
  }

/**
 * Socket.io handshake and returns the handshake data
 */
static SockIoHandshakeData* handshake(const char* host, int port, const char* page) 
  {
  struct sockaddr_in *remote;
  int sock;
  int tmpres;
  char *ip;
  char *get;
  char buf[BUFSIZ+1];
  sock = create_tcp_socket();
  ip = get_ip(host);
  /*fprintf(stderr, "IP is %s\n", ip);*/
  remote = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in *));
  remote->sin_family = AF_INET;
  tmpres = inet_pton(AF_INET, ip, (void *)(&(remote->sin_addr.s_addr)));
  if( tmpres < 0)  
  {
    perror("Can't set remote->sin_addr.s_addr");
    exit(1);
  }else if(tmpres == 0)
  {
    fprintf(stderr, "%s is not a valid IP address\n", ip);
    exit(1);
  }
  remote->sin_port = htons(port);
 
  if(connect(sock, (struct sockaddr *)remote, sizeof(struct sockaddr)) < 0){
    perror("Could not connect");
    exit(1);
  }
  get = build_get_query(host, page);
  /*fprintf(stderr, "Query is:\n<<START>>\n%s<<END>>\n", get);*/
 
  //Send the query to the server
  int sent = 0;
  while(sent < strlen(get))
  {
    tmpres = send(sock, get+sent, strlen(get)-sent, 0);
    if(tmpres == -1){
      perror("Can't send query");
      exit(1);
    }
    sent += tmpres;
  }
  //now it is time to receive the page
  char reply[1024];
  reply[0] = '\0';

  memset(buf, 0, sizeof(buf));
  int htmlstart = 0, cursor = 0;
  char * htmlcontent;
  while((tmpres = recv(sock, buf, BUFSIZ, 0)) > 0){
    if(htmlstart == 0)
    {
      /* Under certain conditions this will not work.
      * If the \r\n\r\n part is splitted into two messages
      * it will fail to detect the beginning of HTML content
      */
      htmlcontent = strstr(buf, "\r\n\r\n");
      if(htmlcontent != NULL){
        htmlstart = 1;
        htmlcontent += 4;
      }
    }else{
      htmlcontent = buf;
    }
    if(htmlstart){
      /*fprintf(stdout, "%s", htmlcontent);*/
      if (cursor < sizeof(reply))
      {
        strcpy(reply+cursor, htmlcontent);
      }
    }
    memset(buf, 0, tmpres);
  }

  if (strlen(reply) == 0)
    {
    return NULL;
    }

  SockIoHandshakeData* pHandshakeData = calloc(sizeof(SockIoHandshakeData), 1);
  sscanf(reply, "%[^':']:%d:%d",  pHandshakeData->sessionId, &pHandshakeData->heartbeatInterval, &pHandshakeData->connTimeoutInterval );

  /*
  char ignore;
  sscanf(reply, "%20s%c%d%c%d", pHandshakeData->sessionId, &ignore,&pHandshakeData->heartbeatInterval, &ignore, &pHandshakeData->connTimeoutInterval );
  
  sscanf(reply, "%s:%d:%d", pHandshakeData->sessionId, &pHandshakeData->heartbeatInterval, &pHandshakeData->connTimeoutInterval );
  */
  if(tmpres < 0)
  {
    perror("Error receiving data");
  }
  free(get);
  free(remote);
  free(ip);
  close(sock);
  return pHandshakeData;
  }
 
static void usage()
{
  fprintf(stderr, "USAGE: htmlget host port [page]\n\
\thost: the website hostname. ex: coding.debuntu.org\n\
\tpage: the page to retrieve. ex: index.html, default: /\n");
}
 
 
static int create_tcp_socket()
{
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0){
    perror("Can't create TCP socket");
    exit(1);
  }
  return sock;
}
 
 
static char *get_ip(const char *host)
{
  struct hostent *hent;
  int iplen = 15; //XXX.XXX.XXX.XXX
  char *ip = (char *)malloc(iplen+1);
  memset(ip, 0, iplen+1);
  if((hent = gethostbyname(host)) == NULL)
  {
    herror("Can't get IP");
    exit(1);
  }
  if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0], ip, iplen) == NULL)
  {
    perror("Can't resolve host");
    exit(1);
  }
  return ip;
}
 
static char *build_get_query(const char *host, const char *page)
{
  char *query;
  char *getpage = (char*)page;
  char *tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n\r\n";
  tpl = "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\nConnection: close\r\n\r\n";
  if(getpage[0] == '/'){
    getpage = getpage + 1;
    /*fprintf(stderr,"Removing leading \"/\", converting %s to %s\n", page, getpage);*/
  }
  // -5 is to consider the %s %s %s in tpl and the ending \0
  query = (char *)malloc(strlen(host)+strlen(getpage)+strlen(USERAGENT)+strlen(tpl)-5);
  sprintf(query, tpl, getpage, host, USERAGENT);
  return query;
}


/* customChannel protocol */

static int
callback_customChannel(struct libwebsocket_context *context,
      struct libwebsocket *wsi,
      enum libwebsocket_callback_reasons reason,
                 void *user, void *in, size_t len)
{

  int deny_deflate = 0;
  int deny_mux;
  switch (reason) {

  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    fprintf(stderr, "callback_customChannel: LWS_CALLBACK_CLIENT_ESTABLISHED\n");

    /*libwebsocket_callback_on_writable(context, wsi);*/
    break;

  case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    fprintf(stderr, "LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
    was_closed = 1;
    break;

  case LWS_CALLBACK_CLOSED:
    fprintf(stderr, "LWS_CALLBACK_CLOSED\n");
    was_closed = 1;
    break;

  case LWS_CALLBACK_CLIENT_RECEIVE:
    ((char *)in)[len] = '\0';
    fprintf(stderr, "rx %d '%s'\n", (int)len, (char *)in);
    libwebsocket_callback_on_writable(context, wsi);
    break;

  /* because we are protocols[0] ... */

  case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
    if ((strcmp(in, "deflate-stream") == 0) && deny_deflate) {
      fprintf(stderr, "denied deflate-stream extension\n");
      return 1;
    }
    if ((strcmp(in, "deflate-frame") == 0) && deny_deflate) {
      fprintf(stderr, "denied deflate-frame extension\n");
      return 1;
    }
    if ((strcmp(in, "x-google-mux") == 0) && deny_mux) {
      fprintf(stderr, "denied x-google-mux extension\n");
      return 1;
    }

    break;

  case LWS_CALLBACK_CLIENT_WRITEABLE: {
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 4096 +
						  LWS_SEND_BUFFER_POST_PADDING];
	char* pBuf = (char*)(buf + LWS_SEND_BUFFER_POST_PADDING);
	static int msgId = 1;
	// the 3 below is the socket.io code for "MESSAGE"
	sprintf(pBuf, "3:%d::hello node server!", msgId++);
	int n = 0;
	n = libwebsocket_write(wsi, pBuf, strlen(pBuf), LWS_WRITE_TEXT);
	if (n < 0) {
	  lwsl_err("ERROR %d writing to socket, hanging up\n", n);
	  return 1;
	}
	if (n < (int)strlen(pBuf)){
	  lwsl_err("Partial write\n");
	  return -1;
	}
  }
    break;

  default:
    break;
  }

  return 0;
}

