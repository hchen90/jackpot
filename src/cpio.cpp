/* ***
 * @ $cpio.cpp
 * 
 * Copyright (C) 2020 Hsiang Chen
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * ***/
#include <cstring>

#include <sys/types.h>
#include <sys/sysmacros.h>

#include "cpio.h"
#include "utils.h"

using namespace std;
using namespace utils;

void CPIOAttr::clear()
{
  memset(&c_attr, 0, sizeof(c_attr));
  memset(&c_stat, 0, sizeof(c_stat));
}

CPIOAttr& CPIOAttr::operator= (CPIOAttr& obj)
{
  c_type = obj.c_type;
  memcpy(&c_attr, &obj.c_attr, sizeof(c_attr));
  memcpy(&c_stat, &obj.c_stat, sizeof(c_stat));
  return *this;
}

CPIOContent::CPIOContent() : c_ptr(nullptr), c_len(0) {}

CPIOContent::~CPIOContent() { clear(); }

void CPIOContent::clear()
{
  if (c_ptr != nullptr) {
    delete[] c_ptr;
    c_ptr = nullptr;
  }
  if (c_len != 0) {
    c_len = 0;
  }
}

CPIOContent& CPIOContent::operator= (CPIOContent& obj)
{
  clear();
  if (obj.c_ptr != nullptr && obj.c_len > 0) {
    c_ptr = new uint8_t[c_len = obj.c_len];
    memcpy(c_ptr, obj.c_ptr, c_len);
  }
  return *this;
}

/////////////////////////////////////////////////


CPIO::CPIO(CPIOType cty)
: _cpio_type(cty) {
  _cpio_filename.clear();
}

CPIO::~CPIO() {}

bool CPIO::open(const string& filename)
{
  ifstream ins(filename, ios::in);

  if (ins.bad()) return false;

  _cpio_filename = filename;

  bool okay = true;

  while (okay) {
    char ch = 0;

    ins.get(ch);

    if (ins.bad()) {
      break;
    }

    CPIOAttr attr = { .c_type = CPIO_INVAL };

    if (ch == '0') { // odc, newc, crc
      ins.putback(ch);

      uint8_t magic[6];

      ins.read((char*) magic, sizeof(magic));

      if (ins.bad()) {
        break;
      }

      string s_magic = string((char*) magic, 6);

      if (s_magic == C_MAGIC) { // odc
        _cpio_type = attr.c_type = CPIO_ODC;
        memcpy(attr.c_attr.c_odc.c_magic, magic, sizeof(attr.c_attr.c_odc.c_magic));
        ins.read((char*) attr.c_attr.c_odc.c_magic + sizeof(attr.c_attr.c_odc.c_magic), sizeof(attr.c_attr.c_odc) - sizeof(attr.c_attr.c_odc.c_magic));
      } else if (s_magic == C_MAGICNC || s_magic == C_MAGICRC) { // newc, crc
        _cpio_type = attr.c_type = s_magic == C_MAGICNC ? CPIO_NEWC : CPIO_CRC;
        memcpy(attr.c_attr.c_crc.c_magic, magic, sizeof(attr.c_attr.c_crc.c_magic));
        ins.read((char*) attr.c_attr.c_crc.c_magic + sizeof(attr.c_attr.c_crc.c_magic), sizeof(attr.c_attr.c_crc) - sizeof(attr.c_attr.c_crc.c_magic));
      }

      if (ins.bad()) {
        okay = false;
        break;
      }
    } else { // bin
      ins.putback(ch);

      ins.read((char*) &attr.c_attr.c_bin, sizeof(attr.c_attr.c_bin));

      if (ins.bad()) {
        break;
      }

      if (attr.c_attr.c_bin.c_magic == from_oct((const uint8_t*) C_MAGIC, sizeof(C_MAGIC))) {
        _cpio_type = attr.c_type = CPIO_BIN;
      } else {
        okay = false;
        break;
      }
    }

    string filename;

    CPIOContent ctx;

    switch (attr.c_type) {
      case CPIO_BIN:
        okay = parse_bin(ins, attr, filename, ctx);
        break;
      case CPIO_ODC:
        okay = parse_odc(ins, attr, filename, ctx);
        break;
      case CPIO_NEWC:
      case CPIO_CRC:
        okay = parse_crc(ins, attr, filename, ctx);
        break;
      default:
        okay = false;
        break;
    }

    if (okay) {
      okay = input(filename, attr, ctx);
    }
  }

  ins.close();

  return true;
}

