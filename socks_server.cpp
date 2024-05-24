#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

using boost::asio::ip::tcp;
using namespace std;

struct sock4AStruct {
    int vn;
    int cd;
    string srcIP;
    string srcPort;
    string dstIP;
    string dstPort;
    string command;
    string reply;
};

class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket, boost::asio::io_context &io_context)
        : socketSrc(std::move(socket)), socketTarget(io_context),
          io_context_(io_context) {}

    void start() { do_request(); }

  private:
    void do_request() {
        auto self(shared_from_this());
        memset(data_, '\0', sizeof(data_));
        socketSrc.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    if(length == 0) exit(0);
                    cout << "-------------\n";
                    cout << "length: " << length << endl;
                    for (int i=0;i<length;++i){
                        cout << (int)data_[i] << ' ';
                    }
                    cout << "\n-------------\n";
                    cout << to_string((unsigned int)(data_[2] << 8) | data_[3]);
                    cout << "\n--------------\n";

                    parse_request(length);
                    if(request.reply != "Reject"){
                        install_firewall_rule();
                    }

                    cout << "<S_IP>: " << request.srcIP << '\n' << flush;
                    cout << "<S_PORT>: " << request.srcPort << '\n' << flush;
                    cout << "<D_IP>: " << request.dstIP << '\n' << flush;
                    cout << "<D_PORT>: " << request.dstPort << '\n' << flush;
                    cout << "<Command>: " << request.command << '\n' << flush;
                    cout << "<Reply>: " << request.reply << '\n' << flush;

                    replyFormat[0] = 0; 
                    if (request.reply == "Accept") {
                        replyFormat[1] = 90;
                        if (request.cd == 1) {
                            do_connect();
                        } else {
                            do_bind();
                        }
                    } else if(request.reply == "Reject") {
                        replyFormat[1] = 91;
                        do_write_reply();
                        socketSrc.close();
                        exit(0);
                    }
                }
            });
    }

    void do_write_reply() {
        auto self(shared_from_this());
        boost::asio::async_write(
            socketSrc, boost::asio::buffer(replyFormat, 8),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                }
            });
    }

    void do_connect() {
        auto self(shared_from_this());
        tcp::resolver resolver_(io_context_);
        tcp::resolver::results_type endpoint_ =
            resolver_.resolve(request.dstIP, request.dstPort);
        boost::asio::async_connect(
            socketTarget, endpoint_,
            [this, self](boost::system::error_code ec, tcp::endpoint ed) {
                if (!ec) {
                    do_write_reply();
                    do_read_CGI();
                    do_read_target();
                } else {
                    replyFormat[1] = 91;
                    do_write_reply();
                    socketSrc.close();
                    socketTarget.close();
                }
            });
    }

    void do_bind() {
        tcp::acceptor acceptor_(io_context_, tcp::endpoint(tcp::v4(), 0));
        acceptor_.listen();
        unsigned int port = acceptor_.local_endpoint().port();
        cout << "port: " << port << '\n' << flush;
        replyFormat[2] = (port >> 8) & 0x000000FF;
        replyFormat[3] = port & 0x000000FF;
        do_write_reply();
        acceptor_.accept(socketTarget);
        do_write_reply();
        do_read_CGI();
        do_read_target();
    }

    void do_read_CGI() {
        auto self(shared_from_this());
        memset(data_, '\0', sizeof(data_));
        socketSrc.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    do_write_target(length);
                } else {
                    socketSrc.close();
                    socketTarget.close();
                    exit(0);
                }
            });
    }

    void do_read_target() {
        auto self(shared_from_this());
        memset(data_, '\0', sizeof(data_));
        socketTarget.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    do_write_CGI(length);
                } else {
                    socketSrc.close();
                    socketTarget.close();
                    exit(0);
                }
            });
    }

    void do_write_CGI(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socketSrc, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    do_read_target();
                }
            });
    }

    void do_write_target(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socketTarget, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    do_read_CGI();
                }
            });
    }

    void install_firewall_rule() {
        request.reply = "Reject";
        ifstream in("./socks.conf");
        if (!in.is_open()) {
            cout << "File open fail!\n";
            return;
        }
        string rule = "";
        while (getline(in, rule)) {
            vector<string> tmp;
            split_string(tmp, rule, ' ');

            if (tmp[0] != "permit" || (tmp[1] == "c" && request.cd == 2) || (tmp[1] == "b" && request.cd == 1))
                continue;

            vector<string> ipFirewall;
            vector<string> ipDst;
            split_string(ipFirewall, tmp[2], '.');
            split_string(ipDst, request.dstIP, '.');
            bool permit = true;
            for (int i = 0; i < 4; i++) {
                
                if (ipFirewall[i] == "*")
                    continue;
                if (ipFirewall[i] != ipDst[i]) {
                    permit = false;
                    break;
                }
            }
            //cout << "permit: " << permit << '\n' << flush;
            if (permit) {
                request.reply = "Accept";
                break;
            }
        }
    }

    void split_string(vector<string> &output, string &input, char key) {
        stringstream ss(input);
        string token;
        while (getline(ss, token, key))
            output.push_back(token);
    }

    void parse_request(size_t length) {
        for (int i = 2; i < 8; i++)
            replyFormat[i] = 0; // replyFormat[i] = data_[i];

        if (length < 9) {
            request.reply = "Reject";
            cerr << "not match sock4 least length\n";
            return;
        } 

        request.vn = data_[0];
        if (request.vn != 4) {
            request.reply = "Reject";
            cerr << "version not 4\n";
            return;
        }
            
        request.cd = data_[1];
        //request.command = request.cd == 1 ? "CONNECT" : "BIND";
        if(request.cd == 1){
            request.command = "CONNECT";
        } else if(request.cd == 2){
            request.command = "BIND";
        } else {
            request.reply = "Reject";
            cerr << "cd not 1 or 2\n";
            return;
        }

        request.dstPort = to_string((unsigned int)(data_[2] << 8) | data_[3]);
        if (data_[4] == 0 && data_[5] == 0 && data_[6] == 0 && data_[7] != 0) { // sock4A
            int index = 8;
            while (data_[index] != 0)
                index++;
            index++;
            string domain = "";
            while (data_[index] != 0)
                domain.push_back(data_[index++]);
            tcp::resolver resolver_(io_context_);
            tcp::endpoint endpoint_ =
                resolver_.resolve(domain, request.dstPort)->endpoint();
            request.dstIP = endpoint_.address().to_string();
        } else { // sock4
            char ipv4[20];
            snprintf(ipv4, 20, "%d.%d.%d.%d", data_[4], data_[5], data_[6],
                     data_[7]);
            request.dstIP = string(ipv4);
        }
        request.srcIP = socketSrc.remote_endpoint().address().to_string();
        request.srcPort = to_string(socketSrc.remote_endpoint().port());
    }

    tcp::socket socketSrc;
    tcp::socket socketTarget;
    boost::asio::io_context &io_context_;
    sock4AStruct request;
    unsigned char replyFormat[8];
    enum { max_length = 10240 };
    unsigned char data_[max_length];
};

