
// g++ -g  -DNDEBUG -std=c++11  -I/usr/include/cryptopp -I. -L.  ciph_process.cpp  -o ciph_process -lcryptopp -lpthread

#include <iostream>
#include <unordered_map>
#include <string>
#include <list>
#include <fstream>
#include <locale>

#include "languages.cpp"

using namespace std;

const size_t SUBWORD_LENGTH = 2;
const list<string> empty_wordlist = { };

class crib_dictionary_builder {

  string alphabet; 
  vector<string> frequently_used_words;
  unordered_map< string, list< string >> uo_m;  
  const string to_lowercase( const string& input ) const;
public:
  void store_word( const string& word ); 
  // convert all sub_word, word strings to lowercase, since all strings stored in the hash table 
  // are also in lowercase.
  const list<string> & retrieve_possible_words( const string& sub_word ) const;
  const list<string> retrieve_possible_words_ending_with( const string& sub_word ) const;
  const list<string> retrieve_possible_words_starting_with( const string& sub_word ) const;
  const list<string> retrieve_whole_words( const string& word ) const;
  crib_dictionary_builder( char* input_file, language lng ); 
  crib_dictionary_builder( void ) { ; }
  const string& get_alphabet(void) const { return alphabet; }
  const vector<string>& get_frequently_used_words() const { return frequently_used_words;  }

};


const string crib_dictionary_builder::to_lowercase( const string& input ) const {

  string copy = input;  
  std::transform(copy.begin(), copy.end(), copy.begin(), ::tolower);  
  return copy;
}


crib_dictionary_builder::crib_dictionary_builder( char* input_file, language lng ) {

  ifstream my_filestream;
  string fetched_line = "";
  alphabet = used_alphabet( lng );
  frequently_used_words = fetch_most_frequently_words( lng );

  my_filestream.open( input_file, ios::in );
  while( std::getline( my_filestream, fetched_line ) ) {

     cout << fetched_line << endl; 
     store_word( fetched_line ); // store all subwords 
  }

  my_filestream.close();
  cout << endl;
}


void crib_dictionary_builder::store_word( const string& word ) {

  for( size_t sub_len = SUBWORD_LENGTH; sub_len < word.length(); sub_len++ )  
      [&] ( size_t & sub_len, const string & word ) -> void {
          for( size_t i = 0; i <= word.length() - sub_len; i++ ) 
            uo_m[ word.substr( i, sub_len ) ].push_front( word );     
      } ( sub_len, word );  
 
  uo_m[ word ].push_front( word ); // store the entire word as well 
}  


const list<string> crib_dictionary_builder::retrieve_possible_words_ending_with( const string& sub_word ) const {

  if( sub_word.length() == 0 || sub_word.length() == 1 ) return empty_wordlist;

  cout << "Possible words ending with "<< sub_word << " : ";

  auto res = uo_m.find( to_lowercase( sub_word ) ); // returns the pair key,linked_list from unordered_map

  if( res == uo_m.end() ) { /*cout<<endl;*/ return empty_wordlist; } 
 
  list<string> result = {}; 
  for( auto word : res->second ) { // fetching all words that contain sub_word from the hash table
   
    size_t idx_res = 0; 
    if( string::npos != (idx_res = word.rfind( to_lowercase( sub_word ) )) && idx_res + sub_word.length() == word.length() ) {   
      cout << word <<" ";  result.push_front( word ); 
    }
  } cout << endl;  

  return result;
}  


const list<string> crib_dictionary_builder::retrieve_possible_words_starting_with( const string& sub_word ) const {

  if( sub_word.length() == 0 || sub_word.length() == 1 ) return empty_wordlist;

  cout << "Possible words starting with "<< sub_word << " : ";

  auto res = uo_m.find( to_lowercase( sub_word ) ); // returns the pair key,linked_list from unordered_map

  if( res == uo_m.end() ) { /*cout<<endl;*/ return empty_wordlist; } 
 
  list<string> result = {}; 
  for( auto word : res->second ) { // fetching all words that contain sub_word from the hash table
   
    size_t idx_res = 0; 
    if( string::npos != (idx_res = word.find( to_lowercase( sub_word ) )) && idx_res == 0 ) {  
    
      cout << word <<" ";  result.push_front( word );  
    }
  } cout << endl;  

  return result;
}


const list<string> crib_dictionary_builder::retrieve_whole_words( const string& word ) const {

  if( word.length() == 0 ) return empty_wordlist;

  auto res = uo_m.find( to_lowercase( word ) ); // returns the pair key,linked_list from unordered_map

  if( res == uo_m.end() ) { /*cout<<endl;*/ return empty_wordlist; } 

  for( const auto & fetched_word : res->second ) { 
    if( fetched_word.compare( word ) == 0 ) {  
      cout << "Found whole word stored : "<< word << endl; 
      return list<string>() = { word }; 
    }
  } // cout << word <<" "; cout << endl;

  return empty_wordlist;
}


const list<string> & crib_dictionary_builder::retrieve_possible_words( const string& sub_word ) const { 

  if( sub_word.length() == 0 ) return empty_wordlist;

  cout << "Possible words containing "<< sub_word << " : ";

  auto res = uo_m.find( to_lowercase( sub_word ) ); // returns the pair key,linked_list from unordered_map

  if( res == uo_m.end() ) { /*cout<<endl;*/ return empty_wordlist; } 

  for( auto word : res->second ) cout << word <<" "; cout << endl;

  return res->second;
}


