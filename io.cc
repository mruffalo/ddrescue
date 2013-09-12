/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
    2013 Antonio Diaz Diaz.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "block.h"
#include "ddrescue.h"


namespace {

bool volatile interrupted_ = false;		// user pressed Ctrl-C
extern "C" void sighandler( int ) { interrupted_ = true; }


bool block_is_zero( const uint8_t * const buf, const int size )
  {
  for( int i = 0; i < size; ++i ) if( buf[i] != 0 ) return false;
  return true;
  }


// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
//
int readblock( const int fd, uint8_t * const buf, const int size,
               const long long pos )
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      const int n = read( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( n == 0 ) break;				// EOF
      else if( errno != EINTR && errno != EAGAIN ) break;
      }
  return size - rest;
  }


// Returns the number of bytes really written.
// If (returned value < size), it is always an error.
//
int writeblock( const int fd, const uint8_t * const buf, const int size,
                const long long pos )
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      const int n = write( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( n < 0 && errno != EINTR && errno != EAGAIN ) break;
      }
  return size - rest;
  }

} // end namespace


// Return values: 1 write error, 0 OK.
//
int Fillbook::fill_block( const Block & b )
  {
  if( b.size() <= 0 ) internal_error( "bad size filling a Block" );
  const int size = b.size();

  if( writeblock( odes_, iobuf(), size, b.pos() + offset() ) != size ||
      ( synchronous_ && fsync( odes_ ) < 0 && errno != EINVAL ) )
    {
    if( !ignore_write_errors_ )
      { final_msg( "Write error" ); final_errno( errno ); }
    return 1;
    }
  filled_size += size; remaining_size -= size;
  return 0;
  }


bool Fillbook::read_buffer( const int ides )
  {
  const int rd = readblock( ides, iobuf(), softbs(), 0 );
  if( rd <= 0 ) return false;
  for( int i = rd; i < softbs(); i *= 2 )
    {
    const int size = std::min( i, softbs() - i );
    std::memcpy( iobuf() + i, iobuf(), size );
    }
  return true;
  }


// If copied_size + error_size < b.size(), it means EOF has been reached.
//
void Genbook::check_block( const Block & b, int & copied_size, int & error_size )
  {
  if( b.size() <= 0 ) internal_error( "bad size checking a Block" );
  copied_size = readblock( odes_, iobuf(), b.size(), b.pos() + offset() );
  if( errno ) error_size = b.size() - copied_size;

  for( int pos = 0; pos < copied_size; )
    {
    const int size = std::min( hardbs(), copied_size - pos );
    if( !block_is_zero( iobuf() + pos, size ) )
      {
      change_chunk_status( Block( b.pos() + pos, size ),
                           Sblock::finished, domain() );
      recsize += size;
      }
    gensize += size;
    pos += size;
    }
  }


bool Rescuebook::extend_outfile_size()
  {
  if( min_outfile_size > 0 || sparse_size > 0 )
    {
    const long long min_size = std::max( min_outfile_size, sparse_size );
    const long long size = lseek( odes_, 0, SEEK_END );
    if( size < 0 ) return false;
    if( min_size > size )
      {
      const uint8_t zero = 0;
      if( writeblock( odes_, &zero, 1, min_size - 1 ) != 1 ) return false;
      fsync( odes_ );
      }
    }
  return true;
  }


// Return values: 1 write error, 0 OK.
// If !OK, copied_size and error_size are set to 0.
// If OK && copied_size + error_size < b.size(), it means EOF has been reached.
//
int Rescuebook::copy_block( const Block & b, int & copied_size, int & error_size )
  {
  if( b.size() <= 0 ) internal_error( "bad size copying a Block" );
  copied_size = readblock( ides_, iobuf(), b.size(), b.pos() );
  if( errno ) error_size = b.size() - copied_size;

  if( copied_size > 0 )
    {
    const long long pos = b.pos() + offset();
    if( sparse_size >= 0 && block_is_zero( iobuf(), copied_size ) )
      {
      const long long end = pos + copied_size;
      if( end > sparse_size ) sparse_size = end;
      }
    else if( writeblock( odes_, iobuf(), copied_size, pos ) != copied_size ||
             ( synchronous_ && fsync( odes_ ) < 0 && errno != EINVAL ) )
      {
      copied_size = 0; error_size = 0;
      final_msg( "Write error" ); final_errno( errno );
      return 1;
      }
    }
  return 0;
  }


const char * format_time( long t )
  {
  enum { buffers = 8, bufsize = 16 };
  static char buffer[buffers][bufsize];	// circle of static buffers for printf
  static int current = 0;
  char * const buf = buffer[current++]; current %= buffers;
  int fraction = 0;
  char unit = 's';

  if( t >= 86400 ) { fraction = ( t % 86400 ) / 864; t /= 86400; unit = 'd'; }
  else if( t >= 3600 ) { fraction = ( t % 3600 ) / 36; t /= 3600; unit = 'h'; }
  else if( t >= 60 ) { fraction = (10 * ( t % 60 )) / 6; t /= 60; unit = 'm'; }
  if( fraction == 0 )
    snprintf( buf, bufsize, "%ld %c", t, unit );
  else
    snprintf( buf, bufsize, "%ld.%02d %c", t, fraction, unit );
  return buf;
  }


bool interrupted() { return interrupted_; }


void set_signals()
  {
  interrupted_ = false;
  std::signal( SIGINT, sighandler );
  std::signal( SIGHUP, sighandler );
  std::signal( SIGTERM, sighandler );
  std::signal( SIGUSR1, SIG_IGN );
  std::signal( SIGUSR2, SIG_IGN );
  }
