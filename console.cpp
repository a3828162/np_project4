#include <boost/algorithm/string.hpp>
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
#include <vector>

using boost::asio::ip::tcp;
using namespace std;

struct clientInfo {
    string hostName;
    string port;
    string testFile;
};

struct sockServerInfo {
    string hostName;
    string port;
};

const string env_Variables[9] = {
    "REQUEST_METHOD",  "REQUEST_URI", "QUERY_STRING",
    "SERVER_PROTOCOL", "HTTP_HOST",   "SERVER_ADDR",
    "SERVER_PORT",     "REMOTE_ADDR", "REMOTE_PORT"};

map<string, string> env;
vector<clientInfo> clients(5);
sockServerInfo sockServer;

string get_console_html() {
    string consoleHead = R"(
		<!DOCTYPE html>
		<html lang="en">
		  <head>
		    <meta charset="UTF-8" />
		    <title>NP Project 3 Sample Console</title>
		    <link
		      rel="stylesheet"
		      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
		      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
		      crossorigin="anonymous"
		    />
		    <link
		      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
		      rel="stylesheet"
		    />
		    <link
		      rel="icon"
		      type="image/png"
		      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
		    />
		    <style>
		      * {
		        font-family: 'Source Code Pro', monospace;
		        font-size: 1rem !important;
		      }
		      body {
		        background-color: #212529;
		      }
		      pre {
		        color: #cccccc;
		      }
		      b {
		        color: #01b468;
		      }
		    </style>
		  </head>
		  <body>
		    <table class="table table-dark table-bordered">
		      <thead>
		        <tr>
	)";
    string consoleBody1;
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].hostName != "" && clients[i].port != "" &&
            clients[i].testFile != "") {
            consoleBody1 += "<th scope=\"col\">" + clients[i].hostName + ":" +
                            clients[i].port + "</th>\r\n";
        }
    }

    string consoleBody2 = R"(
				</tr>
		      </thead>
		      <tbody>
		        <tr>
	)";

    string consoleBody3;
    for (int i = 0; i < clients.size(); i++) {
        if (clients[i].hostName != "" && clients[i].port != "" &&
            clients[i].testFile != "") {
            consoleBody3 += "<td><pre id=\"s" + to_string(i) +
                            "\" class=\"mb-0\"></pre></td>\r\n";
        }
    }
    string consoleBody4 = R"(
		        </tr>
		      </tbody>
		    </table>
		  </body>
		</html>
	)";

    return consoleHead + consoleBody1 + consoleBody2 + consoleBody3 +
           consoleBody4;
}

class shellClient : public std::enable_shared_from_this<shellClient> {
  public:
    shellClient(boost::asio::io_context &io_context, int index)
        : resolver(io_context), socket_(io_context), index(index) {}

    void start() { do_resolve(); }

  private:
    void do_resolve() {
        auto self(shared_from_this());
        resolver.async_resolve(
            clients[index].hostName, clients[index].port,
            [this, self](boost::system::error_code ec,
                         tcp::resolver::results_type result) {
                if (!ec) {
                    memset(data_, '\0', sizeof(data_));

                    do_connect(result);
                } else {
                    cerr << "resolv error code: " << ec.message() << '\n';
                }
            });
    }

    void do_connect(tcp::resolver::results_type &result) {
        auto self(shared_from_this());
        boost::asio::async_connect(
            socket_, result,
            [this, self](boost::system::error_code ec, tcp::endpoint ed) {
                if (!ec) {
                    in.open("./test_case/" + clients[index].testFile);
                    if (!in.is_open()) {
                        cout << clients[index].testFile << " open fail\n";
                        socket_.close();
                    }
                    do_read();
                } else {
                    cerr << "connect error code: " << ec.message() << '\n';
                    socket_.close();
                }
            });
    }

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    data_[length] = '\0';
                    string msg = string(data_);
                    memset(data_, '\0', sizeof(data_));
                    string tr_msg = transform_http_type(msg);
                    cout << "<script>document.getElementById('s" << index
                         << "').innerHTML += '" << tr_msg << "';</script>\n"
                         << flush;
                    if (msg.find("%") != string::npos) {
                        do_write();
                    } else {
                        do_read();
                    }
                } else {
                    cerr << "read error code: " << ec.message() << '\n';
                    socket_.close();
                }
            });
    }

    void do_write() {
        auto self(shared_from_this());
        string cmd;
        getline(in, cmd);
        cmd.push_back('\n');
        string tr_cmd = transform_http_type(cmd);
        cout << "<script>document.getElementById('s" << index
             << "').innerHTML += '<b>" << tr_cmd << "</b>';</script>\n"
             << flush;
        boost::asio::async_write(
            socket_, boost::asio::buffer(cmd, cmd.length()),
            [this, self, cmd](boost::system::error_code ec,
                              std::size_t length) {
                if (!ec) {
                    cmd == "exit\n" ? socket_.close() : do_read();
                }
            });
    }

    string transform_http_type(string &input) {

        string output(input);
        boost::replace_all(output, "&", "&amp;");
        boost::replace_all(output, ">", "&gt;");
        boost::replace_all(output, "<", "&lt;");
        boost::replace_all(output, "\"", "&quot;");
        boost::replace_all(output, "\'", "&apos;");
        boost::replace_all(output, "\n", "&NewLine;");
        boost::replace_all(output, "\r", "");

        return output;
    }
    tcp::resolver resolver;
    tcp::socket socket_;
    int index;
    ifstream in;
    enum { max_length = 4096 };
    char data_[max_length];
};

class socksClient : public std::enable_shared_from_this<shellClient> {

  public:
    socksClient(boost::asio::io_context &io_context, int index)
        : resolver(io_context), socket_(io_context), index(index) {}

