# MSVC + jsoncpp + 资源文件 makefile
VCToolsDir = E:\App\VisualStudio\IDE\VC\Tools\MSVC\14.29.30133
WindowsKits = C:\Program Files (x86)\Windows Kits\10
SDKVersion = 10.0.19041.0

# jsoncpp 路径
JSONCPP_INCLUDE = E:\App\JsonCpp\include
JSONCPP_LIB = E:\App\JsonCpp\build\lib\Release

# 编译选项（C++ 源文件）
CFLAGS = /EHsc /I"$(JSONCPP_INCLUDE)" /I"$(VCToolsDir)\include" /I"$(WindowsKits)\Include\$(SDKVersion)\ucrt" /I"$(WindowsKits)\Include\$(SDKVersion)\shared" /I"$(WindowsKits)\Include\$(SDKVersion)\um"

# 链接选项（添加 jsoncpp 库、Windows 系统库）
LFLAGS = /LIBPATH:"$(JSONCPP_LIB)" jsoncpp.lib advapi32.lib user32.lib comdlg32.lib

# 资源编译选项
RCFLAGS = /I"$(VCToolsDir)\include" /I"$(WindowsKits)\Include\$(SDKVersion)\um"

# 工具路径
CC = $(VCToolsDir)\bin\Hostx64\x64\cl.exe
LINK = $(VCToolsDir)\bin\Hostx64\x64\link.exe
RC = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\rc.exe"

TARGET = StartOnlyOne.exe
SOURCES = main.cpp
OBJECT = main.obj
RESOURCE = resource.res
RC_SOURCE = resource.rc

all: $(TARGET)

$(OBJECT): $(SOURCES)
	$(CC) $(CFLAGS) /c $< /Fo$@

$(RESOURCE): $(RC_SOURCE)
	$(RC) $(RCFLAGS) /fo $@ $<

$(TARGET): $(OBJECT) $(RESOURCE)
	$(LINK) $(OBJECT) $(RESOURCE) /OUT:$@ $(LFLAGS)

clean:
	del $(TARGET) $(OBJECT) $(RESOURCE) 2>nul

.PHONY: all clean