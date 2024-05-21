#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <string.h>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>

#define ACCEPT 90
using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std;
using namespace boost::placeholders;


struct HostInfo{
    string host;
    string port;
    string file;
    bool used = 0;
};
HostInfo hostInfo[5];

string socksHost;
string socksPort;
io_context io_Context;

class client : public enable_shared_from_this<client>{
        public:
            client(string id)
                : id_(id){
                    file_.open(("./test_case/" + hostInfo[stoi(id)].file), ios::in);
                }
            
  		    void start(){
    		    do_resolve();
  		    }            
 
        private:
            tcp::socket socket_{io_Context};
            tcp::resolver resolver_{io_Context};
            //tcp::resolver::query que_;
            string id_;
            fstream file_;
            enum { max_length = 15000 };
            char data_[max_length];
            //unsigned char sock_[max_length];
			array<unsigned char, 8> req_;
            array<unsigned char, 8> reply_;
            string data1 = "";            
            string data2 = ""; 
            string text = "";
            string IP = "";

            string do_replace(string t){
                string ret = "";
                for (int i=0; i<(int)t.length(); i++){
                    if (t[i] == '\n')
                        ret += "<br>";
                    else if (t[i] == '\r')
                        ret += "";
                    else if (t[i] == '\'')
                        ret += "&apos;";
                    else if (t[i] == '\"')
                        ret += "&quot;";  
                    else if (t[i] == '&')
                        ret += "&amp;";  
                    else if (t[i] == '<')
                        ret += "&lt;";
                    else if (t[i] == '>')
                        ret += "&gt;";
                    else
                        ret += t[i];
                }
                return ret;
            }

            void do_write(){
                auto self(shared_from_this());
                data2 = "";
                getline(file_, data2);
			    if(data2.find("exit") != string::npos){
				    file_.close();
				    hostInfo[stoi(id_)].used = 0;
			    }
                data2 = data2 + "\n";
                text = do_replace(data2);
                cout << "<script>document.getElementById('s" << id_ << "').innerHTML += '<b>" << text << "</b>';</script>"<<endl;
                text = "";
                //const char *mes = text.c_str();
                async_write(socket_, buffer(data2.c_str(), data2.length()), [this, self](boost::system::error_code ec, size_t /*length*/){
                    if (!ec){
                        if (hostInfo[stoi(id_)].used == 1)
                            do_read();
                    }
                });
            }

            void do_read(){
                auto self(shared_from_this());
                bzero(data_, max_length);
                socket_.async_read_some(buffer(data_, max_length), [this, self](boost::system::error_code ec, size_t length){
                    if (!ec){
                        data1 = "";
                        data1.assign(data_);
					    bzero(data_, max_length);
                        text = do_replace(data1);
                        //strcpy(data_, text.c_str());
                        //turn shell output to web page
                        cout<<"<script>document.getElementById('s" << id_ << "').innerHTML += '"<< text <<"';</script>"<<endl;
                        if (text.find("% ") != string::npos){
                            do_write();
                        }
                        else{
                            do_read();
                        }                    
                    }
                });
            }

            void do_reply_handler(boost::system::error_code ec, size_t length){
                if(!ec){
				    if(reply_[1] == (unsigned char)ACCEPT)
					    do_read();
                }
            }

            void do_reply(){
                reply_.fill(0);
                socket_.async_read_some(buffer(reply_, reply_.size()), boost::bind(&client::do_reply_handler, shared_from_this(), _1, _2));
            }           

            void do_send_handler(boost::system::error_code ec, size_t length){
                if(!ec){
                    do_reply();
                }
            }

