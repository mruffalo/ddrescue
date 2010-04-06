/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010
    Antonio Diaz Diaz.

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
#include <unistd.h>

#include "block.h"
#include "ddrescue.h"


namespace {

bool volatile interrupted = false;		// user pressed Ctrl-C
extern "C" void sighandler( int ) throw() { interrupted = true; }


bool block_is_zero( const char * const buf, const int size ) throw()
  {
  for( int i = 0; i < size; ++i ) if( buf[i] != 0 ) return false;
  return true;
  }


const char * format_time( time_t t ) throw()
  {
  static char buf[16];
  int fraction = 0;
  char unit = 's';

  if( t >= 86400 ) { fraction = ( t % 86400 ) / 8640; t /= 86400; unit = 'd'; }
  else if( t >= 3600 ) { fraction = ( t % 3600 ) / 360; t /= 3600; unit = 'h'; }
  else if( t >= 60 ) { fraction = ( t % 60 ) / 6; t /= 60; unit = 'm'; }
  if( fraction == 0 )
    snprintf( buf, sizeof buf, "%ld %c", t, unit );
  else
    snprintf( buf, sizeof buf, "%ld.%d %c", t, fraction, unit );
  return buf;
  }


// Returns the number of bytes really read.
// If (returned value < size) and (errno == 0), means EOF was reached.
//
int readblock( const int fd, char * const buf, const int size,
               const long long pos ) throw()
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
int writeblock( const int fd, const char * const buf, const int size,
                const long long pos ) throw()
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

} // end namespace


// Return values: 1 write error, 0 OK, -1 interrupted.
//
int Fillbook::fill_block( const Block & b )
  {
  current_pos( b.pos() );
  if( interrupted ) return -1;
  if( b.size() <= 0 ) internal_error( "bad size filling a Block" );
  const int size = b.size();

  if( writeblock( odes_, iobuf(), size, b.pos() + offset() ) != size ||
      ( synchronous_ && fsync( odes_ ) < 0 ) )
    { final_msg( "write error" ); final_errno( errno ); return 1; }
  filled_size += size; remaining_size -= size;
  return 0;
  }


void Fillbook::show_status( const long long ipos, bool force ) throw()
  {
  const char * const up = "\x1b[A";
  if( t0 == 0 )
    {
    t0 = t1 = std::time( 0 );
    first_size = last_size = filled_size;
    force = true;
    std::printf( "\n\n\n" );
    }

  if( ipos >= 0 ) last_ipos = ipos;
  const time_t t2 = std::time( 0 );
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
    std::printf( "current pos: %10sB\n", format_num( last_ipos + offset() ) );
    std::fflush( stdout );
    }
  }


bool Fillbook::read_buffer( const int ides ) throw()
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


bool Rescuebook::sync_sparse_file() throw()
  {
  bool done = true;
  if( sparse_ && sparse_size > 0 )
    {
    const long long size = lseek( odes_, 0, SEEK_END );
    if( size < 0 ) done = false;
    if( sparse_size > size )
      {
      const char zero = 0;
      if( writeblock( odes_, &zero, 1, sparse_size - 1 ) != 1 )
        done = false;
      fsync( odes_ );
      }
    }
  return done;
  }


// Return values: 0 OK, -1 interrupted.
// If !OK, copied_size and error_size are set to 0.
// If OK && copied_size + error_size < b.size(), it means EOF has been reached.
//
int Rescuebook::check_block( const Block & b, int & copied_size, int & error_size )
  {
  current_pos( b.pos() );
  copied_size = 0; error_size = 0;
  if( interrupted ) return -1;
  if( b.size() <= 0 ) internal_error( "bad size checking a Block" );
  copied_size = readblock( odes_, iobuf(), b.size(), b.pos() + offset() );
  if( errno ) error_size = b.size() - copied_size;

  for( int pos = 0; pos < copied_size; )
    {
    const int size = std::min( hardbs(), copied_size - pos );
    if( !block_is_zero( iobuf() + pos, size ) )
      {
      change_chunk_status( Block( b.pos() + pos, size ), Sblock::finished );
      recsize += size;
      }
    pos += size;
    }
  return 0;
  }


