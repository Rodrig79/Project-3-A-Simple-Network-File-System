// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#ifndef FILESYS_H
#define FILESYS_H

#include "BasicFileSys.h"
#include "Blocks.h"

class FileSys {
  
  public:
    // mounts the file system
    void mount(int sock);

    // unmounts the file system
    void unmount();

    // make a directory
    void mkdir(const char *name);

    // switch to a directory
    void cd(const char *name);
    
    // switch to home directory
    void home();
    
    // remove a directory
    void rmdir(const char *name);

    // list the contents of current directory
    void ls();

    // create an empty data file
    void create(const char *name);

    // append data to a data file
    void append(const char *name, const char *data);

    // display the contents of a data file
    void cat(const char *name);

    // display the first N bytes of the file
    void head(const char *name, unsigned int n);

    // delete a data file
    void rm(const char *name);

    // display stats about file or directory
    void stat(const char *name);

  private:
    BasicFileSys bfs;	// basic file system
    short curr_dir;	// current directory

    int fs_sock;  // file server socket

	dirblock_t curr_dir_block; // current directory block, used to avoid excessive reads from disk
	
    // Additional private variables and Helper functions - if desired
	
	//returns block number of the corresponding name in the current directory
	//returns -1 if it doesn't exist
	short get_block_index(const char* name);
	
	//uses send_message to send a response to the client with the given status and message using the four line format provided
	void send_response(std::string status, std::string message);
	
	//helper function for send_response, actually sends the message to the client
	void send_message(std::string message);
	
	//status commands used for responses
	const char* ERR_500 = "500 File is not a directory";
	const char* ERR_501 = "501 File is a directory";
	const char* ERR_502 = "502 File exists";
	const char* ERR_503 = "503 File does not exist";
	const char* ERR_504 = "504 File name is too long";
	const char* ERR_505 = "505 Disk is full";
	const char* ERR_506 = "506 Directory is full";
	const char* ERR_507 = "507 Directory is not empty";
	const char* ERR_508 = "508 Append exceeds maximum file size";
	const char* STAT_200 = "200 OK";
};

#endif 
