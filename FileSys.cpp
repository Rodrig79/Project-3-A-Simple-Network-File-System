// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include <algorithm>
#include <sys/socket.h>
using namespace std;

#include "FileSys.h"
#include "BasicFileSys.h"
#include "Blocks.h"

// mounts the file system
void FileSys::mount(int sock) {
  bfs.mount();
  curr_dir = 1; //by default current directory is home directory, in disk block #1
  bfs.read_block(curr_dir, (void*) &curr_dir_block);
  fs_sock = sock; //use this socket to receive file system operations from the client and send back response messages
}

// unmounts the file system
void FileSys::unmount() {
  bfs.unmount();
  close(fs_sock);
}

// make a directory
void FileSys::mkdir(const char *name)
{
	short newBlock;
	dirblock_t newDir;
	
	if(get_block_index(name) >= 0){
		send_response(ERR_502, "");
		return;
	} if(strlen(name) > MAX_FNAME_SIZE){
		send_response(ERR_504, "");
		return;
	} if(curr_dir_block.num_entries == MAX_DIR_ENTRIES){
		send_response(ERR_506, "");
		return;
	} if(!(newBlock = bfs.get_free_block())){
		send_response(ERR_505, "");
		return;
	}
	
	newDir.num_entries = 0;
	newDir.magic = DIR_MAGIC_NUM;
	bfs.write_block(newBlock, &newDir);
	
	//copy name of file into directory
	int i = 0;
	for(i = 0; name[i] != '\0'; i++)
		curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	
	curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = newBlock;
	curr_dir_block.num_entries++;
	bfs.write_block(curr_dir, &curr_dir_block);
	
	send_response(STAT_200, "success");
}

// switch to a directory
void FileSys::cd(const char *name)
{
	dirblock_t new_dir_block;
	short i;
	
	if((i = get_block_index(name)) == -1){
		send_response(ERR_503, "");
		return;
	}
	
	bfs.read_block(curr_dir_block.dir_entries[i].block_num, (void*) &new_dir_block);
	if(new_dir_block.magic != DIR_MAGIC_NUM){
		send_response(ERR_500, "");
		return;
	}
	
	curr_dir = curr_dir_block.dir_entries[i].block_num;
	curr_dir_block = new_dir_block;
	send_response(STAT_200, "success");
}

// switch to home directory
void FileSys::home() {
	curr_dir = 1;
	bfs.read_block(curr_dir, (void*) &curr_dir_block);
	send_response(STAT_200, "success");
}

// remove a directory
void FileSys::rmdir(const char *name)
{
	short i;
	dirblock_t rem_block;
	if((i = get_block_index(name)) == -1){
		send_response(ERR_503, "");
		return;
	}
	short rem = curr_dir_block.dir_entries[i].block_num;
	bfs.read_block(rem, (void*) &rem_block);
	if(rem_block.magic != DIR_MAGIC_NUM){
		send_response(ERR_500, "");
		return;
	}
	if(rem_block.num_entries > 0){
		send_response(ERR_507, "");
		return;
	}
	//move directory to remove to the end of the list then decrease size of list
	swap(curr_dir_block.dir_entries[i], curr_dir_block.dir_entries[curr_dir_block.num_entries - 1]);
	curr_dir_block.num_entries--;
	bfs.write_block(curr_dir, (void*) &curr_dir_block);
	bfs.reclaim_block(rem);
	send_response(STAT_200, "success");
}

// list the contents of current directory
void FileSys::ls()
{
	string file_list = "";
	
	//if nothing in directory return empty
	if(curr_dir_block.num_entries == 0){
		send_response(STAT_200, "empty folder");
		return;
	}
	
	dirblock_t temp;
	for(int i = 0; i < curr_dir_block.num_entries - 1; i++){
		file_list.append(curr_dir_block.dir_entries[i].name);
		bfs.read_block(curr_dir_block.dir_entries[i].block_num, (void*) &temp);
		if(temp.magic == DIR_MAGIC_NUM)
			file_list.append("/");
		file_list += " ";
	}
	file_list.append(curr_dir_block.dir_entries[curr_dir_block.num_entries - 1].name);
	bfs.read_block(curr_dir_block.dir_entries[curr_dir_block.num_entries - 1].block_num, (void*) &temp);
		if(temp.magic == DIR_MAGIC_NUM)
	file_list.append("/");
	
	send_response(STAT_200, file_list);
}

