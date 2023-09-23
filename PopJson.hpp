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
#include <span>
#include <shared_mutex>

namespace PopJson
{
	class Value_t;		//	a value is meta to a value to the underlying data, it requires a pointer(view) to the underlying data; a string, or object, or null, number etc, which has child members in case it's an object or an array
	class Node_t;		//	a node is a Value with a key attached
	class View_t;		//	a value, but has a view (temporary) pointer to the underlying data
	class Json_t;		//	a json is a Value but holds onto its own data and supplys views(values), and becomes writable

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


class PopJson::Node_t
{
public:
	Node_t(){};
	//	expect key to be a string, but doesn't have to be?
	Node_t(Value_t Key,Value_t Value);
	Node_t(Value_t Value);
	
	std::string_view	GetKey(std::string_view JsonData);
	Value_t				GetValue();
	
public:
	size_t		mKeyPosition = 0;
	size_t		mKeyLength = 0;
	size_t		mValuePosition = 0;
	size_t		mValueLength = 0;
	ValueType_t::Type	mValueType = ValueType_t::Null;
};


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
	double				GetDouble(std::string_view JsonData);
	float				GetFloat(std::string_view JsonData);
	std::string_view	GetString(std::string& Buffer,std::string_view JsonData);	//	if the string needs escaping, Buffer will be used and returned. If we can use the raw string, that gets returned
	std::string			GetString(std::string_view JsonData);					//	get an escaped string (even if it doesnt need it)
	bool				GetBool(std::string_view JsonData)	{	return GetBool();	}
	bool				GetBool();

	//	gr: references... should be protected? when does a user need this to be a reference
	//		value is abstract enough to be copied
	Value_t				GetValue(std::string_view Key,std::string_view JsonData);	//	object element
	Value_t				GetValue(size_t Index,std::string_view JsonData);			//	array element

	bool				HasKey(std::string_view Key,std::string_view JsonData);
	
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




class PopJson::Json_t : protected Value_t
{
public:
	Json_t(){};
	Json_t(std::string_view Json);		//	parser
	Json_t(const Json_t& Copy) :
		Value_t	( Copy )
	{
		mStorage = Copy.mStorage;
	}
	Json_t(Json_t&& Move) :
		Value_t	( Move )
	{
		mStorage = std::move( Move.mStorage );
	};
	
	Json_t&				operator=(const Json_t& Copy) noexcept
	{
		static_cast<Value_t&>(*this) = Copy;
		mStorage = Copy.mStorage;
		return *this;
	}
	Json_t&				operator=(const Json_t&& Move) noexcept
	{
		static_cast<Value_t&>(*this) = Move;
		mStorage = std::move( Move.mStorage );
		return *this;
	}

	std::string			GetJsonString() const;	//	stringify
	
	//	write interface
	void				Set(std::string_view Key,std::string_view Value);
	void				Set(std::string_view Key,int Value);
	void				Set(std::string_view Key,size_t Value);
	void				Set(std::string_view Key,unsigned Value);
	void				Set(std::string_view Key,long Value);
	void				Set(std::string_view Key,bool Value);
	void				Set(std::string_view Key,const std::vector<Json_t>& Values);
	void				Set(std::string_view Key,const Json_t& Value);	//	change to accept View_t
	void				PushBack(const Json_t& Value);	//	change to accept View_t
	
	//	read interface without requiring storage
	int					GetInteger()					{	std::shared_lock Lock(mStorageLock);	return Value_t::GetInteger( GetJsonString() );	}
	std::string_view	GetString(std::string& Buffer)	{	std::shared_lock Lock(mStorageLock);	return Value_t::GetString( Buffer, GetJsonString() );	}
	std::string			GetString()						{	std::shared_lock Lock(mStorageLock);	return Value_t::GetString( GetJsonString() );	}

	bool				HasKey(std::string_view Key)	{	std::shared_lock Lock(mStorageLock);	return Value_t::HasKey( Key, GetJsonString() );	}

	//	gr: this does a copy, we want to change this to return a View_t?
	Json_t				GetValue(std::string_view Key);

private:
	std::shared_mutex	mStorageLock;
	std::string_view	GetStorageString()	{	return std::string_view( mStorage.data(), mStorage.size() );	}
	std::vector<char>	mStorage;
};
