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

class Block
  {
  long long _pos, _size;		// size == -1 means undefined size

  static bool verify( const long long p, const long long s ) throw()
    { return ( p >= 0 && s >= -1 && ( s < 0 || p + s >= 0 ) ); }

public:
  Block() throw() : _pos( 0 ), _size( 0 ) {}
  Block( const long long p, const long long s ) throw();

  long long pos() const throw() { return _pos; }
  long long size() const throw() { return _size; }
  long long end() const throw() { return (_size < 0) ? -1 : _pos + _size; }

  void pos( const long long p ) throw();
  void size( const long long s ) throw();
  void end( const long long e ) throw();
  void align_pos( const int hardbs ) throw();
  void align_end( const int hardbs ) throw();
  void inc_size( const long long delta ) throw();

  bool operator<( const Block & b ) const throw() { return _pos < b._pos; }
  bool follows( const Block & b ) const throw()
    { return ( b._size >= 0 && b._pos + b._size == _pos ); }
  bool includes( const Block & b ) const throw()
    { return ( _pos <= b._pos &&
               ( _size < 0 || ( b._size >= 0 && _pos + _size >= b._pos + b._size ) ) ); }
  bool includes( const long long pos ) const throw()
    { return ( _pos <= pos && ( _size < 0 || _pos + _size > pos ) ); }

  bool join( const Block & b ) throw();
  void overlap( const Block & b ) throw();
  Block split( long long pos, const int hardbs = 1 ) throw();
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
  Sblock( const Block & b, const Status st ) throw();
  Sblock( const long long p, const long long s, const Status st ) throw();

  Status status() const throw() { return _status; }
  void status( const Status st ) throw();

  bool join( const Sblock & sb ) throw()
    { if( _status == sb._status ) return Block::join( sb ); else return false; }
  Sblock split( const long long pos, const int hardbs = 1 ) throw()
    { return Sblock( Block::split( pos, hardbs ), _status ); }
  static bool isstatus( const int st ) throw()
    { return ( st == non_tried || st == non_trimmed || st == non_split ||
               st == bad_block || st == finished ); }
  };


class Logbook
  {
public:
  enum Status
    { copying = '?', trimming = '*', splitting = '/', retrying = '-',
      finished = '+' };

private:
  long long _current_pos;
  Status _current_status;
  Block _domain;			// rescue domain
  char *iobuf_base, *_iobuf;		// iobuf is aligned to page and hardbs
  const char * const _filename;
  const int _hardbs, _softbs, _verbosity;
  const char * _final_msg;
  int _final_errno;
  mutable int _index;			// cached index of last find or change
  std::vector< Sblock > sblock_vector;	// note: blocks are consecutive

  bool read_logfile() throw();
  bool check_domain_size( const long long isize ) throw();
  void erase_sblock( const int i ) throw()
    { sblock_vector.erase( sblock_vector.begin() + i ); }
  void insert_sblock( const int i, const Sblock & sb ) throw()
    { sblock_vector.insert( sblock_vector.begin() + i, sb ); }

public:
  Logbook( const long long pos, const long long max_size,
           const long long isize, const char * name,
           const int cluster, const int hardbs,
           const int verbosity, const bool complete_only ) throw();
  ~Logbook() throw() { delete[] iobuf_base; }

  bool blank() const throw();
  void compact_sblock_vector() throw();
  void split_domain_border_sblocks() throw();
  bool update_logfile( const int odes, const bool force = false ) throw();

  long long current_pos() const throw() { return _current_pos; }
  Status current_status() const throw() { return _current_status; }
  const Block & domain() const throw() { return _domain; }
  const char *filename() const throw() { return _filename; }
  char * iobuf() const throw() { return _iobuf; }
  int hardbs() const throw() { return _hardbs; }
  int softbs() const throw() { return _softbs; }
  int verbosity() const throw() { return _verbosity; }
  const char * final_msg() const throw() { return _final_msg; }
  int final_errno() const throw() { return _final_errno; }

  void current_pos( const long long pos ) throw() { _current_pos = pos; }
  void current_status( Status st ) throw() { _current_status = st; }
  void final_msg( const char * const msg ) throw() { _final_msg = msg; }
  void final_errno( const int e ) throw() { _final_errno = e; }

  const Sblock & sblock( const int i ) const throw() { return sblock_vector[i]; }
  int sblocks() const throw() { return sblock_vector.size(); }
  void change_sblock_status( const int i, const Sblock::Status st ) throw()
    { sblock_vector[i].status( st ); }
  void truncate_vector( const long long pos ) throw();

  int find_index( const long long pos ) const throw();
  void find_chunk( Block & b, const Sblock::Status st ) const throw();
  void rfind_chunk( Block & b, const Sblock::Status st ) const throw();
  void change_chunk_status( const Block & b, const Sblock::Status st ) throw();

  static bool isstatus( const int st ) throw()
    { return ( st == copying || st == trimming || st == splitting ||
               st == retrying || st == finished ); }
  };


