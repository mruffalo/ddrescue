/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007 Antonio Diaz Diaz.

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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <signal.h>
#include <unistd.h>

#include "ddrescue.h"


namespace {

bool volatile interrupted = false;		// user pressed Ctrl-C
void sighandler( int ) throw() { interrupted = true; }


// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
//
int readblock( const int fd, char * buf, const int size, const long long pos ) throw()
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      const int n = read( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( n == 0 ) break;
      else if( errno != EINTR && errno != EAGAIN ) break;
      }
  return ( rest > 0 ) ? size - rest : size;
  }


// Returns the number of bytes really written.
// If (returned value < size), it is always an error.
//
int writeblock( const int fd, const char * buf, const int size, const long long pos ) throw()
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      const int n = write( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( errno && errno != EINTR && errno != EAGAIN ) break;
      }
  return ( rest > 0 ) ? size - rest : size;
  }


bool block_is_zero( const char * buf, const int size ) throw()
  {
  for( int i = 0; i < size; ++i ) if( buf[i] != 0 ) return false;
  return true;
  }

} // end namespace


// Return values: 1 write error, 0 OK, -1 interrupted.
//
int Fillbook::fill_block( const Block & b ) throw()
  {
  current_pos( b.pos() );
  if( interrupted ) return -1;
  if( b.size() <= 0 ) internal_error( "bad size filling a Block" );
  const int size = b.size();

  if( writeblock( _odes, iobuf(), size, b.pos() ) != size )
    { final_msg( "write error" ); final_errno( errno ); return 1; }
  filled_size += size; remaining_size -= size;
  return 0;
  }


void Fillbook::show_status( const long long opos, bool force ) throw()
  {
  static const char * const up = "\x1b[A";
  static long long a_rate = 0, c_rate = 0, first_size = 0, last_size = 0;
  static long long last_opos = 0;
  static time_t t0 = 0, t1 = 0;
  if( t0 == 0 )
    {
    t0 = t1 = std::time( 0 ); first_size = last_size = filled_size; force = true;
    std::printf( "\n\n\n" );
    }

  if( opos >= 0 ) last_opos = opos;
  time_t t2 = std::time( 0 );
  if( t2 > t1 || force )
    {
    if( t2 > t1 )
      {
      a_rate = ( filled_size - first_size ) / ( t2 - t0 );
      c_rate = ( filled_size - last_size ) / ( t2 - t1 );
      t1 = t2; last_size = filled_size;
      }
    std::printf( "\r%s%s%s", up, up, up );
    std::printf( "filled size: %10sB,", format_num( filled_size ) );
    std::printf( "  filled areas: %6u,", filled_areas );
    std::printf( "  current rate: %9sB/s\n", format_num( c_rate, 99999 ) );
    std::printf( "remain size: %10sB,", format_num( remaining_size ) );
    std::printf( "  remain areas: %6u,", remaining_areas );
    std::printf( "  average rate: %9sB/s\n", format_num( a_rate, 99999 ) );
    std::printf( "current pos: %10sB\n", format_num( last_opos ) );
    std::fflush( stdout );
    }
  }


bool Fillbook::read_buffer( const long long ipos, const int ides ) throw()
  {
  const int rd = readblock( ides, iobuf(), softbs(), ipos );
  if( rd <= 0 ) return false;
  for( int i = rd; i < softbs(); i *= 2 )
    {
    const int size = std::min( i, softbs() - i );
    std::memcpy( iobuf() + i, iobuf(), size );
    }
  return true;
  }


bool Rescuebook::sync_sparse_file() throw()
  {
  bool done = true;
  if( _sparse && sparse_size > 0 )
    {
    const long long size = lseek( _odes, 0, SEEK_END );
    if( size < 0 ) done = false;
    if( sparse_size > size )
      {
      const char zero = 0;
      if( writeblock( _odes, &zero, 1, sparse_size - 1 ) != 1 )
        done = false;
      fsync( _odes );
      }
    }
  return done;
  }


int Rescuebook::write_block_or_move( const int fd, const char * buf,
                                     const int size, const long long pos ) throw()
  {
  if( _sparse && block_is_zero( buf, size ) &&
      lseek( fd, pos + size, SEEK_SET ) >= 0 )
    {
    if( pos + size > sparse_size ) sparse_size = pos + size;
    return size;
    }
  return writeblock( fd, buf, size, pos );
  }


