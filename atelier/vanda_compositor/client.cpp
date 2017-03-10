#include "adler32.hpp"
#include <vanda/Client.hpp>

int main(int, char **)
{
    vanda::Client client;
    client.connectShellOpened([](){ printf("connectShellOpened\n"); });
    client.connectShellClosed([](){ printf("connectShellClosed\n"); });
    client.connectParcelMessage([](seppo::ReferencedPointer<seppo::ParcelMessage> msg){ printf("connectParcelMessage\n"); });
    client.connectParcelArray([](const std::vector<seppo::ReferencedPointer<seppo::ParcelMessage>>& msgs){ printf("connectParcelArray\n"); });
    client.connectEndOfMessages([](){ printf("connectEndOfMessages\n"); });
    
    client.start();
    return 0;
}
