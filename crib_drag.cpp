
#include "osrng.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include "cryptlib.h"
#include "hex.h"
#include "filters.h"
#include "aes.h"
#include "ccm.h"
#include "assert.h"
#include <sstream>
#include <tuple>
#include <stack>
#include <string>
#include <iostream>
#include <sstream>

#include "crib_dictionary.cpp"

using namespace std;
using namespace CryptoPP;

string operator^( const string& arg_1, const string& arg_2 ) {

    string result;  
    if( arg_1.length() != arg_2.length() ) { 
      cout <<"Uneven strings!, arg_1 len "<< arg_1.length() <<", arg_2 len "<< arg_2.length() << endl;  
      return "";  
    }  

    for(size_t i=0; i<arg_1.length(); i++) result.append( 1, ((byte) arg_1[i])^((byte) arg_2[i]) );

    return result;  
}


class crib_drag {

  stack< tuple<string, string> > Q_main, Q_temp_store;
  const crib_dictionary_builder* cdb_ptr = nullptr; // pointer to crib dictionary
  string ciphA_xor_ciphB = "";
  string ciphA_xor_ciphB_base16 = "";
  void print_stack_state( const tuple<string, string>& top_state ) {  
      cout << "msg_A :"<< std::get<0>(top_state) << endl;
      cout << "msg_B :"<< std::get<1>(top_state) << endl;
  };
  const tuple<string, string>& use_current_state(); 
  void save_state( const string& msg_A, const string& msg_B );
  const tuple<string, string>& fetch_prev_state();
  const tuple<string, string>& fetch_next_state();
  void display_messages_A_B_XoredAB();// const string& ciphA_xor_ciphB );
  void process_messages( const vector<tuple<string, string, pair<int,int>, int>>& revealed_text_results ); 
  vector<tuple<string, string, pair<int,int>, int>> xor_phrase( const string & xored_texts, const string & phrase, 
                                                                const crib_dictionary_builder & cdb ); 

public:
  void print_menu(); 
  crib_drag( const string& ciphtext_A_base16, const string& ciphtext_B_base16, 
             const crib_dictionary_builder& cdb );
};

const tuple<string, string>& crib_drag::use_current_state() {  

  // Use top state of stack Q_main as current state. Erase elements of Q_temp_store
  while( !Q_temp_store.empty() ) Q_temp_store.pop(); 
  return Q_main.top(); 
} 


void crib_drag::save_state( const string& msg_A, const string& msg_B ) {

  cout <<"Saving messages A and B"<<endl; 
  Q_main.push( make_tuple( msg_A, msg_B ) );
}

// Fetches the previous states of msg_A,B, stores the current states in Q_temp_store
const tuple<string, string>& crib_drag::fetch_prev_state() {

  if( Q_main.empty() ) { cout<<"Error! No states present for messages A, B!!"<<endl; exit(0); }

  Q_temp_store.push( Q_main.top() ); // Q_main MUST have an entry constantly 
  Q_main.pop();

  if( Q_main.empty() ) { // We reached the first state stored in Q_main. Q_main must NOT be empty
    cout << "Reached earliest state for messages A, B"<<endl;   
    Q_main.push( Q_temp_store.top() ); // reclaiming back the only value that Q_main had 
    Q_temp_store.pop();
    return Q_main.top();
  } else { 
    return Q_main.top(); 
  }
}  

// Fetches the next state of msg_A,B
const tuple<string, string>& crib_drag::fetch_next_state() {

 if( Q_temp_store.empty() ) { cout<<"Reached latest state for messages A, B"<<endl; return Q_main.top(); }

 Q_main.push( Q_temp_store.top() ); 
 Q_temp_store.pop(); 
 return Q_main.top(); 
}


