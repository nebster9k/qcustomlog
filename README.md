# QCustomLog
Customizable logging class for Qt applications

## Overview
QCustomLog provides static methods for handling logging operations and is compatible with `qInstallMessageHandler` to intercept and process Qt messages. The class supports log buffering, log rotation, custom error handling, and the ability to override logging behavior

## Features
- Customizable log message handling
- Support for log buffering to improve performance
- Automatic log rotation based on file size and count
- Colored standard output
- Custom error handling function
- Custom timestamp formats support
- UTC time mode for consistent timestamps
- Configurable minimum log levels for console and file output
- Clean log category feature for automation like CI/CD
- Convenient macros for easy logging management
- Calculating the average time spent writing to files and rotating them
- Requires an active event loop for buffering to work correctly

## Installation
Just include `qcustomlog.h` and `qcustomlog.cpp` in your Qt project

## Usage
### Basic Setup
```cpp
#include "qcustomlog.h"

int main(int argc, char *argv[])
{
   QApplication app(argc, argv);

   // Initialize logging with log directory
   QCustomLog::initLogging("/path/to/logs");

   // Example log messages before the main event loop starts
   qInfo() << "Application started.";
   qWarning() << "This is a warning message.";
   qCritical() << "Critical error encountered!";

   // Example of log messages using custom categories, recommended usage
   // Also you can use preprocessor directives
   qInfo(QLoggingCategory("CATEGORY_NAME")).noquote() << "Hello, world!";

   return app.exec(); // logs will be processed correctly also inside the event loops and in different threads
}
```

### Included Preprocessor Directives Usage
```cpp
#include "qcustomlog.h"

#define _qclog_category "CLASS_NAME"
CLASS_NAME::CLASS_NAME(QObject* parent) : QObject(parent)
{
   logInfo("Info log with the predefined category"); // [yyyy.MM.dd HH:mm:ss.zzz] [INF] [CLASS_NAME] Info log with the predefined category
   logInfo("Info log with a custom category named DB","DB"); // [yyyy.MM.dd HH:mm:ss.zzz] [INF] [DB] Info log with a custom category named DB

   logInfo("Info log with clean category","CI/CD"); // [yyyy.MM.dd HH:mm:ss.zzz] [INF] [CI/CD] Info log with clean category
   // If "CI/CD" is set as the clean caterogy name, standard output will contain only "Info log with clean category" unchanged,
   // but log file will contain everything if allowed by the setCleanLogCategory() 2-nd argument
}
```

### Custom Error Handler
```cpp
QCustomLog::setErrorHandler([](const QString& msg) // qcustomlog error, e.g. if the log directory is not writable
{
   QApplication::exit(EXIT_FAILURE); // or QCoreApplication::exit(EXIT_FAILURE);
});
```

### Overriding the Log Behavior
You can subclass `QCustomLog` to define custom log handling behavior, such as logging to a database

```cpp
class DbCustomLog : public QCustomLog
{
protected:
   void sendLog(const QDateTime& time, const QtMsgType type, const QString& category, const QString& msg) override
   {
      // Send log to a database or external system
      // ...
      // ...
   }
};

DbCustomLog dbCustomLog;
QCustomLog::setInstance(&dbCustomLog);
```

### Setting Minimum Log Levels for Standard Output and Files
```cpp
QCustomLog::setMinLevels(QtWarningMsg, QtCriticalMsg);
```

### Enabling UTC Mode
```cpp
QCustomLog::setUtcMode(true);
```

### Clean Log Category (e.g. for CI/CD)
```cpp
QCustomLog::setCleanLogCategory("CI/CD",false); // false -> prohibit write of the "CI/CD" category in the file or overrided sendLog()
```

## Contributing
Issues and pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change

Please remember to test your changes as extensively as possible

## License
[MIT](./LICENSE)

## Copyright
(c) 2025 Dmitrii Permiakov [[nebster9k](https://github.com/nebster9k)]