bool CPIO::update(const string& filename)
{
  ofstream outs(filename, ios::out);

  if (outs.bad()) return false;

  bool okay = true;

  string filenm;
  CPIOAttr attr;
  CPIOContent ctx;

  while (okay) {
    if ((okay = output(filenm, attr, ctx)) == true) {
      switch (_cpio_type) {
        case CPIO_BIN:
          okay = dump_bin(outs, attr, filenm, ctx);
          break;
        case CPIO_ODC:
          okay = dump_odc(outs, attr, filenm, ctx);
          break;
        case CPIO_NEWC:
        case CPIO_CRC:
          okay = dump_crc(outs, attr, filenm, ctx);
          break;
        default:
          break;
      }
    }
  }

  attr.clear();
  ctx.clear();

  attr.c_stat.st_nlink = 1;
  filenm = C_TRAILER;

  switch (_cpio_type) {
    case CPIO_BIN:
      dump_bin(outs, attr, filenm, ctx);
      break;
    case CPIO_ODC:
      dump_odc(outs, attr, filenm, ctx);
      break;
    case CPIO_NEWC:
    case CPIO_CRC:
      dump_crc(outs, attr, filenm, ctx);
      break;
    default:
      break;
  }

  outs.close();

  return true;
}

bool CPIO::update()
{
  return update(_cpio_filename);
}

/////////////////////////////////////////////////

bool CPIO::parse_bin(ifstream& ins, CPIOAttr& attr, string& filename, CPIOContent& ctx)
{
  // attr.c_stat
  memset(&attr.c_stat, 0, sizeof(attr.c_stat));
  attr.c_stat.st_dev = (dev_t) attr.c_attr.c_bin.c_dev;
  attr.c_stat.st_ino =  (ino_t) attr.c_attr.c_bin.c_ino;
  attr.c_stat.st_mode = (mode_t) attr.c_attr.c_bin.c_mode;
  attr.c_stat.st_nlink = (nlink_t) attr.c_attr.c_bin.c_nlink;
  attr.c_stat.st_uid = (uid_t) attr.c_attr.c_bin.c_uid;
  attr.c_stat.st_gid = (gid_t) attr.c_attr.c_bin.c_gid;
  attr.c_stat.st_rdev = (dev_t) attr.c_attr.c_bin.c_rdev;
  attr.c_stat.st_size = (off_t) (attr.c_attr.c_bin.c_filesize[0] << 16) | attr.c_attr.c_bin.c_filesize[1];
  attr.c_stat.st_atim.tv_sec = (time_t) (attr.c_attr.c_bin.c_mtime[0] << 16) | attr.c_attr.c_bin.c_mtime[1];
  attr.c_stat.st_mtim.tv_sec = attr.c_stat.st_atim.tv_sec;
  attr.c_stat.st_ctim.tv_sec = attr.c_stat.st_atim.tv_sec;

  // filename
  char* fl = new char[attr.c_attr.c_bin.c_namesize + 1];

  if (fl == nullptr) return false;

  ins.read(fl, attr.c_attr.c_bin.c_namesize);

  if (! ins.bad()) filename = string(fl, attr.c_attr.c_bin.c_namesize);

  delete[] fl;

  if (! filename.compare(0, sizeof(C_TRAILER) - 1, C_TRAILER)) return false;

  auto sz = padding_size(attr);

  if (sz.first > 0) {
    char buf[8];
    ins.read(buf, sz.first);
  }

  if (ins.bad()) return false;

  // ctx
  ctx.c_len = (attr.c_attr.c_bin.c_filesize[0] << 16) | attr.c_attr.c_bin.c_filesize[1];

  if (ctx.c_len > 0) ctx.c_ptr = new uint8_t[ctx.c_len];
  else ctx.c_ptr = nullptr;

  if (ctx.c_ptr != nullptr) {
    ins.read((char*) ctx.c_ptr, ctx.c_len);

    if (sz.second > 0) {
      char buf[8];
      ins.read(buf, sz.second);
    }
  }

  return !ins.bad();
}

