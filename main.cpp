#include <iostream>

#include "http_client.hpp"

int main() {
    std::cout << "HTTP test" << std::endl;
    boost::asio::io_service ios;

    hc::http_client client(ios);
    client.connect("http://amedama1x1.hatenablog.com");
    auto res=client.get("/entry/2014/06/16/210600", {{"Host", "amedama1x1.hatenablog.com"}});

    if(!res.internal_error()) {
        std::cout<<res.status()<<std::endl;
        if(res) {
            std::cout<<res.body()<<std::endl;
        }
    }else{
        std::cerr<<"Failed: Internal error"<<std::endl;
    }

    return 0;
}