#pragma once

#include <AnchoCommons/detail/JSValueIterators.hpp>
#include <map>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <Exceptions.h>

namespace Ancho {
namespace Utils {

namespace detail {

inline CComVariant getMember(CComVariant &aObject, const std::wstring &aProperty)
{
  if (aObject.vt != VT_DISPATCH) {
    ANCHO_THROW(ENotAnObject());
  }
  DISPID did = 0;
  LPOLESTR lpNames[] = {(LPOLESTR)aProperty.c_str()};
  if (FAILED(aObject.pdispVal->GetIDsOfNames(IID_NULL, lpNames, 1, LOCALE_USER_DEFAULT, &did))) {
    return CComVariant();
  }

  CComVariant result;
  DISPPARAMS params = {0};
  IF_FAILED_THROW(aObject.pdispVal->Invoke(did, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &result, NULL, NULL));

  return result;
}

inline void setMember(CComVariant &aObject, const std::wstring &aProperty, const CComVariant &aValue)
{
  if (aObject.vt != VT_DISPATCH) {
    ANCHO_THROW(ENotAnObject());
  }

  CIDispatchHelper helper(aObject.pdispVal);

  IF_FAILED_THROW(helper.SetPropertyByRef((LPOLESTR)aProperty.c_str(), aValue));
}


inline bool isArray(CComVariant &aObject) {
  if (aObject.vt != VT_DISPATCH) {
    return false;
  }

  CComVariant len = getMember(aObject, L"length");
  return len.vt != VT_EMPTY;
}

} //namespace detail


//* Representation of null value
struct null_t{null_t(){}};
inline bool operator==(const null_t &, const null_t &) { return true; }
inline bool operator!=(const null_t &, const null_t &) { return false; }

class JSValueWrapper;
class JSValueWrapperConst;
class JSObjectWrapper;
class JSObjectWrapperConst;
class JSArrayWrapper;
class JSArrayWrapperConst;

#define JS_VALUE_TO_TYPE(TYPENAME, NAME, VARTYPE, EXCEPTION)\
  TYPENAME to##NAME() const { \
    if (!is##NAME()) { \
      ANCHO_THROW(EXCEPTION()); \
    } \
    return TYPENAME((VARTYPE)(mCurrentValue.llVal));\
  }
#define JS_VALUE_IS_TYPE(NAME, VT)\
  bool is##NAME() const { \
      HRESULT hr = mCurrentValue.ChangeType(VT);\
      if (FAILED(hr)) {\
        return false;\
      }\
      return true;\
  }

#define JS_VALUE_TYPE_METHODS(TYPENAME, NAME, VARTYPE, VT, EXCEPTION)\
  JS_VALUE_IS_TYPE(NAME, VT)\
  JS_VALUE_TO_TYPE(TYPENAME, NAME, VARTYPE, EXCEPTION)

//Wrapper for JS objects - currently read-only access
class JSValueWrapperConst
{
public:
  friend void swap(JSValueWrapperConst &aVal1, JSValueWrapperConst &aVal2);
  friend class JSObjectWrapper;
  friend class JSArrayWrapper;
  friend class JSObjectWrapperConst;
  friend class JSArrayWrapperConst;

  JSValueWrapperConst() {}

  JSValueWrapperConst(const CComVariant &aVariant)
    : mCurrentValue(aVariant) {}

  JSValueWrapperConst(const VARIANT &aVariant)
    : mCurrentValue(aVariant) {}

  JSValueWrapperConst(const JSValueWrapperConst &aValue)
    : mCurrentValue(aValue.mCurrentValue) {}

  JSValueWrapperConst(const JSValueWrapper &aValue);

  JSValueWrapperConst(int aValue)
    : mCurrentValue(aValue) {}

  JSValueWrapperConst(double aValue)
    : mCurrentValue(aValue) {}

  JSValueWrapperConst(const std::wstring &aValue)
    : mCurrentValue(aValue.c_str()) {}

  JSValueWrapperConst(bool aValue)
    : mCurrentValue(aValue) {}

  JSValueWrapperConst(CComPtr<IDispatch> aValue)
    : mCurrentValue(aValue) {}

  JS_VALUE_TYPE_METHODS(std::wstring, String, BSTR, VT_BSTR, ENotAString)
#pragma warning( push )
#pragma warning( disable : 4800 )
  JS_VALUE_TYPE_METHODS(bool, Bool, BOOL, VT_BOOL, ENotABool)
#pragma warning( pop )
  JS_VALUE_TYPE_METHODS(int, Int, INT, VT_I4, ENotAnInt)
  JS_VALUE_TYPE_METHODS(double, Double, double, VT_R8, ENotADouble)
  //JS_VALUE_TYPE_METHODS(CComPtr<IDispatch>, Object, IDispatch*, VT_DISPATCH, ENotAnObject)

