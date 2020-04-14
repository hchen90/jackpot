/* $ @cpio.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CPIO_H_
#define	_CPIO_H_

#include <string>
#include <fstream>

#include <sys/stat.h>

#include "conf.h"

#define C_TRAILER "TRAILER!!!"

#define C_MAGIC   "070707"
#define C_MAGICNC "070701"
#define C_MAGICRC "070702"

#define N_MAGIC   070707
#define N_MAGICNC 070701
#define N_MAGICRC 070702

#define C_IRUSR		000400
#define C_IWUSR		000200
#define C_IXUSR		000100
#define C_IRGRP		000040
#define C_IWGRP		000020
#define C_IXGRP		000010
#define C_IROTH		000004
#define C_IWOTH		000002
#define C_IXOTH		000001

#define C_ISUID		004000
#define C_ISGID		002000
#define C_ISVTX		001000

#define C_ISBLK		060000
#define C_ISCHR		020000
#define C_ISDIR		040000
#define C_ISFIFO	010000
#define C_ISSOCK	0140000
#define C_ISLNK		0120000
#define C_ISCTG		0110000
#define C_ISREG		0100000

enum CPIOType { CPIO_INVAL, CPIO_BIN, CPIO_ODC, CPIO_NEWC, CPIO_CRC };

struct CPIOAttr_bin {
  uint16_t c_magic; // C_MAGIC
  uint16_t c_dev;
  uint16_t c_ino;
  uint16_t c_mode;
  uint16_t c_uid;
  uint16_t c_gid;
  uint16_t c_nlink;
  uint16_t c_rdev;
  uint16_t c_mtime[2];
  uint16_t c_namesize;
  uint16_t c_filesize[2];
};

struct CPIOAttr_odc {
  uint8_t c_magic[6]; // C_MAGIC
  uint8_t c_dev[6];
  uint8_t c_ino[6];
  uint8_t c_mode[6];
  uint8_t c_uid[6];
  uint8_t c_gid[6];
  uint8_t c_nlink[6];
  uint8_t c_rdev[6];
  uint8_t c_mtime[11];
  uint8_t c_namesize[6];
  uint8_t c_filesize[11];
};

struct CPIOAttr_crc {
  uint8_t c_magic[6]; // C_MAGICNP || C_MAGICRC
  uint8_t c_ino[8];
  uint8_t c_mode[8];
  uint8_t c_uid[8];
  uint8_t c_gid[8];
  uint8_t c_nlink[8];
  uint8_t c_mtime[8];
  uint8_t c_filesize[8];
  uint8_t c_dev_maj[8];
  uint8_t c_dev_min[8];
  uint8_t c_rdev_maj[8];
  uint8_t c_rdev_min[8];
  uint8_t c_namesize[8];
  uint8_t c_checksum[8];
};

struct CPIOAttr {
  CPIOType c_type;
  union {
    struct CPIOAttr_bin c_bin;
    struct CPIOAttr_odc c_odc;
    struct CPIOAttr_crc c_crc;
  } c_attr;
  struct stat c_stat;
  void clear();
  CPIOAttr& operator= (CPIOAttr&);
};

struct CPIOContent {
  CPIOContent();
  ~CPIOContent();
  uint8_t* c_ptr;
  uint32_t c_len;
  void clear();
  CPIOContent& operator= (CPIOContent&);
};

class CPIO {
public:
  CPIO(CPIOType cty = CPIO_INVAL);
  ~CPIO();

  bool open(const std::string& filename);
  bool update(const std::string& filename);
  bool update();
protected:
  virtual bool input(std::string& filename, CPIOAttr& attr, CPIOContent& ctx) = 0;
  virtual bool output(std::string& filename, CPIOAttr& attr, CPIOContent& ctx) = 0;
private:
  bool parse_bin(std::ifstream& ins, CPIOAttr& attr, std::string& filename, CPIOContent& ctx);
  bool parse_odc(std::ifstream& ins, CPIOAttr& attr, std::string& filename, CPIOContent& ctx);
  bool parse_crc(std::ifstream& ins, CPIOAttr& attr, std::string& filename, CPIOContent& ctx);
  bool dump_bin(std::ofstream& outs, CPIOAttr& attr, std::string& filename, CPIOContent& ctx);
  bool dump_odc(std::ofstream& outs, CPIOAttr& attr, std::string& filename, CPIOContent& ctx);
  bool dump_crc(std::ofstream& outs, CPIOAttr& attr, std::string& filename, CPIOContent& ctx);

  uint32_t from_hex(const uint8_t* hex, uint8_t len);
  uint32_t from_oct(const uint8_t* oct, uint8_t len);
  void to_hex(uint32_t val, int digits, uint8_t* buf);
  void to_oct(uint32_t val, int digits, uint8_t* buf);
  std::pair<uint32_t, uint32_t> padding_size(CPIOAttr& attr);

  std::string _cpio_filename;
  CPIOType _cpio_type;
};

#endif	/* _CTXWRAPPER_H_ */