bool CPIO::parse_odc(ifstream& ins, CPIOAttr& attr, string& filename, CPIOContent& ctx)
{
  // attr.c_stat
  memset(&attr.c_stat, 0, sizeof(attr.c_stat));
  attr.c_stat.st_dev = (dev_t) from_oct(attr.c_attr.c_odc.c_dev, sizeof(attr.c_attr.c_odc.c_dev));
  attr.c_stat.st_ino = (ino_t) from_oct(attr.c_attr.c_odc.c_ino, sizeof(attr.c_attr.c_odc.c_ino));
  attr.c_stat.st_mode = (mode_t) from_oct(attr.c_attr.c_odc.c_mode, sizeof(attr.c_attr.c_odc.c_mode));
  attr.c_stat.st_nlink = (nlink_t) from_oct(attr.c_attr.c_odc.c_nlink, sizeof(attr.c_attr.c_odc.c_nlink));
  attr.c_stat.st_uid = (uid_t) from_oct(attr.c_attr.c_odc.c_uid, sizeof(attr.c_attr.c_odc.c_uid));
  attr.c_stat.st_gid = (gid_t) from_oct(attr.c_attr.c_odc.c_gid, sizeof(attr.c_attr.c_odc.c_gid));
  attr.c_stat.st_rdev = (dev_t) from_oct(attr.c_attr.c_odc.c_rdev, sizeof(attr.c_attr.c_odc.c_rdev));
  attr.c_stat.st_size = (off_t) from_oct(attr.c_attr.c_odc.c_filesize, sizeof(attr.c_attr.c_odc.c_filesize));
  attr.c_stat.st_atim.tv_sec = (time_t) from_oct(attr.c_attr.c_odc.c_mtime, sizeof(attr.c_attr.c_odc.c_mtime));
  attr.c_stat.st_mtim.tv_sec = attr.c_stat.st_atim.tv_sec;
  attr.c_stat.st_ctim.tv_sec = attr.c_stat.st_atim.tv_sec;

  // filename
  auto nmsz = from_oct(attr.c_attr.c_odc.c_namesize, sizeof(attr.c_attr.c_odc.c_namesize));
  char* fl = new char[nmsz + 1];

  if (fl == nullptr) return false;

  ins.read(fl, nmsz);

  if (! ins.bad()) filename = string(fl, nmsz);

  delete[] fl;

  if (! filename.compare(0, sizeof(C_TRAILER) - 1, C_TRAILER)) return false;

  auto sz = padding_size(attr);

  if (sz.first > 0) {
    char buf[8];
    ins.read(buf, sz.first);
  }

  if (ins.bad()) return false;

  // ctx
  ctx.c_len = from_oct(attr.c_attr.c_odc.c_filesize, sizeof(attr.c_attr.c_odc.c_filesize));

  if (ctx.c_len > 0) ctx.c_ptr = new uint8_t[ctx.c_len];
  else ctx.c_ptr = nullptr;

  if (ctx.c_ptr != nullptr) {
    ins.read((char*) ctx.c_ptr, ctx.c_len);

    if (sz.second > 0) {
      char buf[8];
      ins.read(buf, sz.second);
    }
  }

  return !ins.bad();
}

