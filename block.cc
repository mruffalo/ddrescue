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
#include <cstdio>
#include <vector>
#include "ddrescue.h"


Block::Block( const long long p, const long long s ) throw()
  {
  if( p < 0 || s < -1 )
    {
    std::fprintf( stderr, "p = %lld, s = %lld\n", p, s );
    internal_error( "bad parameter building a Block" );
    }
  _pos = p; _size = s;
  }


long long Block::hard_blocks( const int hardbs ) const throw()
  {
  if( _size <= 0 ) return _size;
  long long s = _size + ( _pos % hardbs );
  int tail = ( _pos + _size ) % hardbs;
  if( tail != 0 ) s += ( hardbs - tail );
  return s / hardbs;
  }


void Block::pos( const long long p ) throw()
  {
  if( p < 0 ) internal_error( "bad pos relocating a Block" );
  _pos = p;
  }


void Block::size( const long long s ) throw()
  {
  if( s < -1 ) internal_error( "bad size resizing a Block" );
  _size = s;
  }


// Returns true if 'block' spans more than one hardware block.
//
bool Block::can_be_split( const int hardbs ) const throw()
  {
  if( _size <= 0 ) return ( _size < 0 );
  return ( _size > hardbs - ( _pos % hardbs ) );
  }


bool Block::follows( const Block & b ) const throw()
  {
  return ( b._size >= 0 && b._pos + b._size == _pos );
  }


bool Block::includes( const long long pos ) const throw()
  {
  return ( _pos <= pos && ( _size < 0 || _pos + _size > pos ) );
  }


bool Block::overlaps( const Block & b ) const throw()
  {
  return ( ( _size < 0 || _pos + _size > b._pos ) &&
           ( b._size < 0 || b._pos + b._size > _pos ) );
  }


bool Block::join( const Block & b ) throw()
  {
  if( _size >= 0 && _pos + _size == b._pos )
    { if( b._size >= 0 ) _size += b._size; else _size = -1; return true; }
  if( b._size >= 0 && b._pos + b._size == _pos )
    { _pos = b._pos; if( _size >= 0 ) _size += b._size; return true; }
  return false;
  }


Block Block::overlap( const Block & b ) const throw()
  {
  long long p = std::max( _pos, b._pos );
  long long s;
  if( _size < 0 )
    { if( b._size < 0 ) s = -1; else s = std::max( 0LL, b.end() - p ); }
  else
    {
    if( b._size < 0 ) s = std::max( 0LL, end() - p );
    else s = std::max( 0LL, std::min( end(), b.end() ) - p );
    }
  return Block( p, s );
  }


Block Block::split( const long long pos ) throw()
  {
  if( _pos < pos && ( _size < 0 || _pos + _size > pos ) )
    {
    Block b( _pos, pos - _pos );
    _pos = pos; if( _size > 0 ) _size -= b._size;
    return b;
    }
  return Block();
  }


Block Block::split_hb( const int hardbs ) throw()
  {
  return this->split( _pos + hardbs - ( _pos % hardbs ) );
  }


Sblock::Sblock( const Block & b, const Status st ) throw()
  : Block( b )
  {
  if( isstatus( st ) ) _status = st;
  else internal_error( "bad status building a Sblock" );
  }


Sblock::Sblock( const long long p, const long long s, const Status st ) throw()
  : Block( p, s )
  {
  if( isstatus( st ) ) _status = st;
  else internal_error( "bad status building a Sblock" );
  }


void Sblock::status( const Status st ) throw()
  {
  if( isstatus( st ) ) _status = st;
  else internal_error( "bad status change in a Sblock" );
  }


bool Sblock::join( const Sblock & sb ) throw()
  {
  if( _status == sb._status ) return Block::join( sb );
  return false;
  }


bool Sblock::isstatus( const int st ) throw()
  {
  return ( st == non_tried || st == bad_cluster ||
           st == bad_block || st == done );
  }
