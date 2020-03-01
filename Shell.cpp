// CPSC 3500: Shell
// Implements a basic shell (command line interface) for the file system

#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <cstring>
#include <string>
#include <climits>
using namespace std;

#include "Shell.h"

static const string PROMPT_STRING = "NFS> ";	// shell prompt

// Mount the network file system with server name and port number in the format of server:port
void Shell::mountNFS(string fs_loc) {
	//create the socket cs_sock and connect it to the server and port specified in fs_loc
	//if all the above operations are completed successfully, set is_mounted to true  
	struct addrinfo hints, *result, *p;
	int sockfd;
	string name, port;
	int split;
	bool failed = false;
	
	split = fs_loc.find(":");
	
	name = fs_loc.substr(0, split);
	port = fs_loc.substr(split + 1);
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if(getaddrinfo(name.c_str(), port.c_str(), &hints, &result) != 0) {
		cout << "getaddrinfo failed" << endl;
		failed = true;
	}
	
	//iterate through results to find one we can connect to
	for(p = result; p != NULL; p = p -> ai_next) {
		if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
			perror("socket");
		} else {
			if(connect(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
				perror("connect");
			} else {
				break;
			}
		}
	}
	
	if(p == NULL) {
		cout << "couldn't find any sockets to connect to" << endl;
		failed = true;
	}
	
	cs_sock = sockfd;
	if(!failed)
		is_mounted = true;
	
	freeaddrinfo(result);
}

// Unmount the network file system if it was mounted
void Shell::unmountNFS() {
	if(is_mounted && (close(cs_sock) == -1))
		cout << "ran into an issue while closing" << endl;
	is_mounted = false;
	// close the socket if it was mounted
}

