#ifndef FGW_TEXT_H
#define FGW_TEXT_H

#include <cctype>
#include <climits>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

// fgw version 27/01/03
// modified to place everything in fgw namespace
// modified to restrict this to non-graphics features 
// 29/05/03

namespace fgw{
// and exception type
	class problem : public std::exception {
		std::string message;
	public:
		problem(std::string const & m):message(m){}
		std::string const & report()const{return message;}
		~problem()throw(){}
	};
// version to be used by read<> functions	 
class bad_input: public problem {
public:
	bad_input(std::string const & m):problem(m){}
};
	
// pi to 15 places	  
	double const pi = 3.141592653589793;
	
// conversions between degrees and radian measure
	inline double degrees(double rad){return rad*180/pi;}
	inline double radians(double deg){ return deg/180*pi;}

// clear out an input line
	inline void flush_cin(){
		std::cout << "\n***Clearing keyboard input***\n"
				<< "If you do not see 'DONE' press RETURN once.\n";
		while(true){
			int const c(std::cin.get());
			if(c == EOF or c == '\n')break;
		}
		std::cin.clear();
		std::cout << "DONE\n";
	}
	
// the following function assumes that the user wants that starts with a non-whitespace character

inline void getdata(std::istream & in, std::string & data){
	in >> std::ws;
	std::getline(in, data);
}


// deal with problem of opening a file with name in std::string
// in text mode
	inline void open_ifstream(std::ifstream & infile, std::string const & filename){
		return infile.open(filename.c_str());
	}
	inline void open_ofstream(std::ofstream & outfile, std::string const & filename){
		return outfile.open(filename.c_str());
	}
// in binary mode	
	inline void open_binary_ifstream(std::ifstream & infile, std::string const & filename){
		return infile.open(filename.c_str(), std::ios_base::binary);
	}
	inline void open_binary_ofstream(std::ofstream & outfile, std::string const & filename){
		return outfile.open(filename.c_str(), std::ios_base::binary);
	}
	
// get an answer in the form 'y' or 'n'
	inline bool yn_answer(){
		while(true) {
			char answer(std::toupper(std::cin.get()));
			if(answer == 'Y') return true;
			if(answer == 'N') return false;
		};
	}
	
	

// eats input till required character found
	inline bool match(std::istream & input, char c){
		input >> std::ws;
		return (input.get() == c)? true : false ;
	}

// generic input functions
// note to readers, the following use an advanced C++ technology
// called generic programming with templates. Writing code like this is 
// the territory of a library specialist.


// function to clear trailing whitespace to a carriage return

inline void eat_ws_to_eol(std::istream & in){
	while(true){
		int const c(in.peek());
		if(c == '\n' or std::isspace(char(c), in.getloc())) in.get();
		if(c == '\n' or not std::isspace(char(c), in.getloc())) return;
	}
}


namespace {
	int max_tries(3);
}

inline int reset_max_tries(int new_max){ 
	int const old(max_tries); 
	if(max_tries > 0) max_tries = new_max; 
	return old;
}
// These templates provide safe input for any type that supports operator>>
// 1) from console with prompt 


	template<typename in_type>
	in_type read(std::string const & prompt){
		in_type temp = in_type();
		int tries(0);
		while(tries++ != max_tries ){
			std::cout << prompt;
			std::cin >> temp;
			if(not std::cin.eof()) eat_ws_to_eol(std::cin);
			if(not std::cin.fail() or std::cin.eof()) return temp;
		std::cin.clear();	   // if it has failed, reset it to normal
			std::cin.ignore(INT_MAX, '\n');	   // flush cin
			std::cout << "\n That input was incorrect, try again: \n";
		}
		throw bad_input("Too many attempts to read data.");
	}
// 1b) provides a default value
	template<typename in_type>
	in_type read(std::string const & prompt, in_type value){
		in_type temp = value;
		int tries(0);
		while(tries++ != max_tries ){
			if(not std::cin.eof()) eat_ws_to_eol(std::cin);
			std::cout << prompt << "\nPress RETURN for default(" << value << ") " << std::flush;
			if(std::cin.peek() == '\n'){
				std::cin.get();
				return value;
			}
			std::cin >> temp;
			if(not std::cin.eof()) eat_ws_to_eol(std::cin);
			if(not std::cin.fail() or std::cin.eof()) return value;
		std::cin.clear();	   // if it has failed, reset it to normal
			std::cin.ignore(INT_MAX, '\n');	   // flush cin
			std::cout << "\n That input was incorrect, try again: \n";
		}
		throw bad_input("Too many attempts to read data.");
	}

// special case to discard x chars from cin first


	template<typename in_type>
	in_type read(int ignore_chars, std::string const & prompt){
		if(ignore_chars<0) ignore_chars = 0; // ignore negative values
		in_type temp = in_type();
		int tries(0);
		while(tries++ != max_tries ){
			std::cout << prompt;
			for(int i(0); i != ignore_chars; ++i) std::cin.get();
			ignore_chars = 0;	// do not ignore for retries
			std::cin >> temp;
			if(not std::cin.eof()) eat_ws_to_eol(std::cin);
			if(not std::cin.fail() or std::cin.eof()) return temp;
    		std::cin.clear();	   // if it has failed, reset it to normal
			std::cin.ignore(INT_MAX, '\n');	   // flush cin
			std::cout << "\n That input was incorrect, try again: \n";
		}
		throw bad_input("Too many attempts to read data.");
	}

// 2) version without prompt, defaults to colon space prompt	
	template<typename in_type>
	in_type read(){
		return read<in_type>(": ");
	}
	

// 2b) with default value
	template<typename in_type>
	in_type read(in_type value){
		return read<in_type>(": ", value);
	}

// 3) version for general input stream, throws an exception if fails
	template<typename in_type>
	in_type read(std::istream & in){
		in_type temp = in_type();
		in >> temp;
		if(in.fail() and not in.eof())
		         throw fgw::bad_input("Corrupted data in stream");
		if(not in.eof()) eat_ws_to_eol(in);
		return temp;
	}



}

#endif	  

