#pragma once

namespace AnchoBackgroundServer {

template<typename TArguments, typename TReturnValue = void>
class JavaScriptCallback
{
public:
  typedef TArguments Arguments;
  //This is a workaround for isssue with 'void' type
  //- it is incoplete type and we cannot instantiate it.
  //The 'Empty' type will be used instead of void.
  typedef typename AnchoBackgroundServer::SafeVoidTraits<TReturnValue>::type ReturnValue;

  JavaScriptCallback() {}

  JavaScriptCallback(CComPtr<IDispatch> aCallback)//: mStream(NULL)
  {
    ATLASSERT(aCallback);
    IF_FAILED_THROW(CoMarshalInterThreadInterfaceInStream(IID_IDispatch, aCallback.p, &mStream));
  }

  ReturnValue operator()(Arguments aArguments)
  {
    //TODO - ATLASSERT(... && "CoInitializeEx() not called in this thread");
    ATLASSERT(mStream.p);

    CComQIPtr<IDispatch> callback;
    IF_FAILED_THROW(CoUnmarshalInterface(mStream, IID_IDispatch, (LPVOID *) &callback));
    if (callback) {
      std::vector<CComVariant> parameters;

      DISPPARAMS params = {0};
      AnchoBackgroundServer::convert(aArguments, parameters);
      if (!parameters.empty()) {
        std::reverse(parameters.begin(), parameters.end());

        params.rgvarg = &(parameters[0]);
        params.cArgs = parameters.size();
      }

      CComVariant vtResult;
      IF_FAILED_THROW(callback->Invoke((DISPID)0, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &vtResult, NULL, NULL));
      return AnchoBackgroundServer::convert<CComVariant, ReturnValue>(vtResult);
    }
    return ReturnValue();
  }

  bool empty()const
  { return mStream.p != NULL; }
protected:
  JavaScriptCallback(JavaScriptCallback&& aCallback);
  CComPtr<IStream> mStream;
};

} //namespace AnchoBackgroundServer