void crib_drag::display_messages_A_B_XoredAB() {// const string& ciphA_xor_ciphB ) {
 
  std::function<string (const size_t&)> create_ruler =
  [] ( const size_t& max_length ) -> string {  

    string ruler = ""; 
    for(size_t i=0; i<max_length/10+1; i++) {
      ruler += "|" + std::to_string(i*5);   
      while( ruler.length() % 10 != 0 ) ruler.append(" ");
    }
    return ruler;
  };

  const std::tuple<string, string>& msgs_tuple = Q_main.top();
  const string& msg_A = std::get<0>(msgs_tuple);  const string& msg_B = std::get<1>(msgs_tuple);

//  cout <<"ciph_A:\n" << ciph_msg_A << endl; 
  cout <<"Msg_A:"<<endl;  cout << create_ruler( 2*msg_A.length() ) << endl;
  for(size_t i=0; i<msg_A.length(); i++ ) cout << " "<<msg_A[i]; cout<<endl;

//  cout <<"ciph_B:\n" << ciph_msg_B << endl;
  cout <<"Msg_B:"<<endl;  cout << create_ruler( 2*msg_B.length() ) << endl;
  for(size_t i=0; i<msg_B.length(); i++ ) cout << " "<<msg_B[i]; cout<<endl;

  cout <<"XOR_A_B:\n" << ciphA_xor_ciphB_base16 << endl; 
}

 
vector<tuple<string, string, pair<int,int>, int>> crib_drag::xor_phrase( const string & xored_texts, 
                                                                         const string & phrase, 
                                                                         const crib_dictionary_builder & cdb ) { 

  string bytestring = xored_texts;
  vector<tuple<string, string, pair<int,int>, int>> revealed_text_results;

  cout <<"Examining phrase |"<< phrase <<"|" << endl; 
  for( size_t i=0; i< xored_texts.length() - phrase.length(); i++ ) { 
         
    string phrase_str; // constructing the string 0000000phrase000000
    for(size_t j=0;j<i;j++) phrase_str.append( 1, 0x00 );
    phrase_str.append( phrase );
    for(size_t j=i; j< xored_texts.length() - phrase.length(); j++) phrase_str.append( 1, 0x00 ); 

    string res = xored_texts ^ phrase_str;
    string revealed_text = res.substr( i, phrase.length() );

//    cout << revealed_text << " (in "<< i << "-"<< i + phrase.length()<<")" << endl; 

    std::function<int (const string&, bool )> process_revealed_text =
    [&] ( const string& revealed_text, bool is_beginning_of_word )-> int {

        size_t non_alphabetic_char_pos = 0;
        // string contains other characters than just letteres. This means that subwords are possibly included
        if( string::npos != ( non_alphabetic_char_pos = revealed_text.find_first_not_of( cdb.get_alphabet() ) ) ) {
        
          if( !is_beginning_of_word ) {
            auto& list_res = cdb.retrieve_possible_words_ending_with( revealed_text.substr( 0, non_alphabetic_char_pos ) );
            int score = 0; if( !list_res.empty() ) score++;

//            cout << "Recursing, using string "<< revealed_text.substr( non_alphabetic_char_pos+1, revealed_text.length() ) << endl;
            return score + process_revealed_text( revealed_text.substr( non_alphabetic_char_pos+1, revealed_text.length() ), true );
          } else { 
              
//             cout << "continuing process with !"<< revealed_text << "!, examine subword "
//                  << revealed_text.substr( 0, non_alphabetic_char_pos ) << endl;

             auto& list_res = cdb.retrieve_whole_words( revealed_text.substr( 0, non_alphabetic_char_pos ) );
             int score = 0; if( !list_res.empty() ) score++;

             return score + process_revealed_text( revealed_text.substr( non_alphabetic_char_pos+1, revealed_text.length() ), true );
          }
        } else { // string revealed_text contains only alphabetic characters 

          if( !is_beginning_of_word ) {
            auto& list_res = cdb.retrieve_possible_words( revealed_text ); 
            int score = 0; if( !list_res.empty() ) score++; return score;

          } else { 
            auto& list_res = cdb.retrieve_possible_words_starting_with( revealed_text );
            int score = 0; if( !list_res.empty() ) score++; return score;
          } 
        }
    };

    int score = process_revealed_text( revealed_text, false ); // store the result of this iteration step to score    
    cout << revealed_text << " (in "<< i << "-"<< i + phrase.length()<<")" << " achieved score "<< score << endl; 
    if(score > 0) revealed_text_results.push_back( make_tuple(phrase, revealed_text, make_pair(i, i + phrase.length()-1), score) );
  }
  // sort the results of the entire loop based on each step's score. Highest score goes first. 
  std::sort( revealed_text_results.begin(), revealed_text_results.end(), 
             []( tuple<string, string, pair<int,int>, int> tup_a, tuple<string, string, pair<int,int>, int> tup_b )-> bool {       
                   
                 //if( std::get<3>(tup_a) != std::get<3>(tup_b) ) return std::get<3>(tup_a) > std::get<3>(tup_b);
                 //else 
                 return std::get<2>(tup_a) < std::get<2>(tup_b);
             }
  );

  return revealed_text_results;
}