bool CPIO::parse_crc(ifstream& ins, CPIOAttr& attr, string& filename, CPIOContent& ctx)
{
  // attr.c_stat
  memset(&attr.c_stat, 0, sizeof(attr.c_stat));
  attr.c_stat.st_dev = makedev(from_hex(attr.c_attr.c_crc.c_dev_maj, sizeof(attr.c_attr.c_crc.c_dev_maj)), from_hex(attr.c_attr.c_crc.c_dev_min, sizeof(attr.c_attr.c_crc.c_dev_min)));
  attr.c_stat.st_ino = (ino_t) from_hex(attr.c_attr.c_crc.c_ino, sizeof(attr.c_attr.c_crc.c_ino));
  attr.c_stat.st_mode = (mode_t) from_hex(attr.c_attr.c_crc.c_mode, sizeof(attr.c_attr.c_crc.c_mode));
  attr.c_stat.st_nlink = (nlink_t) from_hex(attr.c_attr.c_crc.c_nlink, sizeof(attr.c_attr.c_crc.c_nlink));
  attr.c_stat.st_uid = (uid_t) from_hex(attr.c_attr.c_crc.c_uid, sizeof(attr.c_attr.c_crc.c_uid));
  attr.c_stat.st_gid = (gid_t) from_hex(attr.c_attr.c_crc.c_gid, sizeof(attr.c_attr.c_crc.c_gid));
  attr.c_stat.st_rdev = makedev(from_hex(attr.c_attr.c_crc.c_rdev_maj, sizeof(attr.c_attr.c_crc.c_rdev_maj)), from_hex(attr.c_attr.c_crc.c_rdev_min, sizeof(attr.c_attr.c_crc.c_rdev_min)));
  attr.c_stat.st_size = (off_t) from_hex(attr.c_attr.c_crc.c_filesize, sizeof(attr.c_attr.c_crc.c_filesize));
  attr.c_stat.st_atim.tv_sec = (time_t) from_hex(attr.c_attr.c_crc.c_mtime, sizeof(attr.c_attr.c_crc.c_mtime));
  attr.c_stat.st_mtim.tv_sec = attr.c_stat.st_atim.tv_sec;
  attr.c_stat.st_ctim.tv_sec = attr.c_stat.st_atim.tv_sec;

  // filename
  auto nmsz = from_hex(attr.c_attr.c_crc.c_namesize, sizeof(attr.c_attr.c_crc.c_namesize));
  char* fl = new char[nmsz + 1];

  if (fl == nullptr) return false;

  ins.read(fl, nmsz);

  if (! ins.bad()) filename = string(fl, nmsz);

  delete[] fl;

  if (! filename.compare(0, sizeof(C_TRAILER) - 1, C_TRAILER)) return false;

  auto sz = padding_size(attr);

  if (sz.first > 0) {
    char buf[8];
    ins.read(buf, sz.first);
  }

  if (ins.bad()) return false;

  // ctx
  ctx.c_len = from_hex(attr.c_attr.c_crc.c_filesize, sizeof(attr.c_attr.c_crc.c_filesize));

  if (ctx.c_len > 0) ctx.c_ptr = new uint8_t[ctx.c_len];
  else ctx.c_ptr = nullptr;

  if (ctx.c_ptr != nullptr) {
    ins.read((char*) ctx.c_ptr, ctx.c_len);

    if (sz.second > 0) {
      char buf[8];
      ins.read(buf, sz.second);
    }
  }

  return !ins.bad();
}

bool CPIO::dump_bin(ofstream& outs, CPIOAttr& attr, string& filename, CPIOContent& ctx)
{
  // attr.c_attr.c_bin
  attr.c_attr.c_bin.c_magic = from_oct((uint8_t*) C_MAGIC, sizeof(C_MAGIC));
  attr.c_attr.c_bin.c_dev = attr.c_stat.st_dev;
  attr.c_attr.c_bin.c_ino = attr.c_stat.st_ino;
  attr.c_attr.c_bin.c_mode = attr.c_stat.st_mode;
  attr.c_attr.c_bin.c_uid = attr.c_stat.st_uid;
  attr.c_attr.c_bin.c_gid = attr.c_stat.st_gid;
  attr.c_attr.c_bin.c_nlink = attr.c_stat.st_nlink;
  attr.c_attr.c_bin.c_rdev = attr.c_stat.st_rdev;
  attr.c_attr.c_bin.c_mtime[0] = (attr.c_stat.st_mtim.tv_sec >> 16) & 0xffff;
  attr.c_attr.c_bin.c_mtime[1] = attr.c_stat.st_mtim.tv_sec & 0xffff;
  attr.c_attr.c_bin.c_namesize = filename.size() + 1;
  attr.c_attr.c_bin.c_filesize[0] = (attr.c_stat.st_size >> 16) & 0xffff;
  attr.c_attr.c_bin.c_filesize[1] = attr.c_stat.st_size & 0xffff;

  auto sz = padding_size(attr);
  
  outs.write((char*) &attr.c_attr.c_bin, sizeof(attr.c_attr.c_bin));
  outs.write((char*) filename.c_str(), filename.size());
  outs.put('\0');

  if (sz.first > 0) {
    char buf[4] = { 0 };
    outs.write(buf, sz.first);
  }

  // ctx
  if (ctx.c_ptr != nullptr && ctx.c_len > 0) {
    outs.write((char*) ctx.c_ptr, ctx.c_len);
  }

  if (sz.second > 0) {
    char buf[4] = { 0 };
    outs.write(buf, sz.second);
  }

  return !outs.bad();
}

