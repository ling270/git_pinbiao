QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++17

# FFTW (vcpkg, MSVC x64)
INCLUDEPATH += E:/QT_FFTW/vcpkg/installed/x64-windows/include
LIBS += -LE:/QT_FFTW/vcpkg/installed/x64-windows/lib -lfftw3

# Windows socket (for ntohl / inet APIs)
win32:LIBS += -lws2_32

# 可选：去掉 MSVC 对 fopen 的安全警告
win32:DEFINES += _CRT_SECURE_NO_WARNINGS

SOURCES += \
    crossspectrumworker.cpp \
    decimator10_poly.cpp \
    main.cpp \
    mainwindow.cpp \
    plotwidget.cpp \
    processingworker.cpp \
    qcustomplot.cpp \
    spectrumestimator.cpp \
    streamreader.cpp

HEADERS += \
    crossspectrumworker.h \
    decimator10_poly.h \
    fileio64.h \
    mainwindow.h \
    plotwidget.h \
    processingworker.h \
    qcustomplot.h \
    spectrumestimator.h \
    streamreader.h

FORMS += \
    mainwindow.ui

# Linux/Unix 才额外链接（你写了也没问题）
unix: LIBS += -lfftw3

# Windows: 构建后自动复制 FFTW DLL 到 exe 同目录
win32 {
    FFTW_BIN = E:/QT_FFTW/vcpkg/installed/x64-windows/bin
    QMAKE_POST_LINK += $$quote(cmd /c copy /Y \"$$FFTW_BIN\\fftw3.dll\" \"$$OUT_PWD\\\")
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
