//
// Created by sileader on 18/01/12.
//

#ifndef HTTP_CLIENT_BASIC_HTTP_CLIENT_HPP
#define HTTP_CLIENT_BASIC_HTTP_CLIENT_HPP

#include <string>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

namespace hc {
    class http_response {
    public:
        using header_type=std::unordered_map<std::string, std::string>;
        using error_type=boost::system::error_code;
    private:
        header_type _header;
        std::string _body;
        int _status;
        error_type _internal_error;
    public:
        http_response(int status, header_type ht, std::string body)
                : _header(std::move(ht)), _body(std::move(body)), _status(status) {}
        explicit http_response(error_type ec) : _status(0), _internal_error(ec) {}

    public:
        bool has(const std::string& key)const {
            return _header.count(key)!=0;
        }

        const std::string& operator[](const std::string& key)const {
            return _header.at(key);
        }
        
    public:
        const std::string& body()const noexcept {
            return _body;
        }
        
    public:
        const error_type& internal_error()const noexcept {
            return _internal_error;
        }

        int status()const noexcept {
            return _status;
        }
        
        explicit operator bool()const noexcept {
            return !internal_error() && status()==200;
        }
    };
    
    template<class SocketT> class basic_http_client {
    public:
        using socket_type=SocketT;//boost::asio::ip::tcp::socket;
        using header_type=http_response::header_type;
        using error_type=http_response::error_type;
    private:
        boost::asio::io_service &_ios;
        socket_type _sock;
        boost::asio::streambuf _recv_buf;

    public:
        constexpr std::uint16_t default_port() {
            return 80;
        }

        std::vector<std::string> support_protocols() {
            return {"http"};
        }

    public:
        explicit basic_http_client(boost::asio::io_service& ios) : _ios(ios), _sock(ios) {}

        basic_http_client(const basic_http_client&)=delete;
        basic_http_client(basic_http_client&&)noexcept=default;

        basic_http_client& operator=(const basic_http_client&)=delete;
        basic_http_client& operator=(basic_http_client&&)noexcept=default;

        virtual ~basic_http_client() {
            if(_sock.is_open()) {
                error_type ec;
                _sock.close(ec);
            }
        }

    public:
        error_type connect(const boost::asio::ip::address& addr, std::uint16_t port=default_port()) {
            error_type err;
            _sock.connect(boost::asio::ip::tcp::endpoint(addr, port), err);
            return err;
        }

        error_type connect(const std::string& host, std::uint16_t port) {
            boost::asio::ip::tcp::resolver res(_sock.get_io_service());
            boost::asio::ip::tcp::resolver::query q(host, support_protocols()[0]);

            error_type err;
            _sock.connect(*res.resolve(q));
            return err;
        }

        error_type connect(std::string uri) {
            namespace ba=boost::algorithm;

            std::string protocol=support_protocols()[0];
            std::uint16_t port=default_port();

            if(uri.find("://")!=std::string::npos) {
                std::vector<std::string> ph;
                ba::replace_all(uri, "://", "[");
                ba::split(ph, uri, boost::is_any_of("["));
                uri=ph[1];
                protocol=ph[0];
            }
            if(uri.find(':')!=std::string::npos) {
                std::vector<std::string> hp;
                ba::split(hp, uri, boost::is_any_of(":"));
                uri=hp[0];
                port=static_cast<std::uint16_t>(std::stoul(hp[1]));
            }

            return connect(uri, port);
        }

    private:
        error_type _request_impl(const std::string& message) {
            error_type ec;
            boost::asio::write(_sock, boost::asio::buffer(message), ec);
            return ec;
        }

        std::tuple<int, header_type> _parse(std::string res)const {
            namespace ba=boost::algorithm;

            std::vector<std::string> headers;
            ba::replace_all(res, "\r\n", "\n");
            ba::split(headers, res, boost::is_any_of("\n"));

            int status=0;
            {
                auto res_line=headers[0];
                std::vector<std::string> res_;
                ba::split(res_, res_line, boost::is_any_of(" "));
                status=std::stoi(res_[1]);
            }

            header_type res_headers;
            std::for_each(std::begin(headers)+1, std::end(headers), [&res_headers](const std::string& line) {
                if(line.find(':')==std::string::npos)return;

                std::vector<std::string> line_elem;
                ba::split(line_elem, line, boost::is_any_of(":"));

                for(auto& l : line_elem) {
                    ba::trim(l);
                    std::transform(std::begin(l), std::end(l), std::begin(l), ::tolower);
                }
                res_headers[line_elem[0]]=line_elem[1];
            });

            return std::tuple<int, header_type>(status, res_headers);
        }

        std::string _read_body_with_content_length(std::size_t len) {
            auto size=boost::asio::read(_sock, _recv_buf, boost::asio::transfer_exactly(len));
            std::string body(boost::asio::buffer_cast<const char*>(_recv_buf.data()), size);
            _recv_buf.consume(size);

            return body;
        }
        std::string _read_body_as_chunked() {
            boost::system::error_code ec;
            std::string body;
            std::size_t size=0;
            do{
                size=boost::asio::read_until(_sock, _recv_buf, "\r\n", ec);
                if(ec) {
                    return "";
                }
                std::string body_size(boost::asio::buffer_cast<const char*>(_recv_buf.data()), size);
                _recv_buf.consume(size);
                size=std::stoul(body_size, nullptr, 16);

                boost::asio::read(_sock, _recv_buf, boost::asio::transfer_exactly(size), ec);
                if(ec && ec!=boost::asio::error::eof) {
                    std::cerr<<ec.message()<<std::endl;
                    return body;
                }
                body.append(boost::asio::buffer_cast<const char*>(_recv_buf.data()), size);
                _recv_buf.consume(size+2);
            }while(size!=0);
            return body;
        }

        http_response _communicate_impl(const std::string& request_message, bool header_only=false) {
            auto ec=_request_impl(request_message);
            if(ec) {
                return http_response(ec);
            }

            auto size=boost::asio::read_until(_sock, _recv_buf, "\r\n\r\n", ec);
            if(ec) {
                return http_response(ec);
            }
            std::string res(boost::asio::buffer_cast<const char*>(_recv_buf.data()), size);
            _recv_buf.consume(size);
            int status;
            header_type header;
            {
                auto sh=_parse(res);
                status=std::get<0>(sh);
                header=std::get<1>(sh);
            }

            if(header_only || (header.count("content-length")==0 && header.count("transfer-encoding")==0 && header["transfer-encoding"]!="chunked")) {
                return http_response(status, header, "");
            }

            auto body=header.count("content-length")!=0 ? _read_body_with_content_length(std::stoul(header["content-length"])) : _read_body_as_chunked();

            return http_response(status, header, body);
        }

        std::string _create_header(const std::string& method, const std::string& path, const header_type& request_headers) {
            constexpr char NEW_LINE[]="\r\n";

            std::stringstream ss;
            ss<<method<<" "<<path<<" HTTP/1.1"<<NEW_LINE;
            for(const auto& rh : request_headers) {
                ss<<rh.first<<": "<<rh.second<<NEW_LINE;
            }
            ss<<NEW_LINE;
            return ss.str();
        }

    public:
        http_response get(const std::string& path, const header_type& request_headers) {
            return _communicate_impl(_create_header("GET", path, request_headers));
        }

        http_response head(const std::string& path, const header_type& request_headers) {
            return _communicate_impl(_create_header("HEAD", path, request_headers), true);
        }

        http_response delete_(const std::string& path, const header_type& request_headers) {
            return _communicate_impl(_create_header("DELETE", path, request_headers));
        }

        http_response post(const std::string& path, header_type request_headers, const std::string& body) {
            request_headers["content-length"]=std::to_string(body.size());
            return _communicate_impl(_create_header("POST", path, request_headers)+body);
        }

        http_response put(const std::string& path, header_type request_headers, const std::string& body) {
            request_headers["content-length"]=std::to_string(body.size());
            return _communicate_impl(_create_header("PUT", path, request_headers)+body);
        }
    };
    
    using http_client=basic_http_client<boost::asio::ip::tcp::socket>;
} /* hc */

#endif //HTTP_CLIENT_BASIC_HTTP_CLIENT_HPP
