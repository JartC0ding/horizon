#include <fat32_vfs.h>

#include <fatfs/ff.h>

#include <memory/page_frame_allocator.h>
#include <memory/heap.h>

#include <utils/log.h>
#include <utils/assert.h>


using namespace fs;
using namespace fs::vfs;

fat32_mount::fat32_mount(int disk_id, char* name) {
	this->fatfs = (FATFS*) memory::global_allocator.request_page();
	this->fatfs->pdrv = disk_id;

	f_mount(this->fatfs, name, 1);
}

fat32_mount::~fat32_mount() {
	memory::global_allocator.free_page((void*) this->fatfs);
}

file_t* fat32_mount::open(char* path) {
	debugf("Opening file %s\n", path);

	file_t* file = new file_t;
	memset(file, 0, sizeof(file_t));

	FIL fil;

	FRESULT fr = f_open(&fil, path, FA_READ | FA_WRITE);
	if (fr != FR_OK) {
		delete file;
		return nullptr;
	}

	file->mount = this;
	file->size = f_size(&fil);
	file->data = (void*) memory::malloc(sizeof(FIL));
	memcpy(file->data, &fil, sizeof(FIL));

	strcpy(file->buffer, path);

	return file;
}

void fat32_mount::close(file_t* file) {
	debugf("Closing file\n");

	f_close((FIL*) file->data);
	memory::free(file->data);
	delete file;
}

void fat32_mount::read(file_t* file, void* buffer, size_t size, size_t offset) {
	debugf("Reading %d bytes from %d\n", size, offset);

	f_lseek((FIL*) file->data, offset);

	UINT has_read;
	f_read((FIL*) file->data, buffer, size, &has_read);

	assert(has_read == size);
}

void fat32_mount::write(file_t* file, void* buffer, size_t size, size_t offset) {
	debugf("Writing %d bytes to %d\n", size, offset);

	f_lseek((FIL*) file->data, offset);

	unsigned int has_written;
	f_write((FIL*) file->data, buffer, size, &has_written);

	assert(has_written == size);
}

void fat32_mount::delete_(file_t* file) {
	f_unlink((char*) file->buffer);
	delete file;
}

void fat32_mount::mkdir(char* path) {
	f_mkdir(path);
}

dir_t fat32_mount::dir_at(int idx, char* path) {
	DIR dir_;
	FILINFO file_info;
	f_opendir(&dir_, path);
	
	FRESULT fr = f_readdir(&dir_, &file_info);
	assert(fr == FR_OK);

	int orig_idx = idx;

	while (idx--) {
		FRESULT res = f_readdir(&dir_, &file_info);
		if (res != FR_OK || file_info.fname[0] == 0) {
			return {
				.is_none = true
			};
		}
	}

	dir_t dir;
	memset(&dir, 0, sizeof(dir_t));

	dir.idx = orig_idx;
	dir.is_none = false;

	strcpy(dir.name, file_info.fname);

	f_closedir(&dir_);

	return dir;
}