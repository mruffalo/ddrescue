/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
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

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <stdint.h>
#include <unistd.h>

#include "block.h"
#include "ddrescue.h"


namespace {

int calculate_max_skip_size( const long long isize,
                             const int hardbs, const int skipbs )
  {
  int skip;

  if( isize > 0 && isize / 100 < Rescuebook::max_skipbs ) skip = isize / 100;
  else skip = Rescuebook::max_skipbs;
  if( skip < hardbs || skip < skipbs ) skip = skipbs;
  return round_up( skip, hardbs );
  }

} // end namespace


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


// Return values: 1 I/O error, 0 OK, -1 interrupted.
//
int Rescuebook::copy_and_update( const Block & b, const Sblock::Status st,
                                 int & copied_size, int & error_size,
                                 const char * const msg, bool & first_post,
                                 const bool forward )
  {
  current_pos( forward ? b.pos() : b.end() );
  show_status( current_pos(), msg, first_post );
  first_post = false;
  if( errors_or_timeout() ) return 1;
  if( interrupted() ) return -1;
  int retval = copy_block( b, copied_size, error_size );
  if( !retval )
    {
    if( copied_size + error_size < b.size() )		// EOF
      truncate_vector( b.pos() + copied_size + error_size );
    if( copied_size > 0 )
      {
      errors += change_chunk_status( Block( b.pos(), copied_size ), Sblock::finished );
      recsize += copied_size;
      }
    if( error_size > 0 )
      {
      errors += change_chunk_status( Block( b.pos() + copied_size, error_size ), st );
      if( iname_ && access( iname_, F_OK ) != 0 )
        {
        final_msg( "input file disappeared" ); final_errno( errno );
        retval = 1;
        }
      }
    }
  return retval;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Read the non-damaged part of the domain, skipping over the damaged areas.
//
int Rescuebook::copy_non_tried()
  {
  bool first_post = true;

  for( bool first_pass = true; ; first_pass = false )
    {
    long long pos = 0;
    int skip_size = 0;				// size to skip on error
    bool block_found = false;

    if( first_pass && current_status() == copying &&
        domain().includes( current_pos() ) )
      {
      Block b( current_pos(), 1 );
      find_chunk( b, Sblock::non_tried );
      if( b.size() > 0 ) pos = b.pos();		// resume
      }

    while( pos >= 0 )
      {
      const int alignment = ( skip_size ? hardbs() : softbs() );
      Block b( pos, alignment );
      find_chunk( b, Sblock::non_tried, alignment );
      if( b.size() <= 0 ) break;
      if( pos != b.pos() ) skip_size = 0;	// reset size on block change
      pos = b.end();
      current_status( copying );
      block_found = true;
      const Sblock::Status st = ( skip_size ? Sblock::bad_sector :
                                              Sblock::non_trimmed );
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, st, copied_size, error_size,
                                          "Copying non-tried blocks...",
                                          first_post, true );
      if( error_size > 0 ) errsize += error_size;
      else if( skip_size > 0 && copied_size > 0 )
        { skip_size -= copied_size; if( skip_size < 0 ) skip_size = 0; }
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( error_size > 0 || slow_read() )
        {
        if( pos >= 0 && skip_size > 0 )
          {
          b.pos( pos ); b.size( skip_size ); b.fix_size();
          find_chunk( b, Sblock::non_tried );
          if( pos == b.pos() && b.size() > 0 )
            {
            if( error_size > 0 )
              { errors += change_chunk_status( b, Sblock::non_trimmed );
                errsize += b.size(); }
            pos = b.end();
            }
          }
        if( skip_size < skipbs_ ) skip_size = skipbs_;
        else if( skip_size <= max_skip_size / 2 ) skip_size *= 2;
        else skip_size = max_skip_size;
        }
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found ) break;
    reduce_min_read_rate();
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Read the non-damaged part of the domain in reverse mode, skipping
// over the damaged areas.
//
int Rescuebook::rcopy_non_tried()
  {
  bool first_post = true;

  for( bool first_pass = true; ; first_pass = false )
    {
    long long end = LLONG_MAX;
    int skip_size = 0;				// size to skip on error
    bool block_found = false;

    if( first_pass && current_status() == copying &&
        domain().includes( current_pos() - 1 ) )
      {
      Block b( current_pos() - 1, 1 );
      rfind_chunk( b, Sblock::non_tried );
      if( b.size() > 0 ) end = b.end();		// resume
      }

    while( end > 0 )
      {
      const int alignment = ( skip_size ? hardbs() : softbs() );
      long long pos = end - alignment;
      if( pos < 0 ) pos = 0;
      Block b( pos, end - pos );
      rfind_chunk( b, Sblock::non_tried, alignment );
      if( b.size() <= 0 ) break;
      if( pos != b.pos() ) skip_size = 0;	// reset size on block change
      end = b.pos();
      current_status( copying );
      block_found = true;
      const Sblock::Status st = ( skip_size ? Sblock::bad_sector :
                                              Sblock::non_trimmed );
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, st, copied_size, error_size,
                                          "Copying non-tried blocks...",
                                          first_post, false );
      if( error_size > 0 ) errsize += error_size;
      else if( skip_size > 0 && copied_size > 0 )
        { skip_size -= copied_size; if( skip_size < 0 ) skip_size = 0; }
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( error_size > 0 || slow_read() )
        {
        if( end > 0 && skip_size > 0 )
          {
          b.size( skip_size ); b.end( end ); pos = b.pos();
          rfind_chunk( b, Sblock::non_tried );
          if( pos == b.pos() && b.size() > 0 )
            {
            if( error_size > 0 )
              { errors += change_chunk_status( b, Sblock::non_trimmed );
                errsize += b.size(); }
            end = b.pos();
            }
          }
        if( skip_size < skipbs_ ) skip_size = skipbs_;
        else if( skip_size <= max_skip_size / 2 ) skip_size *= 2;
        else skip_size = max_skip_size;
        }
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found ) break;
    reduce_min_read_rate();
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Trim the damaged areas backwards.
//
int Rescuebook::trim_errors()
  {
  long long end = LLONG_MAX;
  bool first_post = true;

  while( end > 0 )
    {
    long long pos = end - hardbs();
    if( pos < 0 ) pos = 0;
    Block b( pos, end - pos );
    rfind_chunk( b, Sblock::non_trimmed );
    if( b.size() <= 0 ) break;
    end = b.pos();
    current_status( trimming );
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update( b, Sblock::bad_sector, copied_size,
                                        error_size, "Trimming failed blocks...",
                                        first_post, false );
    if( copied_size > 0 ) errsize -= copied_size;
    if( retval ) return retval;
    if( error_size > 0 ) error_rate += error_size;
    if( error_size > 0 && end > 0 )
      {
      const int index = find_index( end - 1 );
      if( index >= 0 && domain().includes( sblock( index ) ) &&
          sblock( index ).status() == Sblock::non_trimmed )
        errors += change_chunk_status( sblock( index ), Sblock::non_split );
      }
    update_rates();
    if( !update_logfile( odes_ ) ) return -2;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Trim the damaged areas in reverse mode, forwards.
//
int Rescuebook::rtrim_errors()
  {
  long long pos = 0;
  bool first_post = true;

  while( pos >= 0 )
    {
    Block b( pos, hardbs() );
    find_chunk( b, Sblock::non_trimmed );
    if( b.size() <= 0 ) break;
    pos = b.end();
    current_status( trimming );
    int copied_size = 0, error_size = 0;
    const int retval = copy_and_update( b, Sblock::bad_sector, copied_size,
                                        error_size, "Trimming failed blocks...",
                                        first_post, true );
    if( copied_size > 0 ) errsize -= copied_size;
    if( retval ) return retval;
    if( error_size > 0 ) error_rate += error_size;
    if( error_size > 0 && pos >= 0 )
      {
      const int index = find_index( pos );
      if( index >= 0 && domain().includes( sblock( index ) ) &&
          sblock( index ).status() == Sblock::non_trimmed )
        errors += change_chunk_status( sblock( index ), Sblock::non_split );
      }
    update_rates();
    if( !update_logfile( odes_ ) ) return -2;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Try to read the damaged areas, splitting them into smaller pieces.
//
int Rescuebook::split_errors()
  {
  bool first_post = true;

  for( bool first_pass = true; ; first_pass = false )
    {
    long long pos = 0;
    int error_counter = 0;
    bool block_found = false;

    if( first_pass && current_status() == splitting &&
        domain().includes( current_pos() ) )
      {
      Block b( current_pos(), 1 );
      find_chunk( b, Sblock::non_split );
      if( b.size() > 0 ) pos = b.pos();		// resume
      }

    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::non_split );
      if( b.size() <= 0 ) break;
      if( pos != b.pos() ) error_counter = 0;	// reset cntr on block change
      pos = b.end();
      current_status( splitting );
      block_found = true;
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, Sblock::bad_sector, copied_size,
                                          error_size, "Splitting failed blocks...",
                                          first_post, true );
      if( copied_size > 0 ) errsize -= copied_size;
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      if( error_size <= 0 ) error_counter = 0;
      else if( pos >= 0 && ++error_counter >= 8 )
        {			// skip after enough consecutive errors
        error_counter = 0;
        const int index = find_index( pos );
        if( index >= 0 )
          {
          const Sblock & sb = sblock( index );
          if( sb.status() == Sblock::non_split && sb.size() >= skipbs_ )
            {
            if( sb.size() >= 2 * skipbs_ )	// split
              {
              if( first_pass || sblocks() < 1000 )
                pos += ( sb.size() / ( 2 * hardbs() ) ) * hardbs();
              else pos = sb.end() - skipbs_;
              }
            else pos = sb.end();		// skip
            }
          }
        }
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found ) break;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Try to read the damaged areas in reverse mode, splitting them into
// smaller pieces.
//
int Rescuebook::rsplit_errors()
  {
  bool first_post = true;

  for( bool first_pass = true; ; first_pass = false )
    {
    long long end = LLONG_MAX;
    int error_counter = 0;
    bool block_found = false;

    if( first_pass && current_status() == splitting &&
        domain().includes( current_pos() - 1 ) )
      {
      Block b( current_pos() - 1, 1 );
      rfind_chunk( b, Sblock::non_split );
      if( b.size() > 0 ) end = b.end();		// resume
      }

    while( end > 0 )
      {
      long long pos = end - hardbs();
      if( pos < 0 ) pos = 0;
      Block b( pos, end - pos );
      rfind_chunk( b, Sblock::non_split );
      if( b.size() <= 0 ) break;
      if( pos != b.pos() ) error_counter = 0;	// reset cntr on block change
      end = b.pos();
      current_status( splitting );
      block_found = true;
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, Sblock::bad_sector, copied_size,
                                          error_size, "Splitting failed blocks...",
                                          first_post, false );
      if( copied_size > 0 ) errsize -= copied_size;
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      if( error_size <= 0 ) error_counter = 0;
      else if( end > 0 && ++error_counter >= 8 )
        {			// skip after enough consecutive errors
        error_counter = 0;
        const int index = find_index( end - 1 );
        if( index >= 0 )
          {
          const Sblock & sb = sblock( index );
          if( sb.status() == Sblock::non_split && sb.size() >= skipbs_ )
            {
            if( sb.size() >= 2 * skipbs_ )	// split
              {
              if( first_pass || sblocks() < 1000 )
                end -= ( sb.size() / ( 2 * hardbs() ) ) * hardbs();
              else end = sb.pos() + skipbs_;
              }
            else end = sb.pos();		// skip
            }
          }
        }
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found ) break;
    }
  return 0;
  }


// Return values: 1 I/O error, 0 OK, -1 interrupted, -2 logfile error.
// Try to read the damaged areas, one sector at a time.
//
int Rescuebook::copy_errors()
  {
  char msgbuf[80] = "Retrying bad sectors... Retry ";
  const int msglen = std::strlen( msgbuf );

  for( int retry = 1; max_retries_ < 0 || retry <= max_retries_; ++retry )
    {
    long long pos = 0;
    bool first_post = true, block_found = false;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d", retry );

    if( retry == 1 && current_status() == retrying &&
        domain().includes( current_pos() ) )
      {
      Block b( current_pos(), 1 );
      find_chunk( b, Sblock::bad_sector );
      if( b.size() > 0 ) pos = b.pos();		// resume
      }

    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::bad_sector );
      if( b.size() <= 0 ) break;
      pos = b.end();
      current_status( retrying );
      block_found = true;
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, Sblock::bad_sector, copied_size,
                                          error_size, msgbuf, first_post, true );
      if( copied_size > 0 ) errsize -= copied_size;
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
  char msgbuf[80] = "Retrying bad sectors... Retry ";
  const int msglen = std::strlen( msgbuf );

  for( int retry = 1; max_retries_ < 0 || retry <= max_retries_; ++retry )
    {
    long long end = LLONG_MAX;
    bool first_post = true, block_found = false;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d", retry );

    if( retry == 1 && current_status() == retrying &&
        domain().includes( current_pos() - 1 ) )
      {
      Block b( current_pos() - 1, 1 );
      rfind_chunk( b, Sblock::bad_sector );
      if( b.size() > 0 ) end = b.end();		// resume
      }

    while( end > 0 )
      {
      long long pos = end - hardbs();
      if( pos < 0 ) pos = 0;
      Block b( pos, end - pos );
      rfind_chunk( b, Sblock::bad_sector );
      if( b.size() <= 0 ) break;
      end = b.pos();
      current_status( retrying );
      block_found = true;
      int copied_size = 0, error_size = 0;
      const int retval = copy_and_update( b, Sblock::bad_sector, copied_size,
                                          error_size, msgbuf, first_post, false );
      if( copied_size > 0 ) errsize -= copied_size;
      if( retval ) return retval;
      if( error_size > 0 ) error_rate += error_size;
      update_rates();
      if( !update_logfile( odes_ ) ) return -2;
      }
    if( !block_found || retry >= INT_MAX ) break;
    }
  return 0;
  }


Rescuebook::Rescuebook( const long long offset, const long long isize,
                        const long long max_error_rate,
                        const long long min_outfile_size,
                        const long long min_read_rate, Domain & dom,
                        const char * const iname, const char * const logname,
                        const long timeout, const int cluster, const int hardbs,
                        const int skipbs, const int max_errors,
                        const int max_retries, const bool complete_only,
                        const bool new_errors_only, const bool nosplit,
                        const bool retrim, const bool sparse,
                        const bool synchronous, const bool try_again )
  : Logbook( offset, isize, dom, logname, cluster, hardbs, complete_only ),
    max_error_rate_( max_error_rate ),
    min_outfile_size_( min_outfile_size ),
    error_rate( 0 ),
    min_read_rate_( min_read_rate ),
    sparse_size( sparse ? 0 : -1 ),
    recsize( 0 ),
    errsize( 0 ),
    timeout_( timeout ),
    iname_( ( access( iname, F_OK ) == 0 ) ? iname : 0 ),
    max_retries_( max_retries ),
    skipbs_( skipbs ),
    max_skip_size( calculate_max_skip_size( isize, hardbs, skipbs ) ),
    max_errors_( max_errors ),
    e_code( 0 ),
    nosplit_( nosplit ), synchronous_( synchronous ),
    a_rate( 0 ), c_rate( 0 ), first_size( 0 ), last_size( 0 ),
    last_ipos( 0 ), t0( 0 ), t1( 0 ), ts( 0 ), oldlen( 0 ),
    rates_updated( false )
  {
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
  if( new_errors_only ) max_errors_ += errors;
  }


// Return values: 1 I/O error, 0 OK.
//
int Rescuebook::do_rescue( const int ides, const int odes, const bool reverse )
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
      std::printf( "rescued: %10sB,", format_num( recsize ) );
      std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
      std::printf( "  errors: %7u\n", errors );
      if( verbosity >= 2 )
        {
        std::printf( "current position:  %10sB,", format_num( current_pos() ) );
        std::printf( "     current sector: %7lld\n", current_pos() / hardbs() );
        if( sblocks() )
          std::printf( "last block size:   %10sB\n",
                       format_num( sblock( sblocks() - 1 ).size() ) );
        std::printf( "\n" );
        }
      std::printf( "Current status\n" );
      }
    }
  int retval = 0;
  update_rates();				// first call
  if( copy_pending && !errors_or_timeout() )
    retval = ( reverse ? rcopy_non_tried() : copy_non_tried() );
  if( !retval && trim_pending && !errors_or_timeout() )
    retval = ( reverse ? rtrim_errors() : trim_errors() );
  if( !retval && split_pending && !nosplit_ && !errors_or_timeout() )
    retval = ( reverse ? rsplit_errors() : split_errors() );
  if( !retval && max_retries_ != 0 && !errors_or_timeout() )
    retval = ( reverse ? rcopy_errors() : copy_errors() );
  if( !rates_updated ) update_rates( true );	// force update of rates
  show_status( -1, (retval ? 0 : "Finished"), true );
  if( !retval && errors_or_timeout() ) retval = 1;
  if( verbosity >= 0 )
    {
    if( retval == -2 ) std::printf( "\nLogfile error" );
    else if( retval < 0 ) std::printf( "\nInterrupted by user" );
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
    if( retval == 0 ) current_status( finished );
    else if( retval < 0 ) retval = 0;		// interrupted by user
    if( !extend_outfile_size() )		// sparse or -x option
      {
      show_error( "Error extending output file size." );
      if( retval == 0 ) retval = 1;
      }
    compact_sblock_vector();
    if( !update_logfile( odes_, true ) && retval == 0 ) retval = 1;
    }
  if( final_msg() ) show_error( final_msg(), final_errno() );
  return retval;
  }
