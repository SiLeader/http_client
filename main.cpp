/*
    Copyright 2017-2018 Pot Project.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

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