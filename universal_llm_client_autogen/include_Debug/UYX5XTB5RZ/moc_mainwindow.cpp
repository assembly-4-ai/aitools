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
    "modelSelectionChanged",
    "modelName",
    "selectServiceGemini",
    "selectServiceOllama",
    "selectServiceLMStudio",
    "selectServiceVLLM",
    "configureCurrentService",
    "startOrchestrationClicked",
    "stopOrchestrationClicked",
    "handleOrchestrationStatus",
    "status",
    "handleOrchestratorCommandSuggestion",
    "command",
    "explanation",
    "handleOrchestratorMessage",
    "role",
    "message",
    "handleOrchestrationFinished",
    "success",
    "summary",
    "onMsysSendCommand",
    "onNasmSendCommand",
    "appendToMsysConsole",
    "text",
    "appendToMsysErrorConsole",
    "onMsysProcessFinished",
    "exitCode",
    "QProcess::ExitStatus",
    "exitStatus",
    "appendToNasmConsole",
    "appendToNasmErrorConsole",
    "onNasmProcessFinished",
    "configureTerminalPaths"
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
      28,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  182,    2, 0x08,    1 /* Private */,
       3,    1,  183,    2, 0x08,    2 /* Private */,
       3,    0,  186,    2, 0x28,    4 /* Private | MethodCloned */,
       5,    0,  187,    2, 0x08,    5 /* Private */,
       6,    0,  188,    2, 0x08,    6 /* Private */,
       7,    0,  189,    2, 0x08,    7 /* Private */,
       8,    0,  190,    2, 0x08,    8 /* Private */,
       9,    1,  191,    2, 0x08,    9 /* Private */,
      11,    0,  194,    2, 0x08,   11 /* Private */,
      12,    0,  195,    2, 0x08,   12 /* Private */,
      13,    0,  196,    2, 0x08,   13 /* Private */,
      14,    0,  197,    2, 0x08,   14 /* Private */,
      15,    0,  198,    2, 0x08,   15 /* Private */,
      16,    0,  199,    2, 0x08,   16 /* Private */,
      17,    0,  200,    2, 0x08,   17 /* Private */,
      18,    1,  201,    2, 0x08,   18 /* Private */,
      20,    2,  204,    2, 0x08,   20 /* Private */,
      23,    2,  209,    2, 0x08,   23 /* Private */,
      26,    2,  214,    2, 0x08,   26 /* Private */,
      29,    0,  219,    2, 0x08,   29 /* Private */,
      30,    0,  220,    2, 0x08,   30 /* Private */,
      31,    1,  221,    2, 0x08,   31 /* Private */,
      33,    1,  224,    2, 0x08,   33 /* Private */,
      34,    2,  227,    2, 0x08,   35 /* Private */,
      38,    1,  232,    2, 0x08,   38 /* Private */,
      39,    1,  235,    2, 0x08,   40 /* Private */,
      40,    2,  238,    2, 0x08,   42 /* Private */,
      41,    0,  243,    2, 0x08,   45 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   19,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   21,   22,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   24,   25,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   27,   28,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   32,
    QMetaType::Void, QMetaType::QString,   32,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 36,   35,   37,
    QMetaType::Void, QMetaType::QString,   32,
    QMetaType::Void, QMetaType::QString,   32,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 36,   35,   37,
    QMetaType::Void,

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
        // method 'modelSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
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
        // method 'startOrchestrationClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stopOrchestrationClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleOrchestrationStatus'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestratorCommandSuggestion'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestratorMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestrationFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onMsysSendCommand'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNasmSendCommand'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'appendToMsysConsole'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'appendToMsysErrorConsole'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onMsysProcessFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QProcess::ExitStatus, std::false_type>,
        // method 'appendToNasmConsole'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'appendToNasmErrorConsole'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onNasmProcessFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QProcess::ExitStatus, std::false_type>,
        // method 'configureTerminalPaths'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
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
        case 7: _t->modelSelectionChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->selectServiceGemini(); break;
        case 9: _t->selectServiceOllama(); break;
        case 10: _t->selectServiceLMStudio(); break;
        case 11: _t->selectServiceVLLM(); break;
        case 12: _t->configureCurrentService(); break;
        case 13: _t->startOrchestrationClicked(); break;
        case 14: _t->stopOrchestrationClicked(); break;
        case 15: _t->handleOrchestrationStatus((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->handleOrchestratorCommandSuggestion((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 17: _t->handleOrchestratorMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 18: _t->handleOrchestrationFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 19: _t->onMsysSendCommand(); break;
        case 20: _t->onNasmSendCommand(); break;
        case 21: _t->appendToMsysConsole((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->appendToMsysErrorConsole((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->onMsysProcessFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 24: _t->appendToNasmConsole((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->appendToNasmErrorConsole((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->onNasmProcessFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 27: _t->configureTerminalPaths(); break;
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
        if (_id < 28)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 28;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 28)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 28;
    }
    return _id;
}
QT_WARNING_POP