bool CPIO::dump_odc(ofstream& outs, CPIOAttr& attr, string& filename, CPIOContent& ctx)
{
  // attr.c_attr.c_odc
  memcpy(attr.c_attr.c_odc.c_magic, C_MAGIC, sizeof(attr.c_attr.c_odc.c_magic));
  sprintf((char*) attr.c_attr.c_odc.c_dev, "%06o", (uint32_t) attr.c_stat.st_dev);
  sprintf((char*) attr.c_attr.c_odc.c_ino, "%06o", (uint32_t) attr.c_stat.st_ino);
  sprintf((char*) attr.c_attr.c_odc.c_mode, "%06o", (uint32_t) attr.c_stat.st_mode);
  sprintf((char*) attr.c_attr.c_odc.c_uid, "%06o", (uint32_t) attr.c_stat.st_uid);
  sprintf((char*) attr.c_attr.c_odc.c_gid, "%06o", (uint32_t) attr.c_stat.st_gid);
  sprintf((char*) attr.c_attr.c_odc.c_nlink, "%06o", (uint32_t) attr.c_stat.st_nlink);
  sprintf((char*) attr.c_attr.c_odc.c_rdev, "%06o", (uint32_t) attr.c_stat.st_rdev);
  sprintf((char*) attr.c_attr.c_odc.c_mtime, "%011o", (uint32_t) attr.c_stat.st_mtim.tv_sec);
  sprintf((char*) attr.c_attr.c_odc.c_namesize, "%06o", (uint32_t) filename.size() + 1);
  sprintf((char*) attr.c_attr.c_odc.c_filesize, "%011o", (uint32_t) attr.c_stat.st_size);
  
  auto sz = padding_size(attr);

  outs.write((char*) &attr.c_attr.c_odc, sizeof(attr.c_attr.c_odc));
  outs.write((char*) filename.c_str(), filename.size());
  outs.put('\0');

  if (sz.first > 0) {
    char buf[4] = { 0 };
    outs.write(buf, sz.first);
  }

  // ctx
  if (ctx.c_ptr != nullptr && ctx.c_len > 0) {
    outs.write((char*) ctx.c_ptr, ctx.c_len);
  }

  if (sz.second > 0) {
    char buf[4] = { 0 };
    outs.write(buf, sz.second);
  }

  return !outs.bad();
}