    void start() { do_resolve(); }

  private:
    void do_resolve() {
        auto self(shared_from_this());
        resolver.async_resolve(
            clients[index].hostName, clients[index].port,
            [this, self](boost::system::error_code ec,
                         tcp::resolver::results_type result) {
                if (!ec) {
                    memset(data_, '\0', sizeof(data_));

                    // do_connect(result);
                } else {
                    cerr << "resolv error code: " << ec.message() << '\n';
                }
            });
    }

    void do_connect(tcp::resolver::results_type &result) {
        auto self(shared_from_this());
        boost::asio::async_connect(
            socket_, result,
            [this, self](boost::system::error_code ec, tcp::endpoint ed) {
                if (!ec) {
                    in.open("./test_case/" + clients[index].testFile);
                    if (!in.is_open()) {
                        cout << clients[index].testFile << " open fail\n";
                        socket_.close();
                    }
                    do_read();
                } else {
                    cerr << "connect error code: " << ec.message() << '\n';
                    socket_.close();
                }
            });
    }

    void do_socks() {
        string request;
        unsigned char reply[8];
        // version=4
        request.push_back(4);
        // cd=1 connect
        request.push_back(1);
        // port
        int port = stoi(clients[index].port);
        int high = port / 256;
        int low = port % 256;
        request.push_back(high > 128 ? high - 256 : high);
        request.push_back(low > 128 ? low - 256 : low);
        // ip=0.0.0.x
        request.push_back(0);
        request.push_back(0);
        request.push_back(0);
        request.push_back(1);
        // null
        request.push_back(0);
        // host
        request += clients[index].hostName;
        // null
        request.push_back(0);

        boost::asio::write(socket_, boost::asio::buffer(request),
                           boost::asio::transfer_all());
        boost::asio::read(socket_, boost::asio::buffer(reply),
                          boost::asio::transfer_all());

        if (reply[1] != 90) {
            cerr << "socks" << '\n';
            socket_.close();
        }
    }

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            boost::asio::buffer(data_, max_length),
            [this, self](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    data_[length] = '\0';
                    string msg = string(data_);
                    memset(data_, '\0', sizeof(data_));
                    string tr_msg = transform_http_type(msg);
                    cout << "<script>document.getElementById('s" << index
                         << "').innerHTML += '" << tr_msg << "';</script>\n"
                         << flush;
                    if (msg.find("%") != string::npos) {
                        do_write();
                    } else {
                        do_read();
                    }
                } else {
                    cerr << "read error code: " << ec.message() << '\n';
                    socket_.close();
                }
            });
    }

    void do_write() {
        auto self(shared_from_this());
        string cmd;
        getline(in, cmd);
        cmd.push_back('\n');
        string tr_cmd = transform_http_type(cmd);
        cout << "<script>document.getElementById('s" << index
             << "').innerHTML += '<b>" << tr_cmd << "</b>';</script>\n"
             << flush;
        boost::asio::async_write(
            socket_, boost::asio::buffer(cmd, cmd.length()),
            [this, self, cmd](boost::system::error_code ec,
                              std::size_t length) {
                if (!ec) {
                    cmd == "exit\n" ? socket_.close() : do_read();
                }
            });
    }

    string transform_http_type(string &input) {

        string output(input);
        boost::replace_all(output, "&", "&amp;");
        boost::replace_all(output, ">", "&gt;");
        boost::replace_all(output, "<", "&lt;");
        boost::replace_all(output, "\"", "&quot;");
        boost::replace_all(output, "\'", "&apos;");
        boost::replace_all(output, "\n", "&NewLine;");
        boost::replace_all(output, "\r", "");

        return output;
    }

    tcp::resolver resolver;
    tcp::socket socket_;
    int index;
    ifstream in;
    enum { max_length = 4096 };
    char data_[max_length];
};

void setEnvVar() {
    for (auto &s : env_Variables) {
        env[s] = string(getenv(s.c_str()));
    }
}

void setClientInfo() {
    string query = env["QUERY_STRING"];
    stringstream ss(query);
    string token;
    int i = 0;
    while (getline(ss, token, '&')) {
        clients[i].hostName =
            token.substr(token.find("=") + 1, token.size() - 1);
        getline(ss, token, '&');
        clients[i].port = token.substr(token.find("=") + 1, token.size() - 1);
        getline(ss, token, '&');
        clients[i].testFile =
            token.substr(token.find("=") + 1, token.size() - 1);
        ++i;
        if (i == 5)
            break;
    }
    getline(ss, token, '&');
    sockServer.hostName = token.substr(token.find("=") + 1, token.size() - 1);
    getline(ss, token, '&');
    sockServer.port = token.substr(token.find("=") + 1, token.size() - 1);
    // cerr << "Server\n";
    /*cerr << "Server: " << sockServer.hostName << ":" << sockServer.port <<
    endl; for (int i = 0; i < clients.size(); i++) { cerr << "Client " << i <<
    ": " << clients[i].hostName << ":"
             << clients[i].port << " " << clients[i].testFile << endl;
    }*/
}

void printhttp() { cout << get_console_html() << flush; }

int main() {
    try {
        cout << "Content-type: text/html\r\n\r\n" << flush;
        setEnvVar();
        setClientInfo();
        printhttp();
        boost::asio::io_context io_context;
        if (sockServer.hostName != "" && sockServer.port != "") {
            for (int i = 0; i < clients.size(); i++) {
                std::make_shared<shellClient>(io_context, i)->start();
            }
        } else {
            for (int i = 0; i < clients.size(); i++) {
                std::make_shared<shellClient>(io_context, i)->start();
            }
        }

        io_context.run();
    } catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}