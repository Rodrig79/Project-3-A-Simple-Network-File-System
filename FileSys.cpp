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
	cout << "file system using socket of: " << sock << endl;
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

// TODO: directory is full error
// make a directory
void FileSys::mkdir(const char *name)
{
	cout << "running mkdir " << name << endl;
	short newBlock;
	dirblock_t newDir;
	
	if(get_block_index(name) >= 0){
		send_message(ERR_502);
		return;
	} if(strlen(name) > MAX_FNAME_SIZE){
		send_message(ERR_504);
		return;
	} if(curr_dir_block.num_entries == MAX_DIR_ENTRIES){
		send_message(ERR_506);
		return;
	} if(!(newBlock = bfs.get_free_block())){
		send_message(ERR_505);
		return;
	}
	
	newDir.num_entries = 0;
	newDir.magic = DIR_MAGIC_NUM;
	bfs.write_block(newBlock, &newDir);
	
	int i = 0;
	for(i = 0; name[i] != '\0'; i++)
		curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	
	curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = newBlock;
	curr_dir_block.num_entries++;
	bfs.write_block(curr_dir, &curr_dir_block);
	send_message("OKAY");
}

// TODO: entry not found response
// TODO cstring equality?
// switch to a directory
void FileSys::cd(const char *name)
{
	dirblock_t new_dir_block;
	short i;
	if((i = get_block_index(name)) == -1){
		send_message(ERR_503);
		return;
	}
	bfs.read_block(curr_dir_block.dir_entries[i].block_num, (void*) &new_dir_block);
	cout << "moving to: " << curr_dir_block.dir_entries[i].name << endl;
	cout << new_dir_block.magic << " " << DIR_MAGIC_NUM << endl;
	if(new_dir_block.magic != DIR_MAGIC_NUM){
		send_message(ERR_500);
		return;
	}
	curr_dir = curr_dir_block.dir_entries[i].block_num;
	curr_dir_block = new_dir_block;
	send_message("OKAY");
}

// switch to home directory
void FileSys::home() {
	curr_dir = 1;
	bfs.read_block(curr_dir, (void*) &curr_dir_block);
	send_message("OKAY");
}

//TODO: make sure directory is empty
// remove a directory
void FileSys::rmdir(const char *name)
{
	short i;
	dirblock_t rem_block;
	if((i = get_block_index(name)) == -1){
		send_message(ERR_503);
		return;
	}
	short rem = curr_dir_block.dir_entries[i].block_num;
	bfs.read_block(rem, (void*) &rem_block);
	if(rem_block.magic != DIR_MAGIC_NUM){
		send_message(ERR_500);
		return;
	}
	if(rem_block.num_entries > 0){
		send_message(ERR_507);
		return;
	}
	swap(curr_dir_block.dir_entries[rem], curr_dir_block.dir_entries[curr_dir_block.num_entries - 1]);
	curr_dir_block.num_entries--;
	bfs.write_block(curr_dir, (void*) &curr_dir_block);
	bfs.reclaim_block(rem);
	send_message("OKAY");
}

// list the contents of current directory
//TODO: remove extra space at end
void FileSys::ls()
{
	cout << "running ls" << endl;
	string file_list = "*";
	for(int i = 0; i < curr_dir_block.num_entries; i++){
		file_list.append(curr_dir_block.dir_entries[i].name);
		file_list += "*";
	}
	cout << "result of ls is: " << file_list << endl;
	send_message(file_list);
}
// TODO: error message if full
// create an empty data file
void FileSys::create(const char *name)
{
	inode_t new_file;
	short new_block;
	cout << get_block_index(name) << endl;
	if(get_block_index(name) >= 0){
		send_message(ERR_502);
		return;
	}
	if(strlen(name) > MAX_FNAME_SIZE){
		send_message(ERR_504);
		return;
	}
	if(curr_dir_block.num_entries == MAX_DIR_ENTRIES){
		send_message(ERR_506);
		return;
	}
	if(!(new_block = bfs.get_free_block())) {
		send_message(ERR_505);
		return;
	}
	new_file.size = 0;
	new_file.magic = INODE_MAGIC_NUM;
	bfs.write_block(new_block, &new_file);
	
	int i = 0;
	for(i = 0; name[i] != '\0'; i++)
		curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	
	curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = new_block;
	curr_dir_block.num_entries++;
	bfs.write_block(curr_dir, (void*) &curr_dir_block);
	send_message("OKAY");
}

