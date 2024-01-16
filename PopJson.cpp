#include "PopJson.hpp"
#include <string>
#include <charconv>
#include <sstream>



void PopJson::UnitTest()
{
	{
		auto Json = R"JSON( {"Array":[0,1,2,3]} )JSON";
		Value_t Data( Json );
		
		auto ArrayData = Data.GetValue("Array",Json);
		if ( ArrayData.GetType() != ValueType_t::Array )
			throw std::runtime_error(".Array not an array");
		auto Value0 = ArrayData.GetValue( 0, Json ).GetInteger(Json);
		auto Value1 = ArrayData.GetValue( 1, Json ).GetInteger(Json);
		auto Value2 = ArrayData.GetValue( 2, Json ).GetInteger(Json);
		auto Value3 = ArrayData.GetValue( 3, Json ).GetInteger(Json);
	}
	
	{
		auto Json = R"JSON( {"Array":[{ }]} )JSON";
		Value_t Data( Json );
		
		auto ArrayData = Data.GetValue("Array",Json);
		if ( ArrayData.GetType() != ValueType_t::Array )
			throw std::runtime_error(".Array not an array");
		auto Value0 = ArrayData.GetValue( 0, Json );
		if ( Value0.GetType() != ValueType_t::Object )
			throw std::runtime_error(".Array[0] not an object");
	}
	


	
}


static inline std::string esc(char c)
{
	char buf[12];
	if (static_cast<uint8_t>(c) >= 0x20 && static_cast<uint8_t>(c) <= 0x7f) {
		snprintf(buf, sizeof buf, "'%c' (%d)", c, c);
	} else {
		snprintf(buf, sizeof buf, "(%d)", c);
	}
	return std::string(buf);
}

//	input is the second part of \X
static char GetEscapedChar(char SlashChar)
{
	switch ( SlashChar )
	{
		case 'b':	return '\b';
		case 'f':	return '\f';
		case 'n':	return '\n';
		case 'r':	return '\r';
		case 't':	return '\t';
			
		case '"':
		case '\\':
		case '/':
			return SlashChar;
			
		default:
			break;
	}

	throw std::runtime_error("invalid escape character " + esc(SlashChar));
}

static inline bool in_range(long x, long lower, long upper) {
	return (x >= lower && x <= upper);
}

bool in_rangeHex(long x)
{
	if ( !in_range( x, 'a', 'f') )
		return false;
	if ( !in_range( x, 'A', 'F') )
		return false;
	if ( !in_range( x, '0', '9') )
		return false;
	return true;
}


static const int max_depth = 200;

//	JsonParser stolen from dropbox/json11
struct JsonParser final
{
	JsonParser(std::string_view InputJson,bool AllowComments=false) :
		str				( InputJson ),
		AllowComments	( AllowComments )
	{
	}

    /* State
     */
    std::string_view str;	//	input
    size_t i = 0;				//	parsing position
	bool AllowComments = false;		//	allow json with comments

	/* consume_whitespace()
  *
  * Advance until the current character is non-whitespace.
  */
 void consume_whitespace() {
	 while (str[i] == ' ' || str[i] == '\r' || str[i] == '\n' || str[i] == '\t')
		 i++;
 }

    /* consume_comment()
     *
     * Advance comments (c-style inline and multiline).
     */
    bool consume_comment()
	{
      bool comment_found = false;
      if (str[i] == '/') {
        i++;
        if (i == str.size())
          throw std::runtime_error("unexpected end of input after start of comment");
		  
        if (str[i] == '/') { // inline comment
          i++;
          // advance until next line, or end of input
          while (i < str.size() && str[i] != '\n') {
            i++;
          }
          comment_found = true;
        }
        else if (str[i] == '*') { // multiline comment
          i++;
          if (i > str.size()-2)
			  throw std::runtime_error("unexpected end of input inside multi-line comment");
			
          // advance until closing tokens
          while (!(str[i] == '*' && str[i+1] == '/')) {
            i++;
            if (i > str.size()-2)
				throw std::runtime_error("unexpected end of input inside multi-line comment");
          }
          i += 2;
          comment_found = true;
        }
        else
			throw std::runtime_error("malformed comment");
      }
      return comment_found;
    }

