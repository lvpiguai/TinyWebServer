#include "./web_server/WebServer.h"

int main(int argc, char const *argv[])
{
    WebServer server;
    server.init(9000,5);
    server.start();
    
    return 0;
}