// argument tuple contains: used phrase, revealed text, begin-end indexes of revealed text, score of revealed text
void crib_drag::process_messages( const vector<tuple<string, string, pair<int,int>, int>>& revealed_text_results ) {

  if( revealed_text_results.empty() ) { cout <<"No new text revealed"<<endl; return; }

  size_t result_index = 0;
  for( const tuple<string, string, pair<int,int>, int>& tup : revealed_text_results ) {
    cout << "("<<result_index<<") using |"<<std::get<0>(tup)<<"| : reveraled text: "<< std::get<1>(tup) << " at indexes "
         << std::get<2>(tup).first <<"-"<< std::get<2>(tup).second << " with score " << std::get<3>(tup) << endl;   
    result_index++;
  }

  size_t result_choice = -1; string res_string;
  while( 1 ) { // endless loop, until correct input is given  
    cout << "Type the index of the revealed text you want to use, or -1 to leave (choose nothing) :";
    std::getline(std::cin, res_string);  result_choice = std::stoi(res_string);
    
    if( result_choice == -1 ) return;
    else if( result_choice < revealed_text_results.size() && result_choice >= 0 ) {
  
      print_stack_state( use_current_state() ); // store current state of messages A, B to msg_A, msg_B 
      const std::tuple<string, string>& msgs_tuple = use_current_state(); 
      string msg_A = std::get<0>(msgs_tuple);  string msg_B = std::get<1>(msgs_tuple);

      string msg_choice = "f";
      cout << "Type a to write |"<< std::get<0>(revealed_text_results[result_choice]) 
           << "| to message A, b to write it to message B :"; 
      while( msg_choice != "a" && msg_choice != "b" ) std::getline( std::cin, msg_choice ); // answer must be a or b only!

      if( msg_choice == "a" ) {
        msg_A.replace( std::get<2>(revealed_text_results[result_choice]).first, 
                       std::get<0>(revealed_text_results[result_choice]).length(), 
                       std::get<1>(revealed_text_results[result_choice]) ); 
        msg_B.replace( std::get<2>(revealed_text_results[result_choice]).first, 
                       std::get<0>(revealed_text_results[result_choice]).length(), 
                       std::get<0>(revealed_text_results[result_choice]) );  
      } else if( msg_choice == "b" ) {
        msg_B.replace( std::get<2>(revealed_text_results[result_choice]).first, 
                       std::get<0>(revealed_text_results[result_choice]).length(),
                       std::get<1>(revealed_text_results[result_choice]) ); 
        msg_A.replace( std::get<2>(revealed_text_results[result_choice]).first, 
                       std::get<0>(revealed_text_results[result_choice]).length(), 
                       std::get<0>(revealed_text_results[result_choice]) );
      }

      cout <<"Are you sure about this? Y for Yes, any other key for No :"; std::getline( std::cin, res_string );

      if( res_string != "Y" ) { cout<<"Not storing any changes to messages A, B"<<endl; }
      else save_state( msg_A, msg_B ); // add a new state for messages A, B
    }
  }

}  