    /* consume_garbage()
     *
     * Advance until the current character is non-whitespace and non-comment.
     */
    void consume_garbage() {
      consume_whitespace();
      if ( AllowComments )
	  {
        bool comment_found = false;
        do {
          comment_found = consume_comment();
          consume_whitespace();
        }
        while(comment_found);
      }
    }

    /* get_next_token()
     *
     * Return the next non-whitespace character. If the end of the input is reached,
     * flag an error and return 0.
     */
    char get_next_token()
	{
        consume_garbage();
        if (i == str.size())
            throw std::runtime_error("unexpected end of json");

        return str[i++];
    }

    /* encode_utf8(pt, out)
     *
     * Encode pt as UTF-8 and add it to out.
     */
    void encode_utf8(long pt, std::string & out) {
        if (pt < 0)
            return;

        if (pt < 0x80) {
            out += static_cast<char>(pt);
        } else if (pt < 0x800) {
            out += static_cast<char>((pt >> 6) | 0xC0);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        } else if (pt < 0x10000) {
            out += static_cast<char>((pt >> 12) | 0xE0);
            out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        } else {
            out += static_cast<char>((pt >> 18) | 0xF0);
            out += static_cast<char>(((pt >> 12) & 0x3F) | 0x80);
            out += static_cast<char>(((pt >> 6) & 0x3F) | 0x80);
            out += static_cast<char>((pt & 0x3F) | 0x80);
        }
    }

    /* parse_string()
     *
     * Parse a string, starting at the current position.
     */
	PopJson::Value_t parse_string()
	{
        std::string out;
        long last_escaped_codepoint = -1;
		auto StartPosition = i;
		
        while (true)
		{
			if (i == str.size())
				throw std::runtime_error("unexpected end of input in string");

			char ch = str[i++];

			if (ch == '"')
			{
				encode_utf8(last_escaped_codepoint, out);
				//return out;
				auto End = i-1;
				return PopJson::Value_t( PopJson::ValueType_t::String, StartPosition, End-StartPosition );
			}

            if (in_range(ch, 0, 0x1f))
				throw std::runtime_error("unescaped " + esc(ch) + " in string");

            // The usual case: non-escaped characters
            if (ch != '\\')
			{
                encode_utf8(last_escaped_codepoint, out);
                last_escaped_codepoint = -1;
                out += ch;
                continue;
            }

            // Handle escapes
            if ( i == str.size() )
                throw std::runtime_error("unexpected end of input in string");

            ch = str[i++];

            if (ch == 'u')
			{
                // Extract 4-byte escape sequence
                auto esc = str.substr(i, 4);
                // Explicitly check length of the substring. The following loop
                // relies on std::string returning the terminating NUL when
                // accessing str[length]. Checking here reduces brittleness.
                if (esc.length() < 4)
					throw std::runtime_error("bad \\u escape: " + std::string(esc));

				for (size_t j = 0; j < 4; j++)
				{
					if ( !in_rangeHex(esc[j]) )
						throw std::runtime_error("bad \\u escape: " + std::string(esc));
                }

                long codepoint = strtol( std::string(esc).data(), nullptr, 16);

                // JSON specifies that characters outside the BMP shall be encoded as a pair
                // of 4-hex-digit \u escapes encoding their surrogate pair components. Check
                // whether we're in the middle of such a beast: the previous codepoint was an
                // escaped lead (high) surrogate, and this is a trail (low) surrogate.
                if (in_range(last_escaped_codepoint, 0xD800, 0xDBFF)
                        && in_range(codepoint, 0xDC00, 0xDFFF)) {
                    // Reassemble the two surrogate pairs into one astral-plane character, per
                    // the UTF-16 algorithm.
                    encode_utf8((((last_escaped_codepoint - 0xD800) << 10)
                                 | (codepoint - 0xDC00)) + 0x10000, out);
                    last_escaped_codepoint = -1;
                } else {
                    encode_utf8(last_escaped_codepoint, out);
                    last_escaped_codepoint = codepoint;
                }

                i += 4;
                continue;
            }

            encode_utf8(last_escaped_codepoint, out);
            last_escaped_codepoint = -1;

            if (ch == 'b') {
                out += '\b';
            } else if (ch == 'f') {
                out += '\f';
            } else if (ch == 'n') {
                out += '\n';
            } else if (ch == 'r') {
                out += '\r';
            } else if (ch == 't') {
                out += '\t';
            } else if (ch == '"' || ch == '\\' || ch == '/') {
                out += ch;
            } else {
				throw std::runtime_error("invalid escape character " + esc(ch));
            }
        }
    }

