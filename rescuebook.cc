/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012,
    2013, 2014 Antonio Diaz Diaz.

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
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "block.h"
#include "ddrescue.h"
#include "loggers.h"


void Rescuebook::count_errors()
  {
  bool good = true;
  errors = 0;

  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) )
      { if( domain() < sb ) break; else { good = true; continue; } }
    switch( sb.status() )
      {
      case Sblock::non_tried:
      case Sblock::finished:   good = true; break;
      case Sblock::non_trimmed:
      case Sblock::non_split:
      case Sblock::bad_sector: if( good ) { good = false; ++errors; } break;
      }
    }
  }


// Return values: 1 error, 0 OK.
//
int Rescuebook::update( const Block & b, const Sblock::Status st,
                        const int copied_size, const int error_size )
  {
  int retval = 0;
  if( copied_size + error_size < b.size() )			// EOF
    {
    if( complete_only ) truncate_domain( b.pos() + copied_size + error_size );
    else if( !truncate_vector( b.pos() + copied_size + error_size ) )
      { final_msg( "EOF found before end of logfile" ); retval = 1; }
    }
  if( copied_size > 0 )
    {
    errors += change_chunk_status( Block( b.pos(), copied_size ),
                                   Sblock::finished, domain() );
    recsize += copied_size;
    }
  if( error_size > 0 )
    {
    errors += change_chunk_status( Block( b.pos() + copied_size, error_size ),
              ( error_size > hardbs() ) ? st : Sblock::bad_sector, domain() );
    if( access_works && access( iname_, F_OK ) != 0 )
      { final_msg( "Input file disappeared", errno ); retval = 1; }
    }
  return retval;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted.
//
int Rescuebook::copy_and_update( const Block & b, int & error_size,
                                 const char * const msg, const bool forward )
  {
  current_pos( forward ? b.pos() : b.end() );
  show_status( b.pos(), msg );
  if( errors_or_timeout() ) return 1;
  if( interrupted() ) return -1;
  int copied_size = 0;
  int retval = copy_block( b, copied_size, error_size );
  if( retval == 0 )
    {
    if( copied_size > 0 ) errsize -= copied_size;
    retval = update( b, Sblock::bad_sector, copied_size, error_size );
    }
  return retval;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted.
//
int Rescuebook::copy_and_update2( const Block & b, int & copied_size,
                                  int & error_size, const char * const msg,
                                  const bool forward, const bool small_try )
  {
  if( first_post ) read_logger.print_msg( t1 - t0, msg );
  current_pos( forward ? b.pos() : b.end() );
  current_status( copying, msg );
  show_status( b.pos(), msg );
  if( errors_or_timeout() ) return 1;
  if( interrupted() ) return -1;
  if( small_try && b.size() > hardbs() )	// try one sector first
    {
    const Block b1( b.pos(), hardbs() );
    const Block b2( b1.end(), b.size() - b1.size() );
    int retval = copy_block( b1, copied_size, error_size );
    if( retval ) return retval;
    if( copied_size != b1.size() )		// mark full block on error
      { error_size += b2.size();
        return update( b, Sblock::non_trimmed, copied_size, error_size ); }
    retval = copy_block( b2, copied_size, error_size );
    if( retval ) return update( b1, Sblock::bad_sector, b1.size(), 0 );
    copied_size += b1.size();
    return update( b, Sblock::non_trimmed, copied_size, error_size );
    }
  int retval = copy_block( b, copied_size, error_size );
  if( retval == 0 )
    retval = update( b, Sblock::non_trimmed, copied_size, error_size );
  return retval;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Read the non-damaged part of the domain, skipping over the damaged areas.
//
int Rescuebook::copy_non_tried()
  {
  char msgbuf[80] = "Copying non-tried blocks... Pass ";
  const int msglen = std::strlen( msgbuf );
  bool forward = !reverse;

  for( int pass = 1; ; ++pass )
    {
    first_post = true;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d %s",
              pass, forward ? "(forwards)" : "(backwards)" );
    int retval = forward ? fcopy_non_tried( msgbuf, pass ) :
                           rcopy_non_tried( msgbuf, pass );
    if( retval != -3 ) return retval;
    reduce_min_read_rate();
    forward = !forward;
    }
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Read forwards the non-damaged part of the domain, skipping over the
// damaged areas.
//
int Rescuebook::fcopy_non_tried( const char * const msg, const int pass )
  {
  long long pos = 0;
  int skip_size = 0;			// size to skip on error if skipbs > 0
  bool block_found = false;

  if( pass == 1 && current_status() == copying &&
      domain().includes( current_pos() ) )
    {
    Block b( current_pos(), 1 );
    find_chunk( b, Sblock::non_tried, domain(), hardbs() );
    if( b.size() > 0 ) pos = b.pos();		// resume
    }

  while( pos >= 0 )
    {
    Block b( pos, softbs() );
    find_chunk( b, Sblock::non_tried, domain(), softbs() );
    if( b.size() <= 0 ) break;
    if( pos != b.pos() ) skip_size = 0;		// reset size on block change
    pos = b.end();
    block_found = true;
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update2( b, copied_size, error_size,
                                         msg, true, skip_size > 0 );
    if( error_size > 0 ) { errsize += error_size; error_rate += error_size; }
    if( retval ) return retval;
    update_rates();
    if( ( error_size > 0 || slow_read() ) && pos >= 0 )
      {
      if( skip_size > 0 )			// do not skip until 2nd error
        {
        if( reopen_on_error && !reopen_infile() ) return 1;
        if( skipbs > 0 && pass <= 2 )		// do not skip if skipbs == 0
          {
          b.assign( pos, skip_size );
          find_chunk( b, Sblock::non_tried, domain(), hardbs() );
          if( pos == b.pos() && b.size() > 0 )
            {
            if( error_size > 0 && b.size() <= softbs() && b.size() <= skip_size )
              { errors += change_chunk_status( b, Sblock::non_trimmed, domain() );
                errsize += b.size(); }
            pos = b.end();
            }
          }
        }
      if( skip_size < skipbs ) skip_size = skipbs;
      else if( skip_size <= max_skipbs / 2 ) skip_size *= 2;
      else skip_size = max_skipbs;
      }
    else if( skip_size > 0 && copied_size > 0 )		// reset in two steps
      { if( skip_size > skipbs ) skip_size = skipbs; else skip_size = 0; }
    if( !update_logfile( odes_ ) ) return -2;
    }
  if( !block_found ) return 0;
  return -3;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Read backwards the non-damaged part of the domain, skipping over the
// damaged areas.
//
int Rescuebook::rcopy_non_tried( const char * const msg, const int pass )
  {
  long long end = LLONG_MAX;
  int skip_size = 0;			// size to skip on error if skipbs > 0
  bool block_found = false;

  if( pass == 1 && current_status() == copying &&
      domain().includes( current_pos() - 1 ) )
    {
    Block b( current_pos() - 1, 1 );
    rfind_chunk( b, Sblock::non_tried, domain(), hardbs() );
    if( b.size() > 0 ) end = b.end();		// resume
    }

  while( end > 0 )
    {
    Block b( end - softbs(), softbs() );
    rfind_chunk( b, Sblock::non_tried, domain(), softbs() );
    if( b.size() <= 0 ) break;
    if( end != b.end() ) skip_size = 0;		// reset size on block change
    end = b.pos();
    block_found = true;
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update2( b, copied_size, error_size,
                                         msg, false, skip_size > 0 );
    if( error_size > 0 ) { errsize += error_size; error_rate += error_size; }
    if( retval ) return retval;
    update_rates();
    if( ( error_size > 0 || slow_read() ) && end > 0 )
      {
      if( skip_size > 0 )			// do not skip until 2nd error
        {
        if( reopen_on_error && !reopen_infile() ) return 1;
        if( skipbs > 0 && pass <= 2 )		// do not skip if skipbs == 0
          {
          b.assign( end - skip_size, skip_size );
          rfind_chunk( b, Sblock::non_tried, domain(), hardbs() );
          if( end == b.end() && b.size() > 0 )
            {
            if( error_size > 0 && b.size() <= softbs() && b.size() <= skip_size )
              { errors += change_chunk_status( b, Sblock::non_trimmed, domain() );
                errsize += b.size(); }
            end = b.pos();
            }
          }
        }
      if( skip_size < skipbs ) skip_size = skipbs;
      else if( skip_size <= max_skipbs / 2 ) skip_size *= 2;
      else skip_size = max_skipbs;
      }
    else if( skip_size > 0 && copied_size > 0 )		// reset in two steps
      { if( skip_size > skipbs ) skip_size = skipbs; else skip_size = 0; }
    if( !update_logfile( odes_ ) ) return -2;
    }
  if( !block_found ) return 0;
  return -3;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Trim the damaged areas (smallest first) from both edges.
//
int Rescuebook::trim_errors()
  {
  const char * const msg = "Trimming failed blocks...";
  first_post = true;
  read_logger.print_msg( t1 - t0, msg );

  while( true )
    {
    const int index =
      find_smallest_sblock( Sblock::non_trimmed, domain(), softbs() );
    if( index < 0 ) break;				// no more blocks
    const Block block = sblock( index );
    long long pos = block.pos();
    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::non_trimmed, domain(), hardbs() );
      if( pos != b.pos() || b.size() <= 0 ) break;	// block change
      pos = b.end();
      current_status( trimming, msg );
      int error_size = 0;
      const int retval = copy_and_update( b, error_size, msg, true );
      if( retval ) return retval;
      if( error_size > 0 ) { error_rate += error_size; pos = -1; }
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    long long end = block.end();
    while( end > 0 )
      {
      Block b( end - hardbs(), hardbs() );
      rfind_chunk( b, Sblock::non_trimmed, domain(), hardbs() );
      if( end != b.end() || b.size() <= 0 ) break;	// block change
      end = b.pos();
      current_status( trimming, msg );
      int error_size = 0;
      const int retval = copy_and_update( b, error_size, msg, false );
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      if( error_size > 0 && end > 0 )
        {
        const int index = find_index( end - 1 );
        if( index >= 0 && domain().includes( sblock( index ) ) &&
            sblock( index ).status() == Sblock::non_trimmed )
          errors += change_chunk_status( sblock( index ), Sblock::non_split,
                                         domain() );
        end = -1;
        }
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Split the damaged areas (largest first).
// Then read the remaining small areas (less than 7 sectors) sequentially.
//
int Rescuebook::split_errors()
  {
  const char * const msg = "Splitting failed blocks...";
  first_post = true;
  read_logger.print_msg( t1 - t0, msg );

  while( true )
    {
    const int index = find_largest_sblock( Sblock::non_split, domain() );
    if( index < 0 ) break;				// no more blocks
    const Block block = sblock( index );
    if( block.size() / hardbs() < 7 ) break;		// no more large blocks
    if( sblocks() < max_logfile_size )			// split largest block
      {
      long long midpos =
        block.pos() + ( ( block.size() / ( 2 * hardbs() ) ) * hardbs() );
      long long pos = midpos;
      while( pos >= 0 )
        {
        Block b( pos, hardbs() );
        find_chunk( b, Sblock::non_split, domain(), hardbs() );
        if( pos != b.pos() || b.size() <= 0 ) break;	// block change
        pos = b.end();
        current_status( splitting, msg );
        int error_size = 0;
        const int retval = copy_and_update( b, error_size, msg, true );
        if( retval ) return retval;
        if( error_size > 0 )
          { error_rate += error_size; pos = -1;
            if( b.pos() == midpos ) midpos = -1; }	// skip backwards reads
        update_rates();
        if( !update_logfile( odes_ ) ) return -2;
        }
      long long end = midpos;
      while( end > 0 )
        {
        Block b( end - hardbs(), hardbs() );
        rfind_chunk( b, Sblock::non_split, domain(), hardbs() );
        if( end != b.end() || b.size() <= 0 ) break;	// block change
        end = b.pos();
        current_status( splitting, msg );
        int error_size = 0;
        const int retval = copy_and_update( b, error_size, msg, true );
        if( retval ) return retval;
        if( error_size > 0 ) { error_rate += error_size; end = -1; }
        update_rates();
        if( !update_logfile( odes_ ) ) return -2;
        }
      }
    else			// logfile is full; read largest block
      {
      long long pos = block.pos();
      while( pos >= 0 )
        {
        Block b( pos, hardbs() );
        find_chunk( b, Sblock::non_split, domain(), hardbs() );
        if( pos != b.pos() || b.size() <= 0 ) break;	// block change
        pos = b.end();
        current_status( splitting, msg );
        int error_size = 0;
        const int retval = copy_and_update( b, error_size, msg, true );
        if( retval ) return retval;
        if( error_size > 0 ) error_rate += error_size;
        update_rates();
        if( !update_logfile( odes_ ) ) return -2;
        }
      }
    }
  if( !reverse )		// read the remaining small areas forwards
    {
    long long pos = 0;
    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::non_split, domain(), hardbs() );
      if( b.size() <= 0 ) break;			// no more blocks
      pos = b.end();
      current_status( splitting, msg );
      int error_size = 0;
      const int retval = copy_and_update( b, error_size, msg, true );
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    }
  else				// read the remaining small areas backwards
    {
    long long end = LLONG_MAX;
    while( end > 0 )
      {
      Block b( end - hardbs(), hardbs() );
      rfind_chunk( b, Sblock::non_split, domain(), hardbs() );
      if( b.size() <= 0 ) break;			// no more blocks
      end = b.pos();
      current_status( splitting, msg );
      int error_size = 0;
      const int retval = copy_and_update( b, error_size, msg, true );
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Try to read the damaged areas, one sector at a time.
//
int Rescuebook::copy_errors()
  {
  char msgbuf[80] = "Retrying bad sectors (forwards)... Retry ";
  const int msglen = std::strlen( msgbuf );

  for( int retry = 1; max_retries < 0 || retry <= max_retries; ++retry )
    {
    long long pos = 0;
    bool block_found = false;
    first_post = true;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d", retry );
    read_logger.print_msg( t1 - t0, msgbuf );

    if( retry == 1 && current_status() == retrying &&
        domain().includes( current_pos() ) )
      {
      Block b( current_pos(), 1 );
      find_chunk( b, Sblock::bad_sector, domain(), hardbs() );
      if( b.size() > 0 ) pos = b.pos();		// resume
      }

    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::bad_sector, domain(), hardbs() );
      if( b.size() <= 0 ) break;			// no more blocks
      pos = b.end();
      current_status( retrying, msgbuf );
      block_found = true;
      int error_size = 0;
      const int retval = copy_and_update( b, error_size, msgbuf, true );
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found || retry >= INT_MAX ) break;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Try to read the damaged areas in reverse mode, one sector at a time.
//
int Rescuebook::rcopy_errors()
  {
  char msgbuf[80] = "Retrying bad sectors (backwards)... Retry ";
  const int msglen = std::strlen( msgbuf );

  for( int retry = 1; max_retries < 0 || retry <= max_retries; ++retry )
    {
    long long end = LLONG_MAX;
    bool block_found = false;
    first_post = true;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d", retry );
    read_logger.print_msg( t1 - t0, msgbuf );

    if( retry == 1 && current_status() == retrying &&
        domain().includes( current_pos() - 1 ) )
      {
      Block b( current_pos() - 1, 1 );
      rfind_chunk( b, Sblock::bad_sector, domain(), hardbs() );
      if( b.size() > 0 ) end = b.end();		// resume
      }

    while( end > 0 )
      {
      Block b( end - hardbs(), hardbs() );
      rfind_chunk( b, Sblock::bad_sector, domain(), hardbs() );
      if( b.size() <= 0 ) break;			// no more blocks
      end = b.pos();
      current_status( retrying, msgbuf );
      block_found = true;
      int error_size = 0;
      const int retval = copy_and_update( b, error_size, msgbuf, false );
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found || retry >= INT_MAX ) break;
    }
  return 0;
  }


void Rescuebook::update_rates( const bool force )
  {
  if( t0 == 0 )
    {
    t0 = t1 = ts = initial_time();
    first_size = last_size = recsize;
    rates_updated = true;
    if( verbosity >= 0 ) std::printf( "\n\n\n" );
    }

  long t2 = std::time( 0 );
  if( force && t2 <= t1 ) t2 = t1 + 1;		// force update of e_code
  if( t2 > t1 )
    {
    a_rate = ( recsize - first_size ) / ( t2 - t0 );
    c_rate = ( recsize - last_size ) / ( t2 - t1 );
    if( !( e_code & 4 ) )
      {
      if( recsize != last_size ) { last_size = recsize; ts = t2; }
      else if( timeout >= 0 && t2 - ts > timeout ) e_code |= 4;
      }
    if( max_error_rate >= 0 && !( e_code & 1 ) )
      {
      error_rate /= ( t2 - t1 );
      if( error_rate > max_error_rate ) e_code |= 1;
      else error_rate = 0;
      }
    t1 = t2;
    rates_updated = true;
    }
  else if( t2 < t1 )			// clock jumped back
    {
    const long delta = std::min( t0, t1 - t2 );
    t0 -= delta;
    ts -= delta;
    t1 = t2;
    }
  }


void Rescuebook::show_status( const long long ipos, const char * const msg,
                              const bool force )
  {
  const char * const up = "\x1b[A";

  if( ipos >= 0 ) last_ipos = ipos;
  if( rates_updated || force || first_post )
    {
    if( verbosity >= 0 )
      {
      std::printf( "\r%s%s%s", up, up, up );
      std::printf( "rescued: %10sB,  errsize:%9sB,  current rate: %9sB/s\n",
                   format_num( recsize ), format_num( errsize, 99999 ),
                   format_num( c_rate, 99999 ) );
      std::printf( "   ipos: %10sB,   errors: %7u,    average rate: %9sB/s\n",
                   format_num( last_ipos ), errors,
                   format_num( a_rate, 99999 ) );
      std::printf( "   opos: %10sB, run time: %9s,  successful read: %9s ago\n",
                   format_num( last_ipos + offset() ),
                   format_time( t1 - t0 ), format_time( t1 - ts ) );
      if( msg && msg[0] && !errors_or_timeout() )
        {
        const int len = std::strlen( msg ); std::printf( "\r%s", msg );
        for( int i = len; i < oldlen; ++i ) std::fputc( ' ', stdout );
        oldlen = len;
        }
      std::fflush( stdout );
      }
    rate_logger.print_line( t1 - t0, last_ipos, a_rate, c_rate, errors, errsize );
    if( !force && !first_post ) read_logger.print_time( t1 - t0 );
    rates_updated = false;
    first_post = false;
    }
  }


Rescuebook::Rescuebook( const long long offset, const long long isize,
                        Domain & dom, const Domain * const test_dom,
                        const Rb_options & rb_opts, const char * const iname,
                        const char * const logname, const int cluster,
                        const int hardbs, const bool synchronous )
  : Logbook( offset, isize, dom, logname, cluster, hardbs, rb_opts.complete_only ),
    Rb_options( rb_opts ),
    error_rate( 0 ),
    sparse_size( sparse ? 0 : -1 ),
    recsize( 0 ),
    errsize( 0 ),
    test_domain( test_dom ),
    iname_( iname ),
    e_code( 0 ),
    access_works( access( iname, F_OK ) == 0 ),
    synchronous_( synchronous ),
    a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
    last_ipos( 0 ), t0( 0 ), t1( 0 ), ts( 0 ), oldlen( 0 ),
    rates_updated( false ), first_post( false )
  {
  const long long csize = isize / 100;
  if( isize > 0 && skipbs > 0 && max_skipbs == Rb_options::max_max_skipbs &&
      csize < max_skipbs )
    max_skipbs = std::max( (long long)skipbs, csize );
  skipbs = round_up( skipbs, hardbs );		// make multiple of hardbs
  max_skipbs = round_up( max_skipbs, hardbs );

  if( retrim )
    for( int index = 0; index < sblocks(); ++index )
      {
      const Sblock & sb = sblock( index );
      if( !domain().includes( sb ) )
        { if( domain() < sb ) break; else continue; }
      if( sb.status() == Sblock::non_split ||
          sb.status() == Sblock::bad_sector )
        change_sblock_status( index, Sblock::non_trimmed );
      }
  if( try_again )
    for( int index = 0; index < sblocks(); ++index )
      {
      const Sblock & sb = sblock( index );
      if( !domain().includes( sb ) )
        { if( domain() < sb ) break; else continue; }
      if( sb.status() == Sblock::non_split ||
          sb.status() == Sblock::non_trimmed )
        change_sblock_status( index, Sblock::non_tried );
      }
  count_errors();
  if( new_errors_only ) max_errors += errors;
  }


// Return values: 1 I/O error, 0 OK.
//
int Rescuebook::do_rescue( const int ides, const int odes )
  {
  bool copy_pending = false, trim_pending = false, split_pending = false;
  ides_ = ides; odes_ = odes;

  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) ) { if( domain() < sb ) break; else continue; }
    switch( sb.status() )
      {
      case Sblock::non_tried:   copy_pending = trim_pending = split_pending = true;
                                break;
      case Sblock::non_trimmed: trim_pending = true;	// fall through
      case Sblock::non_split:   split_pending = true;	// fall through
      case Sblock::bad_sector:  errsize += sb.size(); break;
      case Sblock::finished:    recsize += sb.size(); break;
      }
    }
  set_signals();
  if( verbosity >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( logfile_exists() )
      {
      std::printf( "Initial status (read from logfile)\n" );
      if( verbosity >= 2 )
        {
        std::printf( "current position: %10sB,     current sector: %7lld\n",
                     format_num( current_pos() ), current_pos() / hardbs() );
        if( sblocks() )
          std::printf( " last block size: %10sB\n",
                       format_num( sblock( sblocks() - 1 ).size() ) );
        }
      if( domain().pos() > 0 || domain().end() < logfile_isize() )
        std::printf( "(sizes below are limited to the domain %sB to %sB)\n",
                     format_num( domain().pos() ), format_num( domain().end() ) );
      std::printf( "rescued: %10sB,  errsize:%9sB,  errors: %7u\n",
                   format_num( recsize ), format_num( errsize, 99999 ), errors );
      std::printf( "\nCurrent status\n" );
      }
    }
  int retval = 0;
  update_rates();				// first call
  if( copy_pending && !errors_or_timeout() )
    retval = copy_non_tried();
  if( retval == 0 && trim_pending && !notrim && !errors_or_timeout() )
    retval = trim_errors();
  if( retval == 0 && split_pending && !nosplit && !errors_or_timeout() )
    retval = split_errors();
  if( retval == 0 && max_retries != 0 && !errors_or_timeout() )
    retval = reverse ? rcopy_errors() : copy_errors();
  if( !rates_updated ) update_rates( true );	// force update of e_code
  show_status( -1, retval ? 0 : "Finished", true );

  const bool signaled = ( retval == -1 );
  if( signaled ) retval = 0;
  if( retval == 0 && errors_or_timeout() ) retval = 1;
  if( verbosity >= 0 )
    {
    if( retval == -2 ) std::printf( "\nLogfile error" );
    else if( retval == 0 && signaled ) std::printf( "\nInterrupted by user" );
    else
      {
      if( e_code & 1 )
        std::printf( "\nToo high error rate reading input file (%sB/s)",
                     format_num( error_rate ) );
      if( e_code & 2 ) std::printf( "\nToo many errors in input file" );
      if( e_code & 4 ) std::printf( "\nTimeout expired" );
      }
    std::fputc( '\n', stdout );
    }
  if( retval == -2 ) retval = 1;		// logfile error
  else
    {
    if( retval == 0 && !signaled ) current_status( finished );
    if( !extend_outfile_size() )		// sparse or -x option
      {
      show_error( "Error extending output file size." );
      if( retval == 0 ) retval = 1;
      }
    compact_sblock_vector();
    if( !update_logfile( odes_, true ) && retval == 0 ) retval = 1;
    }
  if( close( odes_ ) != 0 )
    { show_error( "Can't close outfile", errno );
      if( retval == 0 ) retval = 1; }
  if( !rate_logger.close_file() )
    show_error( "warning: Error closing the rates logging file." );
  if( !read_logger.close_file() )
    show_error( "warning: Error closing the reads logging file." );
  if( final_msg() ) show_error( final_msg(), final_errno() );
  if( retval ) return retval;		// errors have priority over signals
  if( signaled ) return signaled_exit();
  return 0;
  }
