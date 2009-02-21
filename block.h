/*  GNU ddrescue - Data recovery tool
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Antonio Diaz Diaz.

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

#ifndef LLONG_MAX
#define LLONG_MAX  0x7FFFFFFFFFFFFFFFLL
#endif
#ifndef LLONG_MIN
#define LLONG_MIN  (-LLONG_MAX - 1LL)
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX 0xFFFFFFFFFFFFFFFFULL
#endif


class Block
  {
  long long _pos, _size;		// pos + size <= LLONG_MAX

public:
  Block( const long long p, const long long s ) throw()
    : _pos( p ), _size( s ) {}

  long long pos() const throw() { return _pos; }
  long long size() const throw() { return _size; }
  long long end() const throw() { return _pos + _size; }

  void pos( const long long p ) throw() { _pos = p; }
  void size( const long long s ) throw() { _size = s; }
  void fix_size() throw()	// limit _size to largest possible value
    { if( _size < 0 || _size > LLONG_MAX - _pos ) _size = LLONG_MAX - _pos; }
  void end( const long long e ) throw()
    { _pos = e - _size; if( _pos < 0 ) { _size += _pos; _pos = 0; } }
  void align_pos( const int hardbs ) throw();
  void align_end( const int hardbs ) throw();
  void inc_size( const long long delta ) throw() { _size += delta; }

  bool follows( const Block & b ) const throw()
    { return ( _pos == b.end() ); }
  bool includes( const Block & b ) const throw()
    { return ( _pos <= b._pos && end() >= b.end() ); }
  bool includes( const long long pos ) const throw()
    { return ( _pos <= pos && end() > pos ); }

  void crop( const Block & b ) throw();
  bool join( const Block & b );
  Block split( long long pos, const int hardbs = 1 );
  };


class Sblock : public Block
  {
public:
  enum Status
    { non_tried = '?', non_trimmed = '*', non_split = '/', bad_block = '-',
      finished = '+' };
private:
  Status _status;

public:
  Sblock( const Block & b, const Status st ) throw()
    : Block( b ), _status( st ) {}
  Sblock( const long long p, const long long s, const Status st ) throw()
    : Block( p, s ), _status( st ) {}

  Status status() const throw() { return _status; }
  void status( const Status st ) throw() { _status = st; }

  bool join( const Sblock & sb ) throw()
    { if( _status == sb._status ) return Block::join( sb ); else return false; }
  Sblock split( const long long pos, const int hardbs = 1 )
    { return Sblock( Block::split( pos, hardbs ), _status ); }
  static bool isstatus( const int st ) throw()
    { return ( st == non_tried || st == non_trimmed || st == non_split ||
               st == bad_block || st == finished ); }
  };


class Domain
  {
  std::vector< Block > block_vector;	// blocks are ordered and don't overlap

public:
  Domain( const char * name, const long long p, const long long s );

  long long pos() const throw()
    { if( block_vector.size() ) return block_vector[0].pos(); else return 0; }

  long long size() const throw()
    {
    long long s = 0;
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      s += block_vector[i].size();
    return s;
    }

  bool operator<( const Block & b ) const throw()
    { return ( block_vector.size() && block_vector.back().end() <= b.pos() ); }

  long long breaks_block_by( const Block & b ) const throw()
    {
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      {
      const Block & db = block_vector[i];
      if( b.includes( db.pos() ) && b.pos() < db.pos() ) return db.pos();
      const long long end = db.end();
      if( b.includes( end ) && b.pos() < end ) return end;
      }
    return 0;
    }

  bool includes( const Block & b ) const throw()
    {
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      if( block_vector[i].includes( b ) ) return true;
    return false;
    }

  bool includes( const long long pos ) const throw()
    {
    for( unsigned int i = 0; i < block_vector.size(); ++i )
      if( block_vector[i].includes( pos ) ) return true;
    return false;
    }

  void clear() { block_vector.clear(); }
  void crop( const Block & b );
  bool crop_by_file_size( const long long isize );
  };
