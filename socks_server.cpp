#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

using boost::asio::ip::tcp;
using namespace std;

struct socksRequestStruct {
    int vn;
    int cd;
    string s_IP;
    string s_Port;
    string d_IP;
    string d_Port;
    string command;
    string reply;
};

class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket, boost::asio::io_context &io_context)
        : socketSrc(std::move(socket)), socketDst(io_context),
          io_context_(io_context), resolver_(io_context),
          acceptor_(io_context) {}

    void start() { do_read(); }

  private:
    void do_print_info() {
        cout << "<S_IP>: " << request.s_IP << '\n' << flush;
        cout << "<S_PORT>: " << request.s_Port << '\n' << flush;
        cout << "<D_IP>: " << request.d_IP << '\n' << flush;
        cout << "<D_PORT>: " << request.d_Port << '\n' << flush;
        cout << "<Command>: " << request.command << '\n' << flush;
        cout << "<Reply>: " << request.reply << '\n' << flush;
    }

    void parse_request(size_t length) {
        for (int i = 2; i < 8; i++)
            replyFormat[i] = 0;

        if (length < 9) {
            request.reply = "Reject";
            // cerr << "not match sock4 least length\n";
            return;
        }

        request.vn = data_[0];
        if (request.vn != 4) {
            request.reply = "Reject";
            // cerr << "version not 4\n";
            return;
        }

        request.cd = data_[1];
        if (request.cd == 1) {
            request.command = "CONNECT";
        } else if (request.cd == 2) {
            request.command = "BIND";
        } else {
            request.reply = "Reject";
            // cerr << "cd not 1 or 2\n";
            return;
        }

        request.s_IP = socketSrc.remote_endpoint().address().to_string();
        request.s_Port = to_string(socketSrc.remote_endpoint().port());

        if (data_[4] == 0 && data_[5] == 0 && data_[6] == 0 &&
            data_[7] != 0) { // sock4A
            int index = 8;
            while (data_[index] != 0)
                index++;
            index++;
            request.d_IP = "";
            while (data_[index] != 0)
                request.d_IP.push_back(data_[index++]);

        } else { // sock4
            request.d_IP = to_string((unsigned int)data_[4]) + "." +
                           to_string((unsigned int)data_[5]) + "." +
                           to_string((unsigned int)data_[6]) + "." +
                           to_string((unsigned int)data_[7]);
        }
        request.d_Port = to_string((unsigned int)(data_[2] << 8) | data_[3]);
    }

    void do_read() {
        auto self(shared_from_this());
        memset(data_, '\0', sizeof(data_));
        socketSrc.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    if (length == 0)
                        exit(0);
                    // cout << "-------------\n";
                    // cout << "length: " << length << endl;
                    // for (int i=0;i<length;++i){
                    //     cout << (int)data_[i] << ' ';
                    // }
                    // cout << "\n-------------\n";
                    // cout << to_string((unsigned int)(data_[2] << 8) |
                    // data_[3]); cout << "\n--------------\n";
                    // cout << "Before Reply: " << request.reply << '\n';
                    parse_request(length);

                    do_resolver();
                }
            });
    }

    void do_resolver() {
        auto self(shared_from_this());
        resolver_.async_resolve(
            request.d_IP, request.d_Port,
            [this, self](boost::system::error_code ec,
                         tcp::resolver::results_type result) {
                if (!ec) {
                    if (request.reply != "Reject") {
                        install_firewall_rule();
                    }
                    replyFormat[0] = 0;
                    request.d_IP = result->endpoint().address().to_string();
                    do_print_info();
                    if (request.reply == "Accept") {
                        replyFormat[1] = 90;
                        if (request.cd == 1) {
                            do_connect(result);
                        } else {
                            do_bind();
                        }
                    } else if (request.reply == "Reject") {
                        replyFormat[1] = 91;
                        do_write_reply();
                        socketSrc.close();
                        exit(0);
                    }
                    //
                } else {
                    // cerr << "resolv error code: " << ec.message() << '\n';
                }
            });
    }

    void do_connect(tcp::resolver::results_type result) {
        auto self(shared_from_this());
        boost::asio::async_connect(
            socketDst, result,
            [this, self](boost::system::error_code ec, tcp::endpoint ed) {
                if (!ec) {
                    do_write_reply();
                    do_read_src();
                    do_read_dst();
                } else {
                    replyFormat[1] = 91;
                    do_write_reply();
                    close_both_side_socket();
                    exit(0);
                }
            });
    }

    void do_bind() {

        boost::system::error_code ec;
        tcp::endpoint endpoint_(tcp::v4(), 0);
        acceptor_.open(endpoint_.protocol());
        acceptor_.bind(endpoint_, ec);
        if (ec) {
            // cerr << "bind error code: " << ec.message() << '\n';
            acceptor_.close();
            socketSrc.close();
            exit(0);
        }

        acceptor_.listen();
        unsigned int port = acceptor_.local_endpoint().port();
        // cout << "port: " << port << '\n' << flush;
        replyFormat[2] = (port >> 8) & 0x000000FF;
        replyFormat[3] = port & 0x000000FF;
        do_write_reply();
        acceptor_.accept(socketDst, ec);
        do_write_reply();
        do_read_src();
        do_read_dst();
    }

    void do_read_src() {
        auto self(shared_from_this());
        memset(data_, '\0', sizeof(data_));
        socketSrc.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    do_write_dst(length);
                } else {
                    close_both_side_socket();
                    exit(0);
                }
            });
    }

    void do_read_dst() {
        auto self(shared_from_this());
        memset(data_, '\0', sizeof(data_));
        socketDst.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    do_write_src(length);
                } else {
                    close_both_side_socket();
                    exit(0);
                }
            });
    }

    void do_write_src(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socketSrc, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    do_read_dst();
                } else {
                    close_both_side_socket();
                    exit(0);
                }
            });
    }

    void do_write_dst(size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socketDst, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, size_t length) {
                if (!ec) {
                    do_read_src();
                } else {
                    close_both_side_socket();
                    exit(0);
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

    void close_both_side_socket() {
        socketSrc.close();
        socketDst.close();
    }

    void install_firewall_rule() {
        request.reply = "Reject";
        ifstream in("./socks.conf");
        if (!in.is_open()) {
            // cout << "File open fail!\n";
            return;
        }
        string rule = "";
        while (getline(in, rule)) {
            vector<string> tmp;
            split_string(tmp, rule, ' ');

            if (tmp[0] != "permit" || (tmp[1] == "c" && request.cd == 2) ||
                (tmp[1] == "b" && request.cd == 1))
                continue;

            vector<string> ipFirewall;
            vector<string> ipDst;
            split_string(ipFirewall, tmp[2], '.');
            split_string(ipDst, request.d_IP, '.');

            if (adsl(ipFirewall, ipDst)) {
                request.reply = "Accept";
                break;
            }
        }
    }

    bool adsl(vector<string> &ipFirewall, vector<string> &ipDst) {
        bool accept = true;
        for (int i = 0; i < ipFirewall.size(); i++) {
            if (ipFirewall[i] == "*")
                continue;
            if (ipFirewall[i] != ipDst[i]) {
                accept = false;
                break;
            }
        }
        return accept;
    }

    void split_string(vector<string> &output, string &input, char key) {
        stringstream ss(input);
        string token;
        while (getline(ss, token, key))
            output.push_back(token);
    }

    tcp::socket socketSrc;
    tcp::socket socketDst;
    tcp::acceptor acceptor_;
    tcp::resolver resolver_;
    boost::asio::io_context &io_context_;
    socksRequestStruct request;
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
        signal_child_handler();
        do_accept();
    }

  private:
    void signal_child_handler() {
        signal_.async_wait([this](boost::system::error_code ec, int signo) {
            while (waitpid(-1, NULL, WNOHANG) > 0)
                ;
            signal_child_handler();
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