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
	class ViewBase_t;
	class View_t;		//	a value, but has a view (temporary) pointer to the underlying data
	class Json_t;		//	a json is a Value but holds onto its own data and supplys views(values), and becomes writable
	class ValueProxy_t;	//	to enable a mutable value, this returns an object which calls Set() on a Json_t

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
	
	void		UnitTest();
}


class PopJson::Node_t
{
public:
	Node_t(){};
	//	expect key to be a string, but doesn't have to be?
	Node_t(Value_t Key,Value_t Value);
	Node_t(Value_t Value);
	
	bool				HasKey()			{	return mKeyLength > 0;	}
	std::string_view	GetKey(std::string_view JsonData);
	Value_t				GetValue(std::string_view JsonData);
	
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
	Value_t(std::string_view Json,size_t WritePositionOffset=0);		//	parser
	Value_t(ValueType_t::Type Type,size_t Position,size_t Length) :
		mType		( Type ),
		mPosition	( Position ),
		mLength		( Length )
	{
	}
	Value_t(const Value_t& Copy) :
		mType		( Copy.mType ),
		mPosition	( Copy.mPosition ),
		mLength		( Copy.mLength )
	{
	}

	ValueType_t::Type	GetType()			{	return mType;	}
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
	size_t				GetChildCount()	{	return mNodes.size();	}

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
	std::vector<Value_t>	mChildren;
};




class PopJson::ViewBase_t : protected Value_t
{
public:
	using Value_t::Value_t;
	ViewBase_t(const Value_t& Copy) :
		Value_t	( Copy )
	{
	}
	
	
	//	stringify
	std::string			GetJsonString();
	void				GetJsonString(std::stringstream& Json);


	//	read interface without requiring storage
	int					GetInteger()					{	std::shared_lock Lock(mStorageLock);	return Value_t::GetInteger( GetStorageString() );	}
	std::string_view	GetString(std::string& Buffer)	{	std::shared_lock Lock(mStorageLock);	return Value_t::GetString( Buffer, GetStorageString() );	}
	std::string			GetString()						{	std::shared_lock Lock(mStorageLock);	return Value_t::GetString( GetStorageString() );	}
	bool				GetBool()						{	std::shared_lock Lock(mStorageLock);	return Value_t::GetBool( GetStorageString() );	}

	bool				HasKey(std::string_view Key)	{	std::shared_lock Lock(mStorageLock);	return Value_t::HasKey( Key, GetStorageString() );	}

	//	gr: this does a copy, we want to change this to return a View_t?
	//Value_t				GetValue(std::string_view Key)	{	std::shared_lock Lock(mStorageLock);	return Value_t::GetValue( Key, GetStorageString() );	}
	View_t				GetValue(std::string_view Key);//	{	std::shared_lock Lock(mStorageLock);	return Value_t::GetValue( Key, GetStorageString() );	}
	View_t				operator[](std::string_view Key);

protected:
	std::shared_mutex			mStorageLock;		//	not needed in base class, but makes code a lot easier
	virtual std::string_view	GetStorageString()=0;
};
	
class PopJson::View_t : public ViewBase_t
{
public:
	View_t(std::string_view Json) :
		mStorage	( Json )
	{
	}
	View_t(const Value_t& Value,std::string_view Storage) :
		ViewBase_t	( Value ),
		mStorage	( Storage )
	{
	}

protected:
	virtual std::string_view	GetStorageString() override	{	return mStorage;	}
	std::string_view			mStorage;
};
	



class PopJson::Json_t : public ViewBase_t
{
public:
	Json_t(){};
	Json_t(std::string_view Json);		//	parser but copies the incoming data to become mutable
	Json_t(const Json_t& Copy)
	{
		mStorage = Copy.mStorage;
		//*this = Value_t(GetStorageString());
	}
	Json_t(Json_t&& Move)
	{
		mStorage = std::move( Move.mStorage );
		//*this = Value_t(GetStorageString());
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

	//	write interface
	void				Set(std::string_view Key,std::string_view Value);
	void				Set(std::string_view Key,int32_t Value);
	void				Set(std::string_view Key,uint32_t Value);
	void				Set(std::string_view Key,int64_t Value);
	void				Set(std::string_view Key,uint64_t Value);
	void				Set(std::string_view Key,bool Value);
	void				Set(std::string_view Key,const std::vector<Json_t>& Values);
	void				Set(std::string_view Key,const Json_t& Value);	//	change to accept View_t
	void				PushBack(const Json_t& Value);	//	change to accept View_t
	
	//	allow [] operator by giving out a mutable value... but might just have to be a proxy to Set()
	ValueProxy_t		operator[](std::string_view Key);

protected:
	//	todo: we _could_ store the data here as a raw type (eg. int) and convert during write
	//			to do that, have additional "non-stringified" value types for int, float, maybe even arrays
	Node_t				AppendNodeToStorage(std::string_view Key,std::string_view ValueAsString,ValueType_t::Type Type);
	
private:
	std::string_view	GetStorageString()	{	return std::string_view( mStorage.data(), mStorage.size() );	}
	std::vector<char>	mStorage;
};



class PopJson::ValueProxy_t
{
	friend class Json_t;
protected:
	ValueProxy_t()=delete;
	ValueProxy_t(Json_t& This,std::string_view Key) :
		mJson	( This ),
		mKey	( Key )
	{
	}

public:
	ValueProxy_t&	operator=(std::string_view String)	{	mJson.Set(mKey,String);	return *this;	}
	ValueProxy_t&	operator=(bool Boolean)				{	mJson.Set(mKey,Boolean);	return *this;	}
	ValueProxy_t&	operator=(int Integer)				{	mJson.Set(mKey,Integer);	return *this;	}
	//ValueProxy_t&	operator=(float Float)				{	mJson.Set(mKey,Float);	return *this;	}

private:
	std::string		mKey;	//	the original caller may hold onto this proxy, but not their initial key in the []operator, so we need a copy
	Json_t&			mJson;
};
