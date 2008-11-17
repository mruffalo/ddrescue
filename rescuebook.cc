/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Antonio Diaz Diaz.

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

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "block.h"
#include "ddrescue.h"


int Rescuebook::check_all() throw()
  {
  long long pos = ( (offset() >= 0) ? 0 : -offset() );
  if( current_status() == generating && domain().includes( current_pos() ) &&
      ( offset() >= 0 || current_pos() >= -offset() ) )
    pos = current_pos();
  bool first_post = true;

  while( pos >= 0 )
    {
    Block b( pos, softbs() );
    find_chunk( b, Sblock::non_tried );
    if( b.size() == 0 ) break;
    pos = b.end();
    current_status( generating );
    if( verbosity >= 0 )
      { show_status( b.pos(), "Generating logfile...", first_post ); first_post = false; }
    int copied_size, error_size;
    const int retval = check_block( b, copied_size, error_size );
    if( !retval )
      {
      if( copied_size + error_size < b.size() )		// EOF
        truncate_vector( b.pos() + copied_size + error_size );
      }
    if( retval ) return retval;
    if( !update_logfile() ) return 1;
    }
  return 0;
  }


void Rescuebook::count_errors() throw()
  {
  bool good = true;
  errors = 0;
  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) ) { if( domain() < sb ) break; else continue; }
    switch( sb.status() )
      {
      case Sblock::non_tried:
      case Sblock::finished: good = true; break;
      case Sblock::non_trimmed:
      case Sblock::non_split:
      case Sblock::bad_block: if( good ) { good = false; ++errors; } break;
      }
    }
  }


int Rescuebook::copy_and_update( const Block & b, const Sblock::Status st,
                                 int & copied_size, int & error_size,
                                 const char * msg, bool & first_post ) throw()
  {
  if( verbosity >= 0 )
    { show_status( b.pos(), msg, first_post ); first_post = false; }
  const int retval = copy_block( b, copied_size, error_size );
  if( !retval )
    {
    if( copied_size + error_size < b.size() )		// EOF
      truncate_vector( b.pos() + copied_size + error_size );
    if( copied_size > 0 )
      {
      change_chunk_status( Block( b.pos(), copied_size ), Sblock::finished );
      recsize += copied_size;
      }
    if( error_size > 0 )
      change_chunk_status( Block( b.pos() + copied_size, error_size ), st );
    count_errors();
    }
  return retval;
  }


// Read the non-damaged part of the domain, skipping over the damaged areas.
//
int Rescuebook::copy_non_tried() throw()
  {
  long long pos = 0;
  long long skip_size = hardbs();	// size to skip on error
  bool first_post = true;

  while( pos >= 0 )
    {
    Block b( pos, skip_size ? hardbs() : softbs() );
    find_chunk( b, Sblock::non_tried );
    if( b.size() == 0 ) break;
    if( pos != b.pos() ) skip_size = 0;	// reset size on block change
    pos = b.end();
    current_status( copying );
    int copied_size, error_size;
    const int retval = copy_and_update( b, skip_size ? Sblock::bad_block : Sblock::non_trimmed,
                                        copied_size, error_size,
                                        "Copying data...", first_post );
    if( error_size > 0 )
      {
      if( skip_size >= softbs() )
        {
        b.pos( pos ); b.size( skip_size );
        find_chunk( b, Sblock::non_tried );
        if( pos == b.pos() && b.size() > 0 )
          { change_chunk_status( b, Sblock::non_trimmed );
            pos = b.end(); errsize += b.size(); }
        }
      errsize += error_size; skip_size += softbs();
      }
    else if( skip_size > 0 && copied_size > 0 )
      { skip_size -= copied_size; if( skip_size < 0 ) skip_size = 0; }
    if( retval || too_many_errors() ) return retval;
    if( !update_logfile( _odes ) ) return 1;
    }
  return 0;
  }


