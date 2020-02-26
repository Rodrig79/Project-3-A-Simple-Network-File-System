// CPSC 3500: File System
// Implements the file system commands that are available to the shell.

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <algorithm>
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

// TODO: directory is full error
// make a directory
void FileSys::mkdir(const char *name)
{
	short newBlock;
	dirblock_t newDir;
	
	if(get_block_index(name) >= 0){
		send_message(ERR_502);
		return;
	} if(std::strlen(name) > MAX_FNAME_SIZE){
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
	bfs.write_block(newBlock, &newDir);
	
	for(int i = 0; name[i] != '\0';i++)
		curr_dir_block.dir_entries[curr_dir_block.num_entries].name[i] = name[i];
	curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = newBlock;
	curr_dir_block.num_entries++;
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
	if(new_dir_block.magic != DIR_MAGIC_NUM){
		send_message(ERR_500);
		return;
	}
	curr_dir = curr_dir_block.dir_entries[i].block_num;
	curr_dir_block = new_dir_block;
}

// switch to home directory
void FileSys::home() {
	curr_dir = 1;
	bfs.read_block(curr_dir, (void*) &curr_dir_block);
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
	std::move(curr_dir_block.dir_entries + i + 1, curr_dir_block.dir_entries + curr_dir_block.num_entries,curr_dir_block.dir_entries +  i);
	curr_dir_block.num_entries--;
	bfs.reclaim_block(rem);
}

// list the contents of current directory
//TODO: remove extra space at end
void FileSys::ls()
{
	std::string file_list = "";
	for(int i = 0; i < curr_dir_block.num_entries; i++){
		file_list.append(curr_dir_block.dir_entries[i].name);
		file_list += " ";
	}
	file_list += "\n";
	send_message(file_list);
}
// TODO: error message if full
// create an empty data file
void FileSys::create(const char *name)
{
	inode_t new_file;
	short new_block;
	
	if(get_block_index(name) >= 0){
		send_message(ERR_502);
		return;
	}
	if(std::strlen(name) > MAX_FNAME_SIZE){
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
	bfs.write_block(new_block, &new_file);
	
	std::copy(name, name + std::strlen(name), curr_dir_block.dir_entries[curr_dir_block.num_entries].name);
	curr_dir_block.dir_entries[curr_dir_block.num_entries].block_num = new_block;
	curr_dir_block.num_entries++;
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
}

// display the contents of a data file
void FileSys::cat(const char *name)
{
	short inode_index;
	inode_t inode_block;
	datablock_t data_block;
	char* file_data;
	if((inode_index = get_block_index(name)) == -1) {
		send_message(ERR_503);
		return;
	}
	bfs.read_block(curr_dir_block.dir_entries[inode_index].block_num, &inode_block);
	if(inode_block.magic != INODE_MAGIC_NUM) {
		send_message(ERR_501);
		return;
	}
	
	while()
}

// display the first N bytes of the file
void FileSys::head(const char *name, unsigned int n)
{
}

// delete a data file
void FileSys::rm(const char *name)
{
}

// display stats about file or directory
void FileSys::stat(const char *name)
{
}

// HELPER FUNCTIONS (optional)
short FileSys::get_block_index(const char* name) {
	for(int i = 0; i < curr_dir_block.num_entries; i++) {
		if(std::strcmp(curr_dir_block.dir_entries[i].name, name))
			return i;
	}
	return -1;
}

void FileSys::send_message(std::string message) {
	
}
