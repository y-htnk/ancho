#pragma once

#include <map>
#include <vector>
#include <boost/variant.hpp>
#include <SimpleWrappers.h>

namespace AnchoBackgroundServer {

struct Empty {};

template<typename TType>
struct SafeVoidTraits
{
  typedef TType type;
};

template<>
struct SafeVoidTraits<void>
{
  typedef Empty type;
};

//* Representation of null value
struct null_t{null_t(){}};
inline bool operator==(const null_t &, const null_t &) { return true; }
inline bool operator!=(const null_t &, const null_t &) { return false; }

//* Variant which can contain arbitrary JSONable value (no functions)
typedef boost::make_recursive_variant<
                null_t,
                bool,
                int,
                double,
                std::wstring,
                std::vector<boost::recursive_variant_>,
                std::map<std::wstring, boost::recursive_variant_>
            >::type JSVariant;

enum JSTypeID {
  jsNull   = 0,
  jsBool   = 1,
  jsInt    = 2,
  jsDouble = 3,
  jsString = 4,
  jsArray  = 5,
  jsObject = 6
};


//* Array of JSONable values
typedef std::vector<JSVariant> JSArray;

//* JSONable object
typedef std::map<std::wstring, JSVariant> JSObject;


JSVariant convertToJSVariant(JSValue &aValue);
JSVariant convertToJSVariant(IDispatchEx &aDispatch);

namespace detail {
//forward declarations
JSObject convertObject(JSValue &aValue);
JSArray convertArray(JSValue &aValue);
JSVariant convert(JSValue &aValue);

struct ConvertToVariantVisitor
{
  typedef JSVariant result_type;

  template<typename TType>
  result_type operator()(const TType &aValue) const
  { return result_type(aValue); }

  result_type operator()(JSValue &aValue) const
  {
    if (aValue.isNull()) {
      return result_type(null_t());
    }
    ATLASSERT(aValue.isObject() || aValue.isArray());
    return convert(aValue);
  }
};

inline JSObject convertObject(JSValue &aValue)
{
  JSObject result;

  JSValue::NameIterator namesIterator = aValue.memberNames();
  JSValue::NameIterator namesEnd;
  for (; namesIterator != namesEnd; ++namesIterator) {
    result[*namesIterator] = convertToJSVariant(aValue[*namesIterator]);
  }
  return result;
}

inline JSArray convertArray(JSValue &aValue)
{
  ATLASSERT(aValue.isArray());
  JSArray result;
  size_t len = aValue.length();
  result.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    result.push_back(convertToJSVariant(aValue[i]));
  }
  return result;
}

inline JSVariant convert(JSValue &aValue)
{
  if (aValue.isObject()) {
    return convertObject(aValue);
  }
  if (aValue.isArray()) {
    return convertArray(aValue);
  }

  ANCHO_THROW(EInvalidArgument());
  return JSVariant();
}

inline JSVariant convert(IDispatchEx &aDispatch)
{
  JSValue jsvalue((CComVariant(&aDispatch)));//Double parenthese because of the "most vexing parse"
  return convert(jsvalue);
}

} //namespace detail

/** This will convert JavaScript value wrapper into native C++ representation
 **/
inline JSVariant convertToJSVariant(JSValue &aValue)
{
  return aValue.applyVisitor(detail::ConvertToVariantVisitor());
}

inline JSVariant convertToJSVariant(IDispatchEx &aDispatch)
{
  return detail::convert(aDispatch);
}

namespace detail {
/** Conversion traits implementation - generic version is undefined - compile time error when using conversion without specialization
 **/
template<typename TFrom, typename TTo>
struct ConversionTraits;

/** Specialization for cases when we don't need any conversion
 **/
template<typename TType>
struct ConversionTraits<TType, TType>
{
  static void convert(TType &aFrom, TType &aTo)
  { aTo = aFrom; }
};

template<>
struct ConversionTraits<int, CComVariant>
{
  static void convert(int &aFrom, CComVariant &aTo)
  { aTo = CComVariant(aFrom); }
};

template<>
struct ConversionTraits<std::wstring, CComVariant>
{
  static void convert(std::wstring &aFrom, CComVariant &aTo)
  { aTo = CComVariant(aFrom.c_str()); }
};

template<>
struct ConversionTraits<bool, CComVariant>
{
  static void convert(bool &aFrom, CComVariant &aTo)
  { aTo = CComVariant(aFrom); }
};

template<typename TFrom>
struct ConversionTraits<TFrom, Empty>
{
  //This will ignore the aFrom value
  static void convert(TFrom &aFrom, Empty &aTo)
  { /*empty*/ }
};
//---------------------------------------------------------------------
template<typename TFrom, bool tIsSequence, typename TTo>
struct ConversionTraitsToContainer;

template<typename TFrom>
struct ConversionTraitsToContainer<TFrom, false, std::vector<CComVariant> >
{
  static void convert(TFrom &aFrom, std::vector<CComVariant> &aTo)
  {
    //Single value to vector
    ATLASSERT(aTo.empty());
    CComVariant tmp;
    ConversionTraits<TFrom, CComVariant>::convert(aFrom, tmp);
    aTo.push_back(tmp);
  }
};

template<typename TFrom>
struct ConversionTraitsToContainer<TFrom, true, std::vector<CComVariant> >
{
  struct MemberConvertor
  {
    MemberConvertor(std::vector<CComVariant> &aData): data(aData)
    {}
    mutable std::vector<CComVariant> &data;

    template<typename T>
    void operator()(T& aArg)const
    {
      CComVariant tmp;
      ConversionTraits<T, CComVariant>::convert(aArg, tmp);
      data.push_back(tmp);
    }
  };

  static void convert(TFrom &aFrom, std::vector<CComVariant> &aTo)
  {
    ATLASSERT(aTo.empty());
    fusion::for_each(aFrom, MemberConvertor(aTo));
  }
};

template<typename TFrom>
struct ConversionTraits<TFrom, std::vector<CComVariant> >
  : ConversionTraitsToContainer<TFrom, fusion::traits::is_sequence<TFrom>::value, std::vector<CComVariant> >
{
  /* convert() inherited */
};

} //namespace detail

template<typename TFrom, typename TTo >
void convert(TFrom &aFrom, TTo &aTo)
{
  detail::ConversionTraits<TFrom, TTo>::convert(aFrom, aTo);
}

template<typename TFrom, typename TTo >
TTo convert(TFrom &aFrom)
{
  TTo tmp;
  convert(aFrom, tmp);
  return tmp;
}

}
