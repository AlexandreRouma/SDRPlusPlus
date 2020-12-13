echo OFF
copy /b/v/y build\Release\* root\
copy /b/v/y build\radio\Release\radio.dll root\modules\radio.dll
copy /b/v/y build\recorder\Release\recorder.dll root\modules\recorder.dll
copy /b/v/y build\rtl_tcp_source\Release\rtl_tcp_source.dll root\modules\rtl_tcp_source.dll
copy /b/v/y build\soapy\Release\soapy.dll root\modules\soapy.dll