	PopJson::Value_t parse_string_faster(size_t WritePositionOffset)
	{
		long last_escaped_codepoint = -1;
		auto StartPosition = i;
		
		while (true)
		{
			if (i == str.size())
				throw std::runtime_error("unexpected end of input in string");

			char ch = str[i++];

			if (ch == '"')
			{
				auto End = i-1;
				return PopJson::Value_t( PopJson::ValueType_t::String, StartPosition+WritePositionOffset, End-StartPosition );
			}

			if (in_range(ch, 0, 0x1f))
				throw std::runtime_error("unescaped " + esc(ch) + " in string");

			// The usual case: non-escaped characters
			if (ch != '\\')
			{
				last_escaped_codepoint = -1;
				continue;
			}

			// Handle escapes
			if ( i == str.size() )
				throw std::runtime_error("unexpected end of input in string");

			ch = str[i++];

			if (ch == 'u')
			{
				// Extract 4-byte escape sequence
				auto esc = str.substr(i, 4);
				// Explicitly check length of the substring. The following loop
				// relies on std::string returning the terminating NUL when
				// accessing str[length]. Checking here reduces brittleness.
				if (esc.length() < 4)
					throw std::runtime_error("bad \\u escape: " + std::string(esc));

				for (size_t j = 0; j < 4; j++)
				{
					if ( !in_rangeHex(esc[j]) )
						throw std::runtime_error("bad \\u escape: " + std::string(esc));
				}

				long codepoint = strtol( std::string(esc).data(), nullptr, 16);

				// JSON specifies that characters outside the BMP shall be encoded as a pair
				// of 4-hex-digit \u escapes encoding their surrogate pair components. Check
				// whether we're in the middle of such a beast: the previous codepoint was an
				// escaped lead (high) surrogate, and this is a trail (low) surrogate.
				if (in_range(last_escaped_codepoint, 0xD800, 0xDBFF)
						&& in_range(codepoint, 0xDC00, 0xDFFF)) {
					// Reassemble the two surrogate pairs into one astral-plane character, per
					// the UTF-16 algorithm.
					last_escaped_codepoint = -1;
				} else {
					last_escaped_codepoint = codepoint;
				}

				i += 4;
				continue;
			}

			last_escaped_codepoint = -1;
			//	will throw if not valid
			auto EscapedChar = GetEscapedChar(ch);
		}
	}

	PopJson::Value_t parse_number(size_t WritePositionOffset)
	{
		size_t start_pos = i;

		if (str[i] == '-')
			i++;

		//	Integer part
		if (str[i] == '0')
		{
			i++;
			if (in_range(str[i], '0', '9'))
				throw std::runtime_error("leading 0s not permitted in numbers");
			
		}
		else if (in_range(str[i], '1', '9'))
		{
			i++;
			while (in_range(str[i], '0', '9'))
				i++;
		}
		else
		{
			throw std::runtime_error("invalid " + esc(str[i]) + " in number");
		}

		bool IsDecimal = str[i] == '.';
		{
			bool IsExponentChar = str[i] == 'e' || str[i] == 'E';
			if ( !IsDecimal && !IsExponentChar && (i - start_pos) <= static_cast<size_t>(std::numeric_limits<int>::digits10))
			{
				PopJson::Value_t Value( PopJson::ValueType_t::NumberInteger, start_pos + WritePositionOffset, i-start_pos );
				//return std::atoi(str.c_str() + start_pos);
				return Value;
			}
		}
		
		//	verify decimal part
		if ( IsDecimal )
		{
			i++;
			if (!in_range(str[i], '0', '9'))
				throw std::runtime_error("at least one digit required in fractional part");

			while (in_range(str[i], '0', '9'))
				i++;
		}

		//	verify exponent part
		if (str[i] == 'e' || str[i] == 'E')
		{
			i++;

			if (str[i] == '+' || str[i] == '-')
				i++;

			if (!in_range(str[i], '0', '9'))
				throw std::runtime_error("at least one digit required in exponent");

			while (in_range(str[i], '0', '9'))
				i++;
		}

		PopJson::Value_t Value( PopJson::ValueType_t::NumberDouble, start_pos + WritePositionOffset, i-start_pos );
		//return std::strtod(str.c_str() + start_pos, nullptr);
		return Value;
	}

