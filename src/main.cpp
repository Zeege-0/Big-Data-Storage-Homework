
#define FUSE_USE_VERSION 30

#include <dirent.h>
#include <sys/time.h>

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

#define LZJ_CHECK_OPEN(f, filename)                 \
  if (!f) {                                         \
    std::cerr << "Error open " << filename << "\n"; \
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

void lzjWriteBin(const std::string &filename, const char *buffer, size_t size, size_t offset = 0) {
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

FSSettings GlobalSettings;

std::string getRealPath(const std::string &path, const FSSettings &settings) {
  auto ssdpath = joinpath(settings.ssdMountPoint, path);
  auto hddpath = joinpath(settings.hddMountPoint, path);
  struct stat st;
  if (stat(ssdpath.c_str(), &st) == 0) {
    printf("ssd ;%s;\n", ssdpath.c_str());
    return ssdpath;
  } else if (stat(hddpath.c_str(), &st) == 0) {
    printf("hdd ;%s;\n", hddpath.c_str());
    return hddpath;
  } else {
    printf("getRealPath: no such file %s\n", path.c_str());
    return "[]";
  }
}

void lzjRead(const std::string &path, const FSSettings &settings, char *buffer, size_t size, size_t offset) {
  auto realpath = getRealPath(path, settings);
  lzjReadBin(realpath, buffer, size, offset);
}

void lzjWrite(const std::string &path, const FSSettings &settings, char *buffer, size_t size, size_t offset) {
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
  printf("--> GetAttr\n");
  auto realpath = getRealPath(path, GlobalSettings);
  if(realpath == "[]"){
    return -ENOENT;
  }
  return stat(realpath.c_str(), st);
  // st->st_mode = S_IFREG | 0644;
  // st->st_nlink = 1;
  // st->st_size = 1024;

  return 0;
}

static int do_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags) {
  DIR *dir;
  struct dirent *diread;

  auto realpath = joinpath(GlobalSettings.ssdMountPoint, path);
  printf("--> Getting SSD Files %s in %s\n", path, realpath.c_str());
  if ((dir = opendir(realpath.c_str())) != nullptr) {
    while ((diread = readdir(dir)) != nullptr) {
      struct stat *st = new struct stat();
      auto joined = joinpath(path, diread->d_name);
      stat(joined.c_str(), st);
      printf("name: %s, %ld\n", diread->d_name, st->st_ctim.tv_sec);
      filler(buffer, diread->d_name, NULL, 0, fuse_fill_dir_flags(0));
    }
    closedir(dir);
  } else {
    return -ENOENT;
  }

  realpath = joinpath(GlobalSettings.hddMountPoint, path);
  printf("--> Getting HDD Files %s in %s\n", path, realpath.c_str());
  if ((dir = opendir(realpath.c_str())) != nullptr) {
    while ((diread = readdir(dir)) != nullptr) {
      if ((diread->d_name == std::string(".")) or (diread->d_name == std::string(".."))) {
        continue;
      }
      struct stat *st = new struct stat();
      auto joined = joinpath(path, diread->d_name);
      stat(joined.c_str(), st);
      printf("name: %s, %ld\n", diread->d_name, st->st_ctim.tv_sec);
      filler(buffer, diread->d_name, NULL, 0, fuse_fill_dir_flags(0));
    }
    closedir(dir);
  } else {
    return -ENOENT;
  }
  printf("--> Finish Get File\n");
  return 0;
}

static int do_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  printf("-->  Read\n");
  auto realpath = getRealPath(path, GlobalSettings);
  lzjReadBin(realpath, buffer, size, offset);
  return size;
}

static int do_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *) {
  printf("-->  Write\n");
  auto realpath = getRealPath(path, GlobalSettings);
  lzjWriteBin(realpath, buffer, size, offset);
  return size;
}

static int do_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  printf("-->  Create\n");
  auto realpath = joinpath(GlobalSettings.ssdMountPoint, path);
  std::ofstream fout(realpath);
  fout.close();
  return 0;
}

int do_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
  printf("-->  Utimens\n");
  auto realpath = getRealPath(path, GlobalSettings);
  timeval t[2];
  t[0].tv_sec = tv[0].tv_sec;
  t[0].tv_usec = tv[0].tv_nsec;
  t[1].tv_sec = tv[1].tv_sec;
  t[1].tv_usec = tv[1].tv_nsec;
  utimes(realpath.c_str(), t);
  return 0;
}

int do_mknod(const char* path, mode_t mode, dev_t dev) {
  printf("--> Mknod %s\n", path);
  return 0;
}


static struct fuse_operations operations = {
    .getattr = do_getattr,
    .read = do_read,
    .write = do_write,
    .readdir = do_readdir,
    .create = do_create,
    .utimens = do_utimens,
};

int main(int argc, char *argv[]) {
  GlobalSettings.hddMountPoint = "/home/ubuntu/work/dashuju/data/hdd";
  GlobalSettings.ssdMountPoint = "/home/ubuntu/work/dashuju/data/ssd";
  GlobalSettings.ssdMaxBytes = 4096;
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  return fuse_main(argc, argv, &operations, NULL);
}
