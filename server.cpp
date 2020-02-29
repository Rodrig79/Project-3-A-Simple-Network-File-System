#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include "FileSys.h"
using namespace std;

void recieve_message(int socket, string &message);
void send_message(int socket, string message);

int main(int argc, char* argv[]) {
	if (argc < 2) {
		cout << "Usage: ./nfsserver port#\n";
        return -1;
    }
    int port = atoi(argv[1]);
	
	socklen_t addr_size;
	struct addrinfo hints, *res;
	struct sockaddr_storage their_addr;
	int sockfd, accepted_fd;
    //networking part: create the socket and accept the client connection
	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if(getaddrinfo(NULL, argv[1], &hints, &res) != 0)
		cout << "getaddrinfo failed" << endl;
	//cout << res << endl;
	//cout << "server name is: -" << res -> ai_canonname << "-"<< endl;

	sockfd = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol);
	
	if(bind(sockfd, res -> ai_addr, res -> ai_addrlen) != 0)
		cout << "bind failed" << endl;
	if(listen(sockfd, 0) != 0) // create an acceptable backlog ??? increase???
		cout << "listen failed" << endl;
	addr_size = sizeof their_addr;
	accepted_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
	//can communicate

    //mount the file system
    FileSys fs;
    fs.mount(accepted_fd); //assume that sock is the new socket created 
                    //for a TCP connection between the client and the server.   
 
    //loop: get the command from the client and invoke the file
    //system operation which returns the results or error messages back to the clinet
    //until the client closes the TCP connection.
	cout << "server using socket_fd of: " << accepted_fd << endl;
	string full_cmd = "";
	string cmd = "";
	string fname = "";
	string num = "";
	while(true) {
		recieve_message(accepted_fd, full_cmd);
		cout << "recieved " << full_cmd << endl;
		istringstream msg(full_cmd);
		if(msg >> cmd){
			if(msg >> fname){
				msg >> num;
			}
		}
		cout << "calling command now" << endl;
		if(cmd == "mkdir") {
			fs.mkdir(fname.c_str());
		} else if (cmd == "cd") {
			fs.cd(fname.c_str());
		} else if (cmd == "home") {
			fs.home();
		} else if (cmd == "rmdir") {
			fs.rmdir(fname.c_str());
		} else if (cmd == "ls") {
			fs.ls();
		} else if (cmd == "create") {
			fs.create(fname.c_str());
		} else if (cmd == "append") {
			fs.append(fname.c_str(), num.c_str());
		} else if (cmd == "cat") {
			fs.cat(fname.c_str());
		} else if(cmd == "head") {
			cout << "$" << num << "$";
			fs.head(fname.c_str(), stoi(num));
		} else if(cmd == "rm") {
			fs.rm(fname.c_str());
		} else if(cmd == "stat") {
			fs.stat(fname.c_str());
		} else {
			send_message(accepted_fd, "Invalid Command");
		}
	}
	/*
	char buf[100];
	int chars_recieved = recv(accepted_fd, (void*) buf, 100, 0);
	send(accepted_fd, (void*) buf, chars_recieved, 0);
	*/
    //close the listening socket
	close(accepted_fd);
    //unmout the file system
    fs.unmount();

    return 0;
}

void recieve_message(int socket, string &message) {
	char buf[100];
	message = "";
	int recently_recieved_data = 0;
	string recently_recieved_string = "";
	while(true) {
		recently_recieved_data = recv(socket, (void*) buf, 100, 0);
		if(recently_recieved_data == -1) {
			cout << "error recieving data" << endl;
			break;
		}
		buf[recently_recieved_data] = '\0';
		recently_recieved_string = string(buf);
		//make this faster somehow
		/*
		if(recently_recieved_string.find("\r\n"))
			break;
		*/
		if(message.find("\r\n") >= 0)
			break;
		message += recently_recieved_string;
	}
	message += recently_recieved_string;
}

void send_message(int socket, string message) {
	const char* buf = (message + "\r\n").c_str();
	int len = message.length();
	int sent_data = 0;
	int recently_sent_data = 0;
	while(sent_data < len){
		recently_sent_data = send(socket, (void*) (buf + sent_data), len - sent_data, 0);
		if(recently_sent_data == -1) {
			cout << "error sending data" << endl;
			break;
		}
		sent_data += recently_sent_data;
	}
}