// Trim the damaged areas backwards.
//
int Rescuebook::trim_errors() throw()
  {
  long long pos = LLONG_MAX - hardbs();
  bool first_post = true;

  while( pos >= 0 )
    {
    Block b( pos, hardbs() );
    rfind_chunk( b, Sblock::non_trimmed );
    if( b.size() == 0 ) break;
    pos = b.pos() - hardbs();
    current_status( trimming );
    int copied_size, error_size;
    const int retval = copy_and_update( b, Sblock::bad_block, copied_size,
                                        error_size, "Trimming error areas...",
                                        first_post );
    if( copied_size > 0 ) errsize -= copied_size;
    if( error_size > 0 && b.pos() > 0 )
      {
      const int index = find_index( b.pos() - 1 );
      if( index >= 0 && domain().includes( sblock( index ) ) &&
          sblock( index ).status() == Sblock::non_trimmed )
        change_chunk_status( sblock( index ), Sblock::non_split );
      }
    if( retval || too_many_errors() ) return retval;
    if( !update_logfile( _odes ) ) return 1;
    }
  return 0;
  }


// Try to read the damaged areas, splitting them into smaller pieces.
//
int Rescuebook::split_errors() throw()
  {
  bool first_post = true;
  bool resume = ( current_status() == splitting &&
                  domain().includes( current_pos() ) );
  while( true )
    {
    long long pos = 0;
    if( resume ) { resume = false; pos = current_pos(); }
    int error_counter = 0;
    bool block_found = false;

    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::non_split );
      if( b.size() == 0 ) break;
      pos = b.end();
      block_found = true;
      current_status( splitting );
      int copied_size, error_size;
      const int retval = copy_and_update( b, Sblock::bad_block, copied_size,
                                          error_size, "Splitting error areas...",
                                          first_post );
      if( copied_size > 0 ) errsize -= copied_size;
      if( error_size <= 0 ) error_counter = 0;
      else if( ++error_counter >= 2 )	// skip after 2 consecutive errors
        {
        error_counter = 0;
        const int index = find_index( pos );
        if( index >= 0 && sblock( index ).status() == Sblock::non_split )
          { pos += softbs();
            if( !sblock( index ).includes( pos ) ) pos = sblock( index ).end(); }
        }
      if( retval || too_many_errors() ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      }
    if( !block_found ) break;
    }
  return 0;
  }


int Rescuebook::copy_errors() throw()
  {
  char msgbuf[80] = "Copying bad blocks... Retry ";
  const int msglen = std::strlen( msgbuf );
  bool resume = ( current_status() == retrying &&
                  domain().includes( current_pos() ) );

  for( int retry = 1; _max_retries < 0 || retry <= _max_retries; ++retry )
    {
    long long pos = 0;
    if( resume ) { resume = false; pos = current_pos(); }
    bool first_post = true, block_found = false;
    snprintf( msgbuf + msglen, ( sizeof msgbuf ) - msglen, "%d", retry );

    while( pos >= 0 )
      {
      Block b( pos, hardbs() );
      find_chunk( b, Sblock::bad_block );
      if( b.size() == 0 ) break;
      pos = b.end();
      block_found = true;
      current_status( retrying );
      int copied_size, error_size;
      const int retval = copy_and_update( b, Sblock::bad_block, copied_size,
                                          error_size, msgbuf, first_post );
      if( copied_size > 0 ) errsize -= copied_size;
      if( retval || too_many_errors() ) return retval;
      if( !update_logfile( _odes ) ) return 1;
      }
    if( !block_found ) break;
    }
  return 0;
  }


Rescuebook::Rescuebook( const long long ipos, const long long opos,
                        Domain & dom, const long long isize,
                        const char * name, const int cluster, const int hardbs,
                        const int max_errors, const int max_retries,
                        const bool complete_only, const bool nosplit, const bool retrim,
                        const bool sparse, const bool synchronous ) throw()
  : Logbook( ipos, opos, dom, isize, name, cluster, hardbs, complete_only ),
    sparse_size( 0 ), _max_errors( max_errors ), _max_retries( max_retries ),
    _nosplit( nosplit ), _sparse( sparse ), _synchronous( synchronous )
  {
  if( retrim )
    for( int index = 0; index < sblocks(); ++index )
      {
      const Sblock & sb = sblock( index );
      if( !domain().includes( sb ) ) { if( domain() < sb ) break; else continue; }
      if( sb.status() == Sblock::non_split || sb.status() == Sblock::bad_block )
        change_sblock_status( index, Sblock::non_trimmed );
      }
  }


