#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#include "libwebsockets/lib/libwebsockets.h"


typedef struct  {
  char sessionId[32];
  int heartbeatInterval;
  int connTimeoutInterval;
} SockIoHandshakeData;
enum demo_protocols {

  PROTOCOL_DUMB_INCREMENT,
  PROTOCOL_LWS_MIRROR,

  /* always last */
  DEMO_PROTOCOL_COUNT
};


/* Forward declarations */
static int
callback_dumb_increment(struct libwebsocket_context *this,
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
      "dumb-increment-protocol",
      callback_dumb_increment,
      0,
      20,
    },
    { NULL, NULL, 0, 0 } /* end */
  };


#define USERAGENT "HTMLGET 1.0"
 
int main(int argc, char **argv)
{
  char *host;
  int  port;
  char *page;
  SockIoHandshakeData* pHandshakeData = NULL;

  if(argc < 4){
    usage();
    exit(2);
  }  
  host = argv[1];
  port = atoi(argv[2]);
  page = argv[3];

  pHandshakeData = handshake(host, port, page);
  fprintf(stderr, "sessionId: %20s\nheartbeatInterval: %d\nconnTimeoutInterval: %d\n", pHandshakeData->sessionId,pHandshakeData->heartbeatInterval, pHandshakeData->connTimeoutInterval );

  struct lws_context_creation_info info;
  struct libwebsocket_context *context;
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
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
  struct libwebsocket *wsi_dumb;
  int ietf_version = -1; /* latest */


  wsi_dumb = libwebsocket_client_connect(context, address, port, use_ssl,
      "/", address, address,
       protocols[PROTOCOL_DUMB_INCREMENT].name, ietf_version);

  if (wsi_dumb == NULL) {
    fprintf(stderr, "libwebsocket connect failed\n");
    ret = 1;
    goto bail;
  }

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


/* dumb_increment protocol */

static int
callback_dumb_increment(struct libwebsocket_context *this,
      struct libwebsocket *wsi,
      enum libwebsocket_callback_reasons reason,
                 void *user, void *in, size_t len)
{
  int was_closed;
  int deny_deflate = 0;
  int deny_mux;

  switch (reason) {

  case LWS_CALLBACK_CLIENT_ESTABLISHED:
    fprintf(stderr, "callback_dumb_increment: LWS_CALLBACK_CLIENT_ESTABLISHED\n");
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

  default:
    break;
  }

  return 0;
}

