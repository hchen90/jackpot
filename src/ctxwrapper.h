/* $ @ctxwrapper.h
 * Copyright (C) 2020 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_CTXWRAPPER_H_
#define	_CTXWRAPPER_H_

#include <string>
#include <memory>
#include <vector>
#include <cstring>

#include "config.h"
#include "conf.h"
#include "cpio.h"

struct CtxFile {
  CtxFile();
  ~CtxFile();
  CPIOAttr _attr;
  CPIOContent _ctx;
  bool _visible;
  void clear();
};

class CtxWrapper : public CPIO {
public:
  CtxWrapper();
  ~CtxWrapper();

  bool opencpio(const std::string& filename);
  bool updatecpio(const std::string& filename);
  bool updatecpio();
  void closecpio();

  void gethead(uint16_t errcode, std::string& errstr);
  void getbody(uint16_t errcode, std::string& errstr);
  void request(const std::string& cmd, const std::string& pathname, const std::string& version, CPIOContent& result);
  bool getcontent(const std::string& filename, CPIOContent& ctx);
  bool getfile(const std::string& filename, CPIOAttr& attr, CPIOContent& ctx);
  bool setfile(const std::string& filename, CPIOAttr& attr, CPIOContent& ctx);
  bool delfile(const std::string& filename);

  time_t& timeout();
protected:
  bool input(std::string& filename, CPIOAttr& attr, CPIOContent& ctx);
  bool output(std::string& filename, CPIOAttr& attr, CPIOContent& ctx);
private:
  void updateinfo();

  inline bool cons200resp(const std::string& mt, const std::string& defhdr, CPIOContent& ctx, CPIOContent& content) {
    std::string scstr = defhdr;
    gethead(200, scstr);
    char cnl[BUFSIZE];
    if (_timeout > 0) {
      snprintf(cnl, sizeof(cnl), "\r\nConnection: keep-alive\r\nKeep-Alive: timeout=%lu\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", _timeout, mt.c_str(), ctx.c_len);
    } else {
      snprintf(cnl, sizeof(cnl), "\r\nContent-Type: %s\r\nContent-Length: %u\r\n\r\n", mt.c_str(), ctx.c_len);
    }
    scstr += cnl;
    size_t ll = scstr.size();
    content.clear();
    content.c_len = ll + ctx.c_len;
    content.c_ptr = new uint8_t[content.c_len];
    if (content.c_ptr != nullptr) {
      memcpy(content.c_ptr, scstr.data(), ll);
      memcpy(content.c_ptr + ll, ctx.c_ptr, ctx.c_len);
      return true;
    } 
    return false;
  }

  std::string mimetype(const std::string& filename);
  std::string filepath(const std::string& filename);

  Conf _config;

  std::map<std::string, std::shared_ptr<CtxFile>> _rootfs;
  std::vector<std::string> _indexfl;
  std::vector<std::string> _hidefl;
  std::map<uint16_t, std::string> _schead;
  std::map<uint16_t, std::string> _scbody;
  std::map<std::string, std::string> _mimetype;

  time_t _timeout;
};

#endif	/* _CTXWRAPPER_H_ */