            void do_send(){
                //do_read();
                req_[0] = (unsigned char)4;
                req_[1] = (unsigned char)1;
                req_[2] = (unsigned char)(stoi(hostInfo[stoi(id_)].port)/256);
                req_[3] = (unsigned char)(stoi(hostInfo[stoi(id_)].port)%256);

			    tcp::resolver resolver_(io_Context);
			    tcp::resolver::query que(hostInfo[stoi(id_)].host , "");
			    for(tcp::resolver::iterator it = resolver_.resolve(que); it != tcp::resolver::iterator(); ++it){
				    tcp::endpoint ep = *it;
			        if(ep.address().is_v4())
				        IP = ep.address().to_string();
			    }
			    if(IP != ""){
                    vector<string> temp;
                    boost::split(temp, IP, boost::is_any_of("."), boost::token_compress_on);
				    req_[4] = (unsigned char)(stoi(temp[0]));
				    req_[5] = (unsigned char)(stoi(temp[1]));
				    req_[6] = (unsigned char)(stoi(temp[2]));
				    req_[7] = (unsigned char)(stoi(temp[3]));
			    }
			    socket_.async_write_some(buffer(req_.data(), req_.size()), boost::bind(&client::do_send_handler, shared_from_this(), _1, _2));                    
            }

            void do_connect(tcp::resolver::iterator it){
                auto self(shared_from_this());
                socket_.async_connect(*it, [this, self](const boost::system::error_code ec){
                    if (!ec){
                        do_send();
                    }
                });
            }

            void do_resolve(){
                auto self(shared_from_this());
                tcp::resolver::query que_(hostInfo[stoi(id_)].host, hostInfo[stoi(id_)].port);
                resolver_.async_resolve(que_, [this, self](boost::system::error_code ec, tcp::resolver::iterator it){
                    if(!ec){
                        do_connect(it);
                        //do_read();
                    }
                });
            }             
};

void print_HTML(){
	cout<<"content-type: text/html\r\n\r\n";
	cout<<"<!DOCTYPE html>"<<endl;
	cout<<"<html lang=i\"en\">"<<endl;
 	cout<<"<head>"<<endl;
    cout<<"<meta charset=\"UTF-8\" />"<<endl;
	cout<<"<title>NP Project 3 Sample Console</title>"<<endl;
	cout<<"<link"<<endl;
	cout<<"rel=\"stylesheet\""<<endl;
	cout<<"href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""<< endl;
	cout<<"integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""<<endl;
	cout<<"crossorigin=\"anonymous\""<<endl;
	cout<<"/>"<<endl;
	cout<<"<link"<<endl;
	cout<<"href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""<<endl;
	cout<<"rel=\"stylesheet\""<<endl;
	cout<<"/>"<<endl;
	cout<<"<link"<<endl;
	cout<<"rel=\"icon\""<<endl;
	cout<<"type=\"image/png\""<<endl;
	cout<<"href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""<<endl;
	cout<<"/>"<<endl;
    cout<<"<style>"<<endl;
	cout<<"* {"<<endl;
	cout<<"font-family: 'Source Code Pro', monospace;"<<endl;
	cout<<"font-size: 1rem !important;"<<endl;
	cout<<"}"<<endl;
	cout<<"body {"<<endl;
	cout<<"background-color: #212529;"<<endl;
	cout<<"}"<<endl;
	cout<<"pre {"<<endl;
	cout<<"color: #cccccc;"<<endl;
	cout<<"}"<<endl;
	cout<<"b {"<<endl;
	cout<<"color: #01b468;"<<endl;
	cout<<"}"<<endl;
	cout<<"</style>"<<endl;
  	cout<<"</head>"<<endl;
    cout<<"<body>"<<endl;
	cout<<"<table class=\"table table-dark table-bordered\">"<<endl;
	cout<<"<thead>"<<endl;
	cout<<"<tr>"<<endl;
	for(int i=0; i<5; i++){
		if(hostInfo[i].used == 1){
			cout<<"<th scope=\"col\">";
			cout<<hostInfo[i].host<<":"<<hostInfo[i].port ;
			cout<<"</th>"<<endl;
		}
	}
	cout<<"</tr>"<<endl;
	cout<<"</thead>"<<endl;
	cout<<"<tbody>"<<endl;
	cout<<"<tr>"<<endl;
	for(int i=0; i<5; i++){
		if(hostInfo[i].used == 1){
			cout<<"<td><pre id=\"s";
			cout<<to_string(i);
			cout<<"\" class=\"mb-0\"></pre></td>"<<endl;
		}
	}
	cout<<"</tr>"<<endl;
	cout<<"</tbody>"<<endl;
	cout<<"</table>"<<endl;
	cout<<"</body>"<<endl;
	cout<<"</html>"<<endl;
}

