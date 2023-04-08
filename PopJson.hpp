/*
	PopJson is a library which aims to provide a JSON parser which works on
	existing data without reformatting, making a lot of allocations etc,
	until required.
	
	To achieve this, the concept is designed around & similar to std::string_view
	and std::span.
	PopJsonView is a class which sits on top of data to give access to contents on demand
	by generating a mapping on parsing, rather than creating a whole new structure.
	Then where possible (ie. strings, or slicing json) spans & string_views back to the
	original data are given to the user.
 
	The user is responsible for the life time of the underlying data
*/
#pragma once

#include <vector>
#include <string_view>
//#include <span>
#include "../std_span.hpp"

namespace PopJson
{
	class Node_t;
	class Value_t;

	namespace ValueType_t
	{
		enum Type
		{
			Undefined,
			Null,
			Object,
			Array,
			String,
			NumberInteger,
			NumberDouble,
			BooleanTrue,
			BooleanFalse,
		};
	}
	
}


class PopJson::Value_t
{
	friend class Node_t;
public:
	Value_t(){}
	Value_t(std::string_view Json);		//	parser
	Value_t(ValueType_t::Type Type,size_t Position,size_t Length) :
		mType		( Type ),
		mPosition	( Position ),
		mLength		( Length )
	{
	}

	int					GetInteger(std::string_view JsonData);
	double				GetDouble();
	float				GetFloat();
	std::string_view	GetString(std::string& Buffer);	//	if the string needs escaping, Buffer will be used and returned. If we can use the raw string, that gets returned
	std::string			GetString();					//	get an escaped string (even if it doesnt need it)
	bool				GetBool();

	Value_t&			GetValue(std::string_view Key,std::string_view JsonData);	//	object element
	Value_t&			GetValue(size_t Index,std::string_view JsonData);			//	array element

	bool				HasKey(std::string_view Key);
	
	//	common helpers
	void				GetArray(std::vector<int>& Integers);
	void				GetArray(std::vector<std::string_view>& UnescapedStrings);
	std::span<Node_t>	GetChildren()	{	return std::span( mNodes.data(), mNodes.size() );	}

private:
	std::string_view	GetRawString(std::string_view JsonData);

private:
	ValueType_t::Type	mType = ValueType_t::Undefined;

protected:
	size_t				mPosition = 0;
	size_t				mLength = 0;
	
public:
	//	if an array, empty keys
	std::vector<Node_t>	mNodes;
};


class PopJson::Node_t : public Value_t
{
public:
	Node_t(){};
	//	expect key to be a string, but doesn't have to be?
	Node_t(Value_t Key,Value_t Value) :
		Value_t			( Value ),
		mKeyPosition	( Key.mPosition ),
		mKeyLength		( Key.mLength )
	{
	}
	Node_t(Value_t Value) :
		Value_t	( Value )
	{
	}
	
	std::string_view	GetKey(std::string_view JsonData);

public:
	size_t		mKeyPosition = 0;
	size_t		mKeyLength = 0;
};
