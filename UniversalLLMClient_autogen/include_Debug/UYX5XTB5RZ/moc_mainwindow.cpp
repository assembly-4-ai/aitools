/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../ui/mainwindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSMainWindowENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSMainWindowENDCLASS = QtMocHelpers::stringData(
    "MainWindow",
    "onSendClicked",
    "",
    "newChat",
    "confirm",
    "loadHistory",
    "saveHistory",
    "saveLastResponse",
    "setSystemPrompt",
    "selectServiceGemini",
    "selectServiceOllama",
    "selectServiceLMStudio",
    "selectServiceVLLM",
    "configureCurrentService",
    "modelSelectionChanged",
    "modelName"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSMainWindowENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   92,    2, 0x08,    1 /* Private */,
       3,    1,   93,    2, 0x08,    2 /* Private */,
       3,    0,   96,    2, 0x28,    4 /* Private | MethodCloned */,
       5,    0,   97,    2, 0x08,    5 /* Private */,
       6,    0,   98,    2, 0x08,    6 /* Private */,
       7,    0,   99,    2, 0x08,    7 /* Private */,
       8,    0,  100,    2, 0x08,    8 /* Private */,
       9,    0,  101,    2, 0x08,    9 /* Private */,
      10,    0,  102,    2, 0x08,   10 /* Private */,
      11,    0,  103,    2, 0x08,   11 /* Private */,
      12,    0,  104,    2, 0x08,   12 /* Private */,
      13,    0,  105,    2, 0x08,   13 /* Private */,
      14,    1,  106,    2, 0x08,   14 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   15,

       0        // eod
};

Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_CLASSMainWindowENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSMainWindowENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSMainWindowENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>,
        // method 'onSendClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'newChat'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'newChat'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadHistory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveHistory'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveLastResponse'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setSystemPrompt'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceGemini'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceOllama'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceLMStudio'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceVLLM'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'configureCurrentService'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'modelSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onSendClicked(); break;
        case 1: _t->newChat((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->newChat(); break;
        case 3: _t->loadHistory(); break;
        case 4: _t->saveHistory(); break;
        case 5: _t->saveLastResponse(); break;
        case 6: _t->setSystemPrompt(); break;
        case 7: _t->selectServiceGemini(); break;
        case 8: _t->selectServiceOllama(); break;
        case 9: _t->selectServiceLMStudio(); break;
        case 10: _t->selectServiceVLLM(); break;
        case 11: _t->configureCurrentService(); break;
        case 12: _t->modelSelectionChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSMainWindowENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}
QT_WARNING_POP