// create an empty data file
void FileSys::create(const char *name)
{
	inode_t new_file;
	short new_block;
	
	if(get_block_index(name) >= 0){
		send_response(ERR_502, "");
		return;
	}
	if(strlen(name) > MAX_FNAME_SIZE){
		send_response(ERR_504, "");
		return;
	}
	if(curr_dir_block.num_entries == MAX_DIR_ENTRIES){
		send_response(ERR_506, "");
		return;
	}
	if(!(new_block = bfs.get_free_block())) {
		send_response(ERR_505, "");
		return;
	}
	
	new_file.size = 0;
	new_file.magic = INODE_MAGIC_NUM;
	bfs.write_block(new_block, &new_file);
	
	//copy name of file into directory
	int i = 0;
	for(i = 0; name[i] != '\0'; i++)
		curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	
	curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = new_block;
	curr_dir_block.num_entries++;
	bfs.write_block(curr_dir, (void*) &curr_dir_block);
	
	send_response(STAT_200, "success");
}

// append data to a data file
void FileSys::append(const char *name, const char *data)
{
	short inode;
	inode_t inode_block;
	int data_index;
	int data_offset;
	datablock_t curr_block;
	
	if((inode = get_block_index(name)) == -1) {
		send_response(ERR_503, "");
		return;
	}
	
	bfs.read_block(curr_dir_block.dir_entries[inode].block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM){
		send_response(ERR_501, "");
		return;
	}
	if(inode_block.size == MAX_FILE_SIZE) {
		send_response(ERR_508, "");
		return;
	}
	//get the position of the last byte in the file
	data_index = inode_block.size / BLOCK_SIZE;
	data_offset = inode_block.size % BLOCK_SIZE;
	
	//allocate new block if last inode block is full
	if(data_offset == 0) {
		inode_block.blocks[data_index] = bfs.get_free_block();
	}
	
	bfs.read_block(inode_block.blocks[data_index], (void*) &curr_block);
	int i = 0;
	while(data[i] != '\0'){
		
		//if at end of current data block check to see if we can get a new one
		if(data_offset == BLOCK_SIZE) {
			data_offset = 0;
			bfs.write_block(inode_block.blocks[data_index++], (void*) &curr_block);
			
			if(data_index == MAX_DATA_BLOCKS) {
				inode_block.size = data_index * BLOCK_SIZE;
				bfs.write_block(curr_dir_block.dir_entries[inode].block_num, (void*) &inode_block);
				send_response(ERR_508, "");
				return;
			}
			if((inode_block.blocks[data_index] = bfs.get_free_block()) == 0) {
				inode_block.size = data_index * BLOCK_SIZE;
				bfs.write_block(curr_dir_block.dir_entries[inode].block_num, (void*) &inode_block);
				send_response(ERR_505, "");
				return;
			}
			//create new data block to fill
			curr_block = datablock_t();
		}
		curr_block.data[data_offset++] = data[i++];
	}
	
	bfs.write_block(inode_block.blocks[data_index], (void*) &curr_block);
	inode_block.size = data_index * BLOCK_SIZE + data_offset;
	bfs.write_block(curr_dir_block.dir_entries[inode].block_num, (void*) &inode_block);
	send_response(STAT_200, "success");
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
	short inode_index;
	inode_t inode_block;
	datablock_t data_block;
	int index = 0;
	int offset = 0;
	int block_num = 0;
	
	if((inode_index = get_block_index(name)) == -1) {
		send_response(ERR_503, "");
		return;
	}
	
	bfs.read_block(curr_dir_block.dir_entries[inode_index].block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_response(ERR_501, "");
		return;
	}
	
	char file_data[inode_block.size + 1];
	for(int i = 0; i < inode_block.size; i++) {
		//if starting a new block, read that block of data
		if(offset == 0) {
			bfs.read_block(inode_block.blocks[block_num++], (void*) &data_block);
		}
		file_data[index++] = data_block.data[offset++];
		offset = offset % BLOCK_SIZE;
	}
	
	file_data[index] = '\0';
	send_response(STAT_200, string(file_data));
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
	short inode_index;
	inode_t inode_block;
	datablock_t data_block;
	int index = 0;
	int offset = 0;
	int block_num = 0;
	
	if((inode_index = get_block_index(name)) == -1) {
		send_response(ERR_503, "");
		return;
	}
	
	bfs.read_block(curr_dir_block.dir_entries[inode_index].block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_response(ERR_501, "");
		return;
	}
	// determine if to print whole file or n bytes
	n = (n > inode_block.size) ? inode_block.size : n;
	char file_data[n + 1];
	
	for(int i = 0; i < n; i++) {
		//if starting a new block, read that block of data
		if(offset == 0) {
			bfs.read_block(inode_block.blocks[block_num++], (void*) &data_block);
		}
		file_data[index++] = data_block.data[offset++];
		offset = offset % BLOCK_SIZE;
	}
	file_data[index] = '\0';
	send_response(STAT_200, file_data);
}