  void attach(VARIANT &aVariant)
  {
    IF_FAILED_THROW(mCurrentValue.Attach(&aVariant));
  }

  bool isNull()const
  { return mCurrentValue.vt == VT_EMPTY; }

  bool isObject() const
  { return mCurrentValue.vt == VT_DISPATCH && !isArray(); }

  JSObjectWrapperConst toObject()const;

  bool isArray() const
  { return detail::isArray(mCurrentValue); }

  JSArrayWrapperConst toArray()const;

  JSValueWrapperConst & operator=(JSValueWrapperConst aVal);

  JSValueWrapperConst & operator=(JSValueWrapper aVal);

  template<typename TVisitor>
  typename TVisitor::result_type applyVisitor(TVisitor aVisitor)const
  {
    switch (mCurrentValue.vt) {
    case VT_EMPTY:
      return aVisitor(null_t());
    case VT_BOOL:
      return aVisitor(toBool());
    case VT_I4:
      return aVisitor(toInt());
    case VT_R8:
      return aVisitor(toDouble());
    case VT_BSTR:
      return aVisitor(toString());
    default:
      if (isArray()) {
        return aVisitor(toArray());
      }
      if (isObject()) {
        return aVisitor(toObject());
      }
      ATLASSERT(false);
      return TVisitor::result_type();
    }
  }
protected:
  CComVariant getMember(const std::wstring &aProperty) const
  { return detail::getMember(mCurrentValue, aProperty); }

  mutable CComVariant mCurrentValue;
};

class JSValueWrapper: public JSValueWrapperConst
{
public:
  friend class JSObjectWrapper;
  friend class JSArrayWrapper;
  friend class JSObjectWrapperConst;
  friend class JSArrayWrapperConst;
  //typedef MemberForwardIterator NameIterator;

  JSValueWrapper() {}

  JSValueWrapper(const CComVariant &aVariant)
    : JSValueWrapperConst(aVariant) {}

  JSValueWrapper(const VARIANT &aVariant)
    : JSValueWrapperConst(aVariant) {}

  JSValueWrapper(const JSValueWrapper &aVariant)
    : JSValueWrapperConst(aVariant.mCurrentValue) {}

  JSValueWrapper(int aValue)
    : JSValueWrapperConst(aValue) {}

  JSValueWrapper(double aValue)
    : JSValueWrapperConst(aValue) {}

  JSValueWrapper(const std::wstring &aValue)
    : JSValueWrapperConst(aValue) {}

  JSValueWrapper(bool aValue)
    : JSValueWrapperConst(aValue) {}

  JSValueWrapper(CComPtr<IDispatch> aValue)
    : JSValueWrapperConst(aValue) {}

  void attach(VARIANT &aVariant)
  {
    IF_FAILED_THROW(mCurrentValue.Attach(&aVariant));
  }

  JSObjectWrapper toObject();

  JSArrayWrapper toArray();

  JSValueWrapper & operator=(JSValueWrapper aVal)
  {
    swap(*this, aVal);
    return *this;
  }
protected:

};

//Wrapper for JS objects
class JSObjectWrapperConst
{
public:
  friend class JSValueWrapperConst;

  typedef detail::MemberForwardIterator NameIterator;


  JSObjectWrapperConst & operator=(JSValueWrapperConst aVal);
  /*{
    swap(*this, aVal);
    return *this;
  }*/

  JSValueWrapperConst
  operator[](const std::wstring &aProperty)const
  {
    return JSValueWrapperConst(getMember(aProperty));
  }

  NameIterator memberNames()const
  {
    if (mCurrentValue.vt != VT_DISPATCH) {
      ANCHO_THROW(ENotAnObject());
    }
    CComQIPtr<IDispatchEx> dispex = mCurrentValue.pdispVal;
    if (!dispex) {
      ANCHO_THROW(ENotIDispatchEx());
    }
    return NameIterator(dispex);
  }

  operator JSValueWrapperConst()
  {
    return JSValueWrapperConst(mCurrentValue);
  }

protected:
  JSObjectWrapperConst(const JSValueWrapperConst &aValue)
    : mCurrentValue(aValue.mCurrentValue)
  {
    if (!aValue.isObject()) {
      ANCHO_THROW(ENotAnObject());
    }
  }

  CComVariant getMember(const std::wstring &aProperty) const
  { return detail::getMember(mCurrentValue, aProperty); }

  mutable CComVariant mCurrentValue;
};

//Wrapper for JS objects
class JSObjectWrapper: public JSObjectWrapperConst
{
public:
  friend class JSValueWrapper;

  typedef detail::MemberForwardIterator NameIterator;


  JSObjectWrapper & operator=(JSValueWrapper aVal);
  /*{
    swap(*this, aVal);
    return *this;
  }*/

