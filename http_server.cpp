#include <boost/asio.hpp>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

using namespace boost::asio;
using namespace boost::asio::ip;
using boost::asio::ip::tcp;
using namespace std;

const string env_Variables[9] = {
    "REQUEST_METHOD",  "REQUEST_URI", "QUERY_STRING",
    "SERVER_PROTOCOL", "HTTP_HOST",   "SERVER_ADDR",
    "SERVER_PORT",     "REMOTE_ADDR", "REMOTE_PORT"};

class session : public std::enable_shared_from_this<session> {
  public:
    session(tcp::socket socket) : socket_(std::move(socket)) {}

    void start() { do_read(); }

  private:
    void do_read() {
        auto self(shared_from_this());

        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {

                    data_[length] = '\0';

                    int child_pid;
                    child_pid = fork();
                    if (child_pid < 0) {

                    } else if (child_pid == 0) {
                        http_request_parser();
                        memset(data_, '\0', length);
                        do_write("HTTP/1.1 200 OK\r\n");
                        dup_to_child();

                        // cout << "HTTP/1.1 200 OK\r\n" << flush;

                        string filepath =
                            "." + env[env_Variables[1]].substr(
                                      0, env[env_Variables[1]].find("?"));

                        char *argv[] = {(char *)filepath.c_str(), NULL};
                        if (execvp(argv[0], argv) == -1) {
                            cerr << "Execute error: " << strerror(errno) << ", "
                                 << argv[0] << endl;
                            exit(1);
                        }
                    } else if (child_pid > 0) {
                        socket_.close();
                        while (waitpid(-1, NULL, WNOHANG) > 0)
                            ;
                        env.clear();
                    }
                }
            });
    }

    void do_write(string msg) {
        auto self(shared_from_this());
        boost::asio::async_write(
            socket_, boost::asio::buffer(msg, msg.size()),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    cerr << "Write OK!\n";
                }
            });
    }

    void dup_to_child() {
        dup2(socket_.native_handle(), STDIN_FILENO);
        dup2(socket_.native_handle(), STDOUT_FILENO);
    }

    void http_request_parser() {
        stringstream ss;
        ss << string(data_);
        string token;
        vector<string> tmp;
        while (ss >> token)
            tmp.push_back(token);
        /*for (size_t i = 0; i < tmp.size(); ++i)
            cout << tmp[i] << " : " << tmp[i].size() << "\n";*/

        env[env_Variables[0]] = tmp[0];
        env[env_Variables[1]] = tmp[1];
        env[env_Variables[3]] = tmp[2];
        env[env_Variables[4]] = tmp[4];
        env[env_Variables[5]] = socket_.local_endpoint().address().to_string();
        env[env_Variables[6]] = to_string(socket_.local_endpoint().port());
        env[env_Variables[7]] = socket_.remote_endpoint().address().to_string();
        env[env_Variables[8]] = to_string(socket_.remote_endpoint().port());

        if (env[env_Variables[1]].find("?") != string::npos) {
            env[env_Variables[2]] = env[env_Variables[1]].substr(
                env[env_Variables[1]].find("?") + 1);
        } else {
            env[env_Variables[2]] = "";
        }

        for (auto &[key, value] : env) {
            setenv(key.c_str(), value.c_str(), 1);
        }
    }

    tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
    map<string, string> env;
};

class server {
  public:
    server(boost::asio::io_context &io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }

  private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
};

int main(int argc, char *argv[]) {

    try {
        if (argc != 2) {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
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