// delete a data file
void FileSys::rm(const char *name)
{
	short inode_index;
	short inode_block_num;
	inode_t inode_block;
	int block_num;
	
	if((inode_index = get_block_index(name)) == -1) {
		send_response(ERR_503, "");
		return;
	}
	
	inode_block_num = curr_dir_block.dir_entries[inode_index].block_num;
	bfs.read_block(inode_block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_response(ERR_501, "");
		return;
	}
	
	//move file to remove to the end of the list then decrease size of list
	swap(curr_dir_block.dir_entries[inode_index], curr_dir_block.dir_entries[curr_dir_block.num_entries - 1]);
	curr_dir_block.num_entries--;
	bfs.write_block(curr_dir, (void*) &curr_dir_block);
	block_num = (inode_block.size == 0) ? 0 : ((inode_block.size - 1) / BLOCK_SIZE + 1);
	
	//reclaim all datablocks used by file
	for(int i = 0; i < block_num; i++) {
		bfs.reclaim_block(inode_block.blocks[i]);
	}
	bfs.reclaim_block(inode_block_num);
	send_response(STAT_200, "success");
}

void FileSys::stat(const char* name) {
	dirblock_t d;
	inode_t i;
	short index;
	stringstream message;
	
	if((index = get_block_index(name)) == -1) {
		send_response(ERR_503, "");
		return;
	}
	
	bfs.read_block(curr_dir_block.dir_entries[index].block_num, (void*) &d);
	if(d.magic == DIR_MAGIC_NUM) {
		message << "Directory name:     " << name << "/" << "\n";
		message << "Directory block:     " << curr_dir_block.dir_entries[index].block_num;
	} else {
		bfs.read_block(curr_dir_block.dir_entries[index].block_num, (void*) &i);
		message << "Inode block:     " << curr_dir_block.dir_entries[index].block_num << "\n";
		message << "Bytes in file:     " << i.size << "\n";
		if(i.size == 0){
			message << "Number of blocks:     " << 1 << "\n";
			message << "First block: " << 0;
		} else {
			message << "Number of blocks:     " << (i.size - 1) / BLOCK_SIZE + 2 << "\n";
			message << "First block:     " << i.blocks[0];
		}
	}
	
	send_response(STAT_200, message.str());
}

// HELPER FUNCTIONS (optional)
//returns the index of the file with given name in the current directory
//returns -1 if no such file exists
short FileSys::get_block_index(const char* name) {
	for(int i = 0; i < curr_dir_block.num_entries; i++) {
		if(strcmp(curr_dir_block.dir_entries[i].name, name) == 0)
			return i;
	}
	return -1;
}

//sends a string to the client
void FileSys::send_message(string message) {
	const char* buf = message.c_str();
	int len = message.length();
	int sent_data = 0;
	int recently_sent_data = 0;
	while(sent_data < len){
		recently_sent_data = send(fs_sock, (void*) (buf + sent_data), len - sent_data, 0);
		if(recently_sent_data == -1) {
			cout << "ran into an error while sending data" << endl;
			break;
		}
		sent_data += recently_sent_data;
	}
}

//sends the four line format response using send_message
void FileSys::send_response(string status, string message) {
	send_message(status + "\r\n");
	send_message("Length: " + to_string(message.length()) +	 "\r\n");
	send_message("\r\n");
	send_message(message);
}
