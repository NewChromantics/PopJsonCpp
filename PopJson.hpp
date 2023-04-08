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


namespace PopJson
{
	class ObjectView;
	class Node_t;
	class ValueView_t;

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


class PopJson::ValueView_t
{
public:
	ValueView_t(){}
	ValueView_t(ValueType_t::Type Type,size_t Position,size_t Length=0) :
		mType		( Type ),
		mPosition	( Position ),
		mLength		( Length )
	{
	}

	void				SetEnd(size_t EndPosition)	{	mLength = EndPosition - mPosition;	}
	
	ValueType_t::Type	mType = ValueType_t::Undefined;

	size_t				mPosition = 0;
	size_t				mLength = 0;
	
	//	if an array, empty keys
	std::vector<Node_t>	mNodes;
};

class PopJson::Node_t
{
public:
	size_t				mKeyPosition = 0;
	size_t				mKeyLength = 0;
	ValueView_t			mValue;
};

//	this is essentially a js object
class PopJson::ObjectView
{
public:
	ObjectView(std::string_view Json);		//	parse

private:
	ValueView_t	mRoot;
};