// TODO: file doesn't exist, file too big
// append data to a data file
void FileSys::append(const char *name, const char *data)
{
	short inode;
	inode_t inode_block;
	int data_index;
	int data_offset;
	datablock_t curr_block;
	if((inode = get_block_index(name)) == -1) {
		send_message(ERR_503);
		return;
	}
	bfs.read_block(curr_dir_block.dir_entries[inode].block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM){
		send_message(ERR_501);
		return;
	}
	data_index = inode_block.size / BLOCK_SIZE;
	data_offset = inode_block.size % BLOCK_SIZE;
	int i = 0;
	bfs.read_block(inode_block.blocks[data_index], (void*) &curr_block);
	while(data[i] != '\0'){
		if(data_offset == BLOCK_SIZE) {
			data_offset = 0;
			bfs.write_block(inode_block.blocks[data_index++], (void*) &curr_block);
			//ask which one of these errors to return first
			if(data_index == MAX_DATA_BLOCKS) {
				inode_block.size = data_index * BLOCK_SIZE;
				bfs.write_block(inode, (void*) &inode_block);
				send_message(ERR_508);
				return;
			}
			if((inode_block.blocks[data_index] = bfs.get_free_block()) == 0) {
				inode_block.size = data_index * BLOCK_SIZE;
				bfs.write_block(inode, (void*) &inode_block);
				send_message(ERR_505);
				return;
			}
			curr_block = datablock_t();
		}
		curr_block.data[data_offset++] = data[i++];
	}
	inode_block.size = data_index * BLOCK_SIZE + data_offset;
	bfs.write_block(inode, (void*) &inode_block);
	send_message("OKAY");
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
		send_message(ERR_503);
		return;
	}
	bfs.read_block(curr_dir_block.dir_entries[inode_index].block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_message(ERR_501);
		return;
	}
	char file_data[inode_block.size + 1];
	for(int i = 0; i < inode_block.size; i++) {
		if(offset == 0) {
			bfs.read_block(inode_block.blocks[block_num++], (void*) &data_block);
		}
		file_data[index++] = data_block.data[offset++];
		offset = offset % BLOCK_SIZE;
	}
	file_data[index] = '\0';
	send_message(string(file_data));
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
		send_message(ERR_503);
		return;
	}
	bfs.read_block(curr_dir_block.dir_entries[inode_index].block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_message(ERR_501);
		return;
	}
	n = (n > inode_block.size) ? inode_block.size : n;
	char file_data[n + 1];
	for(int i = 0; i < n; i++) {
		if(offset == 0) {
			bfs.read_block(inode_block.blocks[block_num++], (void*) &data_block);
		}
		file_data[index++] = data_block.data[offset++];
		offset = offset % BLOCK_SIZE;
	}
	file_data[index] = '\0';
	send_message(file_data);
}

// delete a data file
void FileSys::rm(const char *name)
{
	short inode_index;
	short inode_block_num;
	inode_t inode_block;
	int block_num;
	
	if((inode_index = get_block_index(name)) == -1) {
		send_message(ERR_503);
		return;
	}
	inode_block_num = curr_dir_block.dir_entries[inode_index].block_num;
	bfs.read_block(inode_block_num, (void*) &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_message(ERR_501);
		return;
	}
	
	swap(curr_dir_block.dir_entries[inode_index], curr_dir_block.dir_entries[curr_dir_block.num_entries - 1]);
	curr_dir_block.num_entries--;
	bfs.write_block(curr_dir, (void*) &curr_dir_block);
	block_num = (inode_block.size == 0) ? 0 : inode_block.size - 1 / BLOCK_SIZE + 1;
	
	for(int i = 0; i < block_num; i++) {
		bfs.reclaim_block(inode_block.blocks[i]);
	}
	bfs.reclaim_block(inode_block_num);
	send_message("OKAY");
}

void FileSys::stat(const char* name) {
	dirblock_t d;
	inode_t i;
	short index;
	stringstream message;
	if((index = get_block_index(name)) == -1) {
		send_message(ERR_503);
		return;
	}
	bfs.read_block(curr_dir_block.dir_entries[index].block_num, (void*) &d);
	if(d.magic == DIR_MAGIC_NUM) {
		message << "File Name:     " << name << "\n";
		message << "Block Number:     " << curr_dir_block.dir_entries[index].block_num;
	} else {
		bfs.read_block(curr_dir_block.dir_entries[index].block_num, (void*) &i);
		message << "Inode Block:     " << curr_dir_block.dir_entries[index].block_num << "\n";
		message << "Bytes in File:     " << i.size << "\n";
		message << "Number of Blocks:     " << i.size / BLOCK_SIZE << "\n";
		message << "First Number of Blocks:     " << i.blocks[0];
	}
	send_message(message.str());
}

// HELPER FUNCTIONS (optional)
short FileSys::get_block_index(const char* name) {
	for(int i = 0; i < curr_dir_block.num_entries; i++) {
		cout << curr_dir_block.dir_entries[i].name << " " << name << endl;
		cout << strcmp(curr_dir_block.dir_entries[i].name, name) << endl;
		if(strcmp(curr_dir_block.dir_entries[i].name, name) == 0){
			cout << "block index is: " << i << endl;
			return i;
		}
	}
	cout << "block index is: " << -1 << endl;
	return -1;
}

//debug recieving of blank messages, it broke again
void FileSys::send_message(string message) {
	cout << "SENDING MESSAGE: $" << message << "$" << endl;
	const char* buf = (message + "\r\n").c_str();
	int len = message.length();
	int sent_data = 0;
	int recently_sent_data = 0;
	while(sent_data < len){
		recently_sent_data = send(fs_sock, (void*) (buf + sent_data), len - sent_data, 0);
		if(recently_sent_data == -1) {
			cout << "error sending data" << endl;
			break;
		}
		sent_data += recently_sent_data;
	}
}
