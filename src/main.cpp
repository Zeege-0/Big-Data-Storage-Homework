
#define FUSE_USE_VERSION 30

#include <dirent.h>
#include <sys/time.h>

#include <cstring>

#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

std::string lzjJoinPath(const std::string &a, const std::string &b) {
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

std::string lzjRealPath(const std::string &path, const FSSettings &settings) {
  auto ssdpath = lzjJoinPath(settings.ssdMountPoint, path);
  auto hddpath = lzjJoinPath(settings.hddMountPoint, path);
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

bool onSsd(const std::string &path, const FSSettings &settings) {
  auto realpath = lzjRealPath(path, settings);
  if (realpath.find(settings.ssdMountPoint) == 0) {
    return true;
  }
  return false;
}

static int myfuse_getattr(const char *path, struct stat *st,
                          struct fuse_file_info *fi) {
  printf("--> GetAttr\n");
  auto realpath = lzjRealPath(path, GlobalSettings);
  if (realpath == "[]") {
    return -ENOENT;
  }
  return stat(realpath.c_str(), st);
  return 0;
}

static int myfuse_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags) {
  DIR *dir;
  struct dirent *diread;
  std::unordered_set<std::string> filenames;

  auto realpath = lzjJoinPath(GlobalSettings.ssdMountPoint, path);
  printf("--> Getting SSD Files %s in %s\n", path, realpath.c_str());
  if ((dir = opendir(realpath.c_str())) != nullptr) {
    while ((diread = readdir(dir)) != nullptr) {
      struct stat *st = new struct stat();
      auto joined = lzjJoinPath(path, diread->d_name);
      stat(joined.c_str(), st);
      printf("name: %s, %ld\n", diread->d_name, st->st_ctim.tv_sec);
      filenames.insert(diread->d_name);
      filler(buffer, diread->d_name, NULL, 0, fuse_fill_dir_flags(0));
    }
    closedir(dir);
  } else {
    return -ENOENT;
  }

  realpath = lzjJoinPath(GlobalSettings.hddMountPoint, path);
  printf("--> Getting HDD Files %s in %s\n", path, realpath.c_str());
  if ((dir = opendir(realpath.c_str())) != nullptr) {
    while ((diread = readdir(dir)) != nullptr) {
      if (filenames.count(diread->d_name)) {
        continue;
      }
      struct stat *st = new struct stat();
      auto joined = lzjJoinPath(path, diread->d_name);
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

static int myfuse_read(const char *path, char *buffer, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
  printf("-->  Read\n");
  auto realpath = lzjRealPath(path, GlobalSettings);
  lzjReadBin(realpath, buffer, size, offset);
  return size;
}

static int myfuse_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *) {
  printf("-->  Write\n");
  auto realpath = lzjRealPath(path, GlobalSettings);
  // large file move to hdd
  if (onSsd(path, GlobalSettings) and (size + offset > GlobalSettings.ssdMaxBytes)) {
    auto hddpath = lzjJoinPath(GlobalSettings.hddMountPoint, path);
    rename(realpath.c_str(), hddpath.c_str());
    realpath = hddpath;
  }
  lzjWriteBin(realpath, buffer, size, offset);
  return size;
}

static int myfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  printf("-->  Create\n");
  auto realpath = lzjJoinPath(GlobalSettings.ssdMountPoint, path);
  std::ofstream fout(realpath);
  fout.close();
  return 0;
}

int myfuse_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
  printf("-->  Utimens\n");
  auto realpath = lzjRealPath(path, GlobalSettings);
  timeval t[2];
  t[0].tv_sec = tv[0].tv_sec;
  t[0].tv_usec = tv[0].tv_nsec;
  t[1].tv_sec = tv[1].tv_sec;
  t[1].tv_usec = tv[1].tv_nsec;
  utimes(realpath.c_str(), t);
  return 0;
}

int myfuse_mknod(const char *path, mode_t mode, dev_t dev) {
  printf("-->  Mknod %s\n", path);
  return 0;
}

int myfuse_rename(const char *oldPath, const char *newPath, unsigned int flags) {
  printf("-->  Rename %s -> %s\n", oldPath, newPath);
  auto oldrealpath = lzjRealPath(oldPath, GlobalSettings);
  std::string newRealPath;
  if (onSsd(oldPath, GlobalSettings)) {
    newRealPath = lzjJoinPath(GlobalSettings.ssdMountPoint, newPath);
  } else {
    newRealPath = lzjJoinPath(GlobalSettings.hddMountPoint, newPath);
  }
  return rename(oldrealpath.c_str(), newRealPath.c_str());
}

int myfuse_mkdir(const char *path, mode_t mode) {
  printf("-->  Mkdir %s\n", path);
  auto ssd = lzjJoinPath(GlobalSettings.ssdMountPoint, path);
  auto hdd = lzjJoinPath(GlobalSettings.hddMountPoint, path);
  int ret = mkdir(ssd.c_str(), mode);
  ret = mkdir(hdd.c_str(), mode);
  return ret;
}

int myfuse_rmdir(const char *path) {
  printf("-->  Rmdir %s\n", path);
  auto ssd = lzjJoinPath(GlobalSettings.ssdMountPoint, path);
  auto hdd = lzjJoinPath(GlobalSettings.hddMountPoint, path);
  int ret = rmdir(ssd.c_str());
  ret = rmdir(hdd.c_str());
  return ret;
}

int myfuse_unlink(const char *path) {
  printf("-->  Unlink %s\n", path);
  auto realpath = lzjRealPath(path, GlobalSettings);
  remove(realpath.c_str());
  return 0;
}

int myfuse_setattr(const char *path, const char *, const char *, size_t, int) {
  printf("-->  Setxattr %s\n", path);
  int res = 0;
  return res;
}

static struct fuse_operations operations = {
    .getattr = myfuse_getattr,
    .mkdir = myfuse_mkdir,
    .unlink = myfuse_unlink,
    .rmdir = myfuse_rmdir,
    .rename = myfuse_rename,
    .read = myfuse_read,
    .write = myfuse_write,
    .setxattr = myfuse_setattr,
    .readdir = myfuse_readdir,
    .create = myfuse_create,
    .utimens = myfuse_utimens,
};

int main(int argc, char *argv[]) {
  GlobalSettings.hddMountPoint = "/home/ubuntu/work/dashuju/data/hdd";
  GlobalSettings.ssdMountPoint = "/home/ubuntu/work/dashuju/data/ssd";
  GlobalSettings.ssdMaxBytes = 4096;
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  return fuse_main(argc, argv, &operations, NULL);
}
