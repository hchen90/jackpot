/* $ @buffer.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * This file is autogenated by perl script. DO NOT edit it.
 * */
#ifndef	_BUFFER_H_
#define	_BUFFER_H_

class Buffer {
public:
  Buffer();
  Buffer(const Buffer& buf);
  ~Buffer();

  bool    allocate(size_t len); // allocate memory for buffer
  bool    append(const void* mem, size_t len); // append context to buffer
  void    reset(void); // reset buffer
  void    resize(size_t len); // change size of used area in buffer
  void*   data(void) const;
  size_t  size(void) const;
  size_t  capacity(void) const;
  bool    empty(void) const;

  Buffer& operator += (const Buffer& buf);
  char&   operator [] (size_t ix);
  Buffer& operator =  (const Buffer& buf);
private:
  bool    invalid;
  char*   mptr;
  size_t  len_total;
  size_t  len_used;
};

#endif	/* _BUFFER_H_ */