void crib_drag::print_menu() { 

  bool loop_var = true;  
  while( loop_var ) {

    cout<<"*** (1) Show messages A, B ***"<<endl;
    cout<<"*** (2) Use most frequent words ***"<<endl; 
    cout<<"*** (3) Type a phrase/word to examine ***"<<endl;
    cout<<"*** (4) Use the dictionary to find incomplete words ***" <<endl;
    cout<<"*** (5) Use a previous state of messsages A, B ***"<<endl;
    cout<<"*** (6) Quit ***"<<endl;

    size_t choice = 0; string str_choice; 
    while( choice > 6 || choice < 1 ) { std::getline( std::cin, str_choice ); choice = std::stoi( str_choice ); }

    switch( choice ) {
      case 1 : { 
        display_messages_A_B_XoredAB();
        break; 
      }
      case 2 : { 

        for( const string& word : cdb_ptr-> get_frequently_used_words() ) {
 
          const auto& res_1 = xor_phrase( ciphA_xor_ciphB, word , *cdb_ptr );  
          const auto& res_2 = xor_phrase( ciphA_xor_ciphB, " "+word , *cdb_ptr ); 
          const auto& res_3 = xor_phrase( ciphA_xor_ciphB, word+" " , *cdb_ptr ); 
          const auto& res_4 = xor_phrase( ciphA_xor_ciphB, " "+word+" " , *cdb_ptr );
       
          std::function<void ( vector<tuple<string, string, pair<int,int>, int>>& )> sorter =
          []( vector<tuple<string, string, pair<int,int>, int>>& res ) {

            std::sort( res.begin(), res.end(), []( tuple<string, string, pair<int,int>, int> tup_a, 
                                                   tuple<string, string, pair<int,int>, int> tup_b )-> bool {         
                                                      return std::get<2>(tup_a) < std::get<2>(tup_b);
                                               });
          };

          std::function<const vector<tuple<string, string, pair<int,int>, int>> 
                         (const vector<tuple<string, string, pair<int,int>, int>>&, 
                          const vector<tuple<string, string, pair<int,int>, int>>& )> intersector =
          [&]( const vector<tuple<string, string, pair<int,int>, int>>& res_a, 
               const vector<tuple<string, string, pair<int,int>, int>>& res_b ) {

            vector<tuple<string, string, pair<int,int>, int>> intersect_1(res_a.size()), 
                                                              intersect_2(res_b.size()), all_results;

            // gets elements of res_a that have the same pair.first value as elements of res_b
            vector<tuple<string, string, pair<int,int>, int>>::iterator it = 
            std::set_intersection( res_a.begin(), res_a.end(), res_b.begin(), res_b.end(), intersect_1.begin(),
                                   []( const tuple<string, string, pair<int,int>, int>& tuple_a, 
                                       const tuple<string, string, pair<int,int>, int>& tuple_b )-> bool {
                                         return std::get<2>(tuple_a).first < std::get<2>(tuple_b).first;
                                   });
            // gets elements of res_b that have the same pair.first value as elements of res_a
            vector<tuple<string, string, pair<int,int>, int>>::iterator it_2 = 
            std::set_intersection( res_b.begin(), res_b.end(), res_a.begin(), res_a.end(), intersect_2.begin(),
                                   []( const tuple<string, string, pair<int,int>, int>& tuple_a, 
                                       const tuple<string, string, pair<int,int>, int>& tuple_b )-> bool {
                                         return std::get<2>(tuple_a).first < std::get<2>(tuple_b).first;
                                   });
            // Write both results to all_results, return it
            intersect_1.resize( it - intersect_1.begin() );  intersect_2.resize( it_2 - intersect_2.begin() ); 
            std::move( intersect_1.begin(), intersect_1.end(), std::inserter( all_results, all_results.end()));
            std::move( intersect_2.begin(), intersect_2.end(), std::inserter( all_results, all_results.end()));
            sorter( all_results ); 
            return all_results; 
          };

          const vector<tuple<string, string, pair<int,int>, int>> all_results_a = intersector( res_1, res_3 ); 
          const vector<tuple<string, string, pair<int,int>, int>> all_results_b = intersector( res_2, res_4 ); 

          vector<tuple<string, string, pair<int,int>, int>> all_results;
          std::move(all_results_a.begin(), all_results_a.end(), std::inserter( all_results, all_results.end()));
          std::move(all_results_b.begin(), all_results_b.end(), std::inserter( all_results, all_results.end()));
          sorter( all_results );

          process_messages( all_results );
          display_messages_A_B_XoredAB();
        }  

        break; 
      }
      case 3 : {  
        string phrase;
        cout << "   Type the phrase you want to examine :"; 
        std::getline( std::cin, phrase ); 

        const auto& revealed_text_results = xor_phrase( ciphA_xor_ciphB, phrase , *cdb_ptr ); 
        process_messages( revealed_text_results );
        display_messages_A_B_XoredAB();
        break;
      } 
      case 4 : { 
      
        
        break; 
      }         
      case 5 : { 
        cout << "Current state :"<<endl;  print_stack_state( use_current_state() ); 
        string state_choice = "";
        while( state_choice != "U" ) {

          cout<<"   Key < leads to a later state, key > leads to a previous state, U selects state :"<<endl;
          std::getline( std::cin, state_choice );
          if( state_choice == "<" ) print_stack_state( fetch_next_state() ); 
          else if( state_choice == ">" ) print_stack_state( fetch_prev_state() );
          else if( state_choice == "U" ) print_stack_state( use_current_state() );
        }
 
        cout << "Using state :"<<endl;  print_stack_state( use_current_state() );
        break; 
      }
      case 6 : { loop_var = false; break; }
    }
  }

}