int Rescuebook::do_generate( const int odes ) throw()
  {
  recsize = 0; errsize = 0;
  _ides = -1; _odes = odes;

  split_domain_border_sblocks();
  for( int i = 0; i < sblocks(); ++i )
    {
    const Sblock & sb = sblock( i );
    if( !domain().includes( sb ) ) { if( domain() < sb ) break; else continue; }
    switch( sb.status() )
      {
      case Sblock::non_tried:   break;
      case Sblock::non_trimmed: 			// fall through
      case Sblock::non_split:   			// fall through
      case Sblock::bad_block:   errsize += sb.size(); break;
      case Sblock::finished:    recsize += sb.size(); break;
      }
    }
  count_errors();
  set_handler();
  if( verbosity >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( filename() )
      {
      std::printf( "Initial status (read from logfile)\n" );
      std::printf( "rescued: %10sB,", format_num( recsize ) );
      std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
      std::printf( "  errors: %7u\n", errors );
      std::printf( "Current status\n" );
      }
    }
  int retval = check_all();
  if( verbosity >= 0 )
    {
    show_status( -1, (retval ? 0 : "Finished"), true );
    if( retval < 0 ) std::printf( "\nInterrupted by user" );
    std::fputc( '\n', stdout );
    }
  if( retval == 0 ) current_status( finished );
  else if( retval < 0 ) retval = 0;		// interrupted by user
  compact_sblock_vector();
  if( !update_logfile( -1, true ) && retval == 0 ) retval = 1;
  if( final_msg() ) show_error( final_msg(), final_errno() );
  return retval;
  }


int Rescuebook::do_rescue( const int ides, const int odes ) throw()
  {
  bool copy_pending = false, trim_pending = false, split_pending = false;
  recsize = 0; errsize = 0;
  _ides = ides; _odes = odes;

  split_domain_border_sblocks();
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
      case Sblock::bad_block:   errsize += sb.size(); break;
      case Sblock::finished:    recsize += sb.size(); break;
      }
    }
  count_errors();
  set_handler();
  if( verbosity >= 0 )
    {
    std::printf( "Press Ctrl-C to interrupt\n" );
    if( filename() )
      {
      std::printf( "Initial status (read from logfile)\n" );
      std::printf( "rescued: %10sB,", format_num( recsize ) );
      std::printf( "  errsize:%9sB,", format_num( errsize, 99999 ) );
      std::printf( "  errors: %7u\n", errors );
      std::printf( "Current status\n" );
      }
    }
  int retval = 0;
  if( copy_pending && !too_many_errors() )
    retval = copy_non_tried();
  if( !retval && trim_pending && !too_many_errors() )
    retval = trim_errors();
  if( !retval && split_pending && !_nosplit && !too_many_errors() )
    retval = split_errors();
  if( !retval && _max_retries != 0 && !too_many_errors() )
    retval = copy_errors();
  if( verbosity >= 0 )
    {
    show_status( -1, (retval ? 0 : "Finished"), true );
    if( retval < 0 ) std::printf( "\nInterrupted by user" );
    else if( too_many_errors() ) std::printf("\nToo many errors in input file" );
    std::fputc( '\n', stdout );
    }
  if( retval == 0 ) current_status( finished );
  else if( retval < 0 ) retval = 0;		// interrupted by user
  if( !sync_sparse_file() )
    {
    show_error( "error syncing sparse output file" );
    if( retval == 0 ) retval = 1;
    }
  compact_sblock_vector();
  if( !update_logfile( _odes, true ) && retval == 0 ) retval = 1;
  if( final_msg() ) show_error( final_msg(), final_errno() );
  return retval;
  }