// Return values: 1 write error, 0 OK, -1 interrupted.
// If !OK, copied_size and error_size are set to 0.
// If OK && copied_size + error_size < b.size(), it means EOF has been reached.
//
int Rescuebook::copy_block( const Block & b, int & copied_size, int & error_size )
  {
  current_pos( b.pos() );
  copied_size = 0; error_size = 0;
  if( interrupted ) return -1;
  if( b.size() <= 0 ) internal_error( "bad size copying a Block" );
  copied_size = readblock( ides_, iobuf(), b.size(), b.pos() );
  if( errno ) error_size = b.size() - copied_size;

  if( copied_size > 0 )
    {
    const long long pos = b.pos() + offset();
    const long long end = pos + copied_size;
    if( sparse_ && block_is_zero( iobuf(), copied_size ) &&
        lseek( odes_, end, SEEK_SET ) >= 0 )
      { if( end > sparse_size ) sparse_size = end; }
    else if( writeblock( odes_, iobuf(), copied_size, pos ) != copied_size ||
             ( synchronous_ && fsync( odes_ ) < 0 ) )
      {
      copied_size = 0; error_size = 0;
      final_msg( "write error" ); final_errno( errno );
      return 1;
      }
    }
  return 0;
  }


void Rescuebook::show_status( const long long ipos, const char * const msg,
                              bool force ) throw()
  {
  const char * const up = "\x1b[A";
  if( t0 == 0 )
    {
    t0 = t1 = ts = std::time( 0 );
    first_size = last_size = recsize;
    force = true;
    std::printf( "\n\n\n" );
    }

  if( ipos >= 0 ) last_ipos = ipos;
  const time_t t2 = std::time( 0 );
  if( t2 > t1 || force )
    {
    if( t2 > t1 )
      {
      a_rate = ( recsize - first_size ) / ( t2 - t0 );
      c_rate = ( recsize - last_size ) / ( t2 - t1 );
      if( recsize > last_size ) ts = t2;
      t1 = t2;
      last_size = recsize;
      }
    count_errors();
    std::printf( "\r%s%s%s", up, up, up );
    std::printf( "rescued: %10sB,", format_num( recsize ) );
    std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
    std::printf( "  current rate: %9sB/s\n", format_num( c_rate, 99999 ) );
    std::printf( "   ipos: %10sB,   errors: %7u,  ",
                 format_num( last_ipos ), errors );
    std::printf( "  average rate: %9sB/s\n", format_num( a_rate, 99999 ) );
    std::printf( "   opos: %10sB,", format_num( last_ipos + offset() ) );
    std::printf( "     time from last successful read: %9s\n",
                 format_time( t2 - ts ) );
    int len = oldlen;
    if( msg ) { len = std::strlen( msg ); if( len ) std::printf( msg ); }
    for( int i = len; i < oldlen; ++i ) std::fputc( ' ', stdout );
    if( len || oldlen ) std::fputc( '\r', stdout );
    oldlen = len;
    std::fflush( stdout );
    }
  }


const char * format_num( long long num, long long limit,
                         const int set_prefix ) throw()
  {
  const char * const si_prefix[8] =
    { "k", "M", "G", "T", "P", "E", "Z", "Y" };
  const char * const binary_prefix[8] =
    { "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi", "Yi" };
  static bool si = true;
  static char buf[16];

  if( set_prefix ) si = ( set_prefix > 0 );
  const int factor = ( si ) ? 1000 : 1024;
  const char * const *prefix = ( si ) ? si_prefix : binary_prefix;
  const char *p = "";
  limit = std::max( 999LL, std::min( 999999LL, limit ) );

  for( int i = 0; i < 8 && ( llabs( num ) > limit ||
       ( llabs( num ) >= factor && num % factor == 0 ) ); ++i )
    { num /= factor; p = prefix[i]; }
  snprintf( buf, sizeof buf, "%lld %s", num, p );
  return buf;
  }


void set_signals() throw()
  {
  interrupted = false;
  std::signal( SIGINT, sighandler );
  std::signal( SIGHUP, sighandler );
  std::signal( SIGTERM, sighandler );
  std::signal( SIGUSR1, SIG_IGN );
  std::signal( SIGUSR2, SIG_IGN );
  }