class server {
  public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          io_context_(io_context), signal_(io_context, SIGCHLD) {
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
        signal_handler();
        do_accept();
    }

  private:
    void signal_handler() {
        signal_.async_wait([this](boost::system::error_code ec, int signo) {
            if (acceptor_.is_open()) {
                while (waitpid(-1, NULL, WNOHANG) > 0)
                    ;
                signal_handler();
            }
        });
    }

    void do_accept() {
        acceptor_.async_accept([this](boost::system::error_code ec,
                                      tcp::socket socket) {
            if (!ec) {
                io_context_.notify_fork(boost::asio::io_context::fork_prepare);
                int pid;
                pid = fork();
                if (pid == 0) {
                    io_context_.notify_fork(
                        boost::asio::io_context::fork_child);
                    acceptor_.close();
                    signal_.cancel();
                    std::make_shared<session>(std::move(socket), io_context_)
                        ->start();
                } else {
                    io_context_.notify_fork(
                        boost::asio::io_context::fork_parent);
                    socket.close();
                }
            }
            do_accept();
        });
    }
    tcp::acceptor acceptor_;
    boost::asio::io_context &io_context_;
    boost::asio::signal_set signal_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "No input port\n";
            return 1;
        }
        boost::asio::io_context io_context;
        server s(io_context, std::atoi(argv[1]));
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}