#include "PopJson.hpp"
#include <string>

using namespace PopJson;

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
            throw std::runtime_error("unexpected end of input");

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
	Value_t parse_string()
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
				return Value_t( ValueType_t::String, StartPosition, i-StartPosition );
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

    /* parse_number()
     *
     * Parse a double.
     */
	Value_t parse_number()
	{
        size_t start_pos = i;

        if (str[i] == '-')
            i++;

        // Integer part
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

        if (str[i] != '.' && str[i] != 'e' && str[i] != 'E'
                && (i - start_pos) <= static_cast<size_t>(std::numeric_limits<int>::digits10))
		{
			Value_t Value( ValueType_t::NumberInteger, start_pos, i-start_pos );
            //return std::atoi(str.c_str() + start_pos);
			return Value;
        }

        // Decimal part
        if (str[i] == '.')
		{
            i++;
            if (!in_range(str[i], '0', '9'))
				throw std::runtime_error("at least one digit required in fractional part");

            while (in_range(str[i], '0', '9'))
                i++;
        }

        // Exponent part
        if (str[i] == 'e' || str[i] == 'E') {
            i++;

            if (str[i] == '+' || str[i] == '-')
                i++;

            if (!in_range(str[i], '0', '9'))
				throw std::runtime_error("at least one digit required in exponent");

            while (in_range(str[i], '0', '9'))
                i++;
        }

		Value_t Value( ValueType_t::NumberDouble, start_pos, i-start_pos );
        //return std::strtod(str.c_str() + start_pos, nullptr);
		return Value;
    }

    /* expect(str, res)
     *
     * Expect that 'str' starts at the character that was just read. If it does, advance
     * the input and return res. If not, flag an error.
     */
	Value_t expect(std::string_view expected,ValueType_t::Type ResultType)
	{
		if ( i <= 0 )
			throw std::runtime_error("Bad position");
		
        i--;
        if (str.compare(i, expected.length(), expected) != 0)
		{
			auto FoundString = str.substr(i, expected.length());
			throw std::runtime_error("parse error: expected " + std::string(expected) + ", got " + std::string(FoundString) );
		}
		
		Value_t Result( ResultType, i, expected.length() );
		i += expected.length();
        return Result;
    }

    /* parse_json()
     *
     * Parse a JSON object.
     */
	Value_t parse_json(int depth)
	{
        if (depth > max_depth)
			throw std::runtime_error("exceeded maximum nesting depth");

        char ch = get_next_token();

        if (ch == '-' || (ch >= '0' && ch <= '9'))
		{
            i--;
            return parse_number();
        }

        if (ch == 't')
            return expect("true", ValueType_t::BooleanTrue );

        if (ch == 'f')
            return expect("false", ValueType_t::BooleanFalse );

        if (ch == 'n')
			return expect("null", ValueType_t::Null );

        if (ch == '"')
            return parse_string();

        if (ch == '{')
		{
			auto StartPosition = i;
			
			ch = get_next_token();
			if (ch == '}')
			{
				Value_t Object( ValueType_t::Object, StartPosition, i-StartPosition );
				return Object;
			}

			std::vector<Node_t> Nodes;
			while (1)
			{
				if (ch != '"')
					throw std::runtime_error("expected '\"' in object, got " + esc(ch));

				auto Key = parse_string();
				
				ch = get_next_token();
				if (ch != ':')
					throw std::runtime_error("expected ':' in object, got " + esc(ch));

				auto Value = parse_json( depth + 1 );
				
				Nodes.push_back( Node_t( Key, Value ) );

				ch = get_next_token();
				if (ch == '}')
					break;
				if (ch != ',')
					throw std::runtime_error("expected ',' in object, got " + esc(ch));

				ch = get_next_token();
			}
			
			Value_t Object( ValueType_t::Object, StartPosition, i );
			Object.mNodes = std::move(Nodes);
			return Object;
		}

        if (ch == '[')
		{
			auto StartPosition = i;
			
			ch = get_next_token();
			if (ch == ']')
			{
				Value_t Array( ValueType_t::Array, StartPosition, i );
				return Array;
			}

			std::vector<Node_t> Nodes;
			while (1)
			{
				i--;

				auto Value = parse_json(depth + 1);
				
				Node_t Node(Value);
				Nodes.push_back(Node);

				ch = get_next_token();
				if (ch == ']')
					break;
				if (ch != ',')
					throw std::runtime_error("expected ',' in list, got " + esc(ch));

				ch = get_next_token();
			}
			
			Value_t Array( ValueType_t::Array, StartPosition, i-StartPosition );
			Array.mNodes = std::move(Nodes);
			return Array;
		}

		throw std::runtime_error("expected value, got " + esc(ch));
	}
};


PopJson::Value_t::Value_t(std::string_view Json)
{
	bool AllowComments = false;
	JsonParser parser( Json, AllowComments );
	auto Root = parser.parse_json(0);
	*this = Root;
}

