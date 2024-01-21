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
	class ValueInput_t;

	class Location_t;		//	pos + length of a value or key
	typedef uint32_t NodeIndex_t;
	class Map_t;			//	This is a map to every element (MapNode_t) in a json object; it is a tree, but flat. Data is kept elsewhere
	class MapNode_t;		//	replacement of Value_t
	class JsonMutable_t;	//	map + storage
	class JsonReadOnly_t;	//	map + pointer to storage
	class SliceReadOnly_t;	//	access the map from a point in the subtree

	Map_t	Parse(std::string_view Json);

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


class PopJson::Location_t
{
public:
	Location_t(){};
	Location_t(size_t Position,size_t Length) :
		mPosition	( Position ),
		mLength		( Length )
	{
	}
	
	bool				IsEmpty() const		{	return mLength==0;	}
	std::string_view	GetContents(std::string_view Storage) const	{	return Storage.substr( mPosition, mLength );	}
	
public:
	size_t		mPosition = 0;
	size_t		mLength = 0;
};

class PopJson::MapNode_t
{
private:
	constexpr static NodeIndex_t	RootNodeNoParent = 0xffffffff;
	
public:
	bool				HasKey() const			{	return !mKeyPosition.IsEmpty();	}
	std::string_view	GetKey(std::string_view Storage) const		{	return mKeyPosition.GetContents(Storage);	}
	ValueType_t::Type	GetType() const			{	return mValueType;	}
	bool				IsRootNode() const		{	return mParent == RootNodeNoParent;	}
	NodeIndex_t			GetParentIndex() const	{	return mParent;	}

protected:
	std::string_view	GetRawValue(std::string_view Storage) const	{	return mValuePosition.GetContents(Storage);	}

private:
	NodeIndex_t			mParent = RootNodeNoParent;	//	the root node is the only one with no parent. 0 is always the root
	//	if no key, this object is an element in an array (order dictated by map)
	Location_t			mKeyPosition;
	Location_t			mValuePosition;
	ValueType_t::Type	mValueType = ValueType_t::Null;
};

class PopJson::Map_t
{
public:
	//	add new node into storage(and tree)
	NodeIndex_t		AddNode(NodeIndex_t Parent,std::string_view Key,std::string_view RawValue,std::vector<char>& Storage);
	//	record node into tree
	NodeIndex_t		AddNode(NodeIndex_t Parent,Location_t Key,Location_t Value,ValueType_t::Type Type);

	std::string		Stringify(std::string_view Storage);
	
protected:
	std::vector<MapNode_t>	mFlatTree;
};



class PopJson::Node_t
{
public:
	Node_t(){};
	//	expect key to be a string, but doesn't have to be?
	Node_t(Value_t Key,Value_t Value);
	Node_t(Value_t Value);
	
	bool				HasKey()			{	return !mKeyPosition.IsEmpty();	}
	std::string_view	GetKey(std::string_view JsonData)	{	return mKeyPosition.GetContents(JsonData);	}
	Value_t				GetValue(std::string_view JsonData);
	ValueType_t::Type	GetType() const		{	return mValueType;	}
	void				ReplaceValue(Location_t Value,ValueType_t::Type Type)
	{
		mValuePosition = Value;
		mValueType = Type;
	}
	
public:
	Location_t			mKeyPosition;
	Location_t			mValuePosition;
	ValueType_t::Type	mValueType = ValueType_t::Null;
};


class PopJson::Value_t
{
	friend class Node_t;
	friend class Json_t;
public:
	Value_t(){}
	Value_t(std::string_view Json,size_t WritePositionOffset=0);		//	parser
	Value_t(ValueType_t::Type Type,Location_t Position) :
		mType		( Type ),
		mPosition	( Position )
	{
	}
	Value_t(const Value_t& Copy) :
		mType		( Copy.mType ),
		mPosition	( Copy.mPosition )
	{
		mNodes = Copy.mNodes;
	}
	virtual ~Value_t(){};

	ValueType_t::Type	GetType()			{	return mType;	}
	
	//	these need storage, so should be protected
public:
	int							GetInteger(std::string_view JsonData);
	double						GetDouble(std::string_view JsonData);
	float						GetFloat(std::string_view JsonData);
	std::string_view			GetString(std::string& Buffer,std::string_view JsonData);	//	if the string needs escaping, Buffer will be used and returned. If we can use the raw string, that gets returned
	std::string					GetString(std::string_view JsonData);					//	get an escaped string (even if it doesnt need it)
	bool						GetBool(std::string_view JsonData)	{	return GetBool();	}
	bool						GetBool();


	//	gr: references... should be protected? when does a user need this to be a reference
	//		value is abstract enough to be copied
	Value_t				GetValue(std::string_view Key,std::string_view JsonData);	//	object element
	Value_t				GetValue(size_t Index,std::string_view JsonData);			//	array element

	bool				HasKey(std::string_view Key,std::string_view JsonData)		{	return GetNode( Key, JsonData, nullptr );	}
	
public:
	//	common helpers
	void				GetArray(std::vector<int>& OutputValues,std::string_view JsonData);
	void				GetArray(std::vector<std::string>& OutputValues,std::string_view JsonData);
	std::span<Node_t>	GetChildren()	{	return std::span( mNodes.data(), mNodes.size() );	}
	size_t				GetChildCount()	{	return mNodes.size();	}

protected:
	//	returns false if not present
	bool				GetNode(std::string_view Key,std::string_view JsonData,std::function<void(Node_t&)> OnLockedNode);
	
private:
	std::string_view	GetRawString(std::string_view JsonData)	{	return mPosition.GetContents(JsonData);	}

private:
	ValueType_t::Type	mType = ValueType_t::Undefined;
	
protected:
	Location_t			mPosition;
	
public:
	//	if an array, empty keys
	std::vector<Node_t>		mNodes;
};