// Remote procedure call on mkdir
void Shell::mkdir_rpc(string dname) {
	send_message("mkdir " + dname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on cd
void Shell::cd_rpc(string dname) {
	send_message("cd " + dname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on home
void Shell::home_rpc() {
	send_message("home");
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on rmdir
void Shell::rmdir_rpc(string dname) {
	send_message("rmdir " + dname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on ls
void Shell::ls_rpc() {
	send_message("ls");
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on create
void Shell::create_rpc(string fname) {
	send_message("create " + fname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on append
void Shell::append_rpc(string fname, string data) {
	send_message("append " + fname + " " + data);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procesure call on cat
void Shell::cat_rpc(string fname) {
	send_message("cat " + fname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on head
void Shell::head_rpc(string fname, int n) {
	stringstream s;
	s << "head " << fname << " " << n;
	send_message(s.str());
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on rm
void Shell::rm_rpc(string fname) {
	send_message("rm " + fname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

// Remote procedure call on stat
void Shell::stat_rpc(string fname) {
	send_message("stat " + fname);
	string msg;
	receive_message(msg);
	cout << msg << endl;
}

//attempts to send the entire message to cs_sock
void Shell::send_message(string message) {
	const char* buf = (message + "\r\n").c_str();
	int len = message.length() + 2;
	int sent_data = 0;
	int recently_sent_data = 0;
	while(sent_data < len){
		recently_sent_data = send(cs_sock, (void*) (buf + sent_data), len - sent_data, 0);
		if(recently_sent_data == -1) {
			cout << "error sending data" << endl;
			break;
		}
		sent_data += recently_sent_data;
	}
}

//receives a message from cs_sock
void Shell::receive_message(string & message) {
	char buf[100];
	message = "";
	int line_count = 0;
	int recently_received_data = 0;
	int final_msg_len = INT_MAX;
	int final_msg_count = 0;
	string recently_received_string = "";
	string status = "";
	
	while(final_msg_count < final_msg_len) {
		int msg_break = 0;
		
		if((recently_received_data = recv(cs_sock, (void*) buf, 100, 0)) == -1) {
			cout << "error receiving data" << endl;
			break;
		}
		buf[recently_received_data] = '\0';
		recently_received_string = string(buf);
		message += recently_received_string;
		if(line_count == 3)
			final_msg_count += recently_received_data;
		msg_break = message.find("\r\n");
		while(msg_break >= 0) {
			if(line_count == 0)
				status = message.substr(0, msg_break);
			if(line_count == 1)
				final_msg_len = stoi(message.substr(8, msg_break - 8));
			message = message.substr(msg_break + 2);
			if(line_count == 2)
				final_msg_count = message.length();
			line_count++;
			msg_break = message.find("\r\n");
		}
	}
	if(final_msg_len == 0)
		message = status;
}

// Executes the shell until the user quits.
void Shell::run()
{
  // make sure that the file system is mounted
  if (!is_mounted)
 	return; 
  
  // continue until the user quits
  bool user_quit = false;
  while (!user_quit) {

    // print prompt and get command line
    string command_str;
    cout << PROMPT_STRING;
    getline(cin, command_str);
	
    // execute the command
    user_quit = execute_command(command_str);
  }

  // unmount the file system
  unmountNFS();
}

// Execute a script.
void Shell::run_script(char *file_name)
{
  // make sure that the file system is mounted
  if (!is_mounted)
  	return;
  // open script file
  ifstream infile;
  infile.open(file_name);
  if (infile.fail()) {
    cerr << "Could not open script file" << endl;
    return;
  }


  // execute each line in the script
  bool user_quit = false;
  string command_str;
  getline(infile, command_str, '\n');
  while (!infile.eof() && !user_quit) {
    cout << PROMPT_STRING << command_str << endl;
    user_quit = execute_command(command_str);
    getline(infile, command_str);
  }

  // clean up
  unmountNFS();
  infile.close();
}


// Executes the command. Returns true for quit and false otherwise.
bool Shell::execute_command(string command_str)
{
  // parse the command line
  struct Command command = parse_command(command_str);

  // look for the matching command
  if (command.name == "") {
    return false;
  }
  else if (command.name == "mkdir") {
    mkdir_rpc(command.file_name);
  }
  else if (command.name == "cd") {
    cd_rpc(command.file_name);
  }
  else if (command.name == "home") {
    home_rpc();
  }
  else if (command.name == "rmdir") {
    rmdir_rpc(command.file_name);
  }
  else if (command.name == "ls") {
    ls_rpc();
  }
  else if (command.name == "create") {
    create_rpc(command.file_name);
  }
  else if (command.name == "append") {
    append_rpc(command.file_name, command.append_data);
  }
  else if (command.name == "cat") {
    cat_rpc(command.file_name);
  }
  else if (command.name == "head") {
    errno = 0;
    unsigned long n = strtoul(command.append_data.c_str(), NULL, 0);
    if (0 == errno) {
      head_rpc(command.file_name, n);
    } else {
      cerr << "Invalid command line: " << command.append_data;
      cerr << " is not a valid number of bytes" << endl;
      return false;
    }
  }
  else if (command.name == "rm") {
    rm_rpc(command.file_name);
  }
  else if (command.name == "stat") {
    stat_rpc(command.file_name);
  }
  else if (command.name == "quit") {
    return true;
  }

  return false;
}

// Parses a command line into a command struct. Returned name is blank
// for invalid command lines.
Shell::Command Shell::parse_command(string command_str)
{
  // empty command struct returned for errors
  struct Command empty = {"", "", ""};

  // grab each of the tokens (if they exist)
  struct Command command;
  istringstream ss(command_str);
  int num_tokens = 0;
  if (ss >> command.name) {
    num_tokens++;
    if (ss >> command.file_name) {
      num_tokens++;
      if (ss >> command.append_data) {
        num_tokens++;
        string junk;
        if (ss >> junk) {
          num_tokens++;
        }
      }
    }
  }

  // Check for empty command line
  if (num_tokens == 0) {
    return empty;
  }
    
  // Check for invalid command lines
  if (command.name == "ls" ||
      command.name == "home" ||
      command.name == "quit")
  {
    if (num_tokens != 1) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "mkdir" ||
      command.name == "cd"    ||
      command.name == "rmdir" ||
      command.name == "create"||
      command.name == "cat"   ||
      command.name == "rm"    ||
      command.name == "stat")
  {
    if (num_tokens != 2) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else if (command.name == "append" || command.name == "head")
  {
    if (num_tokens != 3) {
      cerr << "Invalid command line: " << command.name;
      cerr << " has improper number of arguments" << endl;
      return empty;
    }
  }
  else {
    cerr << "Invalid command line: " << command.name;
    cerr << " is not a command" << endl; 
    return empty;
  } 

  return command;
}