crib_drag::crib_drag( const string& ciphtext_A_base16, const string& ciphtext_B_base16,
                      const crib_dictionary_builder& cdb ) {

  string msg_A, msg_B;
  string ciphtext_A, ciphtext_B;

  // for ASCII encodings. WARNING! Here, lengths are same, FIX THIS!  
  msg_A.resize( ciphtext_A_base16.length()/2, '#');    
  msg_B.resize( ciphtext_B_base16.length()/2, '#');    

  Q_main.push( make_tuple( msg_A, msg_B ) ); // populate the message A,B stack with its first state

  // The strings we got as input are strings, representing HEX values. We use the HexDecoder to convert them
  // from base 16 values to byte values. 2 HEX digits represent a byte.  
  StringSource( ciphtext_A_base16, true /*pump all*/, new HexDecoder( new StringSink(ciphtext_A) ) );
  StringSource( ciphtext_B_base16, true /*pump all*/, new HexDecoder( new StringSink(ciphtext_B) ) );

  ciphA_xor_ciphB = ciphtext_A ^ ciphtext_B;
  // convert XORed ciphertexts to byte16 representation. 
  StringSource( ciphA_xor_ciphB, true /*pump all*/, new HexEncoder( new StringSink( ciphA_xor_ciphB_base16 ) ) );

  this->cdb_ptr = &cdb;

}


int main( int argc, char* argv[] ) {


  crib_dictionary_builder cdb( argv[1], EN );

  string ciph_A_base16 = argv[2];   
  string ciph_B_base16 = argv[3]; 

  crib_drag cd( ciph_A_base16, ciph_B_base16, cdb );

  cd.print_menu( ); 

}


