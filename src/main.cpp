
#define FUSE_USE_VERSION 30

#include <cstring>

#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include <argparse/argparse.hpp>
#include <indicators/indicators.hpp>

#include "fuse3/fuse.h"

#define LZJ_CHECK_OPEN(f, filename)                                            \
  if (!f) {                                                                    \
    std::cerr << "Error open " << filename << "\n";                            \
  }

/**
 * My Utilities
 */

std::vector<std::string> split(const std::string &str) {
  std::regex ws_re("\\s+"); // whitespace
  return std::vector<std::string>(
      std::sregex_token_iterator(str.begin(), str.end(), ws_re, -1),
      std::sregex_token_iterator());
}

std::string joinpath(const std::string &a, const std::string &b) {
  return a + "/" + b;
}

void lzjReadBin(const std::string &filename, char *buffer, size_t size,
             size_t offset = 0) {
  std::ifstream fin(filename, std::ios::binary);
  LZJ_CHECK_OPEN(fin, filename);
  fin.seekg(offset, std::ios::beg);
  fin.read(buffer, size);
  fin.close();
}

void lzjWriteBin(const std::string &filename, char *buffer, size_t size,
              size_t offset = 0) {
  std::ofstream fout(filename, std::ios::binary);
  LZJ_CHECK_OPEN(fout, filename);
  fout.write(buffer + offset, size);
  fout.close();
}

/**
 * Filesystem settings
 */
struct FSSettings {
  size_t ssdMaxBytes; // max file size for ssd in bytes
  std::string ssdMountPoint;
  std::string hddMountPoint;
};


std::string getRealPath(const std::string &path, const FSSettings &settings){
  auto ssdpath = joinpath(settings.ssdMountPoint, path);
  std::string realpath;
  if (0 == system(("test -e " + ssdpath + ".ssd").c_str())) {
    return joinpath(settings.ssdMountPoint, path);
  } else if (0 == system(("test -e " + ssdpath + ".hdd").c_str())) {
    return joinpath(settings.hddMountPoint, path);
  } else {
    std::cerr << "No such file\n";
    return "";
  }
}

int lzjRead(const std::string &path, const FSSettings &settings, char *buffer, size_t size, size_t offset) {
  auto realpath = getRealPath(path, settings);
  lzjReadBin(realpath, buffer, size, offset);
}

int lzjWrite(const std::string &path, const FSSettings &settings, char *buffer, size_t size, size_t offset) {
  auto realpath = getRealPath(path, settings);
  lzjWriteBin(realpath, buffer, size, offset);
}

void lzjRemove(const std::string &path, const FSSettings &settings) { 
  auto realpath = getRealPath(path, settings);
  std::remove(realpath.c_str());
  std::remove(joinpath(settings.ssdMountPoint, path).c_str());
}


static int do_getattr(const char *path, struct stat *st,
                      struct fuse_file_info *fi) {
  st->st_uid = getuid();
  st->st_gid = getgid();
  st->st_atime = time(nullptr);
  st->st_mtime = time(nullptr);
  if (strcmp(path, "/") == 0) {
    st->st_mode = S_IFDIR | 0755;
    st->st_nlink = 2;
  } else {
    st->st_mode = S_IFREG | 0644;
    st->st_nlink = 1;
    st->st_size = 1024;
  }
  return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags) {
  printf("--> Getting The List of Files of %s\n", path);

  filler(buffer, ".", NULL, 0, fuse_fill_dir_flags());  // Current Directory
  filler(buffer, "..", NULL, 0, fuse_fill_dir_flags()); // Parent Directory

  if (strcmp(path, "/") ==
      0) // If the user is trying to show the files/directories of the root
         // directory show the following
  {
    filler(buffer, "file54", NULL, 0, fuse_fill_dir_flags());
    filler(buffer, "file349", NULL, 0, fuse_fill_dir_flags());
  }

  return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  char file54Text[] = "Hello World From File54!";
  char file349Text[] = "Hello World From File349!";
  char *selectedText = NULL;
  std::cout << "lllll: " << path << "\n";
  if (strcmp(path, "/file54") == 0)
    selectedText = file54Text;
  else if (strcmp(path, "/file349") == 0)
    selectedText = file349Text;
  else
    return -1;
  memcpy(buffer, selectedText + offset, size);

  return strlen(selectedText) - offset;
}

static struct fuse_operations operations = {
    .getattr = do_getattr,
    .read = do_read,
    .readdir = do_readdir,
};

int main(int argc, char *argv[]) {
  int ret;
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  ret = fuse_main(argc, argv, &operations, NULL);
  return ret;
}
