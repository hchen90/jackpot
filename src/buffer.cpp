/* ***
 * @ $buffer.c
 * 
 * Copyright (C) 2018 Hsiang Chen
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

#include "buffer.h"

using namespace std;

/*class: Buffer*/

Buffer::Buffer() : invalid(false), mptr(nullptr), len_total(0), len_used(0)
{}

Buffer::Buffer(const Buffer& buf) : Buffer()
{
  if (allocate(buf.capacity())) {
    append(buf.data(), buf.size());
  }
}

Buffer::~Buffer()
{
  if (mptr != nullptr) {
    delete[] mptr;
    mptr = nullptr;
  }
}

bool Buffer::allocate(size_t len)
{
  if (len > len_total) {
    if (mptr != nullptr)
      delete[] mptr;
    len_total = len + 2;
    mptr = new char[len_total];
    reset();
  }

  return mptr != nullptr;
}

bool Buffer::append(const void* mem, size_t len)
{
  if (len + len_used > len_total) {
    size_t l = len + len_used + 2;
    char* p = new char[l];

    invalid = true;

    if (p != nullptr) {
      if (mptr != nullptr) {
        memcpy(p, mptr, len_used);
        delete[] mptr;
      }
      len_total = l;
      mptr = p; // update `mptr'
      invalid = false;
    }
  }

  if (! invalid) {
    memcpy(mptr + len_used, mem, len);
    len_used += len;
    return true;
  }

  return false;
}

void Buffer::reset(void)
{
  len_used = 0;
}

void Buffer::resize(size_t len)
{
  if (len > len_total)
    allocate(len);
  if (len <= len_total)
    len_used = len;
}

void* Buffer::data(void) const
{
  return mptr;
}

size_t Buffer::size(void) const
{
  return len_used;
}

size_t Buffer::capacity(void) const
{
  return len_total;
}

bool Buffer::empty(void) const
{
  return len_used == 0;
}

Buffer& Buffer::operator += (const Buffer& buf)
{
  void* p = buf.data(); size_t l = buf.size();

  if (p != nullptr && l > 0) {
    append(p, l);
  }

  return *this;
}

char& Buffer::operator [] (size_t ix)
{
  if (ix < len_total) {
    return mptr[ix];
  } else {
    return mptr[0];
  }
}

Buffer& Buffer::operator = (const Buffer& buf)
{
  invalid     = buf.invalid;
  len_total   = buf.len_total;
  len_used    = buf.len_used;

  if (mptr != nullptr) {
    delete mptr;
  }

  if ((mptr = new char[len_total]) != nullptr) {
    memcpy(mptr, buf.mptr, len_total);
  }

  return *this;
}

/*end*/