    /* expect(str, res)
     *
     * Expect that 'str' starts at the character that was just read. If it does, advance
     * the input and return res. If not, flag an error.
     */
	PopJson::Value_t expect(std::string_view expected,PopJson::ValueType_t::Type ResultType,size_t WritePositionOffset)
	{
		if ( i <= 0 )
			throw std::runtime_error("Bad position");
		
		i--;
		auto Slice = str.substr( i, expected.length() );
		//if ( str.compare(i, expected.length(), expected) != 0)
		if ( Slice != expected )
		{
			throw std::runtime_error("parse error: expected " + std::string(expected) + ", got " + std::string(Slice) );
		}
		
		PopJson::Value_t Result( ResultType, i+WritePositionOffset, expected.length() );
		i += expected.length();
		return Result;
	}


	PopJson::Value_t parse_json(int depth,size_t WritePositionOffset)
	{
		const bool IncludeStartAndEndTokensInValue = false;
		
		if (depth > max_depth)
			throw std::runtime_error("exceeded maximum nesting depth");

		char ch = get_next_token();

		if (ch == '-' || (ch >= '0' && ch <= '9'))
		{
			i--;
			return parse_number(WritePositionOffset);
		}

		if (ch == 't')
			return expect("true", PopJson::ValueType_t::BooleanTrue, WritePositionOffset );

		if (ch == 'f')
			return expect("false", PopJson::ValueType_t::BooleanFalse, WritePositionOffset );

		if (ch == 'n')
			return expect("null", PopJson::ValueType_t::Null, WritePositionOffset );

		if (ch == '"')
			return parse_string_faster(WritePositionOffset);

		if (ch == '{')
		{
			auto StartPosition = i;
			
			ch = get_next_token();
			
			std::vector<PopJson::Node_t> Nodes;
			if (ch != '}')
			{
				while (1)
				{
					if (ch != '"')
						throw std::runtime_error("expected '\"' in object, got " + esc(ch));
					
					auto Key = parse_string_faster(WritePositionOffset);
					
					ch = get_next_token();
					if (ch != ':')
						throw std::runtime_error("expected ':' in object, got " + esc(ch));
					
					auto Value = parse_json( depth + 1, WritePositionOffset );
					
					Nodes.push_back( PopJson::Node_t( Key, Value ) );
					
					ch = get_next_token();
					if (ch == '}')
						break;
					if (ch != ',')
						throw std::runtime_error("expected ',' in object, got " + esc(ch));
					
					ch = get_next_token();
				}
			}

			auto ValueStart = StartPosition;
			auto ValueLength = i-StartPosition-1;
			auto ValueRaw = str.substr( ValueStart, ValueLength );
			PopJson::Value_t Object( PopJson::ValueType_t::Object, ValueStart+WritePositionOffset, ValueLength );
			Object.mNodes = std::move(Nodes);
			return Object;
		}

		if (ch == '[')
		{
			auto StartPosition = i;
			
			ch = get_next_token();
			if (ch == ']')
			{
				PopJson::Value_t Array( PopJson::ValueType_t::Array, StartPosition+WritePositionOffset, i );
				return Array;
			}

			std::vector<PopJson::Node_t> Nodes;
			while ( true )
			{
				i--;

				auto Value = parse_json(depth + 1, WritePositionOffset);
				
				PopJson::Node_t Node(Value);
				Nodes.push_back(Node);

				ch = get_next_token();
				if (ch == ']')
					break;
				if (ch != ',')
					throw std::runtime_error("expected ',' in list, got " + esc(ch));

				ch = get_next_token();
			}
			
			auto ValueStart = StartPosition;
			auto ValueLength = i-StartPosition-1;
			auto ValueRaw = str.substr( ValueStart, ValueLength );
			
			PopJson::Value_t Array( PopJson::ValueType_t::Array, ValueStart+WritePositionOffset, ValueLength );
			Array.mNodes = std::move(Nodes);
			return Array;
		}

		throw std::runtime_error("expected value, got " + esc(ch));
	}
};