void parse_qString(){
    string qString = getenv("QUERY_STRING");
    vector<string> c1, c2;
	boost::split(c1, qString, boost::is_any_of("&"), boost::token_compress_on);

	for(int i=0 ; i<5 ; i++){
		for(int j=0 ; j<3 ; j++){
            boost::split(c2, c1[(i*3)+j], boost::is_any_of("="), boost::token_compress_on);
            if(c2[1] == ""){
		        hostInfo[i].used = 0;
		        break;
	        }
	        else{
		        hostInfo[i].used = 1;
            }

			if(j == 0)
				hostInfo[i].host = c2[1];
			else if(j == 1)
				hostInfo[i].port = c2[1];
			else if(j == 2)
				hostInfo[i].file = c2[1];
		}		
	}
    //get the info
    vector<string> c3, c4;
    boost::split(c3, c1[15], boost::is_any_of("="),boost::token_compress_on);
	vector<string> socksHostValue = c3;
	socksHost = socksHostValue[1];
    boost::split(c4, c1[16], boost::is_any_of("="),boost::token_compress_on);
	vector<string> socksPortValue = c4;
	socksPort = socksPortValue[1];
}

int main(int argc, char* argv[]){
    try{
        parse_qString();
        print_HTML();     
        for (int i=0; i<5; i++){
            if (hostInfo[i].used == 1)
                make_shared<client>(to_string(i))->start();         
        }
        io_Context.run();
    } 
    catch (exception& e){
        cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}

// #include <boost/algorithm/string.hpp>
// #include <boost/asio.hpp>
// #include <cstdlib>
// #include <cstring>
// #include <fstream>
// #include <iostream>
// #include <map>
// #include <memory>
// #include <sstream>
// #include <string>
// #include <sys/wait.h>
// #include <unistd.h>
// #include <utility>
// #include <vector>

// using boost::asio::ip::tcp;
// using namespace std;

// struct clientInfo {
//     string hostName;
//     string port;
//     string testFile;
// };

// struct sockServerInfo {
//     string hostName;
//     string port;
// };

// const string env_Variables[9] = {
//     "REQUEST_METHOD",  "REQUEST_URI", "QUERY_STRING",
//     "SERVER_PROTOCOL", "HTTP_HOST",   "SERVER_ADDR",
//     "SERVER_PORT",     "REMOTE_ADDR", "REMOTE_PORT"};

// map<string, string> env;
// vector<clientInfo> clients(5);
// sockServerInfo sockServer;

// string get_console_html() {
//     string consoleHead = R"(
// 		<!DOCTYPE html>
// 		<html lang="en">
// 		  <head>
// 		    <meta charset="UTF-8" />
// 		    <title>NP Project 3 Sample Console</title>
// 		    <link
// 		      rel="stylesheet"
// 		      href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
// 		      integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
// 		      crossorigin="anonymous"
// 		    />
// 		    <link
// 		      href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
// 		      rel="stylesheet"
// 		    />
// 		    <link
// 		      rel="icon"
// 		      type="image/png"
// 		      href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
// 		    />
// 		    <style>
// 		      * {
// 		        font-family: 'Source Code Pro', monospace;
// 		        font-size: 1rem !important;
// 		      }
// 		      body {
// 		        background-color: #212529;
// 		      }
// 		      pre {
// 		        color: #cccccc;
// 		      }
// 		      b {
// 		        color: #01b468;
// 		      }
// 		    </style>
// 		  </head>
// 		  <body>
// 		    <table class="table table-dark table-bordered">
// 		      <thead>
// 		        <tr>
// 	)";
//     string consoleBody1;
//     for (int i = 0; i < clients.size(); i++) {
//         if (clients[i].hostName != "" && clients[i].port != "" &&
//             clients[i].testFile != "") {
//             consoleBody1 += "<th scope=\"col\">" + clients[i].hostName + ":" +
//                             clients[i].port + "</th>\r\n";
//         }
//     }

//     string consoleBody2 = R"(
// 				</tr>
// 		      </thead>
// 		      <tbody>
// 		        <tr>
// 	)";

//     string consoleBody3;
//     for (int i = 0; i < clients.size(); i++) {
//         if (clients[i].hostName != "" && clients[i].port != "" &&
//             clients[i].testFile != "") {
//             consoleBody3 += "<td><pre id=\"s" + to_string(i) +
//                             "\" class=\"mb-0\"></pre></td>\r\n";
//         }
//     }
//     string consoleBody4 = R"(
// 		        </tr>
// 		      </tbody>
// 		    </table>
// 		  </body>
// 		</html>
// 	)";

//     return consoleHead + consoleBody1 + consoleBody2 + consoleBody3 +
//            consoleBody4;
// }

// class shellClient : public std::enable_shared_from_this<shellClient> {
//   public:
//     shellClient(boost::asio::io_context &io_context, int index)
//         : resolver(io_context), socket_(io_context), index(index) {}

//     void start() { do_resolve(); }

//   private:
//     void do_resolve() {
//         auto self(shared_from_this());
//         resolver.async_resolve(
//             clients[index].hostName, clients[index].port,
//             [this, self](boost::system::error_code ec,
//                          tcp::resolver::results_type result) {
//                 if (!ec) {
//                     memset(data_, '\0', sizeof(data_));

//                     do_connect(result);
//                 } else {
//                     cerr << "resolv error code: " << ec.message() << '\n';
//                 }
//             });
//     }

//     void do_connect(tcp::resolver::results_type &result) {
//         auto self(shared_from_this());
//         boost::asio::async_connect(
//             socket_, result,
//             [this, self](boost::system::error_code ec, tcp::endpoint ed) {
//                 if (!ec) {
//                     in.open("./test_case/" + clients[index].testFile);
//                     if (!in.is_open()) {
//                         cout << clients[index].testFile << " open fail\n";
//                         socket_.close();
//                     }
//                     do_read();
//                 } else {
//                     cerr << "connect error code: " << ec.message() << '\n';
//                     socket_.close();
//                 }
//             });
//     }

//     void do_read() {
//         auto self(shared_from_this());
//         socket_.async_read_some(
//             boost::asio::buffer(data_, max_length),
//             [this, self](boost::system::error_code ec, std::size_t length) {
//                 if (!ec) {
//                     data_[length] = '\0';
//                     string msg = string(data_);
//                     memset(data_, '\0', sizeof(data_));
//                     string tr_msg = transform_http_type(msg);
//                     cout << "<script>document.getElementById('s" << index
//                          << "').innerHTML += '" << tr_msg << "';</script>\n"
//                          << flush;
//                     if (msg.find("%") != string::npos) {
//                         do_write();
//                     } else {
//                         do_read();
//                     }
//                 } else {
//                     cerr << "read error code: " << ec.message() << '\n';
//                     socket_.close();
//                 }
//             });
//     }

//     void do_write() {
//         auto self(shared_from_this());
//         string cmd;
//         getline(in, cmd);
//         cmd.push_back('\n');
//         string tr_cmd = transform_http_type(cmd);
//         cout << "<script>document.getElementById('s" << index
//              << "').innerHTML += '<b>" << tr_cmd << "</b>';</script>\n"
//              << flush;
//         boost::asio::async_write(
//             socket_, boost::asio::buffer(cmd, cmd.length()),
//             [this, self, cmd](boost::system::error_code ec,
//                               std::size_t length) {
//                 if (!ec) {
//                     cmd == "exit\n" ? socket_.close() : do_read();
//                 }
//             });
//     }

//     string transform_http_type(string &input) {

//         string output(input);
//         boost::replace_all(output, "&", "&amp;");
//         boost::replace_all(output, ">", "&gt;");
//         boost::replace_all(output, "<", "&lt;");
//         boost::replace_all(output, "\"", "&quot;");
//         boost::replace_all(output, "\'", "&apos;");
//         boost::replace_all(output, "\n", "&NewLine;");
//         boost::replace_all(output, "\r", "");

//         return output;
//     }
//     tcp::resolver resolver;
//     tcp::socket socket_;
//     int index;
//     ifstream in;
//     enum { max_length = 4096 };
//     char data_[max_length];
// };

// class socksClient : public std::enable_shared_from_this<socksClient> {

//   public:
//     socksClient(boost::asio::io_context &io_context, int index)
//         : resolver(io_context), socket_(io_context), index(index) {}

//     void start() { do_resolve(); }

//   private:
//     void do_resolve() {
//         auto self(shared_from_this());
//         resolver.async_resolve(
//             clients[index].hostName, clients[index].port,
//             [this, self](boost::system::error_code ec,
//                          tcp::resolver::results_type result) {
//                 if (!ec) {
//                     memset(data_, '\0', sizeof(data_));

//                     // do_connect(result);
//                 } else {
//                     cerr << "resolv error code: " << ec.message() << '\n';
//                 }
//             });
//     }

//     void do_connect(tcp::resolver::results_type &result) {
//         auto self(shared_from_this());
//         boost::asio::async_connect(
//             socket_, result,
//             [this, self](boost::system::error_code ec, tcp::endpoint ed) {
//                 if (!ec) {
//                     in.open("./test_case/" + clients[index].testFile);
//                     if (!in.is_open()) {
//                         cout << clients[index].testFile << " open fail\n";
//                         socket_.close();
//                     }
//                     do_read();
//                 } else {
//                     cerr << "connect error code: " << ec.message() << '\n';
//                     socket_.close();
//                 }
//             });
//     }

//     void do_socks() {
//         string request;
//         unsigned char reply[8];
//         // version=4
//         request.push_back(4);
//         // cd=1 connect
//         request.push_back(1);
//         // port
//         int port = stoi(clients[index].port);
//         int high = port / 256;
//         int low = port % 256;
//         request.push_back(high > 128 ? high - 256 : high);
//         request.push_back(low > 128 ? low - 256 : low);
//         // ip=0.0.0.x
//         request.push_back(0);
//         request.push_back(0);
//         request.push_back(0);
//         request.push_back(1);
//         // null
//         request.push_back(0);
//         // host
//         request += clients[index].hostName;
//         // null
//         request.push_back(0);

//         boost::asio::write(socket_, boost::asio::buffer(request),
//                            boost::asio::transfer_all());
//         boost::asio::read(socket_, boost::asio::buffer(reply),
//                           boost::asio::transfer_all());

//         if (reply[1] != 90) {
//             cerr << "socks" << '\n';
//             socket_.close();
//         }
//     }

//     void do_read() {
//         auto self(shared_from_this());
//         socket_.async_read_some(
//             boost::asio::buffer(data_, max_length),
//             [this, self](boost::system::error_code ec, std::size_t length) {
//                 if (!ec) {
//                     data_[length] = '\0';
//                     string msg = string(data_);
//                     memset(data_, '\0', sizeof(data_));
//                     string tr_msg = transform_http_type(msg);
//                     cout << "<script>document.getElementById('s" << index
//                          << "').innerHTML += '" << tr_msg << "';</script>\n"
//                          << flush;
//                     if (msg.find("%") != string::npos) {
//                         do_write();
//                     } else {
//                         do_read();
//                     }
//                 } else {
//                     cerr << "read error code: " << ec.message() << '\n';
//                     socket_.close();
//                 }
//             });
//     }

//     void do_write() {
//         auto self(shared_from_this());
//         string cmd;
//         getline(in, cmd);
//         cmd.push_back('\n');
//         string tr_cmd = transform_http_type(cmd);
//         cout << "<script>document.getElementById('s" << index
//              << "').innerHTML += '<b>" << tr_cmd << "</b>';</script>\n"
//              << flush;
//         boost::asio::async_write(
//             socket_, boost::asio::buffer(cmd, cmd.length()),
//             [this, self, cmd](boost::system::error_code ec,
//                               std::size_t length) {
//                 if (!ec) {
//                     cmd == "exit\n" ? socket_.close() : do_read();
//                 }
//             });
//     }

//     string transform_http_type(string &input) {

//         string output(input);
//         boost::replace_all(output, "&", "&amp;");
//         boost::replace_all(output, ">", "&gt;");
//         boost::replace_all(output, "<", "&lt;");
//         boost::replace_all(output, "\"", "&quot;");
//         boost::replace_all(output, "\'", "&apos;");
//         boost::replace_all(output, "\n", "&NewLine;");
//         boost::replace_all(output, "\r", "");

//         return output;
//     }

//     tcp::resolver resolver;
//     tcp::socket socket_;
//     int index;
//     ifstream in;
//     enum { max_length = 4096 };
//     char data_[max_length];
// };

// void setEnvVar() {
//     for (auto &s : env_Variables) {
//         env[s] = string(getenv(s.c_str()));
//     }
// }

// void setClientInfo() {
//     string query = env["QUERY_STRING"];
//     stringstream ss(query);
//     string token;
//     int i = 0;
//     while (getline(ss, token, '&')) {
//         clients[i].hostName =
//             token.substr(token.find("=") + 1, token.size() - 1);
//         getline(ss, token, '&');
//         clients[i].port = token.substr(token.find("=") + 1, token.size() - 1);
//         getline(ss, token, '&');
//         clients[i].testFile =
//             token.substr(token.find("=") + 1, token.size() - 1);
//         ++i;
//         if (i == 5)
//             break;
//     }
//     getline(ss, token, '&');
//     sockServer.hostName = token.substr(token.find("=") + 1, token.size() - 1);
//     getline(ss, token, '&');
//     sockServer.port = token.substr(token.find("=") + 1, token.size() - 1);
//     // cerr << "Server\n";
//     /*cerr << "Server: " << sockServer.hostName << ":" << sockServer.port <<
//     endl; for (int i = 0; i < clients.size(); i++) { cerr << "Client " << i <<
//     ": " << clients[i].hostName << ":"
//              << clients[i].port << " " << clients[i].testFile << endl;
//     }*/
// }

// void printhttp() { cout << get_console_html() << flush; }

// int main() {
//     try {
//         cout << "Content-type: text/html\r\n\r\n" << flush;
//         setEnvVar();
//         setClientInfo();
//         printhttp();
//         boost::asio::io_context io_context;
//         if (sockServer.hostName != "" && sockServer.port != "") {
//             for (int i = 0; i < clients.size(); i++) {
//                 std::make_shared<socksClient>(io_context, i)->start();
//             }
//         } else {
//             for (int i = 0; i < clients.size(); i++) {
//                 std::make_shared<shellClient>(io_context, i)->start();
//             }
//         }

//         io_context.run();
//     } catch (std::exception &e) {
//         std::cerr << "Exception: " << e.what() << "\n";
//     }

//     return 0;
// }