bool CPIO::dump_crc(ofstream& outs, CPIOAttr& attr, string& filename, CPIOContent& ctx)
{
  // attr.c_attr.c_crc
  memcpy(attr.c_attr.c_crc.c_magic, C_MAGICNC, sizeof(attr.c_attr.c_crc.c_magic));
  sprintf((char*) attr.c_attr.c_crc.c_ino, "%08X", (uint32_t) attr.c_stat.st_ino);
  sprintf((char*) attr.c_attr.c_crc.c_mode, "%08X", (uint32_t) attr.c_stat.st_mode);
  sprintf((char*) attr.c_attr.c_crc.c_uid, "%08X", (uint32_t) attr.c_stat.st_uid);
  sprintf((char*) attr.c_attr.c_crc.c_gid, "%08X", (uint32_t) attr.c_stat.st_gid);
  sprintf((char*) attr.c_attr.c_crc.c_nlink, "%08X", (uint32_t) attr.c_stat.st_nlink);
  sprintf((char*) attr.c_attr.c_crc.c_mtime, "%08X", (uint32_t) attr.c_stat.st_mtim.tv_sec);
  sprintf((char*) attr.c_attr.c_crc.c_filesize, "%08X", (uint32_t) attr.c_stat.st_size);
  sprintf((char*) attr.c_attr.c_crc.c_dev_maj, "%08X", (uint32_t) major(attr.c_stat.st_dev));
  sprintf((char*) attr.c_attr.c_crc.c_dev_min, "%08X", (uint32_t) minor(attr.c_stat.st_dev));
  sprintf((char*) attr.c_attr.c_crc.c_rdev_maj, "%08X", (uint32_t) major(attr.c_stat.st_rdev));
  sprintf((char*) attr.c_attr.c_crc.c_rdev_min, "%08X", (uint32_t) minor(attr.c_stat.st_rdev));
  sprintf((char*) attr.c_attr.c_crc.c_namesize, "%08X", (uint32_t) filename.size() + 1);
  sprintf((char*) attr.c_attr.c_crc.c_checksum, "%08x", 0);

  auto sz = padding_size(attr);

  outs.write((char*) &attr.c_attr.c_crc, sizeof(attr.c_attr.c_crc));
  outs.write((char*) filename.c_str(), filename.size());
  outs.put('\0');

  if (sz.first > 0) {
    char buf[4] = { 0 };
    outs.write(buf, sz.first);
  }

  // ctx
  if (ctx.c_ptr != nullptr && ctx.c_len > 0) {
    outs.write((char*) ctx.c_ptr, ctx.c_len);
  }

  if (sz.second > 0) {
    char buf[4] = { 0 };
    outs.write(buf, sz.second);
  }

  return !outs.bad();
}

/////////////////////////////////////////////////

uint32_t CPIO::from_hex(const uint8_t* hex, uint8_t len)
{
  uint32_t raw = 0;

  for (uint8_t i = 0; i < len && ((hex[i] >= '0' && hex[i] <= '9') || (hex[i] >= 'A' && hex[i] <= 'F') || (hex[i] >= 'a' && hex[i] <= 'f')); i++) {
    if (hex[i] >= '0' && hex[i] <= '9') raw = raw * 16 + (hex[i] - '0');
    else if (hex[i] >= 'a' && hex[i] <= 'f') raw = raw * 16 + (hex[i] - 'a' + 10);
    else if (hex[i] >= 'A' && hex[i] <= 'F') raw = raw * 16 + (hex[i] - 'A' + 10);
  }
  return raw;
}

uint32_t CPIO::from_oct(const uint8_t* oct, uint8_t len)
{
  uint32_t raw = 0;
  for (uint8_t i = 0; i < len && oct[i] >= '0' && oct[i] <= '7'; i++) raw = raw * 8 + (oct[i] - '0');
  return raw;
}

pair<uint32_t, uint32_t> CPIO::padding_size(CPIOAttr& attr)
{
  uint32_t hdr = 0, ctx = 0;

  if (attr.c_type == CPIO_INVAL) attr.c_type = _cpio_type;

  switch (attr.c_type) {
    case CPIO_BIN:
      hdr = (2 - (sizeof(CPIOAttr_bin) + attr.c_attr.c_bin.c_namesize) % 2) % 2;
      ctx = (2 - ((attr.c_attr.c_bin.c_filesize[0] << 16) | attr.c_attr.c_bin.c_filesize[1]) % 2) % 2;
      break;
    case CPIO_ODC:
      break;
    case CPIO_NEWC:
    case CPIO_CRC:
      hdr = (4 - (sizeof(CPIOAttr_crc) + from_hex(attr.c_attr.c_crc.c_namesize, sizeof(attr.c_attr.c_crc.c_namesize))) % 4) % 4;
      ctx = (4 - from_hex(attr.c_attr.c_crc.c_filesize, sizeof(attr.c_attr.c_crc.c_filesize)) % 4) % 4;
      break;
    default:
      break;
  }

  return make_pair(hdr, ctx);
}

/*end*/