PopJson::Value_t::Value_t(std::string_view Json,size_t WritePositionOffset)
{
	bool AllowComments = false;
	JsonParser parser( Json, AllowComments );
	auto Root = parser.parse_json( 0, WritePositionOffset );
	*this = Root;
}

std::string PopJson::Value_t::GetString(std::string_view JsonData)
{
	auto EscapedString = GetRawString(JsonData);
	//	todo: decode escaped
	return std::string(EscapedString);
}

std::string_view PopJson::Value_t::GetString(std::string& Buffer,std::string_view JsonData)
{
	auto EscapedString = GetRawString(JsonData);
	//	todo: decode escaped
	return EscapedString;
}


int PopJson::Value_t::GetInteger(std::string_view JsonData)
{
	if ( mType != PopJson::ValueType_t::NumberInteger )
		throw std::runtime_error("todo: conversion of value to integer");
	
	auto ValueString = GetRawString( JsonData );

	int Value = 0;
	auto result = std::from_chars( ValueString.data(), ValueString.data() + ValueString.size(), Value );
	if ( result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range )
		throw std::runtime_error("Failed to convert" + std::string(ValueString) + " to int");

	return Value;
}

bool PopJson::Value_t::GetBool()
{
	if ( mType == PopJson::ValueType_t::BooleanTrue )
		return true;
	if ( mType == PopJson::ValueType_t::BooleanFalse )
		return false;
	
	throw std::runtime_error("todo: conversion of value to bool");
}

std::string_view PopJson::Value_t::GetRawString(std::string_view JsonData)
{
	auto String = JsonData.substr( mPosition, mLength );
	return String;
}


PopJson::Value_t PopJson::Value_t::GetValue(std::string_view Key,std::string_view JsonData)
{
	for ( auto& Child : mNodes )
	{
		auto ChildKey = Child.GetKey(JsonData);
		if ( ChildKey == Key )
			return Child.GetValue(JsonData);
	}
	//	throw or undefined?
	//return Value_t( PopJson::ValueType_t::Undefined, 0, 0 );
	throw std::runtime_error("No key named " + std::string(Key));
}


PopJson::Value_t PopJson::Value_t::GetValue(size_t Index,std::string_view JsonData)
{
	//	gr: should this throw if keys are named? make this only work for arrays?
	if ( Index >= mNodes.size() )
	{
		std::stringstream Error;
		Error << "Key " << Index << "/" << mNodes.size() << " out of range";
		throw std::runtime_error( Error.str() );
	}

	auto& Child = mNodes[Index];
	return Child.GetValue(JsonData);
}


bool PopJson::Value_t::HasKey(std::string_view Key,std::string_view JsonData)
{
	for ( auto& Child : mNodes )
	{
		auto ChildKey = Child.GetKey(JsonData);
		if ( ChildKey == Key )
			return true;
	}
	return false;
}


std::string_view PopJson::Node_t::GetKey(std::string_view JsonData)
{
	return JsonData.substr( mKeyPosition, mKeyLength );
}

PopJson::Node_t::Node_t(Value_t Key,Value_t Value) :
	mValuePosition	( Value.mPosition ),
	mValueLength	( Value.mLength ),
	mValueType		( Value.mType ),
	mKeyPosition	( Key.mPosition ),
	mKeyLength		( Key.mLength )
{
}

