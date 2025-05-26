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
    "configureCurrentService",
    "modelSelectionChanged",
    "modelName",
    "handleServiceResponse",
    "responseText",
    "requestPayload",
    "fullResponsePayload",
    "errorString",
    "handleModelsFetched",
    "modelList",
    "selectServiceGemini",
    "selectServiceOllama",
    "selectServiceLMStudio",
    "selectServiceVLLM",
    "onNewChat",
    "onOrchestratorStartClicked",
    "onOrchestratorStopClicked",
    "handleOrchestrationStatusUpdate",
    "statusMessage",
    "handleOrchestratorNeedsUserInput",
    "question",
    "suggestedResponse",
    "handleOrchestratorSuggestsLocalCommand",
    "commandToExecute",
    "handleOrchestrationFinished",
    "success",
    "summary",
    "handleOrchestratorDisplayMessage",
    "message",
    "isError",
    "onTerminalSendCommandClicked",
    "onTerminalStartShellClicked",
    "onTerminalStopShellClicked",
    "onOpenDefaultTerminal",
    "onConfigureTerminalPaths",
    "handleTerminalOutputReady",
    "output",
    "handleTerminalErrorReady",
    "errorOutput",
    "handleTerminalProcessFinished",
    "exitCode",
    "QProcess::ExitStatus",
    "exitStatus",
    "handleTerminalProcessErrorOccurred",
    "QProcess::ProcessError",
    "error"
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
      26,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  170,    2, 0x08,    1 /* Private */,
       3,    0,  171,    2, 0x08,    2 /* Private */,
       4,    1,  172,    2, 0x08,    3 /* Private */,
       6,    4,  175,    2, 0x08,    5 /* Private */,
      11,    2,  184,    2, 0x08,   10 /* Private */,
      13,    0,  189,    2, 0x08,   13 /* Private */,
      14,    0,  190,    2, 0x08,   14 /* Private */,
      15,    0,  191,    2, 0x08,   15 /* Private */,
      16,    0,  192,    2, 0x08,   16 /* Private */,
      17,    0,  193,    2, 0x08,   17 /* Private */,
      18,    0,  194,    2, 0x08,   18 /* Private */,
      19,    0,  195,    2, 0x08,   19 /* Private */,
      20,    1,  196,    2, 0x08,   20 /* Private */,
      22,    2,  199,    2, 0x08,   22 /* Private */,
      25,    1,  204,    2, 0x08,   25 /* Private */,
      27,    2,  207,    2, 0x08,   27 /* Private */,
      30,    2,  212,    2, 0x08,   30 /* Private */,
      33,    0,  217,    2, 0x08,   33 /* Private */,
      34,    0,  218,    2, 0x08,   34 /* Private */,
      35,    0,  219,    2, 0x08,   35 /* Private */,
      36,    0,  220,    2, 0x08,   36 /* Private */,
      37,    0,  221,    2, 0x08,   37 /* Private */,
      38,    1,  222,    2, 0x08,   38 /* Private */,
      40,    1,  225,    2, 0x08,   40 /* Private */,
      42,    2,  228,    2, 0x08,   42 /* Private */,
      46,    1,  233,    2, 0x08,   45 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::QString, QMetaType::QString, QMetaType::QString, QMetaType::QString,    7,    8,    9,   10,
    QMetaType::Void, QMetaType::QStringList, QMetaType::QString,   12,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   21,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,   23,   24,
    QMetaType::Void, QMetaType::QString,   26,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   28,   29,
    QMetaType::Void, QMetaType::QString, QMetaType::Bool,   31,   32,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   39,
    QMetaType::Void, QMetaType::QString,   41,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 44,   43,   45,
    QMetaType::Void, 0x80000000 | 47,   48,

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
        // method 'configureCurrentService'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'modelSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleServiceResponse'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleModelsFetched'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'selectServiceGemini'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceOllama'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceLMStudio'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'selectServiceVLLM'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNewChat'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOrchestratorStartClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOrchestratorStopClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleOrchestrationStatusUpdate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestratorNeedsUserInput'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestratorSuggestsLocalCommand'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestrationFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleOrchestratorDisplayMessage'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'onTerminalSendCommandClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTerminalStartShellClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTerminalStopShellClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOpenDefaultTerminal'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onConfigureTerminalPaths'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleTerminalOutputReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleTerminalErrorReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'handleTerminalProcessFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QProcess::ExitStatus, std::false_type>,
        // method 'handleTerminalProcessErrorOccurred'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QProcess::ProcessError, std::false_type>
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
        case 1: _t->configureCurrentService(); break;
        case 2: _t->modelSelectionChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->handleServiceResponse((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[4]))); break;
        case 4: _t->handleModelsFetched((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->selectServiceGemini(); break;
        case 6: _t->selectServiceOllama(); break;
        case 7: _t->selectServiceLMStudio(); break;
        case 8: _t->selectServiceVLLM(); break;
        case 9: _t->onNewChat(); break;
        case 10: _t->onOrchestratorStartClicked(); break;
        case 11: _t->onOrchestratorStopClicked(); break;
        case 12: _t->handleOrchestrationStatusUpdate((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->handleOrchestratorNeedsUserInput((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->handleOrchestratorSuggestsLocalCommand((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->handleOrchestrationFinished((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 16: _t->handleOrchestratorDisplayMessage((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 17: _t->onTerminalSendCommandClicked(); break;
        case 18: _t->onTerminalStartShellClicked(); break;
        case 19: _t->onTerminalStopShellClicked(); break;
        case 20: _t->onOpenDefaultTerminal(); break;
        case 21: _t->onConfigureTerminalPaths(); break;
        case 22: _t->handleTerminalOutputReady((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 23: _t->handleTerminalErrorReady((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 24: _t->handleTerminalProcessFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QProcess::ExitStatus>>(_a[2]))); break;
        case 25: _t->handleTerminalProcessErrorOccurred((*reinterpret_cast< std::add_pointer_t<QProcess::ProcessError>>(_a[1]))); break;
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
        if (_id < 26)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 26;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 26)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 26;
    }
    return _id;
}
QT_WARNING_POP