  JSValueWrapper
  operator[](const std::wstring &aProperty)
  {
    return JSValueWrapper(getMember(aProperty));
  }

  NameIterator memberNames()const
  {
    if (mCurrentValue.vt != VT_DISPATCH) {
      ANCHO_THROW(ENotAnObject());
    }
    CComQIPtr<IDispatchEx> dispex = mCurrentValue.pdispVal;
    if (!dispex) {
      ANCHO_THROW(ENotIDispatchEx());
    }
    return NameIterator(dispex);
  }

  operator JSValueWrapper()
  {
    return JSValueWrapper(mCurrentValue);
  }
protected:
  JSObjectWrapper(const JSValueWrapper &aValue)
    : JSObjectWrapperConst(aValue)
  { }
};


//Wrapper for JS objects - currently read-only access
class JSArrayWrapperConst
{
public:
  friend class JSValueWrapperConst;

  typedef detail::MemberForwardIterator NameIterator;

  JSArrayWrapper & operator=(JSValueWrapper aVal);
  /*{
    swap(*this, aVal);
    return *this;
  }*/

  JSValueWrapperConst
  operator[](int aIdx)const
  {
    return operator[](boost::lexical_cast<std::wstring>(aIdx));
  }

  JSValueWrapperConst
  operator[](const std::wstring &aProperty)const
  {
    return JSValueWrapperConst(getMember(aProperty));
  }

  size_t length()const
  {
    CComVariant len = getMember(L"length");
    if (len.vt == VT_EMPTY) {
      ANCHO_THROW(ENotAnArray());
    }
    HRESULT hr = len.ChangeType(VT_I4);
    if (FAILED(hr)) {
      ANCHO_THROW(ENotAnArray());
    }
    return static_cast<size_t>(len.lVal);
  }

  size_t size()const
  {
    return length();
  }
protected:
  JSArrayWrapperConst(const JSValueWrapperConst &aValue)
    : mCurrentValue(aValue.mCurrentValue)
  {
    if (!aValue.isArray()) {
      ANCHO_THROW(ENotAnArray());
    }
  }

  CComVariant getMember(const std::wstring &aProperty) const
  { return detail::getMember(mCurrentValue, aProperty); }

  mutable CComVariant mCurrentValue;
};

class JSArrayWrapper: public JSArrayWrapperConst
{
public:
  friend class JSValueWrapper;

  //TODO : rest of the interface methods required by 'Sequence' concept

  typedef detail::MemberForwardIterator NameIterator;

  JSArrayWrapper & operator=(JSValueWrapper aVal);
  /*{
    swap(*this, aVal);
    return *this;
  }*/

  //TODO: return assignable object
  JSValueWrapper
  operator[](int aIdx)
  {
    return operator[](boost::lexical_cast<std::wstring>(aIdx));
  }

  JSValueWrapper
  operator[](const std::wstring &aProperty)
  {
    return JSValueWrapper(getMember(aProperty));
  }

  void push_back(JSValueWrapperConst aValue)
  {
    detail::setMember(this->mCurrentValue, boost::lexical_cast<std::wstring>(this->size()), aValue.mCurrentValue);
  }
protected:
  JSArrayWrapper(const JSValueWrapper &aValue)
    : JSArrayWrapperConst(aValue)
  { }

};

inline JSValueWrapperConst::JSValueWrapperConst(const JSValueWrapper &aValue)
    : mCurrentValue(aValue.mCurrentValue)
{ /*empty*/ }

inline JSValueWrapperConst & JSValueWrapperConst::operator=(JSValueWrapperConst aVal)
{
  swap(*this, aVal);
  return *this;
}

inline  JSValueWrapperConst & JSValueWrapperConst::operator=(JSValueWrapper aVal)
{
  swap(*this, static_cast<JSValueWrapperConst>(aVal));
  return *this;
}

inline JSObjectWrapperConst JSValueWrapperConst::toObject() const
{
  if (!isObject()) {
    ANCHO_THROW(ENotAnObject());
  }
  return JSObjectWrapperConst(*this);
}

inline JSArrayWrapperConst JSValueWrapperConst::toArray() const
{
  if (!isArray()) {
    ANCHO_THROW(ENotAnObject());
  }
  return JSArrayWrapperConst(*this);
}

inline void swap(JSValueWrapperConst &aVal1, JSValueWrapperConst &aVal2)
{
  std::swap(aVal1.mCurrentValue, aVal2.mCurrentValue);
}

inline JSObjectWrapper JSValueWrapper::toObject()
{
  if (!isObject()) {
    ANCHO_THROW(ENotAnObject());
  }
  return JSObjectWrapper(*this);
}

inline JSArrayWrapper JSValueWrapper::toArray()
{
  if (!isArray()) {
    ANCHO_THROW(ENotAnArray());
  }
  return JSArrayWrapper(*this);
}

} //namespace Utils
} //namespace Ancho