PopJson::Node_t::Node_t(Value_t Value) :
	mValuePosition	( Value.mPosition ),
	mValueLength	( Value.mLength ),
	mValueType		( Value.mType )
{
}
	
PopJson::Value_t PopJson::Node_t::GetValue(std::string_view JsonData)
{
	Value_t Value( mValueType, mValuePosition, mValueLength );

	//	gr: we've lost all the stuff we've parsed when initially going through the tree
	//		so we need to re-parse the data...
	//		we can't really store nodes in nodes, so need a better plan
	if ( mValueType == ValueType_t::Object || mValueType == ValueType_t::Array )
	{
		//	these positions don't include start & end tokens...
		//	which is difficult as they may not be right before & after...
		//	need to adjust parser to have fake bits/parse a specific content type
		auto ValueTokenPosition = mValuePosition-1;
		auto ValueTokenLength = mValueLength+2;
		auto NodeJson = JsonData.substr( ValueTokenPosition, ValueTokenLength );
		Value = Value_t( NodeJson, ValueTokenPosition );
	}
	return Value;
}

PopJson::Json_t::Json_t(std::string_view Json) :
	ViewBase_t		( Json )
{
	//	the base class parses the json which is valid, then we copy the data -which will match the map- to our own storage
	std::copy( Json.begin(), Json.end(), std::back_inserter(mStorage) );
}

PopJson::Json_t::Json_t(ViewBase_t& Copy) :
	ViewBase_t		( Copy )
{
	//	the base class parses the json which is valid, then we copy the data -which will match the map- to our own storage
	auto JsonData = Copy.GetStorageString();
	std::copy( JsonData.begin(), JsonData.end(), std::back_inserter(mStorage) );
}


PopJson::ValueProxy_t PopJson::Json_t::operator[](std::string_view Key)
{
	return ValueProxy_t( *this, Key );
}


//	write interface
void PopJson::Json_t::Set(std::string_view Key,std::string_view Value)
{
	//	write the raw data (without type-encapsulation, ie, no quotes) to our storage
	//	leave escaping for Writing to Json time too
	//	then reference it and add to list
	auto Node = AppendNodeToStorage(Key,Value,ValueType_t::String);
	mNodes.push_back( Node );
}

void PopJson::Json_t::Set(std::string_view Key,int32_t Value)
{
	auto ValueString = std::to_string(Value);
	auto Node = AppendNodeToStorage( Key, ValueString, ValueType_t::String );
	mNodes.push_back( Node );
}

void PopJson::Json_t::Set(std::string_view Key,bool Value)
{
	auto Node = AppendNodeToStorage( Key, "", Value ? ValueType_t::BooleanTrue : ValueType_t::BooleanFalse );
	mNodes.push_back( Node );
}

PopJson::Node_t PopJson::Json_t::AppendNodeToStorage(std::string_view Key,std::string_view ValueAsString,ValueType_t::Type Type)
{
	if ( Key.empty() )
	{
		auto ClippedValue = ValueAsString.substr( 0, std::min<int>(ValueAsString.length(),14) );
		std::stringstream Error;
		Error << "Cannot write key(" << Key << "=" << ClippedValue << ") with no name. Todo: support null & undefined";
		throw std::runtime_error(Error.str());
	}
	
	//	todo: validate the node's content by reading back the value
	
	Node_t Node;
	Node.mValueType = Type;
	
	Node.mKeyPosition = mStorage.size();
	Node.mKeyLength = Key.length();
	std::copy( Key.begin(), Key.end(), std::back_inserter(mStorage) );

	Node.mValuePosition = mStorage.size();
	Node.mValueLength = ValueAsString.length();
	std::copy( ValueAsString.begin(), ValueAsString.end(), std::back_inserter(mStorage) );
	
	return Node;
}




PopJson::View_t PopJson::ViewBase_t::GetValue(std::string_view Key)
{
	std::shared_lock Lock(mStorageLock);
	
	auto Value = Value_t::GetValue( Key, GetStorageString() );
	return View_t( Value, GetStorageString() );
}

