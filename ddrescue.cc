/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005 Antonio Diaz Diaz.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <queue>
#include <signal.h>
#include <unistd.h>
#include "ddrescue.h"


namespace {

bool volatile interrupted = false;		// user pressed Ctrl-C
void sighandler( int ) throw() { interrupted = true; }


// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
//
int readblock( int fd, char * buf, int size, long long pos ) throw()
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      int n = read( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( n == 0 ) break;
      else if( errno != EINTR && errno != EAGAIN ) break;
      }
  return ( rest > 0 ) ? size - rest : size;
  }


// Returns the number of bytes really written.
// If (returned value < size), it is always an error.
//
int writeblock( int fd, char * buf, int size, long long pos ) throw()
  {
  int rest = size;
  errno = 0;
  if( lseek( fd, pos, SEEK_SET ) >= 0 )
    while( rest > 0 )
      {
      errno = 0;
      int n = write( fd, buf + size - rest, rest );
      if( n > 0 ) rest -= n;
      else if( errno && errno != EINTR && errno != EAGAIN ) break;
      }
  return ( rest > 0 ) ? size - rest : size;
  }

} // end namespace


// Return values: 1 write error, 0 OK, -1 interrupted, -2 EOF.
//
int Logbook::copy_non_tried_block( const Block & block,
                                   std::vector< Sblock > & result ) throw()
  {
  result.clear();
  if( interrupted )
    { result.push_back( Sblock( block, Sblock::non_tried ) ); return -1; }
  if( block.size() <= 0 ) return 0;
  const int size = block.size();
  int rd;
  if( size > _hardbs )
    {
    rd = readblock( _ides, iobuf, _hardbs, block.pos() );
    if( rd == _hardbs )
      rd += readblock( _ides, iobuf + rd, size - rd, block.pos() + rd );
    }
  else rd = readblock( _ides, iobuf, size, block.pos() );
  const int errno1 = errno;

  if( rd > 0 )
    {
    if( writeblock( _odes, iobuf, rd, block.pos() + _offset ) != rd )
      {
      result.push_back( Sblock( block, Sblock::non_tried ) );
      show_error( "write error", errno ); return 1;
      }
    result.push_back( Sblock( block.pos(), rd, Sblock::done ) );
    recsize += rd;
    }
  if( rd < size )
    {
    if( !errno1 ) return -2;	// EOF
    else			// Read error
      {
      Block b( block.pos() + rd, size - rd );
      if( b.can_be_split( _hardbs ) )
        result.push_back( Sblock( b, Sblock::bad_cluster ) );
      else result.push_back( Sblock( b, Sblock::bad_block ) );
      ++errors; errsize += size - rd;
      }
    }
  return 0;
  }


// Return values: 1 write error, 0 OK, -1 interrupted, -2 EOF.
//
int Logbook::copy_bad_block( const Block & block,
                             std::vector< Sblock > & result ) throw()
  {
  result.clear();
  if( interrupted )
    { result.push_back( Sblock( block, Sblock::bad_block ) ); return -1; }
  if( block.size() <= 0 ) return 0;
  const int size = block.size();

  const int rd = readblock( _ides, iobuf, size, block.pos() );
  const int errno1 = errno;

  if( rd > 0 )
    {
    if( writeblock( _odes, iobuf, rd, block.pos() + _offset ) != rd )
      {
      result.push_back( Sblock( block, Sblock::bad_block ) );
      show_error( "write error", errno ); return 1;
      }
    result.push_back( Sblock( block.pos(), rd, Sblock::done ) );
    recsize += rd; errsize -= rd;
    }
  if( !errno1 ) --errors;
  if( rd < size )
    {
    if( !errno1 ) { errsize -= size - rd; return -2; }	// EOF
    else						// read_error
      result.push_back( Sblock( block.pos()+rd, size-rd, Sblock::bad_block ) );
    }
  return 0;
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
  int factor = ( si ) ? 1000 : 1024;
  const char * const *prefix = ( si ) ? si_prefix : binary_prefix;
  const char *p = "";
  max = std::max( 999LL, std::min( 999999LL, max ) );

  for( int i = 0; i < 8 && std::llabs( num ) > std::llabs( max ); ++i )
    { num /= factor; p = prefix[i]; }
  std::snprintf( buf, sizeof( buf ), "%lld %s", num, p );
  return buf;
  }


void set_handler() throw()
  {
  interrupted = false;
  signal( SIGINT, sighandler );
  }


void show_status( const long long ipos, const long long opos,
                  const long long recsize, const long long errsize,
                  const int errors, const char * msg, bool force ) throw()
  {
  static const char * const up = "\x1b[A";
  static long long a_rate = 0, c_rate = 0, first_size = 0, last_size = 0;
  static long long last_ipos = 0, last_opos = 0;
  static time_t t0 = 0, t1 = 0;
  static int oldlen = 0;
  if( t0 == 0 )
    {
    t0 = t1 = std::time( 0 ); first_size = last_size = recsize; force = true;
    std::printf( "\n\n\n" );
    }

  if( ipos >= 0 ) last_ipos = ipos;
  if( opos >= 0 ) last_opos = opos;
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
    std::printf( "   opos: %10sB\n", format_num( last_opos ) );
    int len = 0;
    if( msg ) { len = std::strlen( msg ); std::printf( msg ); }
    for( int i = len; i < oldlen; ++i ) std::fputc( ' ', stdout );
    if( len || oldlen ) std::fputc( '\r', stdout );
    oldlen = len;
    std::fflush( stdout );
    }
  }