class PopJson::ViewBase_t : public Value_t
{
	friend class Json_t;	//	allow Json_t access to storage to copy it
protected:
	ViewBase_t(const ViewBase_t& Copy) :
		Value_t	( Copy )
	{
	}
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
	int							GetInteger()					{	std::shared_lock Lock(mStorageLock);	return Value_t::GetInteger( GetStorageString() );	}
	std::string_view			GetString(std::string& Buffer)	{	std::shared_lock Lock(mStorageLock);	return Value_t::GetString( Buffer, GetStorageString() );	}
	std::string					GetString()						{	std::shared_lock Lock(mStorageLock);	return Value_t::GetString( GetStorageString() );	}
	bool						GetBool()						{	std::shared_lock Lock(mStorageLock);	return Value_t::GetBool( GetStorageString() );	}
	void						GetArray(std::vector<std::string>& Values)		{	std::shared_lock Lock(mStorageLock);	return Value_t::GetArray( Values, GetStorageString() );	}
	std::vector<std::string>	GetStringArray()				{	std::vector<std::string> Values;	GetArray(Values);	return Values;	}

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
	




//	wrapper that abuses the use of implicit conversion to generate json-serialised
//	string and reduce duplicating function signatures
class PopJson::ValueInput_t
{
public:
	ValueInput_t();	//	undefined
	ValueInput_t(const int& Value);
	ValueInput_t(const uint32_t& Value);
	ValueInput_t(const bool& Value);
	ValueInput_t(const float& Value);
	ValueInput_t(const std::string& Value);
	ValueInput_t(std::string_view Value);
	ValueInput_t(const std::span<std::string_view>& Value);
	ValueInput_t(const std::span<std::string>& Values);

	//	gr: is there a way to avoid this alloc
	std::string			mSerialisedValue;
	ValueType_t::Type	mType = ValueType_t::Undefined;
};





class PopJson::Json_t : public ViewBase_t
{
	friend class ValueProxy_t;
public:
	Json_t(){};
	Json_t(std::string_view Json);		//	parser but copies the incoming data to become mutable
	Json_t(ViewBase_t& Copy);
	Json_t(const Json_t& Copy) :
		ViewBase_t( Copy )	//	copy map
	{
		mStorage = Copy.mStorage;
	}
	Json_t(Json_t&& Move)
		//ViewBase_t( Copy )
	{
		*this = Move;	//	copy map
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

	//	write interface
	void				Set(std::string_view Key,const ValueInput_t& Value);
	/*
	void				Set(std::string_view Key,std::string_view Value);
	void				Set(std::string_view Key,int32_t Value);
	void				Set(std::string_view Key,uint32_t Value);
	void				Set(std::string_view Key,int64_t Value);
	void				Set(std::string_view Key,uint64_t Value);
	void				Set(std::string_view Key,bool Value);
	void				Set(std::string_view Key,const std::vector<Json_t>& Values);
	void				Set(std::string_view Key,const Json_t& Value);	//	change to accept View_t
	void				Set(std::string_view Key,std::span<std::string_view> Values);
	 */
	
	//	push onto this as an array
	//	enable the ability to do an effecient write straight into storage, and easy conversion
	//	may be able to template this one day...
	//	this writes an array of strings!
	void				PushBack(std::span<uint32_t> Values,std::function<std::string(const uint32_t& Value)> GetStringValue);
	void				PushBack(std::string_view Key,std::span<uint32_t> Values,std::function<std::string(const uint32_t& Value)> GetStringValue);
	//void				PushBack(const Json_t& Value);	//	change to accept View_t

	//	allow [] operator by giving out a mutable value... but might just have to be a proxy to Set()
	ValueProxy_t		operator[](std::string_view Key);

protected:
	//	todo: we _could_ store the data here as a raw type (eg. int) and convert during write
	//			to do that, have additional "non-stringified" value types for int, float, maybe even arrays
	Node_t				AppendNodeToStorage(std::string_view Key,std::string_view ValueAsString,ValueType_t::Type Type);
	Value_t				AppendValueToStorage(std::string_view ValueAsString,ValueType_t::Type Type);
	virtual std::string_view	GetStorageString() override	{	return std::string_view( mStorage.data(), mStorage.size() );	}

private:
	std::vector<char>	mStorage;
};



class PopJson::ValueProxy_t : public PopJson::ViewBase_t
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
	//	enable the ability to do an effecient write straight into storage, and easy conversion
	//	may be able to template this one day...
	//	this writes an array of strings!
	void			PushBack(std::span<uint32_t> Values,std::function<std::string(const uint32_t&)> WriteStringValue);

	ValueProxy_t&	operator=(std::string_view String)	{	mJson.Set(mKey,String);	return *this;	}
	ValueProxy_t&	operator=(bool Boolean)				{	mJson.Set(mKey,Boolean);	return *this;	}
	ValueProxy_t&	operator=(int Integer)				{	mJson.Set(mKey,Integer);	return *this;	}
	//ValueProxy_t&	operator=(float Float)				{	mJson.Set(mKey,Float);	return *this;	}

protected:
	virtual std::string_view	GetStorageString()		{	return mJson.GetStorageString();	}
	
private:
	std::string		mKey;	//	the original caller may hold onto this proxy, but not their initial key in the []operator, so we need a copy
	Json_t&			mJson;
};