PopJson::View_t PopJson::ViewBase_t::operator[](std::string_view Key)
{
	return GetValue(Key);
}

std::string PopJson::ViewBase_t::GetJsonString()
{
	std::stringstream Json;
	GetJsonString(Json);
	return Json.str();
}

//	https://stackoverflow.com/a/24315631/355753
void StringReplaceAll(std::string& str,std::string_view from,std::string_view to)
{
	size_t start_pos = 0;
	while((start_pos = str.find(from, start_pos)) != std::string::npos)
	{
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
	}
}


void StringReplaceAll(std::string& String,char From,std::string_view To)
{
	for ( auto i=0;	i<String.size();	i++ )
	{
		if ( String[i] == From )
		{
			String.replace(i, 1, To);
			//	dont get stuck in a loop
			i += To.length()-1;
		}
	}
}

void WriteSanitisedString(std::stringstream& Json,std::string_view Value)
{
	auto Contains = [&](char Char)
	{
		return Value.find(Char) != std::string_view::npos;
	};
	
	//	sanitise string
	//	todo: lots of other things like line feeds and tabs should be escaped
	if ( Contains('\\') || Contains('"') || Contains('\n') || Contains('\t') )
	{
		//	gr: probably not the fastest but clean
		std::string CleanValue(Value);
		//	escape \ into \\ (BEFORE we escape others!)
		StringReplaceAll( CleanValue, '\\', "\\\\" );
		//	escape "
		StringReplaceAll( CleanValue, '"', "\\\"" );
		//	convert linefeeds
		StringReplaceAll( CleanValue, '\n', "\\n" );
		//	convert tabs
		StringReplaceAll( CleanValue, '\t', "\\t" );

		Json << CleanValue;
	}
	else // safe just use original
	{
		Json << Value;
	}
}

void WriteSanitisedValue(std::stringstream& Json,PopJson::Value_t Value,std::string_view ValueJsonData)
{
	if ( Value.GetType() == PopJson::ValueType_t::BooleanTrue )
	{
		Json << "true";
	}
	else if ( Value.GetType() == PopJson::ValueType_t::BooleanFalse )
	{
		Json << "true";
	}
	else if ( Value.GetType() == PopJson::ValueType_t::String )
	{
		Json << '"';
		//	gr: we're extracting a unsanitised string from GetValue
		//		we should see if it's already sanitised and save the work
		WriteSanitisedString( Json, Value.GetString(ValueJsonData) );
		Json << '"';
	}
	else
	{
		throw std::runtime_error("todo: handle json value type in write");
	}
}


void WriteSanitisedKey(std::stringstream& Json,std::string_view Key)
{
	WriteSanitisedString( Json, Key );
}


void PopJson::ViewBase_t::GetJsonString(std::stringstream& Json)
{
	auto JsonStorageData = GetStorageString();
	
	auto IsArray = !mChildren.empty();
	Json << (IsArray ? "[" : "{");
	
	//	gr: what to do if we're a mix of children and nodes? shouldn't happen? as nodes should be children with no keys?...
	for ( auto& Node : mNodes )
	{
		if ( Node.HasKey() )
		{
			auto Key = Node.GetKey(JsonStorageData);
			Json << '"';
			WriteSanitisedKey( Json, Key );
			Json << '"' << ':';
		}
		auto Value = Node.GetValue(JsonStorageData);
		WriteSanitisedValue( Json, Value, JsonStorageData );
		Json << ',';
	}
	
	for ( auto& Value : mChildren )
	{
		WriteSanitisedValue( Json, Value, JsonStorageData );
		Json << ',';
	}
	
	//	erase last oxford comma
	{
		//	look at last character (move get to back-1)
		Json.seekg(-1,std::ios_base::end);
		auto LastChar = Json.peek();
		//	it is a comma! move the put() to one back to write over it
		if ( LastChar == ',' )
			Json.seekp(-1,std::ios_base::end);
	}
	
	Json << (IsArray ? "]" : "}");
}
	