class Fillbook : public Logbook
  {
  long long filled_size;		// size already filled
  long long remaining_size;		// size to be filled
  int filled_areas;			// areas already filled
  int remaining_areas;			// areas to be filled
  int _odes;				// output file descriptor

  int fill_areas( const std::string & filltypes ) throw();
  int fill_block( const Block & b ) throw();
  void show_status( const long long opos, bool force = false ) throw();

public:
  Fillbook( const long long opos, const long long max_size,
            const char * name, const int cluster, const int hardbs,
            const int verbosity ) throw()
    : Logbook( opos, max_size, 0, name, cluster, hardbs, verbosity, true ) {}

  int do_fill( const int odes, const std::string & filltypes ) throw();
  bool read_buffer( const long long ipos, const int ides ) throw();
  };


class Rescuebook : public Logbook
  {
  long long offset;			// rescue offset (opos - ipos);
  long long sparse_size;		// end position of pending writes
  long long recsize, errsize;		// total recovered and error sizes
  int errors;				// error areas found so far
  int _ides, _odes;			// input and output file descriptors
  const int _max_errors, _max_retries;
  const bool _nosplit;
  const bool _sparse;

  bool sync_sparse_file() throw();
  int write_block_or_move( const int fd, const char * buf,
                           const int size, const long long pos ) throw();
  int copy_block( const Block & b, int & copied_size, int & error_size ) throw();
  void count_errors() throw();
  bool too_many_errors() const throw()
    { return ( _max_errors >= 0 && errors > _max_errors ); }
  int copy_and_update( const Block & b, const Sblock::Status st,
                       int & copied_size, int & error_size,
                       const char * msg, bool & first_post ) throw();
  int copy_non_tried() throw();
  int trim_errors() throw();
  int split_errors() throw();
  int copy_errors() throw();
  void show_status( const long long ipos, const char * msg = 0,
                    bool force = false ) throw();
public:
  Rescuebook( const long long ipos, const long long opos,
              const long long max_size, const long long isize,
              const char * name, const int cluster, const int hardbs,
              const int max_errors, const int max_retries, const int verbosity,
              const bool complete_only, const bool nosplit, const bool retrim,
              const bool sparse ) throw();

  long long rescue_opos() const throw() { return domain().pos() + offset; }
  int do_rescue( const int ides, const int odes ) throw();
  };


// Defined in ddrescue.cc
//
const char * format_num( long long num, long long max = 999999,
                         const int set_prefix = 0 ) throw();
void set_handler() throw();


// Defined in main.cc
//
void input_pos_error( const long long pos, const long long isize ) throw();
void internal_error( const char * msg ) throw() __attribute__ ((noreturn));
void show_error( const char * msg,
                 const int errcode = 0, const bool help = false ) throw();
void write_logfile_header( FILE * f ) throw();