// Return values: 1 write error, 0 OK, -1 interrupted.
// If !OK, copied_size and error_size are set to 0.
// If OK && copied_size + error_size < b.size(), it means EOF has been reached.
//
int Rescuebook::copy_block( const Block & b, int & copied_size, int & error_size ) throw()
  {
  current_pos( b.pos() );
  copied_size = 0; error_size = 0;
  if( interrupted ) return -1;
  if( b.size() <= 0 ) internal_error( "bad size copying a Block" );
  const int size = b.size();
  if( size > hardbs() )
    {
    copied_size = readblock( _ides, iobuf(), hardbs(), b.pos() );
    if( copied_size == hardbs() )
      copied_size += readblock( _ides, iobuf() + copied_size,
                                size - copied_size, b.pos() + copied_size );
    }
  else copied_size = readblock( _ides, iobuf(), size, b.pos() );
  if( errno ) error_size = size - copied_size;

  if( copied_size > 0 &&
      write_block_or_move( _odes, iobuf(), copied_size, b.pos() + offset ) != copied_size )
    {
    copied_size = 0; error_size = 0;
    final_msg( "write error" ); final_errno( errno );
    return 1;
    }
  return 0;
  }


void Rescuebook::show_status( const long long ipos, const char * msg,
                              bool force ) throw()
  {
  static const char * const up = "\x1b[A";
  static long long a_rate = 0, c_rate = 0, first_size = 0, last_size = 0;
  static long long last_ipos = 0;
  static time_t t0 = 0, t1 = 0;
  static int oldlen = 0;
  if( t0 == 0 )
    {
    t0 = t1 = std::time( 0 ); first_size = last_size = recsize; force = true;
    std::printf( "\n\n\n" );
    }

  if( ipos >= 0 ) last_ipos = ipos;
  time_t t2 = std::time( 0 );
  if( t2 > t1 || force )
    {
    if( t2 > t1 )
      {
      a_rate = ( recsize - first_size ) / ( t2 - t0 );
      c_rate = ( recsize - last_size ) / ( t2 - t1 );
      t1 = t2; last_size = recsize;
      }
    std::printf( "\r%s%s%s", up, up, up );
    std::printf( "rescued: %10sB,", format_num( recsize ) );
    std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
    std::printf( "  current rate: %9sB/s\n", format_num( c_rate, 99999 ) );
    std::printf( "   ipos: %10sB,   errors: %7u,  ",
                 format_num( last_ipos ), errors );
    std::printf( "  average rate: %9sB/s\n", format_num( a_rate, 99999 ) );
    std::printf( "   opos: %10sB\n", format_num( last_ipos + offset ) );
    int len = oldlen;
    if( msg ) { len = std::strlen( msg ); if( len ) std::printf( msg ); }
    for( int i = len; i < oldlen; ++i ) std::fputc( ' ', stdout );
    if( len || oldlen ) std::fputc( '\r', stdout );
    oldlen = len;
    std::fflush( stdout );
    }
  }


const char * format_num( long long num, long long max,
                         const int set_prefix ) throw()
  {
  static const char * const si_prefix[8] =
    { "k", "M", "G", "T", "P", "E", "Z", "Y" };
  static const char * const binary_prefix[8] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  static bool si = true;
  static char buf[16];

  if( set_prefix ) si = ( set_prefix > 0 );
  const int factor = ( si ) ? 1000 : 1024;
  const char * const *prefix = ( si ) ? si_prefix : binary_prefix;
  const char *p = "";
  max = std::max( 999LL, std::min( 999999LL, max ) );

  for( int i = 0; i < 8 && llabs( num ) > llabs( max ); ++i )
    { num /= factor; p = prefix[i]; }
  snprintf( buf, sizeof( buf ), "%lld %s", num, p );
  return buf;
  }


void set_handler() throw()
  {
  interrupted = false;
  signal( SIGINT, sighandler );
  signal( SIGHUP, sighandler );
  signal( SIGTERM, sighandler );
  }
